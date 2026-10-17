#include "stems/stemengineinstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>


#include "stems/tararchive.h"
#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("StemEngineInstaller");

// How many times a cut transfer is asked for again before giving up. Each
// attempt starts the file over: a partial file is thrown away rather than
// resumed, because resuming needs ranges, a server that honours them and a way
// to know whether the bytes on disk are the ones that were asked for.
constexpr int kDownloadAttempts = 3;
constexpr int kRetryDelayMs = 2000;

// Unpacking runs on the main thread but gives it back often: one entry is one
// file, and the biggest of them is a few hundred megabytes.
constexpr int kUnpackSliceMs = 50;

// The downloads are kept next to the engine so a reinstall does not fetch them
// again, and together with what they unpack to they need about twice their size.
constexpr qint64 kDiskHeadroom = 300LL * 1024 * 1024;

const QString kEngineFolderName = QStringLiteral("stems/engine");

/// Whether a downloaded file is a gzipped tarball instead of a ZIP, which is
/// the case for everything the Linux engine is made of.
bool isTarArchive(const QString& path) {
    const QString lower = path.toLower();
    return lower.endsWith(QLatin1String(".tar.gz")) ||
            lower.endsWith(QLatin1String(".tgz"));
}

QString friendlyName(const QString& name) {
    // The list is meant to be read by someone who wants stems, not to be a
    // mirror of the file names. Only the ones that need explaining are named
    // here; the rest are recognisable as they are.
    static const QHash<QString, QString> kLabels = {
            {QStringLiteral("python-3.11.9-embed-amd64.zip"),
                    QStringLiteral("Python 3.11 (no se instala, va dentro del motor)")},
            {QStringLiteral("imageio-ffmpeg"),
                    QStringLiteral("ffmpeg (escribe el archivo .stem.mp4)")},
            {QStringLiteral("demucs-onnx"), QStringLiteral("separador de stems")},
    };
    return kLabels.value(name, name);
}

QString sizeText(qint64 bytes) {
    if (bytes >= 1000LL * 1000 * 1000) {
        return QStringLiteral("%1 GB").arg(bytes / 1e9, 0, 'f', 2);
    }
    if (bytes >= 1000LL * 1000) {
        return QStringLiteral("%1 MB").arg(bytes / 1e6, 0, 'f', 1);
    }
    return QStringLiteral("%1 KB").arg(bytes / 1e3, 0, 'f', 0);
}

/// The nearest folder that exists, so the free space of a folder that is about
/// to be created can be asked about.
QString existingAncestor(const QString& path) {
    QDir dir(path);
    while (!dir.exists()) {
        if (!dir.cdUp()) {
            return QString();
        }
    }
    return dir.absolutePath();
}

/// The folder Mixxx keeps its resources in, which is where the engine scripts
/// and the manifest are installed to. The resource path is the resolved one
/// from the settings, not the bare command line value: a development tree
/// resolves it from CMakeCache.txt, and only the installed package passes it on
/// the command line.
QString resourceStemsFolder(const QString& resourcePath) {
    const QDir resourceDir(resourcePath);
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
            resourceDir.absoluteFilePath(QStringLiteral("stems")),
            resourceDir.absoluteFilePath(QStringLiteral("../res/stems")),
            appDir.absoluteFilePath(QStringLiteral("stems")),
            appDir.absoluteFilePath(QStringLiteral("../res/stems")),
            appDir.absoluteFilePath(QStringLiteral("../../res/stems")),
            appDir.absoluteFilePath(QStringLiteral("../share/mixxx/stems")),
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(QDir(candidate).absoluteFilePath(QStringLiteral("stems_worker.py")))) {
            return QDir(candidate).absolutePath();
        }
    }
    return QString();
}

QString gpuProgram() {
#ifdef Q_OS_WIN
    return QDir(QDir(QString::fromLocal8Bit(qgetenv("SystemRoot")))
                        .absoluteFilePath(QStringLiteral("system32")))
            .absoluteFilePath(QStringLiteral("nvidia-smi.exe"));
#else
    return QStringLiteral("nvidia-smi");
#endif
}

} // namespace

StemEngineInstaller::StemEngineInstaller(UserSettingsPointer pConfig, QObject* pParent)
        : QObject(pParent), m_pConfig(std::move(pConfig)) {
    m_pUnpackTimer = new QTimer(this);
    m_pUnpackTimer->setSingleShot(true);
    connect(m_pUnpackTimer, &QTimer::timeout, this, &StemEngineInstaller::slotUnpackStep);
    refresh();
    detectGpu();
}

StemEngineInstaller::~StemEngineInstaller() {
    if (m_pReply) {
        m_pReply->abort();
    }
}

void StemEngineInstaller::detectGpu() {
    if (m_pGpuProbe) {
        return;
    }
    const QString program = gpuProgram();
    if (program.isEmpty() || !QFileInfo::exists(program)) {
        return;
    }
    m_pGpuProbe = new QProcess(this);
    connect(m_pGpuProbe,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &StemEngineInstaller::slotGpuProbeFinished);
    m_pGpuProbe->start(program,
            {QStringLiteral("--query-gpu=name"), QStringLiteral("--format=csv,noheader")});
}

