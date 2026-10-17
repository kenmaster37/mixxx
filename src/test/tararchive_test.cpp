#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstring>

#include <array>

#include <zlib.h>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include "stems/tararchive.h"

namespace {

/// A tar header as a packaging tool writes it: fixed width fields, numbers in
/// octal, and the ustar magic. Everything the engine is made of on Linux goes
/// through this format, so the test builds the bytes itself instead of shipping
/// an archive file as a fixture.
QByteArray tarHeader(const QByteArray& name,
        qint64 size,
        char type,
        const QByteArray& link = QByteArray(),
        int mode = 0644) {
    QByteArray block(512, '\0');
    const auto put = [&block](int offset, int length, const QByteArray& value) {
        std::memcpy(block.data() + offset, value.constData(),
                static_cast<size_t>(qMin<qsizetype>(value.size(), length)));
    };
    const auto octal = [&block](int offset, int length, qint64 value) {
        const QByteArray text = QByteArray::number(value, 8).rightJustified(length - 1, '0');
        std::memcpy(block.data() + offset, text.constData(),
                static_cast<size_t>(qMin<qsizetype>(text.size(), length - 1)));
        block[offset + length - 1] = '\0';
    };
    put(0, 100, name);
    octal(100, 8, mode);
    octal(108, 8, 0);
    octal(116, 8, 0);
    octal(124, 12, size);
    octal(136, 12, 0);
    std::memset(block.data() + 148, ' ', 8);
    block[156] = type;
    put(157, 100, link);
    put(257, 6, QByteArrayLiteral("ustar"));
    block[262] = '0';
    block[263] = '0';

    unsigned int checksum = 0;
    for (const char byte : block) {
        checksum += static_cast<unsigned char>(byte);
    }
    const QByteArray text = QByteArray::number(checksum, 8).rightJustified(6, '0') + '\0' + ' ';
    std::memcpy(block.data() + 148, text.constData(), 8);
    return block;
}

QByteArray padded(const QByteArray& data) {
    QByteArray out = data;
    const qsizetype remainder = out.size() % 512;
    if (remainder != 0) {
        out.append(static_cast<int>(512 - remainder), '\0');
    }
    return out;
}

void appendEntry(QByteArray* archive,
        const QByteArray& name,
        char type,
        const QByteArray& body = QByteArray(),
        const QByteArray& link = QByteArray(),
        int mode = 0644) {
    archive->append(tarHeader(name, body.size(), type, link, mode));
    if (!body.isEmpty()) {
        archive->append(padded(body));
    }
}

/// The GNU extension for a name that does not fit in the 100 bytes of the
/// header: a small descriptor entry carries the name for the one that follows.
void appendLongNameEntry(QByteArray* archive, const QByteArray& name, const QByteArray& body) {
    archive->append(tarHeader("././@LongLink", name.size() + 1, 'L'));
    archive->append(padded(name + '\0'));
    archive->append(tarHeader(name.left(100), body.size(), '0', QByteArray(), 0644));
    archive->append(padded(body));
}

QByteArray gzip(const QByteArray& plain) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                Z_DEFAULT_STRATEGY) != Z_OK) {
        return QByteArray();
    }
    QByteArray out(static_cast<int>(deflateBound(&stream, plain.size())), '\0');
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(plain.constData()));
    stream.avail_in = static_cast<uInt>(plain.size());
    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = static_cast<uInt>(out.size());
    const int result = deflate(&stream, Z_FINISH);
    const qsizetype produced = out.size() - static_cast<qsizetype>(stream.avail_out);
    deflateEnd(&stream);
    if (result != Z_STREAM_END) {
        return QByteArray();
    }
    out.resize(produced);
    return out;
}

