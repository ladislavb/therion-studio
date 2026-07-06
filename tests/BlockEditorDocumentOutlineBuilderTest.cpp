#include "../src/app/text_editor/block_editor/BlockEditorDirectiveRules.h"
#include "../src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h"

#include <QString>
#include <QtTest/QtTest>

using namespace TherionStudio;
using namespace TherionStudio::BlockEditorDirectiveRules;

namespace
{
BlockEditorDocumentOutlineContext outlineContext()
{
    BlockEditorDocumentOutlineContext context;
    context.resolveScopeForCommandAtLine = [](const QString &command, const QStringList &, int lineNumber) {
        return normalizeDirective(command) == QStringLiteral("data") && lineNumber == 5
            ? QStringLiteral("centerline")
            : QStringLiteral("none");
    };
    context.isContainerDirectiveInstanceForParsedLine = [](const QString &directive, const TherionParsedLine &parsedLine) {
        return isContainerDirectiveInstance(directive, parsedLine);
    };
    context.isCommandDirectiveInScope = [](const QString &directive, const QString &scope) {
        const QString normalizedDirective = normalizeDirective(directive);
        const QString normalizedScope = normalizeDirective(scope);
        if (normalizedScope == QStringLiteral("none")) {
            return normalizedDirective == QStringLiteral("encoding");
        }
        if (normalizedScope == QStringLiteral("centerline")) {
            return normalizedDirective == QStringLiteral("date")
                || normalizedDirective == QStringLiteral("data")
                || normalizedDirective == QStringLiteral("extend")
                || normalizedDirective == QStringLiteral("team");
        }
        return false;
    };
    return context;
}

class BlockEditorDocumentOutlineBuilderTest final : public QObject
{
    Q_OBJECT

private slots:
    void dataEntryConsumesExtendRows();
};
}

void BlockEditorDocumentOutlineBuilderTest::dataEntryConsumesExtendRows()
{
    const QString contents = QStringLiteral(
        "encoding utf-8\n"
        "survey test\n"
        "centerline\n"
        "  date 2006.08.12\n"
        "  data normal from to compass clino tape\n"
        "  extend right\n"
        "  2.26 2.33 48.11 48.42 3.040\n"
        "  extend left\n"
        "  # ignore this comment inside the data body\n"
        "  2.36 2.43 154.75 12.26 1.516\n"
        "  team surveyor\n"
        "endcenterline\n"
        "endsurvey\n");

    const BlockEditorDocumentOutline outline = BlockEditorDocumentOutlineBuilder(outlineContext()).buildFromContents(contents);
    const auto dataEntryIndex = outline.entryIndexByStartLine.constFind(5);
    QVERIFY(dataEntryIndex != outline.entryIndexByStartLine.constEnd());

    const BlockEditorDocumentEntry dataEntry = outline.entries.at(*dataEntryIndex);
    QCOMPARE(dataEntry.kind, QStringLiteral("data"));
    QCOMPARE(dataEntry.startLine, 5);
    QCOMPARE(dataEntry.endLine, 10);

    for (const BlockEditorDocumentEntry &entry : outline.entries) {
        QVERIFY(entry.kind != QStringLiteral("extend"));
    }

    const auto teamEntryIndex = outline.entryIndexByStartLine.constFind(11);
    QVERIFY(teamEntryIndex != outline.entryIndexByStartLine.constEnd());
    QCOMPARE(outline.entries.at(*teamEntryIndex).kind, QStringLiteral("team"));
}

int runBlockEditorDocumentOutlineBuilderTest(int argc, char **argv)
{
    BlockEditorDocumentOutlineBuilderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "BlockEditorDocumentOutlineBuilderTest.moc"