void StemEngineInstaller::slotGpuProbeFinished() {
    QProcess* process = m_pGpuProbe;
    m_pGpuProbe = nullptr;
    if (!process) {
        return;
    }
    const QString output = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
    process->deleteLater();
    if (output.isEmpty()) {
        return;
    }
    m_gpuName = output.section(QLatin1Char('\n'), 0, 0).trimmed();
    emit hardwareChanged();

    // Until the answer arrives the CUDA build cannot be the one suggested, so a
    // machine with a card would be left offering the processor. It is changed
    // here, and only while the user has not made a choice of their own, and only
    // when the list of files of this platform actually has that variant: the
    // Linux one carries the processor build on its own for now.
    if (!m_variantChosenByUser && !installed() && !m_gpuName.isEmpty() &&
            m_variant == QLatin1String("cpu") && manifestHasVariant(QStringLiteral("nvidia"))) {
        m_variant = QStringLiteral("nvidia");
        rebuildItems();
        emit selectionChanged();
        emit manifestChanged();
    }
}

// ------------------------------------------------------------- where it is --

QString StemEngineInstaller::folderFor(const UserSettingsPointer& pConfig) {
    if (!pConfig) {
        return QString();
    }
    return QDir(pConfig->getSettingsPath()).absoluteFilePath(kEngineFolderName);
}

QString StemEngineInstaller::pythonFor(const UserSettingsPointer& pConfig) {
    const QString folder = folderFor(pConfig);
    if (folder.isEmpty()) {
        return QString();
    }
    const QDir runtime(QDir(folder).absoluteFilePath(QStringLiteral("runtime")));
    const QStringList candidates = {
            QStringLiteral("python.exe"),
            QStringLiteral("bin/python3"),
            QStringLiteral("bin/python"),
    };
    for (const QString& candidate : candidates) {
        const QString path = runtime.absoluteFilePath(candidate);
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

QString StemEngineInstaller::ffmpegFor(const UserSettingsPointer& pConfig) {
    const QString folder = folderFor(pConfig);
    if (folder.isEmpty()) {
        return QString();
    }
    const QDir binaries(QDir(folder).absoluteFilePath(
            QStringLiteral("site-packages/imageio_ffmpeg/binaries")));
    if (!binaries.exists()) {
        return QString();
    }
    const QStringList entries = binaries.entryList(QDir::Files);
    for (const QString& entry : entries) {
        // The folder also carries a README and an __init__, which sort before
        // the program: taking "the first file that is not a .txt" hands the
        // worker a path that is not an executable, and the run fails later on
        // with "the system cannot find the file specified".
        if (!entry.startsWith(QLatin1String("ffmpeg"), Qt::CaseInsensitive)) {
            continue;
        }
        if (entry.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive) ||
                entry.endsWith(QLatin1String(".md"), Qt::CaseInsensitive)) {
            continue;
        }
        return binaries.absoluteFilePath(entry);
    }
    return QString();
}

QString StemEngineInstaller::engineFolder() const {
    return folderFor(m_pConfig);
}

// ------------------------------------------------------------------ state --

void StemEngineInstaller::setState(const QString& state, const QString& error) {
    const bool changedState = m_state != state;
    m_state = state;
    m_errorText = error;
    if (changedState) {
        kLogger.info() << "stem engine installer:" << state
                       << (error.isEmpty() ? QString() : error);
        emit stateChanged();
    }
}

void StemEngineInstaller::refresh() {
    loadManifest();
    detectGpu();

    const QString statePath = QDir(engineFolder()).absoluteFilePath(QStringLiteral("installed.json"));
    QString installedVersion;
    QString installedVariant;
    QFile stateFile(statePath);
    if (stateFile.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(stateFile.readAll()).object();
        installedVersion = object.value(QStringLiteral("engine_version")).toString();
        installedVariant = object.value(QStringLiteral("variant")).toString();
    }
    m_variant = installedVariant.isEmpty() ? m_variant : installedVariant;

    if (!QFileInfo::exists(pythonFor(m_pConfig))) {
        setState(QStringLiteral("absent"));
        m_statusText = QString();
        emit progressChanged();
        return;
    }
    if (!installedVersion.isEmpty() && installedVersion != availableVersion()) {
        setState(QStringLiteral("outdated"));
    } else {
        setState(QStringLiteral("ready"));
    }
    m_statusText = QStringLiteral("Motor instalado en %1").arg(engineFolder());
    emit progressChanged();
}

bool StemEngineInstaller::loadManifest() {
    // One list of files per platform. They cannot be one: the interpreter is a
    // zip of an embedded Python on Windows and a tarball of
    // python-build-standalone on Linux, and the wheels are built for one
    // platform only. The Windows file keeps its name so an existing install
    // keeps working; the others are read when the platform is not Windows.
    const QDir folder(resourceStemsFolder(m_pConfig->getResourcePath()));
    const bool isWindows = QSysInfo::kernelType() == QLatin1String("winnt");
    QStringList candidates;
    if (!isWindows) {
        candidates << QStringLiteral("engine-linux.json");
    }
    candidates << QStringLiteral("engine.json");
    QString path;
    for (const QString& candidate : candidates) {
        const QString file = folder.absoluteFilePath(candidate);
        if (QFileInfo::exists(file)) {
            path = file;
            break;
        }
    }
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        m_manifestLoaded = false;
        emit manifestChanged();
        return false;
    }
    m_manifest = QJsonDocument::fromJson(file.readAll()).object();
    m_manifestLoaded = !m_manifest.isEmpty();

    if (m_variant.isEmpty() || !manifestHasVariant(m_variant)) {
        // On a machine with an NVIDIA card the CUDA build is worth several times
        // the download, so it is the one offered by default, as long as this
        // platform has it.
        const bool nvidia = manifestHasVariant(QStringLiteral("nvidia"));
        m_variant = nvidia && gpuSuggested() ? QStringLiteral("nvidia")
                                             : QStringLiteral("cpu");
        if (!manifestHasVariant(m_variant)) {
            m_variant = m_manifest.value(QStringLiteral("compute")).toObject().keys().value(0);
        }
    }
    if (m_model.isEmpty()) {
        m_model = m_manifest.value(QStringLiteral("models")).toObject().contains(
                          QStringLiteral("htdemucs_fp16weights"))
                ? QStringLiteral("htdemucs_fp16weights")
                : QStringLiteral("htdemucs");
    }
    emit manifestChanged();
    return m_manifestLoaded;
}

