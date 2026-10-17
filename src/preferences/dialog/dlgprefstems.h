#pragma once

#include <QWidget>

#include "preferences/dialog/dlgpreferencepage.h"
#include "preferences/usersettings.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTableWidget;
class StemEngineInstaller;

/// Preferences page for stem generation, see StemGenerator.
///
/// The separation itself runs in an external worker process, so this page is
/// mostly about showing what the generator found out about the environment and
/// what is in the queue, plus the settings that are handed over to the worker.
class DlgPrefStems : public DlgPreferencePage {
    Q_OBJECT
  public:
    DlgPrefStems(QWidget* pParent, UserSettingsPointer pConfig);
    ~DlgPrefStems() override;

  public slots:
    void slotUpdate() override;
    void slotApply() override;
    void slotResetToDefaults() override;
    void slotShow() override;

  private slots:
    void slotQueueChanged();
    void slotCancelQueue();
    void slotClearFinished();
    void slotOpenStemFolder();
    /// Redraws the engine section, whatever changed on the installer.
    void slotEngineChanged();

  private:
    void refreshEnvironment();
    void refreshQueue();
    void refreshEngine();
    void setControlsEnabled(bool enabled);
    StemEngineInstaller* engineInstaller() const;

    UserSettingsPointer m_pConfig;

    QLabel* m_pEnvironmentLabel{nullptr};
    QLabel* m_pErrorsLabel{nullptr};
    QCheckBox* m_pEnabledCheckBox{nullptr};
    QLineEdit* m_pPythonPathEdit{nullptr};
    QLineEdit* m_pCacheDirectoryEdit{nullptr};
    QLineEdit* m_pFfmpegPathEdit{nullptr};
    QComboBox* m_pCodecComboBox{nullptr};
    QComboBox* m_pModelComboBox{nullptr};
    QComboBox* m_pProviderComboBox{nullptr};
    QCheckBox* m_pWriteNextToTrackCheckBox{nullptr};
    QCheckBox* m_pPauseDuringPlaybackCheckBox{nullptr};
    QPushButton* m_pProbeButton{nullptr};
    QPushButton* m_pCancelButton{nullptr};
    QPushButton* m_pClearButton{nullptr};
    QPushButton* m_pOpenFolderButton{nullptr};
    QTableWidget* m_pQueueTable{nullptr};

    QLabel* m_pEngineStatusLabel{nullptr};
    QComboBox* m_pEngineVariantComboBox{nullptr};
    QComboBox* m_pEngineModelComboBox{nullptr};
    QLabel* m_pEngineSizeLabel{nullptr};
    QListWidget* m_pEngineListWidget{nullptr};
    QProgressBar* m_pEngineProgressBar{nullptr};
    QPushButton* m_pEngineInstallButton{nullptr};
    QPushButton* m_pEngineCancelButton{nullptr};
    QPushButton* m_pEngineRemoveButton{nullptr};
    QPushButton* m_pEngineFolderButton{nullptr};
    /// Set while the widgets are filled from the installer, so that writing them
    /// back does not look like a choice the user made.
    bool m_updatingEngine{false};
};
