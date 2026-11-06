#include "mpv/MpvItem.h"

#include "mpv/MpvEngine.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>

// mpv render API: client.h (via MpvEngine.h) only declares the client API;
// the render context and its OpenGL params live in these two headers.
#include <mpv/render.h>
#include <mpv/render_gl.h>

namespace {

// Invoked by libmpv on its render thread whenever a new frame is available.
// Queues a scene graph update so render() runs again with a fresh FBO.
void mpvRedraw(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<QQuickItem *>(ctx), "update",
                              Qt::QueuedConnection);
}

// Renderer living on the Qt Quick render thread; owns the mpv render context.
class MpvRenderer final : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvItem *item);
    ~MpvRenderer() override;

    void render() override;

private:
    MpvItem *m_item = nullptr;
    mpv_render_context *m_mpvGl = nullptr;
};

MpvRenderer::MpvRenderer(MpvItem *item)
    : m_item(item)
{
    mpv_handle *mpv =
        MpvEngine::instance() ? MpvEngine::instance()->handle() : nullptr;
    if (!mpv)
        return;

    mpv_opengl_init_params glInit;
    glInit.get_proc_address = [](void *, const char *name) -> void * {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        if (!ctx)
            return nullptr;
        return reinterpret_cast<void *>(ctx->getProcAddress(QByteArray(name)));
    };
    glInit.get_proc_address_ctx = nullptr;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
        {},
    };

    int err = mpv_render_context_create(&m_mpvGl, mpv, params);
    if (err < 0) {
        qWarning("mpv_render_context_create failed: %s", mpv_error_string(err));
        m_mpvGl = nullptr;
        return;
    }
    mpv_render_context_set_update_callback(m_mpvGl, &mpvRedraw, m_item);
}

MpvRenderer::~MpvRenderer()
{
    // Runs on the render thread while the GL context is current, before the
    // mpv core is torn down (see destruction-order note in main.cpp).
    if (m_mpvGl)
        mpv_render_context_free(m_mpvGl);
}

void MpvRenderer::render()
{
    if (!m_mpvGl)
        return;

    // Qt6 replacement for QQuickWindow::resetOpenGLState(): resets the state
    // of the context that is current on the render thread.
    QQuickOpenGLUtils::resetOpenGLState();

    QOpenGLFramebufferObject *fbo = framebufferObject();
    if (!fbo)
        return;

    mpv_opengl_fbo mpvFbo;
    mpvFbo.fbo = static_cast<int>(fbo->handle());
    mpvFbo.w = fbo->width();
    mpvFbo.h = fbo->height();
    mpvFbo.internal_format = 0;

    // Qt Quick mirrors the FBO when compositing; flip Y so the picture is
    // right side up (same convention as the reference libmpv QML integration).
    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpvFbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {},
    };
    mpv_render_context_render(m_mpvGl, params);
}

} // namespace

MpvItem::MpvItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    m_engine = MpvEngine::instance();
    if (!m_engine)
        return;

    // Mirror engine state into QML-visible properties.
    connect(m_engine, &MpvEngine::positionChanged, this, [this](double v) {
        m_position = v;
        emit positionChanged(v);
    });
    connect(m_engine, &MpvEngine::durationChanged, this, [this](double v) {
        m_duration = v;
        emit durationChanged(v);
    });
    connect(m_engine, &MpvEngine::pausedChanged, this, [this](bool v) {
        m_paused = v;
        emit pausedChanged(v);
    });
    connect(m_engine, &MpvEngine::volumeChanged, this, [this](double v) {
        m_volume = v;
        emit volumeChanged(v);
    });
    connect(m_engine, &MpvEngine::mutedChanged, this, [this](bool v) {
        m_muted = v;
        emit mutedChanged(v);
    });
    connect(m_engine, &MpvEngine::mediaTitleChanged, this, [this](const QString &v) {
        m_mediaTitle = v;
        emit mediaTitleChanged(v);
    });
    connect(m_engine, &MpvEngine::trackListChanged, this, [this](const QVariantList &v) {
        m_trackList = v;
        emit trackListChanged(v);
    });
    connect(m_engine, &MpvEngine::playlistChanged, this, [this](const QVariantList &v) {
        m_playlist = v;
        emit playlistChanged(v);
    });
    connect(m_engine, &MpvEngine::idleActiveChanged, this, [this](bool v) {
        m_idle = v;
        emit idleChanged(v);
    });
}

MpvItem::~MpvItem() = default;

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    // createRenderer() is const but the renderer keeps a mutable back pointer.
    return new MpvRenderer(const_cast<MpvItem *>(this));
}

void MpvItem::setPaused(bool paused)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("pause"), paused);
}

void MpvItem::setVolume(double volume)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("volume"), volume);
}

void MpvItem::setMuted(bool muted)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("mute"), muted);
}

void MpvItem::togglePlayPause()
{
    if (m_engine)
        m_engine->command({QStringLiteral("cycle"), QStringLiteral("pause")});
}

void MpvItem::play()
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("pause"), false);
}

void MpvItem::pause()
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("pause"), true);
}

void MpvItem::stop()
{
    if (m_engine)
        m_engine->command({QStringLiteral("stop")});
}

void MpvItem::seek(double seconds)
{
    if (m_engine)
        m_engine->command({QStringLiteral("seek"), QString::number(seconds),
                           QStringLiteral("absolute")});
}

void MpvItem::seekBy(double offset)
{
    if (m_engine)
        m_engine->command({QStringLiteral("seek"), QString::number(offset),
                           QStringLiteral("relative")});
}

void MpvItem::selectAudioTrack(const QVariant &id)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("aid"), id.toString());
}

void MpvItem::selectSubtitleTrack(const QVariant &id)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("sid"), id.toString());
}

void MpvItem::loadExternalSubtitle(const QString &path)
{
    if (m_engine)
        m_engine->command({QStringLiteral("sub-add"), MpvEngine::toMediaPath(path),
                           QStringLiteral("select")});
}

void MpvItem::appendToPlaylist(const QString &path)
{
    if (m_engine)
        m_engine->command({QStringLiteral("loadfile"), MpvEngine::toMediaPath(path),
                           QStringLiteral("append-play")});
}

void MpvItem::playlistNext()
{
    if (m_engine)
        m_engine->command({QStringLiteral("playlist-next")});
}

void MpvItem::playlistPrev()
{
    if (m_engine)
        m_engine->command({QStringLiteral("playlist-prev")});
}

void MpvItem::playlistPlayIndex(int index)
{
    if (m_engine)
        m_engine->setProperty(QStringLiteral("playlist-play-index"),
                              QString::number(index));
}

void MpvItem::playlistClear()
{
    if (m_engine)
        m_engine->command({QStringLiteral("playlist-clear")});
}
