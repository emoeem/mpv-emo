#pragma once

#include <QObject>
#include <QStringList>

#include <atomic>

#include "library/LibraryStore.h"

// Runs one directory scan on a worker thread (see LibraryStore::requestScan).
// The worker lives inside the QThread the store owns; every invocation is
// queued through that thread's event loop, so no locking is needed. Results
// travel back through queued connections to the store's own thread.
class LibraryScanWorker : public QObject
{
    Q_OBJECT
public:
    // |cancelled| is the store's flag, checked once per file; it must outlive
    // the worker (the store shuts the thread down in its destructor).
    explicit LibraryScanWorker(const std::atomic_bool *cancelled,
                               QObject *parent = nullptr);

public slots:
    // Walk |dirs| and emit progress()/finished() from this thread.
    void beginScan(const QStringList &dirs, int maxDepth, bool skipHidden);

signals:
    // Media files found so far (throttled by scanDirectories).
    void progress(int fileCount);
    // Scan result; the store replaces its entry list with it.
    void finished(const QList<LibraryEntry> &entries);

private:
    const std::atomic_bool *m_cancelled = nullptr;
};
