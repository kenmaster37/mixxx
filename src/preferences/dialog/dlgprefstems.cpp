#include "preferences/dialog/dlgprefstems.h"

#include "moc_dlgprefstems.cpp"

#include <functional>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "stems/stemengineinstaller.h"
#include "stems/stemgenerator.h"
#include "stems/stemqueuemodel.h"

namespace {

const QStringList kCodecs = {QStringLiteral("aac"),
        QStringLiteral("mp3"),
        QStringLiteral("alac")};
const QStringList kModels = {QStringLiteral("htdemucs"),
        QStringLiteral("htdemucs_ft")};
const QStringList kProviders = {QStringLiteral("auto"),
        QStringLiteral("cuda"),
        QStringLiteral("cpu")};

/// A line edit with a "Browse" button next to it.
QWidget* makePathRow(QWidget* pParent,
        QLineEdit* pEdit,
        const QString& buttonText,
        const std::function<QString()>& choosePath) {
    auto* pRow = new QWidget(pParent);
    auto* pLayout = new QHBoxLayout(pRow);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->addWidget(pEdit, 1);
    auto* pBrowseButton = new QPushButton(buttonText, pRow);
    pLayout->addWidget(pBrowseButton);
    QObject::connect(pBrowseButton, &QPushButton::clicked, pRow, [pEdit, choosePath] {
        const QString path = choosePath();
        if (!path.isEmpty()) {
            pEdit->setText(path);
        }
    });
    return pRow;
}

} // namespace