QString StemEngineInstaller::availableVersion() const {
    return m_manifest.value(QStringLiteral("engine_version")).toString();
}

bool StemEngineInstaller::installed() const {
    return !pythonFor(m_pConfig).isEmpty();
}

bool StemEngineInstaller::needsUpdate() const {
    return installed() && m_state == QLatin1String("outdated");
}

QString StemEngineInstaller::installedVersion() const {
    QFile file(QDir(engineFolder()).absoluteFilePath(QStringLiteral("installed.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QJsonDocument::fromJson(file.readAll())
            .object()
            .value(QStringLiteral("engine_version"))
            .toString();
}

QString StemEngineInstaller::freeSpaceText() const {
    const QString existing = existingAncestor(engineFolder());
    if (existing.isEmpty()) {
        return QString();
    }
    const QStorageInfo storage(existing);
    if (!storage.isValid()) {
        return QString();
    }
    return sizeText(storage.bytesAvailable());
}

bool StemEngineInstaller::manifestHasVariant(const QString& variant) const {
    if (variant.isEmpty()) {
        return false;
    }
    return m_manifest.value(QStringLiteral("compute")).toObject().contains(variant);
}

QString StemEngineInstaller::platformNotice() const {
    const QString target = m_manifest.value(QStringLiteral("target"))
                                   .toObject()
                                   .value(QStringLiteral("platform"))
                                   .toString();
    if (target.isEmpty()) {
        return QString();
    }
    const bool isWindows = QSysInfo::kernelType() == QLatin1String("winnt");
    const bool isLinux = QSysInfo::kernelType() == QLatin1String("linux");
    if (target.startsWith(QLatin1String("win")) && !isWindows) {
        return QStringLiteral("Este motor es para Windows. En este sistema todavia no hay "
                              "version del motor, asi que la instalacion no esta disponible.");
    }
    if (target.contains(QLatin1String("linux")) && !isLinux) {
        return QStringLiteral("Este motor es para Linux y este sistema no lo es.");
    }
    if (target == QLatin1String("win_amd64") && QSysInfo::currentCpuArchitecture() != QLatin1String("x86_64")) {
        return QStringLiteral("Este motor es para Windows de 64 bits.");
    }
    return QString();
}

// -------------------------------------------------------------- selection --

void StemEngineInstaller::setVariant(const QString& variant) {
    if (m_variant == variant || busy()) {
        return;
    }
    m_variantChosenByUser = true;
    m_variant = variant;
    rebuildItems();
    emit selectionChanged();
}

void StemEngineInstaller::setModel(const QString& model) {
    if (m_model == model || busy()) {
        return;
    }
    m_model = model;
    rebuildItems();
    // The size next to each processor option depends on the model, so the list
    // of processors has to be read again too.
    emit selectionChanged();
    emit manifestChanged();
}

QVariantList StemEngineInstaller::variants() const {
    QVariantList list;
    const QJsonObject compute = m_manifest.value(QStringLiteral("compute")).toObject();
    const QJsonObject totals = m_manifest.value(QStringLiteral("totals")).toObject();
    for (const QString& key : {QStringLiteral("cpu"), QStringLiteral("nvidia")}) {
        const QJsonObject entry = compute.value(key).toObject();
        if (entry.isEmpty()) {
            continue;
        }
        const double total = totals.value(key).toObject().value(m_model).toDouble();
        QVariantMap map;
        map.insert(QStringLiteral("key"), key);
        map.insert(QStringLiteral("label"), entry.value(QStringLiteral("label")).toString());
        map.insert(QStringLiteral("sizeText"), sizeText(static_cast<qint64>(total)));
        map.insert(QStringLiteral("recommended"), key == QLatin1String("nvidia") && gpuSuggested());
        list.append(map);
    }
    return list;
}

QVariantList StemEngineInstaller::models() const {
    QVariantList list;
    const QJsonObject models = m_manifest.value(QStringLiteral("models")).toObject();
    for (auto iterator = models.constBegin(); iterator != models.constEnd(); ++iterator) {
        const QJsonObject entry = iterator.value().toObject();
        QVariantMap map;
        map.insert(QStringLiteral("key"), iterator.key());
        map.insert(QStringLiteral("label"), entry.value(QStringLiteral("label")).toString());
        map.insert(QStringLiteral("sizeText"),
                sizeText(entry.value(QStringLiteral("size")).toVariant().toLongLong()));
        list.append(map);
    }
    return list;
}

void StemEngineInstaller::rebuildItems() {
    m_items.clear();
    m_bytesTotal = 0;
    if (m_manifest.isEmpty()) {
        emit itemsChanged();
        emit selectionChanged();
        return;
    }

    const auto append = [this](const QJsonObject& object, const QString& kind,
                                const QString& target) {
        Item item;
        item.name = object.value(QStringLiteral("name")).toString();
        item.version = object.value(QStringLiteral("version")).toString();
        item.url = object.value(QStringLiteral("url")).toString();
        item.sha256 = object.value(QStringLiteral("sha256")).toString();
        item.size = object.value(QStringLiteral("size")).toVariant().toLongLong();
        item.kind = kind;
        item.target = target;
        item.filename = object.value(QStringLiteral("filename")).toString();
        item.stripPrefix = object.value(QStringLiteral("strip_prefix")).toString();
        item.state = QStringLiteral("pending");
        m_bytesTotal += item.size;
        m_items.append(item);
    };

    const QJsonObject runtime = m_manifest.value(QStringLiteral("runtime")).toObject();
    if (!runtime.isEmpty()) {
        append(runtime, QStringLiteral("runtime"),
                runtime.value(QStringLiteral("unpack_to")).toString());
    }
    const QJsonArray common = m_manifest.value(QStringLiteral("common")).toArray();
    for (const QJsonValue& value : common) {
        append(value.toObject(), QStringLiteral("wheel"), QStringLiteral("site-packages"));
    }
    const QJsonArray compute = m_manifest.value(QStringLiteral("compute"))
                                       .toObject()
                                       .value(m_variant)
                                       .toObject()
                                       .value(QStringLiteral("items"))
                                       .toArray();
    for (const QJsonValue& value : compute) {
        append(value.toObject(), QStringLiteral("wheel"), QStringLiteral("site-packages"));
    }
    const QJsonObject model = m_manifest.value(QStringLiteral("models"))
                                      .toObject()
                                      .value(m_model)
                                      .toObject();
    if (!model.isEmpty()) {
        append(model, QStringLiteral("model"), QStringLiteral("models"));
    }

    emit itemsChanged();
    emit selectionChanged();
}

QString StemEngineInstaller::totalSizeText() const {
    return sizeText(m_bytesTotal);
}

double StemEngineInstaller::totalSizeMb() const {
    return m_bytesTotal / 1e6;
}

QVariantList StemEngineInstaller::items() const {
    QVariantList list;
    for (const Item& item : m_items) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), item.name);
        map.insert(QStringLiteral("label"), friendlyName(item.name));
        map.insert(QStringLiteral("version"), item.version);
        map.insert(QStringLiteral("sizeText"), sizeText(item.size));
        map.insert(QStringLiteral("state"), item.state);
        list.append(map);
    }
    return list;
}

