#include "stems/tararchive.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

#include <zlib.h>

#include <array>
#include <memory>

#include "util/logger.h"

namespace {

const mixxx::Logger kLogger("TarArchive");

constexpr qsizetype kBlockSize = 512;
constexpr qsizetype kBufferSize = 64 * 1024;

/// Reads a (possibly gzipped) byte stream in blocks. tar is read front to back
/// and a gzip stream cannot be seeked, so every header and every file body is
/// taken in order.
class StreamReader {
  public:
    StreamReader(const QString& path, QString* pError)
            : m_file(path) {
        if (!m_file.open(QIODevice::ReadOnly)) {
            *pError = QStringLiteral("No se puede abrir %1").arg(path);
            return;
        }
        QByteArray head = m_file.peek(2);
        if (head.size() == 2 && static_cast<unsigned char>(head.at(0)) == 0x1f &&
                static_cast<unsigned char>(head.at(1)) == 0x8b) {
            m_compressed = true;
            m_stream = std::make_unique<z_stream>();
            std::memset(m_stream.get(), 0, sizeof(z_stream));
            // 15 window bits plus 32 lets zlib decide between gzip and zlib on
            // its own, which costs nothing and accepts both.
            if (inflateInit2(m_stream.get(), 15 + 32) != Z_OK) {
                *pError = QStringLiteral("No se puede descomprimir %1").arg(path);
                m_stream.reset();
            }
        }
        m_ok = pError->isEmpty();
    }

    ~StreamReader() {
        if (m_stream) {
            inflateEnd(m_stream.get());
        }
    }

    StreamReader(const StreamReader&) = delete;
    StreamReader& operator=(const StreamReader&) = delete;

    bool ok() const {
        return m_ok;
    }

    /// Bytes taken from the file so far, compressed ones, for the progress bar.
    qint64 consumedBytes() const {
        return m_consumed;
    }

    qint64 totalBytes() const {
        return m_file.size();
    }

    /// Fills ``buffer`` completely, or returns false at the end of the stream.
    /// A short read that is not the end is an error, and tar cannot recover
    /// from it because the block boundaries would be lost.
    bool read(char* buffer, qsizetype size) {
        qsizetype filled = 0;
        while (filled < size) {
            if (!m_decompressed.isEmpty()) {
                const qsizetype take = qMin(m_decompressed.size(), size - filled);
                std::memcpy(buffer + filled, m_decompressed.constData(), take);
                m_decompressed.remove(0, take);
                filled += take;
                continue;
            }
            if (m_finished) {
                return false;
            }
            if (!fill()) {
                m_ok = false;
                return false;
            }
        }
        return true;
    }

    /// Consumes and discards ``size`` bytes, for the bodies that are skipped.
    bool skip(qsizetype size) {
        std::array<char, kBufferSize> scratch{};
        qsizetype left = size;
        while (left > 0) {
            const qsizetype take = qMin<qsizetype>(left, kBufferSize);
            if (!read(scratch.data(), take)) {
                return false;
            }
            left -= take;
        }
        return true;
    }

    /// Copies ``size`` bytes into ``device``.
    bool copyTo(QIODevice* device, qsizetype size) {
        qsizetype left = size;
        while (left > 0) {
            const qsizetype take = qMin<qsizetype>(left, kBufferSize);
            std::array<char, kBufferSize> chunk{};
            if (!read(chunk.data(), take)) {
                return false;
            }
            if (device->write(chunk.data(), take) != take) {
                return false;
            }
            left -= take;
        }
        return true;
    }

