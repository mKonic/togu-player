#include "mpvitem.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <clocale>

namespace {

void *getProcAddress(void *, const char *name)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(name)) : nullptr;
}

// mpv node -> QVariant
QVariant nodeToVariant(const mpv_node *n)
{
    switch (n->format) {
    case MPV_FORMAT_STRING: return QString::fromUtf8(n->u.string);
    case MPV_FORMAT_FLAG: return bool(n->u.flag);
    case MPV_FORMAT_INT64: return qlonglong(n->u.int64);
    case MPV_FORMAT_DOUBLE: return n->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList l;
        for (int i = 0; i < n->u.list->num; ++i) l << nodeToVariant(&n->u.list->values[i]);
        return l;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap m;
        for (int i = 0; i < n->u.list->num; ++i) m.insert(QString::fromUtf8(n->u.list->keys[i]), nodeToVariant(&n->u.list->values[i]));
        return m;
    }
    default: return {};
    }
}

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(MpvItem *item) : m_item(item) {}
    // The item owns the render context and frees it; the renderer may outlive it.

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        if (!m_item->renderContext()) {
            mpv_opengl_init_params gl{getProcAddress, nullptr};
            int advanced = 1;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
                {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
                {MPV_RENDER_PARAM_INVALID, nullptr},
            };
            mpv_render_context *ctx = nullptr;
            if (mpv_render_context_create(&ctx, m_item->handle(), params) >= 0) {
                m_item->setRenderContext(ctx);
                // A file asked for before the first frame loads now.
                QMetaObject::invokeMethod(m_item, &MpvItem::loadPending, Qt::QueuedConnection);
                mpv_render_context_set_update_callback(ctx, [](void *p) {
                    emit static_cast<MpvItem *>(p)->requestUpdate();
                }, m_item);
            }
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        mpv_render_context *ctx = m_item->renderContext();
        if (!ctx) return;
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo mfbo{int(fbo->handle()), fbo->width(), fbo->height(), 0};
        int flip = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(ctx, params);
    }

private:
    MpvItem *m_item;
};

} // namespace

MpvItem::MpvItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{
    std::setlocale(LC_NUMERIC, "C"); // mpv requires it
    m_mpv = mpv_create();
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    // YouTube through yt-dlp: up to 1080p, the original audio rather than an auto-dub.
    mpv_set_option_string(m_mpv, "ytdl-format", "bv*[height<=?1080]+(ba[format_note*=original]/ba[language^=ja]/ba)/b");
    // Stream links are signed for the address yt-dlp used; IPv4 keeps that the same one mpv connects from.
    mpv_set_option_string(m_mpv, "ytdl-raw-options", "force-ipv4=");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "osd-level", "0");
    mpv_set_option_string(m_mpv, "sub-auto", "no");          // Togu chooses subtitles itself
    mpv_set_option_string(m_mpv, "sid", "no");
    mpv_set_option_string(m_mpv, "alang", "ja,jpn,jp");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "screenshot-format", "png");
    mpv_set_option_string(m_mpv, "demuxer-max-back-bytes", "50MiB");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "msg-level", "all=warn");
    // Tests run with TOGU_AO=null so nothing reaches the speakers.
    if (qEnvironmentVariableIsSet("TOGU_AO")) mpv_set_option_string(m_mpv, "ao", qgetenv("TOGU_AO").constData());
    mpv_initialize(m_mpv);
    mpv_request_log_messages(m_mpv, "warn");

    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "chapter-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "aid", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "dwidth", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "dheight", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "demuxer-cache-time", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(m_mpv, &MpvItem::onWakeup, this);

    connect(this, &MpvItem::requestUpdate, this, &QQuickItem::update, Qt::QueuedConnection);
}

MpvItem::~MpvItem()
{
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    if (m_ctx) {
        mpv_render_context_free(m_ctx);
        m_ctx = nullptr;
    }
    mpv_terminate_destroy(m_mpv);
}

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvItem *>(this));
}

void MpvItem::onWakeup(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvItem *>(ctx), &MpvItem::processEvents, Qt::QueuedConnection);
}

void MpvItem::processEvents()
{
    while (m_mpv) {
        mpv_event *e = mpv_wait_event(m_mpv, 0);
        if (e->event_id == MPV_EVENT_NONE) break;
        switch (e->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *p = static_cast<mpv_event_property *>(e->data);
            handleProperty(p->name, p->format, p->data);
            break;
        }
        case MPV_EVENT_FILE_LOADED:
            m_loaded = true;
            emit loadedChanged();
            emit fileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            auto *ef = static_cast<mpv_event_end_file *>(e->data);
            const char *reason = ef->reason == MPV_END_FILE_REASON_EOF ? "eof" : ef->reason == MPV_END_FILE_REASON_ERROR ? "error" : "stop";
            if (ef->reason != MPV_END_FILE_REASON_REDIRECT) {
                m_loaded = false;
                emit loadedChanged();
            }
            emit endOfFile(QString::fromLatin1(reason));
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART: emit seeked(); break;
        case MPV_EVENT_LOG_MESSAGE: {
            auto *m = static_cast<mpv_event_log_message *>(e->data);
            qWarning().noquote() << "mpv:" << m->prefix << QString::fromUtf8(m->text).trimmed();
            break;
        }
        default: break;
        }
    }
}