// -------------------------------------------------------------- installing --

QStringList StemEngineInstaller::requiredHosts() const {
    QStringList hosts;
    for (const Item& item : m_items) {
        const QString host = QUrl(item.url).host();
        if (!host.isEmpty() && !hosts.contains(host)) {
            hosts.append(host);
        }
    }
    return hosts;
}

void StemEngineInstaller::install() {
    if (busy()) {
        return;
    }
    if (m_manifest.isEmpty() && !loadManifest()) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No encuentro la lista de archivos del motor (engine.json). "
                               "La instalacion de Mixxx parece incompleta."));
        return;
    }
    const QString notice = platformNotice();
    if (!notice.isEmpty()) {
        setState(QStringLiteral("failed"), notice);
        return;
    }
    rebuildItems();
    if (m_items.isEmpty()) {
        setState(QStringLiteral("failed"), QStringLiteral("La lista de archivos esta vacia."));
        return;
    }

    const QString existing = existingAncestor(engineFolder());
    if (!existing.isEmpty()) {
        const QStorageInfo storage(existing);
        if (storage.isValid()) {
            const qint64 needed = m_bytesTotal * 2 + kDiskHeadroom;
            if (storage.bytesAvailable() < needed) {
                setState(QStringLiteral("failed"),
                        QStringLiteral("Hacen falta unos %1 libres (lo que se descarga mas lo "
                                       "que ocupa ya descomprimido) y quedan %2.")
                                .arg(sizeText(needed), sizeText(storage.bytesAvailable())));
                return;
            }
        }
    }

    // A variant brings different wheels for the same import package
    // (onnxruntime and onnxruntime-gpu), so unpacking one over the other would
    // leave a mixture of both.
    QFile stateFile(QDir(engineFolder()).absoluteFilePath(QStringLiteral("installed.json")));
    if (stateFile.open(QIODevice::ReadOnly)) {
        const QString installedVariant = QJsonDocument::fromJson(stateFile.readAll())
                                                 .object()
                                                 .value(QStringLiteral("variant"))
                                                 .toString();
        if (!installedVariant.isEmpty() && installedVariant != m_variant) {
            const QString sitePackages =
                    QDir(engineFolder()).absoluteFilePath(QStringLiteral("site-packages"));
            if (QDir(sitePackages).exists()) {
                kLogger.info() << "variant changed, clearing" << sitePackages;
                QDir(sitePackages).removeRecursively();
            }
        }
    }

    for (Item& item : m_items) {
        item.state = QStringLiteral("pending");
    }
    emit itemsChanged();

    m_bytesDone = 0;
    m_currentItem = 0;
    m_attempt = 0;
    m_progress = 0;
    m_hostsToCheck = requiredHosts();
    m_hostsChecked.clear();
    setState(QStringLiteral("installing"));
    m_statusText = QStringLiteral("Comprobando la conexion...");
    emit progressChanged();
    startNetworkCheck();
}