  private:
    bool fill() {
        std::array<char, kBufferSize> input{};
        if (!m_compressed) {
            const qint64 got = m_file.read(input.data(), kBufferSize);
            if (got <= 0) {
                m_finished = true;
                return false;
            }
            m_consumed += got;
            m_decompressed.append(input.data(), static_cast<qsizetype>(got));
            return true;
        }

        if (m_stream->avail_in == 0) {
            const qint64 got = m_file.read(input.data(), kBufferSize);
            if (got <= 0) {
                m_finished = true;
                return false;
            }
            m_consumed += got;
            m_stream->next_in = reinterpret_cast<Bytef*>(input.data());
            m_stream->avail_in = static_cast<uInt>(got);
            // The compressed chunk has to stay alive until the next read, so it
            // is kept in the member instead of the local buffer.
            m_input = QByteArray(input.data(), static_cast<qsizetype>(got));
            m_stream->next_in = reinterpret_cast<Bytef*>(m_input.data());
            m_stream->avail_in = static_cast<uInt>(m_input.size());
        }

        std::array<char, kBufferSize> output{};
        m_stream->next_out = reinterpret_cast<Bytef*>(output.data());
        m_stream->avail_out = kBufferSize;
        const int result = inflate(m_stream.get(), Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR) {
            kLogger.warning() << "inflate failed with" << result;
            m_finished = true;
            return false;
        }
        const qsizetype produced = kBufferSize - static_cast<qsizetype>(m_stream->avail_out);
        if (produced > 0) {
            m_decompressed.append(output.data(), produced);
        }
        if (result == Z_STREAM_END && produced == 0) {
            m_finished = true;
            return false;
        }
        return produced > 0 || m_stream->avail_in > 0 || !m_finished;
    }

    QFile m_file;
    std::unique_ptr<z_stream> m_stream;
    QByteArray m_input;
    QByteArray m_decompressed;
    qint64 m_consumed = 0;
    bool m_compressed = false;
    bool m_finished = false;
    bool m_ok = true;
};

QString field(const char* block, qsizetype offset, qsizetype length) {
    qsizetype end = offset;
    while (end < offset + length && block[end] != '\0') {
        ++end;
    }
    return QString::fromUtf8(block + offset, static_cast<int>(end - offset));
}

/// tar numbers are octal text, except the ones too large for the field, which
/// are written with the high bit of the first byte set and base 256 after it.
qint64 number(const char* block, qsizetype offset, qsizetype length) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(block + offset);
    if (length > 0 && (bytes[0] & 0x80) != 0) {
        qint64 value = bytes[0] & 0x7f;
        for (qsizetype i = 1; i < length; ++i) {
            value = (value << 8) | bytes[i];
        }
        return value;
    }
    qint64 value = 0;
    bool any = false;
    for (qsizetype i = 0; i < length; ++i) {
        const char c = block[offset + i];
        if (c == ' ' || c == '\0') {
            if (any) {
                break;
            }
            continue;
        }
        if (c < '0' || c > '7') {
            break;
        }
        any = true;
        value = value * 8 + (c - '0');
    }
    return value;
}

bool isZeroBlock(const char* block) {
    for (qsizetype i = 0; i < kBlockSize; ++i) {
        if (block[i] != '\0') {
            return false;
        }
    }
    return true;
}

/// Applies the mode of the entry, which is what keeps `bin/python3` runnable
/// and the shared libraries readable after the install.
void applyMode(const QString& path, qint64 mode) {
    QFile::Permissions permissions;
    const auto add = [&permissions](qint64 bits, QFileDevice::Permission read,
                            QFileDevice::Permission write,
                            QFileDevice::Permission execute) {
        if ((bits & 4) != 0) {
            permissions |= read;
        }
        if ((bits & 2) != 0) {
            permissions |= write;
        }
        if ((bits & 1) != 0) {
            permissions |= execute;
        }
    };
    add((mode >> 6) & 7, QFileDevice::ReadOwner, QFileDevice::WriteOwner,
            QFileDevice::ExeOwner);
    add((mode >> 3) & 7, QFileDevice::ReadGroup, QFileDevice::WriteGroup,
            QFileDevice::ExeGroup);
    add(mode & 7, QFileDevice::ReadOther, QFileDevice::WriteOther,
            QFileDevice::ExeOther);
    if (permissions != QFile::permissions(path)) {
        QFile::setPermissions(path, permissions);
    }
}