DlgPrefStems::DlgPrefStems(QWidget* pParent, UserSettingsPointer pConfig)
        : DlgPreferencePage(pParent),
          m_pConfig(std::move(pConfig)) {
    auto* pPageLayout = new QVBoxLayout(this);

    auto* pDescriptionLabel = new QLabel(tr(
            "Stem generation creates a Native Instruments style .stem.mp4 file "
            "with four stems (drums, bass, other, vocals), so the track can be "
            "played and remixed stem by stem. Tracks are added from the library "
            "context menu: right click a selection and choose Stems > Generate "
            "stems. The jobs run one at a time and, unless disabled below, are "
            "postponed while a deck is playing."),
            this);
    pDescriptionLabel->setWordWrap(true);
    pPageLayout->addWidget(pDescriptionLabel);

    auto* pEnvironmentGroup = new QGroupBox(tr("Environment"), this);
    auto* pEnvironmentLayout = new QVBoxLayout(pEnvironmentGroup);
    m_pEnvironmentLabel = new QLabel(pEnvironmentGroup);
    m_pEnvironmentLabel->setWordWrap(true);
    pEnvironmentLayout->addWidget(m_pEnvironmentLabel);
    m_pErrorsLabel = new QLabel(pEnvironmentGroup);
    m_pErrorsLabel->setWordWrap(true);
    pEnvironmentLayout->addWidget(m_pErrorsLabel);
    auto* pEnvironmentButtonsLayout = new QHBoxLayout();
    m_pProbeButton = new QPushButton(tr("Check environment"), pEnvironmentGroup);
    pEnvironmentButtonsLayout->addWidget(m_pProbeButton);
    m_pOpenFolderButton = new QPushButton(tr("Open stem folder"), pEnvironmentGroup);
    pEnvironmentButtonsLayout->addWidget(m_pOpenFolderButton);
    pEnvironmentButtonsLayout->addStretch(1);
    pEnvironmentLayout->addLayout(pEnvironmentButtonsLayout);
    pPageLayout->addWidget(pEnvironmentGroup);

    // Next to the environment section, because it is the other way of getting
    // the same thing: with the engine installed there is nothing else to
    // prepare, and the interpreter below becomes the exception.
    auto* pEngineGroup = new QGroupBox(tr("Engine"), this);
    auto* pEngineLayout = new QVBoxLayout(pEngineGroup);
    m_pEngineStatusLabel = new QLabel(pEngineGroup);
    m_pEngineStatusLabel->setWordWrap(true);
    pEngineLayout->addWidget(m_pEngineStatusLabel);

    auto* pEngineChoicesLayout = new QFormLayout();
    m_pEngineVariantComboBox = new QComboBox(pEngineGroup);
    pEngineChoicesLayout->addRow(tr("Processor"), m_pEngineVariantComboBox);
    m_pEngineModelComboBox = new QComboBox(pEngineGroup);
    pEngineChoicesLayout->addRow(tr("Model quality"), m_pEngineModelComboBox);
    pEngineLayout->addLayout(pEngineChoicesLayout);

    m_pEngineSizeLabel = new QLabel(pEngineGroup);
    m_pEngineSizeLabel->setWordWrap(true);
    pEngineLayout->addWidget(m_pEngineSizeLabel);

    m_pEngineListWidget = new QListWidget(pEngineGroup);
    m_pEngineListWidget->setMaximumHeight(110);
    pEngineLayout->addWidget(m_pEngineListWidget);

    m_pEngineProgressBar = new QProgressBar(pEngineGroup);
    m_pEngineProgressBar->setRange(0, 100);
    pEngineLayout->addWidget(m_pEngineProgressBar);

    auto* pEngineButtonsLayout = new QHBoxLayout();
    m_pEngineInstallButton = new QPushButton(tr("Install the engine"), pEngineGroup);
    pEngineButtonsLayout->addWidget(m_pEngineInstallButton);
    m_pEngineCancelButton = new QPushButton(tr("Cancel"), pEngineGroup);
    pEngineButtonsLayout->addWidget(m_pEngineCancelButton);
    m_pEngineRemoveButton = new QPushButton(tr("Remove the engine"), pEngineGroup);
    pEngineButtonsLayout->addWidget(m_pEngineRemoveButton);
    m_pEngineFolderButton = new QPushButton(tr("Open the engine folder"), pEngineGroup);
    pEngineButtonsLayout->addWidget(m_pEngineFolderButton);
    pEngineButtonsLayout->addStretch(1);
    pEngineLayout->addLayout(pEngineButtonsLayout);
    pPageLayout->addWidget(pEngineGroup);

    auto* pSettingsGroup = new QGroupBox(tr("Stem generation"), this);
    auto* pSettingsLayout = new QFormLayout(pSettingsGroup);
    m_pEnabledCheckBox = new QCheckBox(tr("Generate stems"), pSettingsGroup);
    pSettingsLayout->addRow(m_pEnabledCheckBox);

    m_pPythonPathEdit = new QLineEdit(pSettingsGroup);
    m_pPythonPathEdit->setPlaceholderText(tr("python"));
    pSettingsLayout->addRow(tr("Interpreter"),
            makePathRow(pSettingsGroup,
                    m_pPythonPathEdit,
                    tr("Browse..."),
                    [this] {
                        return QFileDialog::getOpenFileName(this,
                                tr("Select the Python interpreter"),
                                QFileInfo(m_pPythonPathEdit->text()).absolutePath(),
                                tr("Programs (*.exe);;All files (*)"));
                    }));

    m_pCacheDirectoryEdit = new QLineEdit(pSettingsGroup);
    pSettingsLayout->addRow(tr("Stem folder"),
            makePathRow(pSettingsGroup,
                    m_pCacheDirectoryEdit,
                    tr("Browse..."),
                    [this] {
                        return QFileDialog::getExistingDirectory(this,
                                tr("Select the stem folder"),
                                m_pCacheDirectoryEdit->text());
                    }));

    m_pFfmpegPathEdit = new QLineEdit(pSettingsGroup);
    m_pFfmpegPathEdit->setPlaceholderText(QStringLiteral("ffmpeg"));
    pSettingsLayout->addRow(tr("ffmpeg"),
            makePathRow(pSettingsGroup,
                    m_pFfmpegPathEdit,
                    tr("Browse..."),
                    [this] {
                        return QFileDialog::getOpenFileName(this,
                                tr("Select ffmpeg"),
                                QFileInfo(m_pFfmpegPathEdit->text()).absolutePath(),
                                tr("Programs (*.exe);;All files (*)"));
                    }));

    m_pWriteNextToTrackCheckBox =
            new QCheckBox(tr("Write the stem file next to the track"), pSettingsGroup);
    pSettingsLayout->addRow(m_pWriteNextToTrackCheckBox);
    m_pPauseDuringPlaybackCheckBox =
            new QCheckBox(tr("Postpone the jobs while a deck is playing"), pSettingsGroup);
    pSettingsLayout->addRow(m_pPauseDuringPlaybackCheckBox);

    m_pCodecComboBox = new QComboBox(pSettingsGroup);
    m_pCodecComboBox->addItems(kCodecs);
    pSettingsLayout->addRow(tr("Codec"), m_pCodecComboBox);
    m_pModelComboBox = new QComboBox(pSettingsGroup);
    m_pModelComboBox->addItems(kModels);
    pSettingsLayout->addRow(tr("Model"), m_pModelComboBox);
    m_pProviderComboBox = new QComboBox(pSettingsGroup);
    m_pProviderComboBox->addItems(kProviders);
    pSettingsLayout->addRow(tr("Provider"), m_pProviderComboBox);
    pPageLayout->addWidget(pSettingsGroup);

    auto* pQueueGroup = new QGroupBox(tr("Queue"), this);
    auto* pQueueLayout = new QVBoxLayout(pQueueGroup);
    m_pQueueTable = new QTableWidget(0, 4, pQueueGroup);
    m_pQueueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pQueueTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_pQueueTable->setHorizontalHeaderLabels({tr("Track"),
            tr("Status"),
            tr("Progress"),
            tr("Error")});
    m_pQueueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pQueueTable->verticalHeader()->setVisible(false);
    pQueueLayout->addWidget(m_pQueueTable);
    auto* pQueueButtonsLayout = new QHBoxLayout();
    m_pCancelButton = new QPushButton(tr("Cancel"), pQueueGroup);
    pQueueButtonsLayout->addWidget(m_pCancelButton);
    m_pClearButton = new QPushButton(tr("Clear finished"), pQueueGroup);
    pQueueButtonsLayout->addWidget(m_pClearButton);
    pQueueButtonsLayout->addStretch(1);
    pQueueLayout->addLayout(pQueueButtonsLayout);
    pPageLayout->addWidget(pQueueGroup);

    connect(m_pProbeButton, &QPushButton::clicked, this, [this] {
        if (StemGenerator* pGenerator = StemGenerator::instance()) {
            pGenerator->probe();
        }
    });
    if (StemEngineInstaller* pInstaller = engineInstaller()) {
        connect(pInstaller,
                &StemEngineInstaller::stateChanged,
                this,
                &DlgPrefStems::slotEngineChanged);
        connect(pInstaller, &StemEngineInstaller::stateChanged, this, [this] {
            // With the engine installed the interpreter and the worker come out
            // of it, so both what the environment panel says and what the
            // generator thinks it can do have to be worked out again. Only when
            // the engine appears or goes away, not on every step of a download.
            StemEngineInstaller* pEngine = engineInstaller();
            if (!pEngine) {
                return;
            }
            const QString state = pEngine->state();
            if (state != QLatin1String("ready") && state != QLatin1String("outdated") &&
                    state != QLatin1String("absent")) {
                return;
            }
            refreshEnvironment();
            if (StemGenerator* pGenerator = StemGenerator::instance()) {
                pGenerator->probe();
            }
        });
        connect(pInstaller,
                &StemEngineInstaller::manifestChanged,
                this,
                &DlgPrefStems::slotEngineChanged);
        connect(pInstaller,
                &StemEngineInstaller::itemsChanged,
                this,
                &DlgPrefStems::slotEngineChanged);
        connect(pInstaller,
                &StemEngineInstaller::progressChanged,
                this,
                &DlgPrefStems::slotEngineChanged);

        connect(m_pEngineVariantComboBox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) {
                    if (m_updatingEngine) {
                        return;
                    }
                    if (StemEngineInstaller* pChosen = engineInstaller()) {
                        pChosen->setVariant(m_pEngineVariantComboBox->currentData().toString());
                    }
                });
        connect(m_pEngineModelComboBox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int) {
                    if (m_updatingEngine) {
                        return;
                    }
                    if (StemEngineInstaller* pChosen = engineInstaller()) {
                        pChosen->setModel(m_pEngineModelComboBox->currentData().toString());
                    }
                });
        connect(m_pEngineInstallButton, &QPushButton::clicked, this, [this] {
            if (StemEngineInstaller* pChosen = engineInstaller()) {
                pChosen->install();
            }
        });
        connect(m_pEngineCancelButton, &QPushButton::clicked, this, [this] {
            if (StemEngineInstaller* pChosen = engineInstaller()) {
                pChosen->cancel();
            }
        });
        connect(m_pEngineRemoveButton, &QPushButton::clicked, this, [this] {
            StemEngineInstaller* pChosen = engineInstaller();
            if (!pChosen) {
                return;
            }
            const auto answer = QMessageBox::question(this,
                    tr("Remove the engine"),
                    tr("The engine and the model it downloaded will be deleted, and "
                       "separating will need them again. Stems that were already generated "
                       "are not touched."));
            if (answer == QMessageBox::Yes) {
                pChosen->removeEngine();
            }
        });
        connect(m_pEngineFolderButton, &QPushButton::clicked, this, [this] {
            if (StemEngineInstaller* pChosen = engineInstaller()) {
                pChosen->openEngineFolder();
            }
        });
        refreshEngine();
    }

    connect(m_pOpenFolderButton, &QPushButton::clicked, this, &DlgPrefStems::slotOpenStemFolder);
    connect(m_pCancelButton, &QPushButton::clicked, this, &DlgPrefStems::slotCancelQueue);
    connect(m_pClearButton, &QPushButton::clicked, this, &DlgPrefStems::slotClearFinished);

    if (StemGenerator* pGenerator = StemGenerator::instance()) {
        connect(pGenerator,
                &StemGenerator::probeChanged,
                this,
                [this] {
                    refreshEnvironment();
                });
        connect(pGenerator,
                &StemGenerator::queueChanged,
                this,
                &DlgPrefStems::slotQueueChanged);
        if (QAbstractItemModel* pQueue = pGenerator->queue()) {
            // Progress is reported through dataChanged, which does not change
            // the number of jobs and therefore does not emit queueChanged.
            connect(pQueue,
                    &QAbstractItemModel::dataChanged,
                    this,
                    [this] {
                        refreshQueue();
                    });
        }
    }

    slotUpdate();
}