void StemEngineInstaller::startNetworkCheck() {
    // One request per host rather than one per file: the question is whether
    // there is a network at all, and finding out by way of the twenty third
    // archive would waste the download of the first twenty two.
    if (m_hostsToCheck.isEmpty()) {
        startDownload();
        return;
    }
    const QString host = m_hostsToCheck.takeFirst();
    m_hostsChecked.append(host);
    QNetworkRequest request(QUrl(QStringLiteral("https://%1/").arg(host)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network.head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, host] {
        const QVariant status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        reply->deleteLater();
        // Any answer at all means the host was reached; a server that says "not
        // found" for its own root is still a server that answered.
        if (status.isValid() && status.toInt() > 0) {
            kLogger.info() << "stem engine: network is up via" << host;
            startDownload();
            return;
        }
        if (m_hostsToCheck.isEmpty()) {
            setState(QStringLiteral("failed"),
                    QStringLiteral("Para instalar el motor hace falta conexion a internet, y no "
                                   "responde ninguno de estos servidores: %1.")
                            .arg(m_hostsChecked.join(QStringLiteral(", "))));
            return;
        }
        startNetworkCheck();
    });
}

QString StemEngineInstaller::cachePathFor(const Item& item) const {
    // The archive name is the last part of the URL, which is what the servers
    // use and what keeps two files with the same version apart.
    const QString name = QUrl(item.url).fileName();
    return QDir(QDir(engineFolder()).absoluteFilePath(QStringLiteral("cache")))
            .absoluteFilePath(name);
}

bool StemEngineInstaller::archiveIsCached(const Item& item) const {
    const QString path = cachePathFor(item);
    const QFileInfo info(path);
    if (!info.exists() || info.size() != item.size) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }
    return QString::fromLatin1(hash.result().toHex()) == item.sha256;
}

void StemEngineInstaller::startDownload() {
    if (m_currentItem >= m_items.size()) {
        startUnpack();
        return;
    }
    Item& item = m_items[m_currentItem];
    if (archiveIsCached(item)) {
        item.state = QStringLiteral("done");
        m_bytesDone += item.size;
        emit itemsChanged();
        ++m_currentItem;
        startDownload();
        return;
    }

    const QString cache = cachePathFor(item);
    QDir().mkpath(QFileInfo(cache).absolutePath());
    QFile::remove(cache + QStringLiteral(".part"));
    m_download.setFileName(cache + QStringLiteral(".part"));
    if (!m_download.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No se puede escribir en %1: %2")
                        .arg(cache, m_download.errorString()));
        return;
    }
    m_hash.reset();

    item.state = QStringLiteral("downloading");
    emit itemsChanged();
    m_statusText = QStringLiteral("Descargando %1 (%2)").arg(friendlyName(item.name),
            sizeText(item.size));
    emit progressChanged();

    QNetworkRequest request(QUrl(item.url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
            QNetworkRequest::NoLessSafeRedirectPolicy);
    // A cut transfer that gets going again resets this, so it only has to be
    // generous enough for the slowest server.
    request.setTransferTimeout(60000);
    m_pReply = m_network.get(request);
    connect(m_pReply, &QNetworkReply::readyRead, this, &StemEngineInstaller::slotDownloadReadyRead);
    connect(m_pReply, &QNetworkReply::downloadProgress,
            this, &StemEngineInstaller::slotDownloadProgress);
    connect(m_pReply, &QNetworkReply::finished, this, &StemEngineInstaller::slotDownloadFinished);
}

void StemEngineInstaller::slotDownloadReadyRead() {
    if (!m_pReply) {
        return;
    }
    const QByteArray data = m_pReply->readAll();
    if (data.isEmpty()) {
        return;
    }
    m_hash.addData(data);
    if (m_download.write(data) != data.size()) {
        m_pReply->abort();
    }
}

void StemEngineInstaller::slotDownloadProgress(qint64 received, qint64 total) {
    Q_UNUSED(total);
    if (m_currentItem < 0 || m_currentItem >= m_items.size()) {
        return;
    }
    const qint64 done = m_bytesDone + received;
    const qint64 raw = m_bytesTotal > 0 ? done * 100 / m_bytesTotal : 0;
    m_progress = static_cast<int>(qBound(qint64(0), raw, qint64(100)));
    m_statusText = QStringLiteral("Descargando %1: %2 de %3")
                           .arg(m_items[m_currentItem].name, sizeText(done),
                                   sizeText(m_bytesTotal));
    emit progressChanged();
}

