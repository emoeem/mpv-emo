#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Pure queue-scheduling logic for ThumbnailService: the set of media paths
// waiting for thumbnail generation, FIFO ordered. Membership is a QSet so
// duplicate requests are cheap to detect; the parallel list keeps the arrival
// order (a QSet alone has no stable iteration order).
//
// No QObject, no I/O, no processes — directly unit-testable headless (see
// test_thumbnail_service.cpp). ThumbnailService drives it: enqueue on
// request, takeNext() when the serial generation chain is idle.
class ThumbnailQueue
{
public:
    // Queue |path| unless it is already queued. Returns true when newly
    // queued, false for duplicates.
    bool enqueue(const QString &path)
    {
        if (path.isEmpty() || m_membership.contains(path))
            return false;
        m_membership.insert(path);
        m_order.append(path);
        return true;
    }

    // Pop the next path in FIFO order; empty string when drained.
    QString takeNext()
    {
        if (m_order.isEmpty())
            return {};
        const QString next = m_order.takeFirst();
        m_membership.remove(next);
        return next;
    }

    bool contains(const QString &path) const
    {
        return m_membership.contains(path);
    }
    bool isEmpty() const { return m_order.isEmpty(); }
    int size() const { return m_order.size(); }
    void clear()
    {
        m_order.clear();
        m_membership.clear();
    }

private:
    QStringList m_order;
    QSet<QString> m_membership;
};