DlgPrefStems::~DlgPrefStems() = default;

StemEngineInstaller* DlgPrefStems::engineInstaller() const {
    StemGenerator* pGenerator = StemGenerator::instance();
    return pGenerator ? pGenerator->engineInstaller() : nullptr;
}

void DlgPrefStems::slotEngineChanged() {
    refreshEngine();
}

void DlgPrefStems::refreshEngine() {
    StemEngineInstaller* pInstaller = engineInstaller();
    if (!pInstaller) {
        return;
    }

    // The combos are filled from the installer, so writing them back has to be
    // held off while that happens: otherwise a redraw looks like a choice.
    m_updatingEngine = true;
    m_pEngineVariantComboBox->clear();
    const QVariantList variants = pInstaller->variants();
    for (const QVariant& value : variants) {
        const QVariantMap entry = value.toMap();
        m_pEngineVariantComboBox->addItem(
                tr("%1 (%2)")
                        .arg(entry.value(QStringLiteral("label")).toString(),
                                entry.value(QStringLiteral("sizeText")).toString()),
                entry.value(QStringLiteral("key")));
    }
    int variantIndex = m_pEngineVariantComboBox->findData(pInstaller->variant());
    m_pEngineVariantComboBox->setCurrentIndex(variantIndex < 0 ? 0 : variantIndex);

    m_pEngineModelComboBox->clear();
    const QVariantList models = pInstaller->models();
    for (const QVariant& value : models) {
        const QVariantMap entry = value.toMap();
        m_pEngineModelComboBox->addItem(
                tr("%1 (%2)")
                        .arg(entry.value(QStringLiteral("label")).toString(),
                                entry.value(QStringLiteral("sizeText")).toString()),
                entry.value(QStringLiteral("key")));
    }
    int modelIndex = m_pEngineModelComboBox->findData(pInstaller->model());
    m_pEngineModelComboBox->setCurrentIndex(modelIndex < 0 ? 0 : modelIndex);
    m_updatingEngine = false;

    QString status;
    if (!pInstaller->platformNotice().isEmpty()) {
        status = pInstaller->platformNotice();
    } else if (pInstaller->state() == QLatin1String("failed") &&
            !pInstaller->errorText().isEmpty()) {
        status = pInstaller->errorText();
    } else if (!pInstaller->statusText().isEmpty()) {
        status = pInstaller->statusText();
    } else {
        status = tr("Not installed. Separating needs this engine, or a Python of your own "
                    "with the libraries (see the interpreter below).");
    }
    if (!pInstaller->gpuName().isEmpty() && pInstaller->platformNotice().isEmpty()) {
        status += QLatin1Char(' ');
        status += tr("Graphics card found: %1.").arg(pInstaller->gpuName());
    }
    m_pEngineStatusLabel->setText(status);

    const QVariantList items = pInstaller->items();
    m_pEngineSizeLabel->setText(items.isEmpty()
                    ? QString()
                    : tr("To download: %1 files, %2. Keep about twice that free while it is "
                         "being unpacked.")
                              .arg(items.size())
                              .arg(pInstaller->totalSizeText()));

    m_pEngineListWidget->clear();
    for (const QVariant& value : items) {
        const QVariantMap entry = value.toMap();
        QString label = tr("%1  —  %2")
                                .arg(entry.value(QStringLiteral("label")).toString(),
                                        entry.value(QStringLiteral("sizeText")).toString());
        const QString state = entry.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("done")) {
            label += tr("  (ready)");
        } else if (state == QLatin1String("downloading")) {
            label += tr("  (downloading)");
        } else if (state == QLatin1String("failed")) {
            label += tr("  (failed)");
        }
        new QListWidgetItem(label, m_pEngineListWidget);
    }
    if (!pInstaller->freeSpaceText().isEmpty()) {
        new QListWidgetItem(tr("Free space on the drive: %1").arg(pInstaller->freeSpaceText()),
                m_pEngineListWidget);
    }

    const bool busy = pInstaller->busy();
    m_pEngineProgressBar->setVisible(busy);
    m_pEngineProgressBar->setValue(pInstaller->progress());
    m_pEngineInstallButton->setText(pInstaller->installed()
                    ? tr("Reinstall or update the engine")
                    : tr("Install the engine"));
    m_pEngineInstallButton->setEnabled(!busy && pInstaller->platformNotice().isEmpty());
    m_pEngineCancelButton->setEnabled(busy);
    m_pEngineRemoveButton->setEnabled(!busy && pInstaller->installed());
    m_pEngineVariantComboBox->setEnabled(!busy);
    m_pEngineModelComboBox->setEnabled(!busy);
}

