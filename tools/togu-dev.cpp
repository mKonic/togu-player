// Developer tool: drive the core from the command line.
//   togu-dev [--data DIR] import FILE...
//   togu-dev [--data DIR] lookup TEXT
//   togu-dev [--data DIR] tokenize TEXT
//   togu-dev [--data DIR] info KEY...
#include "core/dict/dict.h"
#include "core/dict/glossary.h"
#include "core/jp/tokenizer.h"
#include "core/util/db.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    if (args.size() >= 2 && args[0] == u"--data") {
        db::setDataDir(args[1]);
        args = args.mid(2);
    }
    QTextStream out(stdout);
    if (args.isEmpty()) return 2;
    db::init();
    const QString cmd = args.takeFirst();
    QElapsedTimer t;
    t.start();
    if (cmd == u"import") {
        for (const QString &f : args) {
            QString r = Dict::instance().import(f);
            out << r << "  " << t.restart() << " ms\n";
            out.flush();
        }
    } else if (cmd == u"lookup") {
        auto es = Dict::instance().lookup(args.join(u' '));
        out << es.size() << " entries in " << t.elapsed() << " ms\n";
        for (const auto &e : es.mid(0, 5)) {
            out << e.expression << " [" << e.reading << "] matched=" << e.matched << " rank=" << e.rank;
            for (const auto &p : e.pitches) out << " pitch=" << p.positions.value(0);
            out << "\n";
            for (const auto &b : e.blocks.mid(0, 2))
                out << "  " << b.dict << ": " << glossary::htmlToText(glossary::cardHtml(b.glossary, b.dict)).left(160).replace(QChar(u'\n'), QStringLiteral(" / ")) << "\n";
        }
    } else if (cmd == u"tokenize") {
        for (const auto &tk : jp::tokenize(args.join(u' ')))
            out << tk.surface << "\t" << tk.lemma << "\t" << tk.lemmaReading << "\t" << tk.reading << "\t" << tk.word << "\n";
    } else if (cmd == u"info") {
        auto info = Dict::instance().info(args);
        auto gl = Dict::instance().gloss(args);
        for (const QString &k : args) out << k << " rank=" << info.value(k).rank << " gloss=" << gl.value(k) << "\n";
        out << t.elapsed() << " ms\n";
    }
    return 0;
}