void StemEngineInstaller::slotDownloadFinished() {
    QNetworkReply* reply = m_pReply;
    m_pReply = nullptr;
    if (!reply) {
        return;
    }
    const QString archive = cachePathFor(m_items[m_currentItem]);
    const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QNetworkReply::NetworkError error = reply->error();
    reply->deleteLater();

    m_download.close();
    if (error == QNetworkReply::OperationCanceledError) {
        // cancel() has already said what happened and where things stand.
        return;
    }

    if (error != QNetworkReply::NoError) {
        const int httpStatus = status.toInt();
        const bool fatal = (httpStatus == 404 || httpStatus == 403 ||
                (httpStatus >= 400 && httpStatus < 500));
        if (fatal) {
            QFile::remove(archive + QStringLiteral(".part"));
            failDownload(QStringLiteral("El servidor respondio %1 para %2, el archivo ya no "
                                        "esta ahi. La lista del motor (engine.json) esta "
                                        "desactualizada.")
                                 .arg(httpStatus)
                                 .arg(m_items[m_currentItem].url));
            return;
        }
        failDownload(QStringLiteral("Se corto la descarga de %1 (%2).")
                             .arg(m_items[m_currentItem].name, reply->errorString()));
        return;
    }

    if (m_download.error() != QFileDevice::NoError) {
        QFile::remove(archive + QStringLiteral(".part"));
        failDownload(QStringLiteral("No se puede escribir %1: %2")
                             .arg(archive, m_download.errorString()));
        return;
    }

    const QString sha256 = QString::fromLatin1(m_hash.result().toHex());
    const qint64 written = QFileInfo(archive + QStringLiteral(".part")).size();
    if (sha256 != m_items[m_currentItem].sha256) {
        QFile::remove(archive + QStringLiteral(".part"));
        failDownload(QStringLiteral("%1 llego incompleto o danado.").arg(m_items[m_currentItem].name));
        return;
    }
    if (m_items[m_currentItem].size > 0 && written != m_items[m_currentItem].size) {
        QFile::remove(archive + QStringLiteral(".part"));
        failDownload(QStringLiteral("%1 llego con un tamano distinto al esperado.")
                             .arg(m_items[m_currentItem].name));
        return;
    }
    // A file left behind by an earlier attempt has to go first: the comparison
    // above is what decided it is not the one that was asked for.
    QFile::remove(archive);
    if (!QFile::rename(archive + QStringLiteral(".part"), archive)) {
        QFile::remove(archive + QStringLiteral(".part"));
        failDownload(QStringLiteral("No se puede guardar %1 en el cache.").arg(archive));
        return;
    }

    m_items[m_currentItem].state = QStringLiteral("done");
    m_bytesDone += m_items[m_currentItem].size;
    emit itemsChanged();
    ++m_currentItem;
    m_attempt = 0;
    startDownload();
}

void StemEngineInstaller::failDownload(const QString& reason) {
    ++m_attempt;
    if (m_attempt < kDownloadAttempts) {
        // Starting the file over is the whole of the recovery: whatever arrived
        // is thrown away and the same URL is asked for again.
        m_statusText = QStringLiteral("%1 Intento %2 de %3, se empieza de nuevo.")
                               .arg(reason)
                               .arg(m_attempt + 1)
                               .arg(kDownloadAttempts);
        emit progressChanged();
        QTimer::singleShot(kRetryDelayMs, this, &StemEngineInstaller::startDownload);
        return;
    }
    m_items[m_currentItem].state = QStringLiteral("failed");
    if (m_currentItem >= 0 && m_currentItem < m_items.size()) {
        QFile::remove(cachePathFor(m_items[m_currentItem]) + QStringLiteral(".part"));
    }
    setState(QStringLiteral("failed"),
            QStringLiteral("%1 Se intento %2 veces. Si la conexion esta bien, vuelve a "
                           "intentarlo mas tarde.")
                    .arg(reason)
                    .arg(kDownloadAttempts));
}

void StemEngineInstaller::cancel() {
    if (!busy()) {
        return;
    }
    m_unpackCancelled = true;
    if (m_pReply) {
        QNetworkReply* reply = m_pReply;
        m_pReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_download.isOpen()) {
        m_download.close();
    }
    if (m_currentItem >= 0 && m_currentItem < m_items.size()) {
        QFile::remove(cachePathFor(m_items[m_currentItem]) + QStringLiteral(".part"));
    }
    m_pUnpackTimer->stop();
    m_statusText = QStringLiteral("Cancelado. Lo que ya se habia descargado sigue en el "
                                  "cache, asi que volver a empezar es mas rapido.");
    m_state = QString();
    setState(installed() ? QStringLiteral("ready") : QStringLiteral("absent"),
            QStringLiteral("Instalacion cancelada."));
    emit progressChanged();
}

void StemEngineInstaller::removeEngine() {
    if (busy()) {
        return;
    }
    QDir(engineFolder()).removeRecursively();
    m_statusText.clear();
    setState(QStringLiteral("absent"));
    emit progressChanged();
}