void DlgPrefStems::slotUpdate() {
    refreshEngine();
    StemGenerator* pGenerator = StemGenerator::instance();
    setControlsEnabled(pGenerator != nullptr);
    if (!pGenerator) {
        refreshEnvironment();
        refreshQueue();
        return;
    }

    m_pEnabledCheckBox->setChecked(pGenerator->enabled());
    m_pPythonPathEdit->setText(pGenerator->pythonPath());
    m_pCacheDirectoryEdit->setText(m_pConfig->getValue(
            ConfigKey(QStringLiteral("[Stems]"), QStringLiteral("CacheDirectory")),
            QString()));
    m_pFfmpegPathEdit->setText(pGenerator->ffmpegPath());
    m_pWriteNextToTrackCheckBox->setChecked(pGenerator->writeNextToTrack());
    m_pPauseDuringPlaybackCheckBox->setChecked(pGenerator->pauseDuringPlayback());
    const int codecIndex = m_pCodecComboBox->findText(pGenerator->codec());
    if (codecIndex >= 0) {
        m_pCodecComboBox->setCurrentIndex(codecIndex);
    }
    const int modelIndex = m_pModelComboBox->findText(pGenerator->model());
    if (modelIndex >= 0) {
        m_pModelComboBox->setCurrentIndex(modelIndex);
    }
    const int providerIndex = m_pProviderComboBox->findText(pGenerator->provider());
    if (providerIndex >= 0) {
        m_pProviderComboBox->setCurrentIndex(providerIndex);
    }

    if (!pGenerator->probing() && !pGenerator->ready()) {
        // The environment is unknown, for example the first time the page is
        // opened before the toolbar checked it.
        pGenerator->probe();
    }

    refreshEnvironment();
    refreshQueue();
}

