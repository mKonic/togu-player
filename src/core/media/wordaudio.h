#pragma once
// Word audio from the local-audio collection the Anki add-on "Local Audio Server for Yomichan" uses:
// user_files/entries.db plus <source>_files folders. Read directly, no server needed.

#include <QList>
#include <QString>

namespace wordaudio {

struct Clip {
    QString path, source, speaker;
};

// The collection folder: the setting if set, else the add-on's usual place.
QString collectionDir(const QString &setting);
QList<Clip> find(const QString &dir, const QString &expression, const QString &reading);

} // namespace wordaudio