void StemEngineInstaller::openEngineFolder() const {
    const QString folder = engineFolder();
    if (folder.isEmpty()) {
        return;
    }
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

// -------------------------------------------------------------- unpacking --

void StemEngineInstaller::startUnpack() {
    m_unpackItemIndex = -1;
    m_unpackEntry = 0;
    m_unpackCancelled = false;
    m_zip.close();
    m_statusText = QStringLiteral("Descomprimiendo...");
    m_progress = 90;
    emit progressChanged();
    m_pUnpackTimer->start(0);
}

void StemEngineInstaller::slotUnpackStep() {
    m_unpackBudget.restart();
    while (true) {
        if (!unpackOneItem()) {
            // A cancelled install has already said what happened, and finishing
            // here would report it as if it had gone through.
            if (!m_unpackCancelled) {
                finish();
            }
            return;
        }
        if (m_unpackBudget.elapsed() >= kUnpackSliceMs) {
            // Hand the event loop back: the window keeps painting and the user
            // can still cancel, without a thread and its locks.
            m_pUnpackTimer->start(0);
            return;
        }
    }
}

bool StemEngineInstaller::unpackOneItem() {
    while (true) {
        if (m_zip.isOpen()) {
            const QVector<ZipArchive::Entry>& entries = m_zip.entries();
            if (m_unpackEntry >= entries.size()) {
                // Nothing left in this archive: the tail of the loop moves on to
                // the next item, and that is also the only place that counts
                // them.
                m_zip.close();
                continue;
            }
            const ZipArchive::Entry& entry = entries.at(m_unpackEntry);
            const QString root = QDir(engineFolder()).absoluteFilePath(m_items[m_unpackItemIndex].target);
            if (!m_zip.extract(entry, root)) {
                setState(QStringLiteral("failed"),
                        QStringLiteral("No se puede descomprimir %1: %2")
                                .arg(m_items[m_unpackItemIndex].name, m_zip.errorString()));
                return false;
            }
            ++m_unpackEntry;
            const int percent = entries.isEmpty()
                    ? 0
                    : m_unpackEntry * 100 / entries.size();
            m_statusText = QStringLiteral("Descomprimiendo %1: %2%")
                                   .arg(m_items[m_unpackItemIndex].name)
                                   .arg(percent);
            emit progressChanged();
            return true;
        }

        ++m_unpackItemIndex;
        if (m_unpackItemIndex >= m_items.size()) {
            return false;
        }
        const Item& item = m_items[m_unpackItemIndex];
        const QString archive = cachePathFor(item);
        if (item.kind == QLatin1String("model")) {
            const QString folder = QDir(engineFolder()).absoluteFilePath(item.target);
            QDir().mkpath(folder);
            const QString target = QDir(folder).absoluteFilePath(
                    item.filename.isEmpty() ? item.name : item.filename);
            QFile::remove(target);
            if (!QFile::copy(archive, target)) {
                setState(QStringLiteral("failed"),
                        QStringLiteral("No se puede colocar el modelo en %1").arg(target));
                return false;
            }
            m_statusText = QStringLiteral("Modelo instalado");
            emit progressChanged();
            continue;
        }
        m_zip.close();
        if (isTarArchive(archive)) {
            if (!unpackTarItem(item, archive)) {
                return false;
            }
            continue;
        }
        if (!m_zip.open(archive)) {
            setState(QStringLiteral("failed"),
                    QStringLiteral("No se puede abrir %1: %2").arg(archive, m_zip.errorString()));
            return false;
        }
        m_unpackEntry = 0;
        m_statusText = QStringLiteral("Descomprimiendo %1").arg(item.name);
        emit progressChanged();
    }
}

bool StemEngineInstaller::unpackTarItem(const Item& item, const QString& archive) {
    const QString root = QDir(engineFolder()).absoluteFilePath(item.target);
    if (!QDir().mkpath(root)) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No se puede crear %1").arg(root));
        return false;
    }

    // The extraction runs in one go, unlike the entry by entry loop of the ZIP
    // reader, so the timer that drives it has to stand still: it would fire
    // again from inside the event loop pumped below and start a second
    // extraction on top of this one.
    m_pUnpackTimer->stop();
    QString error;
    const bool ok = TarArchive::extract(archive,
            root,
            item.stripPrefix,
            &error,
            [this, &item](qint64 done, qint64 total) {
                if (!m_unpackCancelled && total > 0) {
                    const int percent = static_cast<int>(done * 100 / total);
                    m_statusText = QStringLiteral("Descomprimiendo %1: %2%")
                                           .arg(item.name)
                                           .arg(percent);
                    m_progress = 90 + percent * 9 / 100;
                    emit progressChanged();
                }
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                return !m_unpackCancelled;
            });
    if (ok) {
        m_statusText = QStringLiteral("Descomprimido %1").arg(item.name);
        emit progressChanged();
        m_pUnpackTimer->start(0);
        return true;
    }
    if (m_unpackCancelled) {
        // cancel() has already told the user, and there is nothing to undo here:
        // the next install extracts over what was left.
        return false;
    }
    setState(QStringLiteral("failed"),
            QStringLiteral("No se puede descomprimir %1: %2").arg(item.name, error));
    return false;
}

void StemEngineInstaller::makeEngineProgramsExecutable() const {
#ifdef Q_OS_UNIX
    // The encoder travels inside the imageio_ffmpeg wheel, and a ZIP does not
    // carry the executable bit, so it has to be set here or the container
    // cannot be written. The interpreter and its programs come from a tarball,
    // which does carry it.
    const QDir binaries(QDir(engineFolder()).absoluteFilePath(
            QStringLiteral("site-packages/imageio_ffmpeg/binaries")));
    if (!binaries.exists()) {
        return;
    }
    for (const QString& entry : binaries.entryList(QDir::Files)) {
        if (!entry.startsWith(QLatin1String("ffmpeg"))) {
            continue;
        }
        const QString program = binaries.absoluteFilePath(entry);
        QFile::setPermissions(program,
                QFile::permissions(program) | QFileDevice::ExeOwner |
                        QFileDevice::ExeGroup | QFileDevice::ExeOther);
    }
#endif
}

