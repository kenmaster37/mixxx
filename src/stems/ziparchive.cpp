#include "stems/ziparchive.h"

#include <QDir>
#include <QFileInfo>

#include <zlib.h>

#include <cstring>

namespace {

// The ZIP format is little endian throughout, and these archives come from
// Python's packaging tools, so they are ordinary ZIPs with at most the ZIP64
// fields a large file needs.
constexpr quint32 kEndOfCentralDirectory = 0x06054b50;
constexpr quint32 kZip64EndOfCentralDirectory = 0x06064b50;
constexpr quint32 kZip64EndOfCentralDirectoryLocator = 0x07064b50;
constexpr quint32 kCentralFileHeader = 0x02014b50;
constexpr quint32 kLocalFileHeader = 0x04034b50;
constexpr quint32 kZip64ExtraField = 0x0001;

// The comment at the end of a ZIP can be up to 64 KiB, so the end of central
// directory record is somewhere in the last 64 KiB plus its own 22 bytes.
constexpr qint64 kEndOfCentralDirectorySearch = 0x10000 + 22;

quint16 readU16(const QByteArray& data, int offset) {
    return static_cast<quint16>(
            static_cast<quint8>(data.at(offset)) |
            (static_cast<quint16>(static_cast<quint8>(data.at(offset + 1))) << 8));
}

quint32 readU32(const QByteArray& data, int offset) {
    return static_cast<quint32>(static_cast<quint8>(data.at(offset))) |
            (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 8) |
            (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 16) |
            (static_cast<quint32>(static_cast<quint8>(data.at(offset + 3))) << 24);
}

quint64 readU64(const QByteArray& data, int offset) {
    return static_cast<quint64>(readU32(data, offset)) |
            (static_cast<quint64>(readU32(data, offset + 4)) << 32);
}

} // namespace

ZipArchive::ZipArchive() = default;

ZipArchive::~ZipArchive() {
    close();
}

void ZipArchive::setError(const QString& message) {
    m_error = message;
}

void ZipArchive::close() {
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_entries.clear();
}

bool ZipArchive::isOpen() const {
    return m_file.isOpen();
}

QString ZipArchive::errorString() const {
    return m_error;
}

bool ZipArchive::open(const QString& path) {
    close();
    m_error.clear();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("no se puede abrir %1: %2").arg(path, m_file.errorString()));
        return false;
    }
    if (!readCentralDirectory()) {
        close();
        return false;
    }
    return true;
}

