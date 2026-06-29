#include "../../src/core/TherionFileTypes.h"
#include "../../src/core/TherionSourceDocument.h"

#include <QString>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionSourceDocumentTest : public QObject
{
    Q_OBJECT

private slots:
    void classifiesCommandAndBlockContentLines();
    void preservesCommentsBlankRowsAndMixedLineEndings();
    void exposesLosslessSourceTextSnapshot();
    void recordsBlockCloseAndUnclosedState();
    void recordsNestedBlockRanges();
    void keepsUnclosedBlockRangesOpen();
    void preservesSourceSnapshotMetadata();
    void infersSourceDocumentTypeFromTherionFileName();
    void normalizesCentrelineAliases();

private:
    static const TherionSourceDocumentLine *lineAt(const TherionSourceDocument &document,
                                                  int oneBasedLineNumber);
};

const TherionSourceDocumentLine *TherionSourceDocumentTest::lineAt(
    const TherionSourceDocument &document,
    int oneBasedLineNumber)
{
    for (const TherionSourceDocumentLine &line : document.lines()) {
        if (line.sourceLine.lineNumber == oneBasedLineNumber) {
            return &line;
        }
    }
    return nullptr;
}

void TherionSourceDocumentTest::classifiesCommandAndBlockContentLines()
{
    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "scrap s1\n"
        "line wall\n"
        "  0 0\n"
        "  smooth off\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n"));

    QCOMPARE(document.lines().size(), 10);
    QVERIFY(lineAt(document, 1)->role == TherionSourceLineRole::Command);
    QVERIFY(lineAt(document, 2)->role == TherionSourceLineRole::Command);
    QVERIFY(lineAt(document, 3)->role == TherionSourceLineRole::BlockContent);
    QVERIFY(lineAt(document, 4)->role == TherionSourceLineRole::BlockContent);
    QVERIFY(lineAt(document, 5)->role == TherionSourceLineRole::Command);
    QVERIFY(lineAt(document, 7)->role == TherionSourceLineRole::BlockContent);
    QVERIFY(document.openBlocksAtEnd().isEmpty());
}

void TherionSourceDocumentTest::preservesCommentsBlankRowsAndMixedLineEndings()
{
    const QString text = QStringLiteral(
        "survey demo\r\n"
        "# keep comment\n"
        "\r"
        "  # indented comment\r\n"
        "endsurvey");
    const TherionSourceDocument document = TherionSourceDocument::fromText(text);

    QCOMPARE(document.toText(), text);
    QCOMPARE(document.lines().size(), 5);
    QVERIFY(lineAt(document, 1)->role == TherionSourceLineRole::Command);
    QVERIFY(lineAt(document, 2)->role == TherionSourceLineRole::Comment);
    QVERIFY(lineAt(document, 3)->role == TherionSourceLineRole::Empty);
    QVERIFY(lineAt(document, 4)->role == TherionSourceLineRole::Comment);
    QVERIFY(lineAt(document, 5)->role == TherionSourceLineRole::Command);
    QCOMPARE(lineAt(document, 2)->sourceLine.text, QStringLiteral("# keep comment"));
    QCOMPARE(lineAt(document, 4)->sourceLine.text, QStringLiteral("  # indented comment"));
}

void TherionSourceDocumentTest::exposesLosslessSourceTextSnapshot()
{
    const QString text = QStringLiteral("encoding utf-8\r\nscrap s1\nendscrap");
    const TherionSourceDocument document = TherionSourceDocument::fromText(text);
    const TherionSourceText &sourceText = document.sourceText();

    QCOMPARE(sourceText.toText(), text);
    QCOMPARE(sourceText.physicalLines().size(), document.parsedDocument().lines.size());

    const QVector<TherionSourceLineSpan> spans = sourceText.lineSpans();
    QCOMPARE(spans.size(), 3);
    QCOMPARE(spans.at(0).startOffset, document.parsedDocument().lines.at(0).startOffset);
    QCOMPARE(spans.at(0).textLength, document.parsedDocument().lines.at(0).textLength);
    QCOMPARE(spans.at(0).lineEndingLength, document.parsedDocument().lines.at(0).lineEndingLength);
    QCOMPARE(spans.at(0).endOffset, document.parsedDocument().lines.at(0).endOffset);
    QCOMPARE(spans.at(2).lineEndingLength, 0);
}

void TherionSourceDocumentTest::recordsBlockCloseAndUnclosedState()
{
    const TherionSourceDocument unmatched = TherionSourceDocument::fromText(QStringLiteral("endscrap\n"));
    QVERIFY(lineAt(unmatched, 1)->hasUnmatchedClose());

    const TherionSourceDocument unclosed = TherionSourceDocument::fromText(QStringLiteral("survey demo\nscrap s1\n"));
    QCOMPARE(unclosed.openBlocksAtEnd().size(), 2);
    QCOMPARE(unclosed.openBlocksAtEnd().at(0).directive, QStringLiteral("survey"));
    QCOMPARE(unclosed.openBlocksAtEnd().at(1).directive, QStringLiteral("scrap"));
}