/// The path of an entry as a path inside the destination, or an empty string
/// when it has to be skipped.
QString destinationPath(const QString& destination,
        const QString& stripPrefix,
        QString name,
        QString* pError) {
    while (name.startsWith(QLatin1Char('/'))) {
        name.remove(0, 1);
    }
    while (name.startsWith(QLatin1String("./"))) {
        name.remove(0, 2);
    }
    if (!stripPrefix.isEmpty()) {
        const QString prefix = stripPrefix + QLatin1Char('/');
        if (name == stripPrefix) {
            return QString();
        }
        if (!name.startsWith(prefix)) {
            // Anything outside the directory that is being stripped does not
            // belong to the layout the engine expects.
            return QString();
        }
        name.remove(0, prefix.size());
    }
    if (name.isEmpty()) {
        return QString();
    }
    const QStringList parts = name.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        if (part == QLatin1String("..")) {
            *pError = QStringLiteral("El archivo tiene una ruta que sale de la carpeta: %1")
                              .arg(name);
            return QString();
        }
    }
    return QDir(destination).absoluteFilePath(parts.join(QLatin1Char('/')));
}

/// Reads a PAX or GNU extended header body, which is a list of "len key=value".
class ExtendedHeader {
  public:
    void feed(const QByteArray& body) {
        qsizetype offset = 0;
        while (offset < body.size()) {
            const qsizetype space = body.indexOf(' ', offset);
            if (space < 0) {
                break;
            }
            bool ok = false;
            const qsizetype length = QString::fromLatin1(
                    body.mid(offset, space - offset)).toInt(&ok);
            if (!ok || length <= 0 || offset + length > body.size()) {
                break;
            }
            const QByteArray record = body.mid(space + 1, length - (space - offset) - 2);
            const qsizetype equals = record.indexOf('=');
            if (equals > 0) {
                const QString key = QString::fromUtf8(record.left(equals));
                const QString value = QString::fromUtf8(record.mid(equals + 1));
                if (key == QLatin1String("path")) {
                    path = value;
                } else if (key == QLatin1String("linkpath")) {
                    linkPath = value;
                } else if (key == QLatin1String("size")) {
                    size = value.toLongLong();
                } else if (key.startsWith(QLatin1String("SCHILY.xattr."))) {
                    // Ignored on purpose: the engine has no use for them.
                }
            }
            offset += length;
        }
    }

    void clear() {
        path.clear();
        linkPath.clear();
        size = -1;
    }

    QString path;
    QString linkPath;
    qint64 size = -1;
};

} // namespace

