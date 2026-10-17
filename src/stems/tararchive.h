#pragma once

#include <QString>

#include <functional>

/// Reader for the tar archives the Linux engine is made of.
///
/// The Windows engine is a set of ZIP files, but the interpreter for Linux is
/// shipped as a gzipped tarball by python-build-standalone, and it is not a list
/// of plain files: it carries symbolic links (``bin/python3`` points at
/// ``bin/python3.11``) and the executable bit of every program. Both have to
/// survive the install, so a ZIP reader that only writes file contents is not
/// enough.
///
/// Only extraction is needed, and only for archives made by a packaging tool,
/// so this understands the formats those produce: ustar, the GNU long name and
/// long link extensions, and the PAX headers that carry paths too long for the
/// fixed header fields. Everything else is skipped instead of failing, because
/// an archive can legitimately contain device nodes the engine has no use for.
///
/// Like the ZIP reader next to it, nothing is written outside the destination
/// directory: an entry whose path climbs out of it is refused.
class TarArchive {
  public:
    /// Called while the archive is being read, with the bytes taken from the
    /// file and its size; returns false to stop, which is how a cancelled
    /// install ends the extraction.
    using ProgressCallback = std::function<bool(qint64 done, qint64 total)>;

    /// Extracts ``archivePath``, which may be gzipped, into ``destination``.
    ///
    /// ``stripPrefix`` removes that leading directory from every entry, which is
    /// how the single ``python/`` directory of the interpreter archive is
    /// dropped to match the layout the engine expects. Returns false and sets
    /// ``pError`` when the archive cannot be read or written.
    static bool extract(const QString& archivePath,
            const QString& destination,
            const QString& stripPrefix = QString(),
            QString* pError = nullptr,
            const ProgressCallback& onProgress = {});
};
