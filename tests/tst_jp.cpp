// Japanese core: the C++ tokenizer port must split and label text exactly as togu does.
#include "core/jp/jp.h"
#include "core/jp/tokenizer.h"

#include <QFile>
#include <QTest>

class TestJp : public QObject {
    Q_OBJECT
private slots:
    void parityWithTogu()
    {
        // Each row: line \t tokens, produced by togu's own tokenize() (fields 0x1f, tokens 0x1e).
        QString path = qEnvironmentVariable("TOGU_PARITY", QStringLiteral(TOGU_TEST_DATA "/tokenizer_parity.tsv"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        int rows = 0, bad = 0;
        while (!f.atEnd()) {
            QString row = QString::fromUtf8(f.readLine()).chopped(1);
            if (row.isEmpty()) continue;
            auto tab = row.indexOf(u'\t');
            QString line = row.left(tab);
            QStringList got;
            for (const jp::Token &t : jp::tokenize(line))
                got << QStringList{t.surface, t.lemma, t.lemmaReading, t.reading, t.word ? "1" : "0"}.join(QChar(0x1f));
            QString want = row.mid(tab + 1);
            if (got.join(QChar(0x1e)) != want) {
                QStringList w = want.split(QChar(0x1e));
                QString diff;
                for (qsizetype k = 0; k < qMax(w.size(), got.size()); ++k) {
                    QString a = k < got.size() ? got[k] : QString(), b = k < w.size() ? w[k] : QString();
                    if (a != b) diff += QString(a).replace(QChar(0x1f), u'/') + " != " + QString(b).replace(QChar(0x1f), u'/') + "; ";
                }
                if (++bad <= 20) qWarning().noquote() << diff.left(300);
            }
            ++rows;
        }
        QVERIFY(rows > 0);
        QCOMPARE(bad, 0);
    }

    void furigana()
    {
        QCOMPARE(jp::furiganaPlain(jp::furiganaSegments("食べ物", "たべもの")), QString("食[た]べ 物[もの]"));
        QCOMPARE(jp::furiganaPlain(jp::furiganaSegments("ひらがな", "ひらがな")), QString("ひらがな"));
    }
};

QTEST_GUILESS_MAIN(TestJp)
#include "tst_jp.moc"