void TherionSourceDocumentTest::recordsNestedBlockRanges()
{
    const QString text = QStringLiteral(
        "survey demo\n"
        "  scrap s1\n"
        "    line wall\n"
        "      0 0\n"
        "    endline\n"
        "  endscrap\n"
        "endsurvey\n");
    const TherionSourceDocument document = TherionSourceDocument::fromText(text);
    const QVector<TherionSourceBlockRange> &ranges = document.blockRanges();

    QCOMPARE(ranges.size(), 3);

    const TherionSourceBlockRange &survey = ranges.at(0);
    QCOMPARE(survey.directive, QStringLiteral("survey"));
    QCOMPARE(survey.openLineNumber, 1);
    QCOMPARE(survey.closeLineNumber, 7);
    QCOMPARE(survey.startOffset, 0);
    QCOMPARE(survey.endOffset, text.size());
    QCOMPARE(survey.openLineText, QStringLiteral("survey demo"));
    QCOMPARE(survey.closeLineText, QStringLiteral("endsurvey"));
    QVERIFY(survey.parentStack.isEmpty());
    QVERIFY(survey.isClosed());

    const TherionSourceBlockRange &scrap = ranges.at(1);
    QCOMPARE(scrap.directive, QStringLiteral("scrap"));
    QCOMPARE(scrap.openLineNumber, 2);
    QCOMPARE(scrap.closeLineNumber, 6);
    QCOMPARE(scrap.parentStack.size(), 1);
    QCOMPARE(scrap.parentStack.constFirst().directive, QStringLiteral("survey"));

    const TherionSourceBlockRange &line = ranges.at(2);
    QCOMPARE(line.directive, QStringLiteral("line"));
    QCOMPARE(line.openLineNumber, 3);
    QCOMPARE(line.closeLineNumber, 5);
    QCOMPARE(line.parentStack.size(), 2);
    QCOMPARE(line.parentStack.at(0).directive, QStringLiteral("survey"));
    QCOMPARE(line.parentStack.at(1).directive, QStringLiteral("scrap"));
}

void TherionSourceDocumentTest::keepsUnclosedBlockRangesOpen()
{
    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "survey demo\n"
        "scrap s1\n"));
    const QVector<TherionSourceBlockRange> &ranges = document.blockRanges();

    QCOMPARE(ranges.size(), 2);
    QCOMPARE(ranges.at(0).directive, QStringLiteral("survey"));
    QCOMPARE(ranges.at(0).openLineNumber, 1);
    QVERIFY(!ranges.at(0).isClosed());
    QCOMPARE(ranges.at(1).directive, QStringLiteral("scrap"));
    QCOMPARE(ranges.at(1).openLineNumber, 2);
    QVERIFY(!ranges.at(1).isClosed());
    QCOMPARE(ranges.at(1).parentStack.size(), 1);
    QCOMPARE(ranges.at(1).parentStack.constFirst().directive, QStringLiteral("survey"));
}

void TherionSourceDocumentTest::preservesSourceSnapshotMetadata()
{
    const TherionSourceDocument defaultDocument =
        TherionSourceDocument::fromText(QStringLiteral("survey demo\n"));
    QVERIFY(defaultDocument.sourceType() == TherionSourceDocumentType::Unknown);
    QVERIFY(defaultDocument.encodingName().isEmpty());
    QCOMPARE(defaultDocument.revisionId(), 0);

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.encodingName = QStringLiteral("windows-1250");
    metadata.revisionId = 42;

    const TherionSourceDocument document =
        TherionSourceDocument::fromText(QStringLiteral("scrap s1\nendscrap\n"), metadata);
    QVERIFY(document.metadata().sourceType == TherionSourceDocumentType::TherionMap);
    QCOMPARE(document.metadata().encodingName, QStringLiteral("windows-1250"));
    QCOMPARE(document.metadata().revisionId, 42);
    QVERIFY(document.sourceType() == TherionSourceDocumentType::TherionMap);
    QCOMPARE(document.encodingName(), QStringLiteral("windows-1250"));
    QCOMPARE(document.revisionId(), 42);
}

void TherionSourceDocumentTest::infersSourceDocumentTypeFromTherionFileName()
{
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("survey.th"))
            == TherionSourceDocumentType::TherionSource);
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("map.th2"))
            == TherionSourceDocumentType::TherionMap);
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("thconfig"))
            == TherionSourceDocumentType::TherionConfig);
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("survey.thconfig"))
            == TherionSourceDocumentType::TherionConfig);
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("thconfig.debug"))
            == TherionSourceDocumentType::TherionConfig);
    QVERIFY(therionSourceDocumentTypeForFileName(QStringLiteral("notes.txt"))
            == TherionSourceDocumentType::Unknown);
    QCOMPARE(therionSourceDocumentTypeCatalogToken(TherionSourceDocumentType::TherionSource),
             QStringLiteral("th"));
    QCOMPARE(therionSourceDocumentTypeCatalogToken(TherionSourceDocumentType::TherionMap),
             QStringLiteral("th2"));
    QCOMPARE(therionSourceDocumentTypeCatalogToken(TherionSourceDocumentType::TherionConfig),
             QStringLiteral("thconfig"));
    QVERIFY(therionSourceDocumentTypeCatalogToken(TherionSourceDocumentType::Unknown).isEmpty());
}

void TherionSourceDocumentTest::normalizesCentrelineAliases()
{
    QCOMPARE(normalizedTherionDirectiveToken(QStringLiteral("centreline")), QStringLiteral("centerline"));
    QCOMPARE(normalizedTherionDirectiveToken(QStringLiteral("endcentreline")), QStringLiteral("endcenterline"));

    const TherionSourceDocument document = TherionSourceDocument::fromText(QStringLiteral(
        "centreline\n"
        "  data normal from to length compass clino\n"
        "endcentreline\n"));
    QVERIFY(lineAt(document, 1)->opensBlock);
    QVERIFY(lineAt(document, 2)->role == TherionSourceLineRole::BlockContent);
    QVERIFY(!lineAt(document, 3)->hasUnmatchedClose());
}
}

int runTherionSourceDocumentTest(int argc, char **argv)
{
    TherionSourceDocumentTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceDocumentTest.moc"
