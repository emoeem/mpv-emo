#include "library/LibraryScanWorker.h"

LibraryScanWorker::LibraryScanWorker(const std::atomic_bool *cancelled,
                                     QObject *parent)
    : QObject(parent), m_cancelled(cancelled)
{
    Q_ASSERT(m_cancelled);
}

void LibraryScanWorker::beginScan(const QStringList &dirs, int maxDepth,
                                  bool skipHidden)
{
    LibraryStore::ScanOptions options;
    options.maxDepth = maxDepth;
    options.skipHidden = skipHidden;

    const QList<LibraryEntry> entries = LibraryStore::scanDirectories(
        dirs, options,
        [this](int fileCount) { emit progress(fileCount); },
        [this]() { return m_cancelled->load(); });

    emit finished(entries);
}
