#pragma once

#include <QFile>
#include <QString>
#include <QVector>

/// Reads the ZIP archives the stem engine downloads.
///
/// Deliberately small: it opens the folder, walks the central directory and
/// inflates one entry at a time, which is what installing an engine needs and
/// nothing more. Mixxx already links zlib for libdjinterop, so this is where a
/// second decompressor library would otherwise have to come in for the sake of
/// unpacking a dozen files.
class ZipArchive {
  public:
    struct Entry {
        QString name;
        quint32 crc32{0};
        quint64 compressedSize{0};
        quint64 uncompressedSize{0};
        quint16 method{0};
        quint64 localHeaderOffset{0};
    };

    ZipArchive();
    ~ZipArchive();

    ZipArchive(const ZipArchive&) = delete;
    ZipArchive& operator=(const ZipArchive&) = delete;

    /// Opens the archive and reads its table of contents.
    bool open(const QString& path);
    void close();
    bool isOpen() const;
    QString errorString() const;
    const QVector<Entry>& entries() const {
        return m_entries;
    }

    /// Writes one entry under `root`, creating the folders it needs and
    /// checking the CRC of what came out. Refuses names pointing outside
    /// `root`, which a ZIP is entitled to contain and must not be trusted on.
    bool extract(const Entry& entry, const QString& root);

  private:
    bool readCentralDirectory();
    QString safeTargetPath(const QString& root, const QString& name) const;
    void setError(const QString& message);

    QFile m_file;
    QVector<Entry> m_entries;
    QString m_error;
};