bool StemEngineInstaller::writeEnginePath() {
    const QString runtime = QDir(engineFolder()).absoluteFilePath(QStringLiteral("runtime"));
    const QDir folder(runtime);
    const QStringList archives = folder.entryList({QStringLiteral("python3*.zip")}, QDir::Files);
    const QString standardLibrary = archives.isEmpty()
            ? QStringLiteral("python311.zip")
            : archives.first();
    QString pathFile = standardLibrary;
    pathFile.chop(4);
    pathFile += QStringLiteral("._pth");

    // With a _pth file next to the interpreter, Python ignores PYTHONPATH and
    // the user site folder, and this list is the whole of its search path. app
    // comes before site-packages on purpose: the engine's own modules have to
    // win over a wheel with the same name, which is how the model shim stands
    // in front of huggingface_hub.
    const QStringList lines = {
            standardLibrary,
            QStringLiteral("."),
            QStringLiteral("../app"),
            QStringLiteral("../site-packages"),
            QStringLiteral("import site"),
    };
    QFile file(folder.absoluteFilePath(pathFile));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No se puede preparar el interprete: %1").arg(file.errorString()));
        return false;
    }
    file.write(lines.join(QLatin1Char('\n')).toUtf8());
    file.write("\n");
    return true;
}

bool StemEngineInstaller::copyEngineCode() {
    const QString source = resourceStemsFolder(m_pConfig->getResourcePath());
    if (source.isEmpty()) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No encuentro los scripts del motor (stems_worker.py)."));
        return false;
    }
    const QString app = QDir(engineFolder()).absoluteFilePath(QStringLiteral("app"));
    QDir().mkpath(app);

    const QStringList files = {
            QStringLiteral("stems_worker.py"),
            QStringLiteral("stem_mp4.py"),
    };
    for (const QString& name : files) {
        const QString target = QDir(app).absoluteFilePath(name);
        QFile::remove(target);
        if (!QFile::copy(QDir(source).absoluteFilePath(name), target)) {
            setState(QStringLiteral("failed"),
                    QStringLiteral("No se puede copiar %1 al motor.").arg(name));
            return false;
        }
    }

    // The stand-in for huggingface_hub travels with the worker: it is what lets
    // the engine find an installed model without the whole client and its
    // fifteen dependencies.
    const QString shimSource = QDir(source).absoluteFilePath(QStringLiteral("huggingface_hub"));
    const QString shimTarget = QDir(app).absoluteFilePath(QStringLiteral("huggingface_hub"));
    if (QDir(shimSource).exists()) {
        QDir(shimTarget).removeRecursively();
        if (!QDir().mkpath(shimTarget)) {
            setState(QStringLiteral("failed"),
                    QStringLiteral("No se puede preparar %1").arg(shimTarget));
            return false;
        }
        const QDir from(shimSource);
        for (const QString& name : from.entryList({QStringLiteral("*.py")}, QDir::Files)) {
            const QString target = QDir(shimTarget).absoluteFilePath(name);
            QFile::remove(target);
            if (!QFile::copy(from.absoluteFilePath(name), target)) {
                setState(QStringLiteral("failed"),
                        QStringLiteral("No se puede copiar %1 al motor.").arg(name));
                return false;
            }
        }
    }
    return true;
}

bool StemEngineInstaller::writeStateFile() {
    // The stamp is written with an explicit format: the Qt::ISODate overload of
    // QDateTime::toString is ambiguous enough here to confuse the compiler about
    // which QJsonObject::insert is being called.
    const QString stamp = QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"));
    QJsonObject state;
    state.insert(QStringLiteral("engine_version"), QJsonValue(availableVersion()));
    state.insert(QStringLiteral("protocol"), m_manifest.value(QStringLiteral("protocol")));
    state.insert(QStringLiteral("variant"), QJsonValue(m_variant));
    state.insert(QStringLiteral("model"), QJsonValue(m_model));
    state.insert(QStringLiteral("installed"), QJsonValue(stamp));
    QFile file(QDir(engineFolder()).absoluteFilePath(QStringLiteral("installed.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setState(QStringLiteral("failed"),
                QStringLiteral("No se puede anotar la instalacion: %1").arg(file.errorString()));
        return false;
    }
    file.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
    return true;
}

void StemEngineInstaller::finish() {
    if (!writeEnginePath()) {
        return;
    }
    if (!copyEngineCode()) {
        return;
    }
    makeEngineProgramsExecutable();
    if (!writeStateFile()) {
        return;
    }
    if (pythonFor(m_pConfig).isEmpty()) {
        setState(QStringLiteral("failed"),
                QStringLiteral("La instalacion termino pero el interprete no esta donde se "
                               "espera (%1).")
                        .arg(engineFolder()));
        return;
    }
    m_progress = 100;
    m_statusText = QStringLiteral("Motor instalado. La separacion usara %1.")
                           .arg(m_variant == QLatin1String("nvidia")
                                           ? QStringLiteral("la tarjeta NVIDIA")
                                           : QStringLiteral("el procesador"));
    setState(QStringLiteral("ready"));
    emit progressChanged();
}

#include "moc_stemengineinstaller.cpp"