void DlgPrefStems::slotShow() {
    refreshEnvironment();
    refreshQueue();
    refreshEngine();
}

void DlgPrefStems::slotApply() {
    StemGenerator* pGenerator = StemGenerator::instance();
    if (!pGenerator) {
        return;
    }
    pGenerator->setPythonPath(m_pPythonPathEdit->text().trimmed());
    m_pConfig->setValue(ConfigKey(QStringLiteral("[Stems]"), QStringLiteral("CacheDirectory")),
            m_pCacheDirectoryEdit->text().trimmed());
    pGenerator->setFfmpegPath(m_pFfmpegPathEdit->text().trimmed());
    pGenerator->setWriteNextToTrack(m_pWriteNextToTrackCheckBox->isChecked());
    pGenerator->setPauseDuringPlayback(m_pPauseDuringPlaybackCheckBox->isChecked());
    pGenerator->setCodec(m_pCodecComboBox->currentText());
    pGenerator->setModel(m_pModelComboBox->currentText());
    pGenerator->setProvider(m_pProviderComboBox->currentText());
    // Enabling last: the generator starts the queue right away, so it should
    // see the settings above.
    pGenerator->setEnabled(m_pEnabledCheckBox->isChecked());
    pGenerator->probe();
    refreshEnvironment();
}