void MpvItem::handleProperty(const char *name, int format, void *data)
{
    const QByteArray n(name);
    const bool has = format != MPV_FORMAT_NONE && data;
    if (n == "time-pos") {
        m_position = has ? *static_cast<double *>(data) : 0;
        emit positionChanged();
    } else if (n == "duration") {
        m_duration = has ? *static_cast<double *>(data) : 0;
        emit durationChanged();
    } else if (n == "pause") {
        m_paused = has && *static_cast<int *>(data);
        emit pausedChanged();
    } else if (n == "volume" && has) {
        m_volume = *static_cast<double *>(data);
        emit volumeChanged();
    } else if (n == "mute") {
        m_muted = has && *static_cast<int *>(data);
        emit mutedChanged();
    } else if (n == "speed" && has) {
        m_speed = *static_cast<double *>(data);
        emit speedChanged();
    } else if (n == "track-list") {
        m_tracks = has ? nodeToVariant(static_cast<mpv_node *>(data)).toList() : QVariantList();
        emit tracksChanged();
    } else if (n == "chapter-list") {
        m_chapters = has ? nodeToVariant(static_cast<mpv_node *>(data)).toList() : QVariantList();
        emit chaptersChanged();
    } else if (n == "aid") {
        m_aid = has ? int(*static_cast<int64_t *>(data)) : 0;
        emit audioTrackChanged();
    } else if (n == "idle-active") {
        m_idle = has && *static_cast<int *>(data);
        emit idleChanged();
    } else if (n == "dwidth" || n == "dheight") {
        const int v = has ? int(*static_cast<int64_t *>(data)) : 0;
        if (n == "dwidth") m_videoSize.setWidth(v);
        else m_videoSize.setHeight(v);
        emit videoSizeChanged();
    } else if (n == "demuxer-cache-time") {
        m_cacheEnd = has ? *static_cast<double *>(data) : 0;
        emit cacheEndChanged();
    }
}

void MpvItem::setSource(const QString &s)
{
    if (s == m_source) return;
    m_source = s;
    emit sourceChanged();
    if (s.isEmpty()) stop();
    else if (!m_ctx) m_pending = s;
    else command({QStringLiteral("loadfile"), s, QStringLiteral("replace")});
}

void MpvItem::loadPending()
{
    if (m_pending.isEmpty()) return;
    command({QStringLiteral("loadfile"), m_pending, QStringLiteral("replace")});
    m_pending.clear();
}

void MpvItem::setPaused(bool p) { set(QStringLiteral("pause"), p); }
void MpvItem::setVolume(double v) { set(QStringLiteral("volume"), std::clamp(v, 0.0, 130.0)); }
void MpvItem::setMuted(bool m) { set(QStringLiteral("mute"), m); }
void MpvItem::setSpeed(double s) { set(QStringLiteral("speed"), std::clamp(s, 0.25, 4.0)); }
void MpvItem::setAudioTrack(int id) { set(QStringLiteral("aid"), id > 0 ? QVariant(id) : QVariant(QStringLiteral("no"))); }

void MpvItem::command(const QVariantList &args)
{
    QList<QByteArray> bufs;
    for (const QVariant &a : args) bufs << a.toString().toUtf8();
    std::vector<const char *> argv;
    for (const QByteArray &b : bufs) argv.push_back(b.constData());
    argv.push_back(nullptr);
    mpv_command_async(m_mpv, 0, argv.data());
}

void MpvItem::set(const QString &name, const QVariant &value)
{
    const QByteArray n = name.toUtf8();
    switch (value.typeId()) {
    case QMetaType::Bool: {
        int f = value.toBool();
        mpv_set_property_async(m_mpv, 0, n.constData(), MPV_FORMAT_FLAG, &f);
        break;
    }
    case QMetaType::Int:
    case QMetaType::LongLong: {
        int64_t i = value.toLongLong();
        mpv_set_property_async(m_mpv, 0, n.constData(), MPV_FORMAT_INT64, &i);
        break;
    }
    case QMetaType::Double:
    case QMetaType::Float: {
        double d = value.toDouble();
        mpv_set_property_async(m_mpv, 0, n.constData(), MPV_FORMAT_DOUBLE, &d);
        break;
    }
    default: {
        QByteArray s = value.toString().toUtf8();
        mpv_set_property_string(m_mpv, n.constData(), s.constData());
    }
    }
}

QVariant MpvItem::get(const QString &name)
{
    mpv_node node;
    if (mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node) < 0) return {};
    QVariant v = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return v;
}

void MpvItem::seek(double seconds, bool exact)
{
    command({QStringLiteral("seek"), QString::number(std::max(0.0, seconds), 'f', 3), QStringLiteral("absolute"),
             exact ? QStringLiteral("exact") : QStringLiteral("keyframes")});
}

void MpvItem::seekRelative(double seconds)
{
    command({QStringLiteral("seek"), QString::number(seconds, 'f', 3), QStringLiteral("relative"), QStringLiteral("exact")});
}

void MpvItem::frameStep(bool back) { command({back ? QStringLiteral("frame-back-step") : QStringLiteral("frame-step")}); }

void MpvItem::stop()
{
    command({QStringLiteral("stop")});
    m_source.clear();
    m_pending.clear();
}
