#pragma once

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "preferences/usersettings.h"
#include "stems/ziparchive.h"

class QNetworkReply;
class QProcess;
class QTimer;

/// Installs the self contained stem engine.
///
/// The engine is a folder holding its own interpreter, its own libraries and
/// its own model, so that separating a track does not ask the user to have
/// Python installed. This class is the whole of the installation: it reads the
/// list of files from ``res/stems/engine.json``, checks that there is a network
/// and room on the disk *before* fetching anything, downloads each file and
/// verifies its checksum, unpacks it, and leaves the folder the way
/// StemGenerator expects to find it.
///
/// It is deliberately not a copy of ``install.ps1``: the manifest is the same
/// idea, but the download restarts a cut file from the beginning instead of
/// resuming it, and every message tells apart "there is no network" from "the
/// server answered with an error" from "the file is no longer there".
class StemEngineInstaller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString variant READ variant WRITE setVariant NOTIFY selectionChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList variants READ variants NOTIFY manifestChanged)
    Q_PROPERTY(QVariantList models READ models NOTIFY manifestChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY selectionChanged)
    Q_PROPERTY(double totalSizeMb READ totalSizeMb NOTIFY selectionChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString engineFolder READ engineFolder CONSTANT)
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY stateChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY manifestChanged)
    Q_PROPERTY(bool needsUpdate READ needsUpdate NOTIFY stateChanged)
    Q_PROPERTY(bool installed READ installed NOTIFY stateChanged)
    Q_PROPERTY(bool manifestLoaded READ manifestLoaded NOTIFY manifestChanged)
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY hardwareChanged)
    Q_PROPERTY(bool gpuSuggested READ gpuSuggested NOTIFY hardwareChanged)
    Q_PROPERTY(QString freeSpaceText READ freeSpaceText NOTIFY stateChanged)
    Q_PROPERTY(QString platformNotice READ platformNotice NOTIFY manifestChanged)

  public:
    explicit StemEngineInstaller(UserSettingsPointer pConfig, QObject* pParent = nullptr);
    ~StemEngineInstaller() override;

    QString state() const {
        return m_state;
    }
    bool busy() const {
        return m_state == QLatin1String("installing");
    }
    QString variant() const {
        return m_variant;
    }
    void setVariant(const QString& variant);
    QString model() const {
        return m_model;
    }
    void setModel(const QString& model);
    QVariantList variants() const;
    QVariantList models() const;
    QVariantList items() const;
    QString totalSizeText() const;
    double totalSizeMb() const;
    int progress() const {
        return m_progress;
    }
    QString statusText() const {
        return m_statusText;
    }
    QString errorText() const {
        return m_errorText;
    }
    QString engineFolder() const;
    QString installedVersion() const;
    QString availableVersion() const;
    bool needsUpdate() const;
    bool installed() const;
    bool manifestLoaded() const {
        return m_manifestLoaded;
    }
    QString gpuName() const {
        return m_gpuName;
    }
    bool gpuSuggested() const {
        return !m_gpuName.isEmpty();
    }
    QString freeSpaceText() const;
    QString platformNotice() const;

    /// Where the engine lives; empty when nothing is installed.
    static QString folderFor(const UserSettingsPointer& pConfig);
    /// The interpreter inside an installed engine, empty when there is none.
    static QString pythonFor(const UserSettingsPointer& pConfig);
    /// The ffmpeg that came with the engine, empty when there is none.
    static QString ffmpegFor(const UserSettingsPointer& pConfig);

    /// Reads the manifest and the installed state. No network involved.
    Q_INVOKABLE void refresh();
    /// Downloads and unpacks what the current selection needs.
    Q_INVOKABLE void install();
    /// Stops the installation and leaves nothing half done.
    Q_INVOKABLE void cancel();
    /// Deletes the engine. Meant for testing and for starting over.
    Q_INVOKABLE void removeEngine();
    Q_INVOKABLE void openEngineFolder() const;

  signals:
    void stateChanged();
    void manifestChanged();
    void selectionChanged();
    void itemsChanged();
    void progressChanged();
    void hardwareChanged();

  private slots:
    void slotDownloadReadyRead();
    void slotDownloadProgress(qint64 received, qint64 total);
    void slotDownloadFinished();
    void slotUnpackStep();
    void slotGpuProbeFinished();

  private:
    struct Item {
        QString name;
        QString version;
        QString url;
        QString sha256;
        qint64 size{0};
        QString kind;
        QString target;
        QString filename;
        QString state;
        /// Directory to drop from every entry, which the interpreter for Linux
        /// needs: its archive keeps everything under a single `python/`.
        QString stripPrefix;
    };

    void setState(const QString& state, const QString& error = QString());
    bool loadManifest();
    /// Whether the manifest of this platform offers that variant at all.
    bool manifestHasVariant(const QString& variant) const;
    void rebuildItems();
    QStringList requiredHosts() const;
    void finishSelection();

    void startNetworkCheck();
    void startDownload();
    /// Asks the driver for the name of the card, without blocking: this runs on
    /// the way into the program and a driver that hangs must not hold up the
    /// window.
    void detectGpu();
    bool archiveIsCached(const Item& item) const;
    QString cachePathFor(const Item& item) const;
    void failDownload(const QString& reason);

    void startUnpack();
    bool unpackOneItem();
    /// Unpacks a gzipped tarball, which is what the interpreter of Linux and
    /// the wheels of any non Windows platform are.
    bool unpackTarItem(const Item& item, const QString& archive);
    /// Gives the executable bit to the encoder the wheels brought, which is the
    /// one program of the engine that is run directly and the ZIP reader does
    /// not restore it.
    void makeEngineProgramsExecutable() const;
    bool writeEnginePath();
    bool copyEngineCode();
    bool writeStateFile();
    void finish();

    UserSettingsPointer m_pConfig;
    QNetworkAccessManager m_network;
    QNetworkReply* m_pReply{nullptr};
    QFile m_download;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QTimer* m_pUnpackTimer{nullptr};
    QElapsedTimer m_unpackBudget;
    ZipArchive m_zip;
    QString m_unpackArchive;
    int m_unpackEntry{0};
    int m_unpackItemIndex{-1};
    /// Set by cancel(), and read by the extraction of a tarball, which runs
    /// inside one step instead of one entry per step.
    bool m_unpackCancelled{false};

    QJsonObject m_manifest;
    bool m_manifestLoaded{false};
    QVector<Item> m_items;
    QString m_variant;
    QString m_model;
    QString m_state{QStringLiteral("absent")};
    QString m_statusText;
    QString m_errorText;
    int m_progress{0};
    qint64 m_bytesDone{0};
    qint64 m_bytesTotal{0};
    int m_currentItem{-1};
    int m_attempt{0};
    QStringList m_hostsToCheck;
    QStringList m_hostsChecked;
    QString m_gpuName;
    QProcess* m_pGpuProbe{nullptr};
    bool m_variantChosenByUser{false};
};
