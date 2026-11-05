#pragma once
// Reading archives (zip, rar, 7z, tar...) through libarchive.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

namespace arc {

QStringList list(const QString &path);
// Calls `fn` for each regular file whose name `want` accepts; stop early by returning false.
bool each(const QString &path, const std::function<bool(const QString &)> &want,
          const std::function<bool(const QString &, const QByteArray &)> &fn);
QByteArray read(const QString &path, const QString &name);
// Same, for an archive held in memory.
bool eachInMemory(const QByteArray &data, const std::function<bool(const QString &)> &want,
                  const std::function<bool(const QString &, const QByteArray &)> &fn);

} // namespace arc
