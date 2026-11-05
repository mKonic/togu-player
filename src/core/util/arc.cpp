#include "arc.h"

#include <archive.h>
#include <archive_entry.h>

namespace arc {

namespace {

struct Reader {
    struct ::archive *a = archive_read_new();
    Reader()
    {
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);
        // Some dictionary zips carry wrong checksums; Yomitan reads them anyway, and so do we.
        archive_read_set_options(a, "zip:ignorecrc32");
    }
    ~Reader() { archive_read_free(a); }
};

bool run(Reader &r, const std::function<bool(const QString &)> &want,
         const std::function<bool(const QString &, const QByteArray &)> &fn)
{
    struct archive_entry *e;
    while (archive_read_next_header(r.a, &e) == ARCHIVE_OK) {
        if (archive_entry_filetype(e) != AE_IFREG) continue;
        const char *p = archive_entry_pathname_utf8(e);
        const QString name = p ? QString::fromUtf8(p) : QString::fromLocal8Bit(archive_entry_pathname(e));
        if (!want(name)) {
            archive_read_data_skip(r.a);
            continue;
        }
        QByteArray data;
        const la_int64_t size = archive_entry_size(e);
        if (size > 0) data.reserve(qsizetype(size));
        char buf[1 << 16];
        la_ssize_t n;
        while ((n = archive_read_data(r.a, buf, sizeof buf)) > 0) data.append(buf, qsizetype(n));
        if (n < 0) return false;
        if (!fn(name, data)) return true;
    }
    return true;
}

} // namespace

QStringList list(const QString &path)
{
    QStringList out;
    Reader r;
    if (archive_read_open_filename(r.a, path.toLocal8Bit().constData(), 1 << 16) != ARCHIVE_OK) return out;
    struct archive_entry *e;
    while (archive_read_next_header(r.a, &e) == ARCHIVE_OK) {
        if (archive_entry_filetype(e) == AE_IFREG) {
            const char *p = archive_entry_pathname_utf8(e);
            out << (p ? QString::fromUtf8(p) : QString::fromLocal8Bit(archive_entry_pathname(e)));
        }
        archive_read_data_skip(r.a);
    }
    return out;
}

bool each(const QString &path, const std::function<bool(const QString &)> &want,
          const std::function<bool(const QString &, const QByteArray &)> &fn)
{
    Reader r;
    if (archive_read_open_filename(r.a, path.toLocal8Bit().constData(), 1 << 16) != ARCHIVE_OK) return false;
    return run(r, want, fn);
}

QByteArray read(const QString &path, const QString &name)
{
    QByteArray out;
    each(path, [&](const QString &n) { return n == name; }, [&](const QString &, const QByteArray &d) {
        out = d;
        return false;
    });
    return out;
}

bool eachInMemory(const QByteArray &data, const std::function<bool(const QString &)> &want,
                  const std::function<bool(const QString &, const QByteArray &)> &fn)
{
    Reader r;
    if (archive_read_open_memory(r.a, data.constData(), size_t(data.size())) != ARCHIVE_OK) return false;
    return run(r, want, fn);
}

} // namespace arc