bool TarArchive::extract(const QString& archivePath,
        const QString& destination,
        const QString& stripPrefix,
        QString* pError,
        const ProgressCallback& onProgress) {
    QString error;
    StreamReader reader(archivePath, &error);
    if (!reader.ok()) {
        if (pError) {
            *pError = error;
        }
        return false;
    }
    if (!QDir().mkpath(destination)) {
        if (pError) {
            *pError = QStringLiteral("No se puede crear %1").arg(destination);
        }
        return false;
    }

    ExtendedHeader next;
    QString longName;
    QString longLink;

    while (true) {
        std::array<char, kBlockSize> block{};
        if (!reader.read(block.data(), kBlockSize)) {
            if (pError && pError->isEmpty()) {
                *pError = QStringLiteral("El archivo termina de forma inesperada");
            }
            return false;
        }
        if (isZeroBlock(block.data())) {
            // Two zero blocks mark the end, but one is enough to stop.
            break;
        }

        const QString headerName = field(block.data(), 0, 100);
        const QString prefix = field(block.data(), 345, 155);
        const qint64 size = number(block.data(), 124, 12);
        const qint64 mode = number(block.data(), 100, 8);
        const char type = block[156];
        const QString linkName = field(block.data(), 157, 100);

        if (type == 'x' || type == 'X' || type == 'g') {
            // Extended header: its body describes the entry that comes next
            // (or, for 'g', defaults for the rest of the archive).
            QByteArray body;
            body.resize(static_cast<int>(qMax<qint64>(0, size)));
            if (size > 0 && !reader.read(body.data(), static_cast<qsizetype>(size))) {
                if (pError) {
                    *pError = QStringLiteral("No se puede leer una cabecera extendida");
                }
                return false;
            }
            if (size % kBlockSize != 0) {
                reader.skip(kBlockSize - (size % kBlockSize));
            }
            if (type != 'g') {
                next.feed(body);
            }
            continue;
        }
        if (type == 'L' || type == 'K') {
            // GNU long name or long link: the body is the string, and it applies
            // to the entry that comes right after.
            QByteArray body;
            body.resize(static_cast<int>(qMax<qint64>(0, size)));
            if (size > 0 && !reader.read(body.data(), static_cast<qsizetype>(size))) {
                if (pError) {
                    *pError = QStringLiteral("No se puede leer un nombre largo");
                }
                return false;
            }
            if (size % kBlockSize != 0) {
                reader.skip(kBlockSize - (size % kBlockSize));
            }
            const QString value = QString::fromUtf8(body).chopped(1);
            if (type == 'L') {
                longName = value;
            } else {
                longLink = value;
            }
            continue;
        }

        QString name = headerName;
        if (!prefix.isEmpty()) {
            name = prefix + QLatin1Char('/') + name;
        }
        if (!longName.isEmpty()) {
            name = longName;
        }
        QString link = linkName;
        if (!longLink.isEmpty()) {
            link = longLink;
        }
        if (!next.path.isEmpty()) {
            name = next.path;
        }
        if (!next.linkPath.isEmpty()) {
            link = next.linkPath;
        }
        qint64 bodySize = size;
        if (next.size >= 0) {
            bodySize = next.size;
        }
        longName.clear();
        longLink.clear();
        next.clear();

        const bool isDirectory = type == '5' || name.endsWith(QLatin1Char('/'));
        const QString target = destinationPath(destination, stripPrefix, name, &error);
        if (!error.isEmpty()) {
            if (pError) {
                *pError = error;
            }
            return false;
        }
        const qint64 stored = isDirectory ? 0 : bodySize;

        if (target.isEmpty()) {
            if (!reader.skip(stored + (kBlockSize - stored % kBlockSize) % kBlockSize)) {
                if (pError) {
                    *pError = QStringLiteral("El archivo termina de forma inesperada");
                }
                return false;
            }
            continue;
        }

        bool ok = true;
        switch (type) {
        case '5':
            ok = QDir().mkpath(target);
            break;
        case '2': { // symbolic link
            QDir().mkpath(QFileInfo(target).absolutePath());
#ifdef Q_OS_UNIX
            QFile::remove(target);
            ok = ::symlink(link.toLocal8Bit().constData(),
                         target.toLocal8Bit().constData()) == 0;
#else
            // Tarballs are what the engine is made of on Linux, so reaching
            // this on Windows means an archive for another platform was fed in.
            // Skipping the link keeps the rest of it usable, which is better
            // than throwing the whole extraction away.
            kLogger.warning() << "Symbolic links are not supported on this platform,"
                              << "skipping" << name;
            ok = true;
#endif
            break;
        }
        case '1': { // hard link, resolved inside the same archive
            const QString source = destinationPath(destination, stripPrefix, link, &error);
            QDir().mkpath(QFileInfo(target).absolutePath());
#ifdef Q_OS_UNIX
            QFile::remove(target);
            ok = ::link(source.toLocal8Bit().constData(),
                         target.toLocal8Bit().constData()) == 0;
#else
            ok = QFile::copy(source, target);
#endif
            break;
        }
        case '0':
        case '\0':
        case '7': { // regular file, or a contiguous one
            QDir().mkpath(QFileInfo(target).absolutePath());
            // Written through a temporary file so an interrupted install never
            // leaves a half written program behind.
            QSaveFile file(target);
            if (!file.open(QIODevice::WriteOnly)) {
                ok = false;
                break;
            }
            ok = reader.copyTo(&file, stored) && file.commit();
            if (!ok) {
                break;
            }
            applyMode(target, mode);
            break;
        }
        default:
            // Device nodes, fifos and the rest are not part of the engine.
            ok = reader.skip(stored);
            break;
        }
        if (!ok) {
            if (pError) {
                *pError = QStringLiteral("No se puede escribir %1").arg(target);
            }
            return false;
        }

        const qsizetype padding = (kBlockSize - stored % kBlockSize) % kBlockSize;
        if (padding > 0 && !reader.skip(padding)) {
            if (pError) {
                *pError = QStringLiteral("El archivo termina de forma inesperada");
            }
            return false;
        }

        // Reported once per entry, which for a release of python-build-standalone
        // is often enough to keep a progress bar alive, and the returned value is
        // the only way to stop: this runs on the caller's thread.
        if (onProgress && !onProgress(reader.consumedBytes(), reader.totalBytes())) {
            if (pError) {
                pError->clear();
            }
            return false;
        }
    }
    return true;
}