void DlgPrefStems::slotResetToDefaults() {
    m_pEnabledCheckBox->setChecked(true);
    m_pPythonPathEdit->clear();
    m_pCacheDirectoryEdit->clear();
    m_pFfmpegPathEdit->setText(QStringLiteral("ffmpeg"));
    m_pWriteNextToTrackCheckBox->setChecked(true);
    m_pPauseDuringPlaybackCheckBox->setChecked(true);
    m_pCodecComboBox->setCurrentIndex(0);
    m_pModelComboBox->setCurrentIndex(0);
    m_pProviderComboBox->setCurrentIndex(0);
    slotApply();
}

void DlgPrefStems::slotQueueChanged() {
    refreshQueue();
}

void DlgPrefStems::slotCancelQueue() {
    if (StemGenerator* pGenerator = StemGenerator::instance()) {
        pGenerator->cancel();
        pGenerator->clearFinished();
    }
    refreshQueue();
}

void DlgPrefStems::slotClearFinished() {
    if (StemGenerator* pGenerator = StemGenerator::instance()) {
        pGenerator->clearFinished();
    }
    refreshQueue();
}

void DlgPrefStems::slotOpenStemFolder() {
    if (StemGenerator* pGenerator = StemGenerator::instance()) {
        pGenerator->openCacheFolder();
    }
}