bool ZipArchive::readCentralDirectory() {
    const qint64 size = m_file.size();
    if (size < 22) {
        setError(QStringLiteral("el archivo es demasiado pequeno para ser un ZIP"));
        return false;
    }

    const qint64 tailLength = qMin(size, kEndOfCentralDirectorySearch);
    if (!m_file.seek(size - tailLength)) {
        setError(QStringLiteral("no se puede leer el final del archivo"));
        return false;
    }
    const QByteArray tail = m_file.read(tailLength);

    int endOfCentralDirectory = -1;
    for (int offset = tail.size() - 22; offset >= 0; --offset) {
        if (readU32(tail, offset) == kEndOfCentralDirectory) {
            endOfCentralDirectory = offset;
            break;
        }
    }
    if (endOfCentralDirectory < 0) {
        setError(QStringLiteral("no es un archivo ZIP (no encuentro el indice)"));
        return false;
    }

    quint64 entryCount = readU16(tail, endOfCentralDirectory + 10);
    quint64 directorySize = readU32(tail, endOfCentralDirectory + 12);
    quint64 directoryOffset = readU32(tail, endOfCentralDirectory + 16);

    // A file over 4 GB, or one with more entries than fit in sixteen bits, is
    // described by the ZIP64 records instead; the locator sits right before the
    // record just read.
    if (entryCount == 0xFFFF || directorySize == 0xFFFFFFFF ||
            directoryOffset == 0xFFFFFFFF) {
        const int locator = endOfCentralDirectory - 20;
        if (locator < 0 || readU32(tail, locator) != kZip64EndOfCentralDirectoryLocator) {
            setError(QStringLiteral("el ZIP dice ser ZIP64 pero no encuentro el registro"));
            return false;
        }
        const quint64 zip64Offset = readU64(tail, locator + 8);
        QByteArray record(56, '\0');
        if (!m_file.seek(static_cast<qint64>(zip64Offset)) ||
                m_file.read(record.data(), record.size()) != record.size() ||
                readU32(record, 0) != kZip64EndOfCentralDirectory) {
            setError(QStringLiteral("no se puede leer el registro ZIP64"));
            return false;
        }
        entryCount = readU64(record, 32);
        directorySize = readU64(record, 40);
        directoryOffset = readU64(record, 48);
    }

    if (directoryOffset + directorySize > static_cast<quint64>(size) ||
            !m_file.seek(static_cast<qint64>(directoryOffset))) {
        setError(QStringLiteral("el indice del ZIP apunta fuera del archivo"));
        return false;
    }
    const QByteArray directory = m_file.read(static_cast<qint64>(directorySize));
    if (static_cast<quint64>(directory.size()) != directorySize) {
        setError(QStringLiteral("no se puede leer el indice del ZIP"));
        return false;
    }

    m_entries.reserve(static_cast<int>(entryCount));
    int offset = 0;
    for (quint64 index = 0; index < entryCount; ++index) {
        if (offset + 46 > directory.size() || readU32(directory, offset) != kCentralFileHeader) {
            setError(QStringLiteral("el indice del ZIP esta danado en la entrada %1")
                             .arg(index));
            return false;
        }
        Entry entry;
        entry.crc32 = readU32(directory, offset + 16);
        entry.compressedSize = readU32(directory, offset + 20);
        entry.uncompressedSize = readU32(directory, offset + 24);
        const int nameLength = readU16(directory, offset + 28);
        const int extraLength = readU16(directory, offset + 30);
        const int commentLength = readU16(directory, offset + 32);
        entry.localHeaderOffset = readU32(directory, offset + 42);
        if (offset + 46 + nameLength + extraLength + commentLength > directory.size()) {
            setError(QStringLiteral("el indice del ZIP se sale del archivo"));
            return false;
        }
        entry.name = QString::fromUtf8(
                directory.constData() + offset + 46, nameLength);
        entry.method = readU16(directory, offset + 10);

        if (entry.compressedSize == 0xFFFFFFFF || entry.uncompressedSize == 0xFFFFFFFF ||
                entry.localHeaderOffset == 0xFFFFFFFF) {
            // The name and the extra field follow each other; inside the extra
            // field, the ZIP64 values appear only for the fields that did not
            // fit, in this order.
            int extra = offset + 46 + nameLength;
            const int extraEnd = extra + extraLength;
            while (extra + 4 <= extraEnd) {
                const quint16 fieldId = readU16(directory, extra);
                const quint16 fieldSize = readU16(directory, extra + 2);
                if (fieldId == kZip64ExtraField) {
                    int value = extra + 4;
                    if (entry.uncompressedSize == 0xFFFFFFFF && value + 8 <= extraEnd) {
                        entry.uncompressedSize = readU64(directory, value);
                        value += 8;
                    }
                    if (entry.compressedSize == 0xFFFFFFFF && value + 8 <= extraEnd) {
                        entry.compressedSize = readU64(directory, value);
                        value += 8;
                    }
                    if (entry.localHeaderOffset == 0xFFFFFFFF && value + 8 <= extraEnd) {
                        entry.localHeaderOffset = readU64(directory, value);
                    }
                    break;
                }
                extra += 4 + fieldSize;
            }
        }

        m_entries.append(entry);
        offset += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

QString ZipArchive::safeTargetPath(const QString& root, const QString& name) const {
    // A ZIP may carry absolute names or "..", and unpacking those would write
    // wherever it liked.
    if (name.isEmpty() || QDir::isAbsolutePath(name) || name.startsWith(QLatin1Char('/')) ||
            name.startsWith(QLatin1Char('\\')) || name.contains(QStringLiteral(":"))) {
        return QString();
    }
    const QString cleaned = QDir::cleanPath(name);
    if (cleaned == QLatin1String("..") || cleaned.startsWith(QStringLiteral("../"))) {
        return QString();
    }
    return QDir(root).absoluteFilePath(cleaned);
}

bool ZipArchive::extract(const Entry& entry, const QString& root) {
    m_error.clear();

    if (entry.name.endsWith(QLatin1Char('/'))) {
        // A folder entry: nothing to write, but it may carry an empty folder.
        const QString path = safeTargetPath(root, entry.name);
        if (path.isEmpty()) {
            setError(QStringLiteral("nombre sospechoso en el ZIP: %1").arg(entry.name));
            return false;
        }
        QDir().mkpath(path);
        return true;
    }

    const QString target = safeTargetPath(root, entry.name);
    if (target.isEmpty()) {
        setError(QStringLiteral("nombre sospechoso en el ZIP: %1").arg(entry.name));
        return false;
    }
    const QFileInfo info(target);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(QStringLiteral("no se puede crear la carpeta %1").arg(info.absolutePath()));
        return false;
    }

    if (!m_file.seek(static_cast<qint64>(entry.localHeaderOffset))) {
        setError(QStringLiteral("no se puede leer %1").arg(entry.name));
        return false;
    }
    QByteArray localHeader(30, '\0');
    if (m_file.read(localHeader.data(), localHeader.size()) != localHeader.size() ||
            readU32(localHeader, 0) != kLocalFileHeader) {
        setError(QStringLiteral("cabecera danada en %1").arg(entry.name));
        return false;
    }
    // The local header repeats the name and the extra field, and the sizes in
    // it can be zero when the entry is streamed, so the data start is the only
    // thing worth taking from here.
    const qint64 dataStart = static_cast<qint64>(entry.localHeaderOffset) + 30 +
            readU16(localHeader, 26) + readU16(localHeader, 28);

    QFile output(target);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("no se puede escribir %1: %2")
                         .arg(target, output.errorString()));
        return false;
    }

    if (entry.method == 0) {
        if (!m_file.seek(dataStart)) {
            setError(QStringLiteral("no se puede leer %1").arg(entry.name));
            return false;
        }
        quint64 remaining = entry.compressedSize;
        QByteArray buffer(1 << 16, '\0');
        while (remaining > 0) {
            const int wanted = static_cast<int>(qMin<quint64>(remaining, buffer.size()));
            const int read = m_file.read(buffer.data(), wanted);
            if (read <= 0) {
                setError(QStringLiteral("el ZIP se acaba antes de tiempo en %1")
                                 .arg(entry.name));
                return false;
            }
            if (output.write(buffer.constData(), read) != read) {
                setError(QStringLiteral("no se puede escribir %1").arg(target));
                return false;
            }
            remaining -= static_cast<quint64>(read);
        }
    } else if (entry.method == 8) {
        if (!m_file.seek(dataStart)) {
            setError(QStringLiteral("no se puede leer %1").arg(entry.name));
            return false;
        }
        z_stream stream;
        std::memset(&stream, 0, sizeof(stream));
        // Negative window size: the data holds a raw deflate stream, without
        // the zlib or gzip wrapper, which is what a ZIP stores.
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            setError(QStringLiteral("no se puede preparar el descompresor"));
            return false;
        }
        QByteArray input(1 << 16, '\0');
        QByteArray uncompressed(1 << 16, '\0');
        quint64 remaining = entry.compressedSize;
        bool ok = true;
        while (ok && remaining > 0) {
            const int read = m_file.read(
                    input.data(), static_cast<int>(qMin<quint64>(remaining, input.size())));
            if (read <= 0) {
                setError(QStringLiteral("el ZIP se acaba antes de tiempo en %1")
                                 .arg(entry.name));
                ok = false;
                break;
            }
            remaining -= static_cast<quint64>(read);
            stream.next_in = reinterpret_cast<Bytef*>(input.data());
            stream.avail_in = static_cast<uInt>(read);
            while (stream.avail_in > 0) {
                stream.next_out = reinterpret_cast<Bytef*>(uncompressed.data());
                stream.avail_out = static_cast<uInt>(uncompressed.size());
                const int result = inflate(&stream, Z_NO_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    setError(QStringLiteral("%1 esta danado: %2")
                                     .arg(entry.name, QString::fromUtf8(stream.msg ? stream.msg : "deflate")));
                    ok = false;
                    break;
                }
                const int produced = uncompressed.size() - static_cast<int>(stream.avail_out);
                if (produced > 0 &&
                        output.write(uncompressed.constData(), produced) != produced) {
                    setError(QStringLiteral("no se puede escribir %1").arg(target));
                    ok = false;
                    break;
                }
                if (result == Z_STREAM_END) {
                    break;
                }
            }
        }
        inflateEnd(&stream);
        if (!ok) {
            output.close();
            QFile::remove(target);
            return false;
        }
    } else {
        setError(QStringLiteral("%1 usa una compresion que no entiendo (%2)")
                         .arg(entry.name)
                         .arg(entry.method));
        output.close();
        QFile::remove(target);
        return false;
    }

    const bool flushed = output.flush();
    const qint64 written = output.size();
    output.close();
    if (!flushed) {
        setError(QStringLiteral("no se puede escribir %1").arg(target));
        return false;
    }

    if (entry.uncompressedSize != 0 && static_cast<quint64>(written) != entry.uncompressedSize) {
        setError(QStringLiteral("%1 salio con %2 bytes en vez de %3")
                         .arg(entry.name)
                         .arg(written)
                         .arg(entry.uncompressedSize));
        QFile::remove(target);
        return false;
    }
    if (entry.crc32 != 0) {
        uLong crc = ::crc32(0L, Z_NULL, 0);
        QFile check(target);
        if (check.open(QIODevice::ReadOnly)) {
            QByteArray buffer(1 << 16, '\0');
            while (true) {
                const int read = check.read(buffer.data(), buffer.size());
                if (read <= 0) {
                    break;
                }
                crc = ::crc32(crc, reinterpret_cast<const Bytef*>(buffer.constData()),
                        static_cast<uInt>(read));
            }
        }
        if (static_cast<quint32>(crc) != entry.crc32) {
            setError(QStringLiteral("%1 no cuadra con su suma de control").arg(entry.name));
            QFile::remove(target);
            return false;
        }
    }
    return true;
}
