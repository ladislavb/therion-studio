#include "../../src/core/TherionCodeBlockRules.h"
#include "../../src/core/TherionSourceDocument.h"

#include <QString>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionCodeBlockRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void marksCodeLinesWithTheirLanguage();
    void endsCodeAtEndlayout();
    void findsTexComments();
    void findsMetaPostComments();
    void findsTherionCommentLines();
};

void TherionCodeBlockRulesTest::marksCodeLinesWithTheirLanguage()
{
    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "layout l\n"
        "  code metapost\n"
        "    % comment\n"
        "  endcode\n"
        "  code tex-atlas\n"
        "    \\def\\x#1{#1}\n"
        "  endcode\n"
        "  code mpost\n"
        "    x\n"
        "  endcode\n"
        "  code\n"
        "    x\n"
        "  endcode\n"
        "  scale 1 500\n"
        "endlayout\n"));
    using L = TherionCodeLanguage;
    const QVector<L> expected{L::None, L::None, L::MetaPost, L::None, L::None, L::TeX, L::None,
                              L::None, L::MetaPost, L::None, L::None, L::Unknown, L::None, L::None, L::None, L::None};
    QCOMPARE(therionCodeLanguagesByLine(document), expected);
}

void TherionCodeBlockRulesTest::endsCodeAtEndlayout()
{
    // Therion ends an open code block with its layout.
    const TherionSourceDocument document = TherionSourceDocument::fromText(
        QStringLiteral("layout l\n  code tex-map\n    x\nendlayout\nsource cave.th\n"));
    using L = TherionCodeLanguage;
    QCOMPARE(therionCodeLanguagesByLine(document), (QVector<L>{L::None, L::None, L::TeX, L::None, L::None, L::None}));
}

void TherionCodeBlockRulesTest::findsTexComments()
{
    QCOMPARE(therionCodeCommentStart(QStringLiteral("a % b"), TherionCodeLanguage::TeX), 2);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("50\\% % b"), TherionCodeLanguage::TeX), 5);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("a\\\\% b"), TherionCodeLanguage::TeX), 3);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("\\def\\x#1{#1}"), TherionCodeLanguage::TeX), -1);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("a % b"), TherionCodeLanguage::Unknown), -1);
}

void TherionCodeBlockRulesTest::findsMetaPostComments()
{
    QCOMPARE(therionCodeCommentStart(QStringLiteral("draw p; % b"), TherionCodeLanguage::MetaPost), 8);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("label(\"50%\", z) % b"), TherionCodeLanguage::MetaPost), 16);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("label(\"open %"), TherionCodeLanguage::MetaPost), -1);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("a \\% b"), TherionCodeLanguage::MetaPost), 3);
}

void TherionCodeBlockRulesTest::findsTherionCommentLines()
{
    // Therion drops a code line starting with `#`, whatever the language; a later `#` reaches TeX as a macro argument.
    QCOMPARE(therionCodeCommentStart(QStringLiteral("  # \\cavename={}"), TherionCodeLanguage::TeX), 2);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("# draw p;"), TherionCodeLanguage::MetaPost), 0);
    QCOMPARE(therionCodeCommentStart(QStringLiteral(" # x"), TherionCodeLanguage::Unknown), 1);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("\\def\\x#1{#1}"), TherionCodeLanguage::TeX), -1);
    QCOMPARE(therionCodeCommentStart(QStringLiteral("# x"), TherionCodeLanguage::None), -1);
}
}

int runTherionCodeBlockRulesTest(int argc, char **argv)
{
    TherionCodeBlockRulesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionCodeBlockRulesTest.moc"
