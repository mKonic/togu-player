#pragma once
// libmpv rendered into Qt Quick through the OpenGL render API.

#include <QQuickFramebufferObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

struct mpv_handle;
struct mpv_render_context;

class MpvItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(int audioTrack READ audioTrack WRITE setAudioTrack NOTIFY audioTrackChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(double cacheEnd READ cacheEnd NOTIFY cacheEndChanged)

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;
    Renderer *createRenderer() const override;

    QString source() const { return m_source; }
    void setSource(const QString &s);
    // Plays separate video and audio URLs (YouTube) under `source`, fetched with `userAgent`.
    void setStream(const QString &source, const QString &video, const QString &audio, const QString &userAgent);
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    double volume() const { return m_volume; }
    void setVolume(double v);
    bool muted() const { return m_muted; }
    void setMuted(bool m);
    double speed() const { return m_speed; }
    void setSpeed(double s);
    QVariantList tracks() const { return m_tracks; }
    int audioTrack() const { return m_aid; }
    void setAudioTrack(int id);
    QVariantList chapters() const { return m_chapters; }
    bool loaded() const { return m_loaded; }
    bool idle() const { return m_idle; }
    QSize videoSize() const { return m_videoSize; }
    double cacheEnd() const { return m_cacheEnd; }

    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void set(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant get(const QString &name);
    Q_INVOKABLE void seek(double seconds, bool exact = true);
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void frameStep(bool back = false);
    Q_INVOKABLE void stop();

    mpv_handle *handle() const { return m_mpv; }
    mpv_render_context *renderContext() const { return m_ctx; }
    void setRenderContext(mpv_render_context *ctx) { m_ctx = ctx; }
    void loadPending();

signals:
    void sourceChanged();
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void tracksChanged();
    void audioTrackChanged();
    void chaptersChanged();
    void loadedChanged();
    void idleChanged();
    void videoSizeChanged();
    void cacheEndChanged();
    void fileLoaded();
    void endOfFile(const QString &reason);
    void seeked();
    void requestUpdate();

private:
    void load(const QVariantList &cmd);
    void processEvents();
    static void onWakeup(void *ctx);
    void handleProperty(const char *name, int format, void *data);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_ctx = nullptr;
    QString m_source;
    QVariantList m_pending; // a loadfile command waiting for the render context
    double m_position = 0, m_duration = 0, m_volume = 100, m_speed = 1, m_cacheEnd = 0;
    bool m_paused = false, m_muted = false, m_loaded = false, m_idle = true;
    int m_aid = 0;
    QVariantList m_tracks, m_chapters;
    QSize m_videoSize;
};
