#include "mpv/MpvEngine.h"

#include <QMetaObject>
#include <QUrl>

namespace {

// Convert an mpv_node tree into QVariant for QML consumption.
QVariant nodeToVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_NONE:
        return {};
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0;
    case MPV_FORMAT_INT64:
        return static_cast<qlonglong>(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node->u.list->num; ++i)
            list.append(nodeToVariant(&node->u.list->values[i]));
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node->u.list->num; ++i)
            map.insert(QString::fromUtf8(node->u.list->keys[i]),
                       nodeToVariant(&node->u.list->values[i]));
        return map;
    }
    case MPV_FORMAT_BYTE_ARRAY:
        // mpv_byte_array::data is void*; QByteArray needs const char*.
        return QString::fromUtf8(QByteArray(static_cast<const char *>(node->u.ba->data),
                                            int(node->u.ba->size)));
    default:
        return {};
    }
}

} // namespace

MpvEngine::MpvEngine(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

MpvEngine::~MpvEngine()
{
    if (s_instance == this)
        s_instance = nullptr;
    if (m_mpv) {
        // Render contexts are owned by QML items (MpvItem::Renderer) and must
        // already be gone by the time we are destroyed; see main.cpp ordering.
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

bool MpvEngine::initialize(const QVariantMap &options)
{
    if (m_mpv) {
        m_lastError = QStringLiteral("Engine already initialized");
        return false;
    }

    m_mpv = mpv_create();
    if (!m_mpv) {
        m_lastError = QStringLiteral("mpv_create() failed");
        return false;
    }

    // Defaults for embedding libmpv behind our own QML UI.
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "osc", "no");          // we draw our own controls
    mpv_set_option_string(m_mpv, "vo", "libmpv");       // render API
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe"); // safe hardware decode
    mpv_set_option_string(m_mpv, "keep-open", "yes");   // stay at EOF, don't close

    for (auto it = options.constBegin(); it != options.constEnd(); ++it)
        mpv_set_option_string(m_mpv, it.key().toUtf8().constData(),
                              it.value().toString().toUtf8().constData());

    int err = mpv_initialize(m_mpv);
    if (err < 0) {
        m_lastError = QString::fromUtf8(mpv_error_string(err));
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    mpv_set_wakeup_callback(m_mpv, &MpvEngine::onWakeup, this);
    observeDefaults();
    return true;
}

void MpvEngine::observeDefaults()
{
    // Property updates are delivered as MPV_EVENT_PROPERTY_CHANGE in
    // handleEvents() and re-emitted as typed Qt signals.
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "playlist", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "idle-active", MPV_FORMAT_FLAG);
}

void MpvEngine::onWakeup(void *ctx)
{
    // Called from arbitrary mpv threads; bounce onto the owner thread.
    QMetaObject::invokeMethod(static_cast<MpvEngine *>(ctx), "handleEvents",
                              Qt::QueuedConnection);
}

void MpvEngine::handleEvents()
{
    while (m_mpv) {
        mpv_event *ev = mpv_wait_event(m_mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE)
            break;

        switch (ev->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            const auto *prop = static_cast<mpv_event_property *>(ev->data);
            if (!prop->name)
                break;
            if (qstrcmp(prop->name, "time-pos") == 0 && prop->format == MPV_FORMAT_DOUBLE)
                emit positionChanged(*static_cast<double *>(prop->data));
            else if (qstrcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_DOUBLE)
                emit durationChanged(*static_cast<double *>(prop->data));
            else if (qstrcmp(prop->name, "pause") == 0 && prop->format == MPV_FORMAT_FLAG)
                emit pausedChanged(*static_cast<int *>(prop->data) != 0);
            else if (qstrcmp(prop->name, "volume") == 0 && prop->format == MPV_FORMAT_DOUBLE)
                emit volumeChanged(*static_cast<double *>(prop->data));
            else if (qstrcmp(prop->name, "mute") == 0 && prop->format == MPV_FORMAT_FLAG)
                emit mutedChanged(*static_cast<int *>(prop->data) != 0);
            else if (qstrcmp(prop->name, "media-title") == 0 && prop->format == MPV_FORMAT_STRING)
                emit mediaTitleChanged(QString::fromUtf8(*static_cast<char **>(prop->data)));
            else if (qstrcmp(prop->name, "track-list") == 0 && prop->format == MPV_FORMAT_NODE)
                emit trackListChanged(
                    nodeToVariant(static_cast<const mpv_node *>(prop->data)).toList());
            else if (qstrcmp(prop->name, "playlist") == 0 && prop->format == MPV_FORMAT_NODE)
                emit playlistChanged(
                    nodeToVariant(static_cast<const mpv_node *>(prop->data)).toList());
            else if (qstrcmp(prop->name, "idle-active") == 0 && prop->format == MPV_FORMAT_FLAG)
                emit idleActiveChanged(*static_cast<int *>(prop->data) != 0);
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            emit fileLoaded();
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            emit playbackRestarted();
            break;
        case MPV_EVENT_END_FILE: {
            // Local must not be named endFile: it would shadow the signal.
            const auto *endFileEvent = static_cast<mpv_event_end_file *>(ev->data);
            emit endFile(int(endFileEvent->reason));
            break;
        }
        default:
            break;
        }
    }
}

void MpvEngine::command(const QVariantList &args)
{
    if (!m_mpv)
        return;

    QVector<QByteArray> storage;
    storage.reserve(args.size());
    for (const QVariant &arg : args)
        storage.append(arg.toString().toUtf8());

    QVector<const char *> argv;
    argv.reserve(storage.size() + 1);
    for (const QByteArray &bytes : storage)
        argv.append(bytes.constData());
    argv.append(nullptr); // mpv_command wants a NULL-terminated array

    int err = mpv_command(m_mpv, argv.data());
    if (err < 0)
        emitError(err, "command");
}

void MpvEngine::setProperty(const QString &name, const QVariant &value)
{
    if (!m_mpv)
        return;

    int err;
    switch (value.typeId()) {
    case QMetaType::Bool: {
        int flag = value.toBool() ? 1 : 0;
        err = mpv_set_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_FLAG, &flag);
        break;
    }
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong: {
        double d = value.toDouble();
        err = mpv_set_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_DOUBLE, &d);
        break;
    }
    case QMetaType::Float:
    case QMetaType::Double: {
        double d = value.toDouble();
        err = mpv_set_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_DOUBLE, &d);
        break;
    }
    default: {
        QByteArray bytes = value.toString().toUtf8();
        err = mpv_set_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_STRING,
                               bytes.data());
        break;
    }
    }
    if (err < 0)
        emitError(err, qPrintable(QStringLiteral("set %1").arg(name)));
}

QVariant MpvEngine::getProperty(const QString &name) const
{
    if (!m_mpv)
        return {};

    mpv_node node;
    int err = mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node);
    if (err < 0)
        return {};
    QVariant result = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return result;
}

QString MpvEngine::toMediaPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    if (url.isLocalFile())
        return url.toLocalFile();
    return pathOrUrl;
}

void MpvEngine::loadFile(const QString &pathOrUrl)
{
    command({QStringLiteral("loadfile"), toMediaPath(pathOrUrl)});
}

void MpvEngine::emitError(int mpvErr, const char *what)
{
    emit errorOccurred(QStringLiteral("%1 failed: %2")
                           .arg(QString::fromLatin1(what),
                                QString::fromUtf8(mpv_error_string(mpvErr))));
}
