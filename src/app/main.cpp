#include "backend.h"
#include "core/dict/dict.h"
#include "core/util/db.h"
#include "core/words/words.h"
#include "settings.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>

#include <clocale>

// image://dict/<dictId>/<path> -> picture stored with a dictionary.
class DictImages : public QQuickImageProvider {
public:
    DictImages() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override
    {
        const qsizetype slash = id.indexOf(u'/');
        QImage img;
        img.loadFromData(Dict::instance().media(id.left(slash).toInt(), QUrl::fromPercentEncoding(id.mid(slash + 1).toUtf8())));
        if (requested.isValid() && !img.isNull()) img = img.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (size) *size = img.size();
        return img;
    }
};

int main(int argc, char *argv[])
{
    // mpv renders through OpenGL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("togu-player"));
    app.setApplicationDisplayName(QStringLiteral("togu"));
    app.setOrganizationName(QStringLiteral("togu"));
    app.setDesktopFileName(QStringLiteral("togu-player"));
    std::setlocale(LC_NUMERIC, "C");

    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/inter.woff2"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/fredoka.woff2"));
    QFont ui(QStringLiteral("Inter"));
    ui.setFamilies({QStringLiteral("Inter"), QStringLiteral("Inter Variable"), QStringLiteral("Noto Sans CJK JP"), QStringLiteral("sans-serif")});
    ui.setPixelSize(14);
    ui.setHintingPreference(QFont::PreferNoHinting);
    app.setFont(ui);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (!db::init()) qWarning("togu: database setup failed");
    Words::instance().load();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("dict"), new DictImages);
    QStringList files;
    for (int i = 1; i < argc; ++i) files << QString::fromLocal8Bit(argv[i]);
    engine.rootContext()->setContextProperty(QStringLiteral("startupFiles"), files);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Togu", "Main");
    return app.exec();
}