void DlgPrefStems::setControlsEnabled(bool enabled) {
    for (QWidget* pWidget : {static_cast<QWidget*>(m_pEnabledCheckBox),
                 static_cast<QWidget*>(m_pPythonPathEdit),
                 static_cast<QWidget*>(m_pCacheDirectoryEdit),
                 static_cast<QWidget*>(m_pFfmpegPathEdit),
                 static_cast<QWidget*>(m_pCodecComboBox),
                 static_cast<QWidget*>(m_pModelComboBox),
                 static_cast<QWidget*>(m_pProviderComboBox),
                 static_cast<QWidget*>(m_pWriteNextToTrackCheckBox),
                 static_cast<QWidget*>(m_pPauseDuringPlaybackCheckBox),
                 static_cast<QWidget*>(m_pProbeButton),
                 static_cast<QWidget*>(m_pCancelButton),
                 static_cast<QWidget*>(m_pClearButton),
                 static_cast<QWidget*>(m_pOpenFolderButton)}) {
        if (pWidget) {
            pWidget->setEnabled(enabled);
        }
    }
}

void DlgPrefStems::refreshEnvironment() {
    if (!m_pEnvironmentLabel || !m_pErrorsLabel) {
        return;
    }
    StemGenerator* pGenerator = StemGenerator::instance();
    if (!pGenerator) {
        m_pEnvironmentLabel->setText(tr("Stem generation is not available in this build."));
        m_pErrorsLabel->hide();
        return;
    }

    QString text;
    if (pGenerator->probing()) {
        text = tr("Checking the environment...");
    } else if (pGenerator->gpuAvailable()) {
        text = tr("GPU detected%1. The separation runs on CUDA at roughly 10x real time, "
                  "about 20 seconds for a four minute track.")
                       .arg(pGenerator->gpuName().isEmpty()
                                       ? QString()
                                       : QStringLiteral(": %1").arg(pGenerator->gpuName()));
    } else if (pGenerator->ready()) {
        text = tr("No CUDA capable GPU found, so the separation would run on the CPU: it "
                  "takes a lot of CPU time, roughly 2.4x real time, about 100 seconds for "
                  "a four minute track, some five times slower than with a GPU. Such jobs "
                  "are postponed while a deck is playing.");
    } else {
        text = tr("The environment has not been checked yet.");
    }
    text += QLatin1Char('\n') +
            tr("Interpreter: %1")
                    .arg(pGenerator->pythonPath().isEmpty()
                                    ? tr("(automatic)")
                                    : pGenerator->pythonPath());
    if (!pGenerator->detectedProvider().isEmpty()) {
        text += QLatin1Char('\n') + tr("Provider in use: %1").arg(pGenerator->detectedProvider());
    }
    m_pEnvironmentLabel->setText(text);

    const QStringList errors = pGenerator->probeErrors();
    m_pErrorsLabel->setText(errors.isEmpty()
                    ? QString()
                    : tr("Setup required:\n%1").arg(errors.join(QLatin1Char('\n'))));
    m_pErrorsLabel->setVisible(!errors.isEmpty());
}

void DlgPrefStems::refreshQueue() {
    if (!m_pQueueTable) {
        return;
    }
    QAbstractItemModel* pQueue = nullptr;
    if (StemGenerator* pGenerator = StemGenerator::instance()) {
        pQueue = pGenerator->queue();
    }
    const int rowCount = pQueue ? pQueue->rowCount() : 0;
    m_pQueueTable->setRowCount(rowCount);
    for (int row = 0; row < rowCount; row++) {
        const auto valueForRole = [pQueue, row](int role) {
            return pQueue->index(row, 0).data(role).toString();
        };
        const QString progress = valueForRole(StemQueueModel::ProgressRole);
        const QStringList texts = {valueForRole(StemQueueModel::TitleRole),
                valueForRole(StemQueueModel::StatusRole),
                progress.isEmpty() ? QString() : tr("%1 %").arg(progress),
                valueForRole(StemQueueModel::ErrorRole)};
        for (int column = 0; column < texts.size(); column++) {
            auto* pItem = m_pQueueTable->item(row, column);
            if (!pItem) {
                pItem = new QTableWidgetItem();
                m_pQueueTable->setItem(row, column, pItem);
            }
            pItem->setText(texts.at(column));
        }
    }
}