bool writeFile(const QString& path, const QByteArray& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

QByteArray contentOf(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

/// An archive shaped like the interpreter python-build-standalone ships: the
/// whole tree under a single directory, a program with its executable bit, a
/// symbolic link to it, and one path too long for the header field.
QByteArray engineArchive() {
    QByteArray archive;
    appendEntry(&archive, "python/", '5', QByteArray(), QByteArray(), 0755);
    appendEntry(&archive, "python/bin/", '5', QByteArray(), QByteArray(), 0755);
    appendEntry(&archive, "python/bin/python3.11", '0', QByteArrayLiteral("#!/bin/sh\n"),
            QByteArray(), 0755);
    appendEntry(&archive, "python/bin/python3", '2', QByteArray(),
            QByteArrayLiteral("python3.11"), 0777);
    const QByteArray longName = "python/lib/"
            + QByteArray(120, 'x') + ".txt";
    appendLongNameEntry(&archive, longName, QByteArrayLiteral("long\n"));
    archive.append(QByteArray(1024, '\0'));
    return gzip(archive);
}

} // namespace

TEST(TarArchiveTest, ExtractsProgramsLinksAndLongNames) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString archivePath = dir.filePath(QStringLiteral("engine.tar.gz"));
    ASSERT_TRUE(writeFile(archivePath, engineArchive()));

    const QString destination = dir.filePath(QStringLiteral("engine"));
    QString error;
    ASSERT_TRUE(TarArchive::extract(archivePath, destination, QStringLiteral("python"),
            &error))
            << error.toStdString();

    // The single `python/` directory is dropped, which is what puts the
    // interpreter where the engine expects it: the reader extracts into the
    // folder it is given, and the installer is the one that names it `runtime`.
    const QFileInfo program(destination + QStringLiteral("/bin/python3.11"));
    ASSERT_TRUE(program.exists());
    EXPECT_EQ(contentOf(program.absoluteFilePath()), QByteArrayLiteral("#!/bin/sh\n"));
    EXPECT_FALSE(QFileInfo::exists(destination + QStringLiteral("/python")));

#ifdef Q_OS_UNIX
    // The executable bit and the link are the two things a ZIP reader cannot
    // carry over, and both are needed to run the interpreter.
    EXPECT_TRUE(program.isExecutable());
    const QString link = destination + QStringLiteral("/bin/python3");
    EXPECT_TRUE(QFileInfo(link).isSymLink());
    EXPECT_EQ(QFileInfo(link).symLinkTarget(), program.absoluteFilePath());
    // Read the link itself instead of asking Qt, which resolves it: it has to
    // stay relative, or moving the engine to another folder would break it.
    std::array<char, 256> raw{};
    const ssize_t length = ::readlink(QFile::encodeName(link).constData(),
            raw.data(), raw.size());
    ASSERT_GT(length, 0);
    EXPECT_EQ(QByteArray(raw.data(), static_cast<int>(length)),
            QByteArrayLiteral("python3.11"));
#endif

    // A path longer than the 100 bytes of a header field needs the GNU
    // extension, and it has to come out with its content and its directory.
    const QDir lib(destination + QStringLiteral("/lib"));
    const QStringList longFiles = lib.entryList(
            {QStringLiteral("x*.txt")}, QDir::Files);
    ASSERT_EQ(longFiles.size(), 1);
    EXPECT_EQ(contentOf(lib.absoluteFilePath(longFiles.first())),
            QByteArrayLiteral("long\n"));
}

TEST(TarArchiveTest, RefusesPathsOutsideTheDestination) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray archive;
    appendEntry(&archive, "../escaped.txt", '0', QByteArrayLiteral("no\n"));
    archive.append(QByteArray(1024, '\0'));
    const QString archivePath = dir.filePath(QStringLiteral("bad.tar.gz"));
    ASSERT_TRUE(writeFile(archivePath, gzip(archive)));

    const QString destination = dir.filePath(QStringLiteral("engine"));
    QString error;
    EXPECT_FALSE(TarArchive::extract(archivePath, destination, QString(), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(QFileInfo::exists(dir.filePath(QStringLiteral("escaped.txt"))));
}

TEST(TarArchiveTest, KeepsEverythingWhenNoPrefixIsStripped) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray archive;
    appendEntry(&archive, "site-packages/module.py", '0', QByteArrayLiteral("x = 1\n"));
    archive.append(QByteArray(1024, '\0'));
    const QString archivePath = dir.filePath(QStringLiteral("wheel.tar.gz"));
    ASSERT_TRUE(writeFile(archivePath, gzip(archive)));

    const QString destination = dir.filePath(QStringLiteral("engine"));
    QString error;
    ASSERT_TRUE(TarArchive::extract(archivePath, destination, QString(), &error))
            << error.toStdString();
    EXPECT_TRUE(QFileInfo::exists(
            destination + QStringLiteral("/site-packages/module.py")));
}

TEST(TarArchiveTest, StopsWhenTheCallerSaysSo) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray archive;
    for (int i = 0; i < 8; ++i) {
        appendEntry(&archive, QByteArray("python/file") + QByteArray::number(i), '0',
                QByteArrayLiteral("contenido\n"));
    }
    archive.append(QByteArray(1024, '\0'));
    const QString archivePath = dir.filePath(QStringLiteral("many.tar.gz"));
    ASSERT_TRUE(writeFile(archivePath, gzip(archive)));

    const QString destination = dir.filePath(QStringLiteral("engine"));
    // Cancelling is reported by the callback returning false, and the error is
    // left empty so that the caller can say "cancelled" instead of "failed".
    QString error = QStringLiteral("previo");
    EXPECT_FALSE(TarArchive::extract(archivePath, destination, QStringLiteral("python"),
            &error, [](qint64, qint64) { return false; }));
    EXPECT_TRUE(error.isEmpty());
}
