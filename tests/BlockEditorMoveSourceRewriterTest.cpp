#include "../src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h"
#include "../src/app/text_editor/block_editor/BlockEditorSourceText.h"
#include "../src/core/TherionDocumentEditor.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class BlockEditorMoveSourceRewriterTest final : public QObject
{
    Q_OBJECT

private slots:
    void rewritesMovedBlockAsOneSourceEdit();
};
}

void BlockEditorMoveSourceRewriterTest::rewritesMovedBlockAsOneSourceEdit()
{
    const QString contents = QStringLiteral(
        "survey cave\n"
        "scrap first\n"
        "endscrap\n"
        "scrap second\n"
        "endscrap\n"
        "endsurvey\n");
    const QStringList lines = blockEditorNormalizedSourceLines(contents);
    const BlockEditorDocumentEntry sourceEntry{QStringLiteral("scrap"), 2, 3, 1};

    const BlockEditorMoveSourceRewriter rewriter;
    const BlockEditorMoveRewriteResult rewriteResult = rewriter.rewriteMovedBlock(lines, sourceEntry, 6);
    QVERIFY(rewriteResult.applied);

    const QString updatedContents = blockEditorJoinSourceLines(contents, rewriteResult.lines);
    QCOMPARE(updatedContents,
             QStringLiteral("survey cave\n"
                            "scrap second\n"
                            "endscrap\n"
                            "scrap first\n"
                            "endscrap\n"
                            "endsurvey\n"));

    TherionSourceTextEdit edit;
    QVERIFY(blockEditorSourceReplacementEdit(contents, updatedContents, &edit));
    QCOMPARE(edit.startOffset, QStringLiteral("survey cave\nscrap ").size());
    QCOMPARE(edit.length, QStringLiteral("first\nendscrap\nscrap second").size());
    QCOMPARE(edit.replacementText, QStringLiteral("second\nendscrap\nscrap first"));

    QString appliedContents = contents;
    appliedContents.replace(edit.startOffset, edit.length, edit.replacementText);
    QCOMPARE(appliedContents, updatedContents);
}

int runBlockEditorMoveSourceRewriterTest(int argc, char **argv)
{
    BlockEditorMoveSourceRewriterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BlockEditorMoveSourceRewriterTest.moc"
