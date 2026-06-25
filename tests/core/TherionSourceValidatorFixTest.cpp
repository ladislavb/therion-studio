#include "../../src/core/TherionSourceValidator.h"

#include <QtTest/QtTest>

namespace
{
using namespace TherionStudio;

TherionSourceValidationCatalog basicCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("area"),
        QStringLiteral("endarea"),
        QStringLiteral("endline"),
        QStringLiteral("endscrap"),
        QStringLiteral("endsurvey"),
        QStringLiteral("line"),
        QStringLiteral("point"),
        QStringLiteral("scrap"),
        QStringLiteral("survey"),
    };
    return catalog;
}

TherionSourceDiagnostic diagnosticByCodeAtLine(const TherionSourceValidationResult &result,
                                               const QString &code,
                                               int lineNumber)
{
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        if (diagnostic.code == code && diagnostic.lineNumber == lineNumber) {
            return diagnostic;
        }
    }
    return {};
}
}

class TherionSourceValidatorFixTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixesUnclosedScrapBeforeNextScrap()
    {
        const QString contents = QStringLiteral("scrap first\n"
                                                "  point 0 0 station -name 1\n"
                                                "scrap second\n"
                                                "endscrap\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());
        const TherionSourceDiagnostic diagnostic =
            diagnosticByCodeAtLine(result, QStringLiteral("unclosed-block"), 1);

        QCOMPARE(diagnostic.code, QStringLiteral("unclosed-block"));
        QVERIFY(diagnostic.hasFix);
        QCOMPARE(diagnostic.suggestedText, QStringLiteral("Insert before line 3:\nendscrap"));
        QCOMPARE(diagnostic.fix.description, QStringLiteral("Insert endscrap before line 3"));
        QCOMPARE(TherionSourceValidator::applyFixes(contents, {diagnostic.fix}),
                 QStringLiteral("scrap first\n"
                                "  point 0 0 station -name 1\n"
                                "endscrap\n"
                                "scrap second\n"
                                "endscrap\n"));
    }

    void fixesUnclosedLineBeforeNextMapObject()
    {
        const QString contents = QStringLiteral("scrap s1\n"
                                                "  line wall\n"
                                                "    0 0\n"
                                                "    1 1\n"
                                                "  point 2 2 label -text next\n"
                                                "endscrap\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());
        const TherionSourceDiagnostic diagnostic =
            diagnosticByCodeAtLine(result, QStringLiteral("unclosed-block"), 2);

        QCOMPARE(diagnostic.code, QStringLiteral("unclosed-block"));
        QVERIFY(diagnostic.hasFix);
        QCOMPARE(diagnostic.suggestedText, QStringLiteral("Insert before line 5:\n  endline"));
        QCOMPARE(diagnostic.fix.description, QStringLiteral("Insert endline before line 5"));
        QCOMPARE(TherionSourceValidator::applyFixes(contents, {diagnostic.fix}),
                 QStringLiteral("scrap s1\n"
                                "  line wall\n"
                                "    0 0\n"
                                "    1 1\n"
                                "  endline\n"
                                "  point 2 2 label -text next\n"
                                "endscrap\n"));
    }

    void fixesUnclosedAreaAtEndWithExistingLineEnding()
    {
        const QString contents = QStringLiteral("scrap s1\r\n"
                                                "  area water\r\n"
                                                "    border-1\r\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());
        const TherionSourceDiagnostic diagnostic =
            diagnosticByCodeAtLine(result, QStringLiteral("unclosed-block"), 2);

        QCOMPARE(diagnostic.code, QStringLiteral("unclosed-block"));
        QVERIFY(diagnostic.hasFix);
        QCOMPARE(diagnostic.suggestedText, QStringLiteral("Insert at end of file:\n  endarea"));
        QCOMPARE(diagnostic.fix.description, QStringLiteral("Insert endarea at end of file"));
        QCOMPARE(TherionSourceValidator::applyFixes(contents, {diagnostic.fix}),
                 QStringLiteral("scrap s1\r\n"
                                "  area water\r\n"
                                "    border-1\r\n"
                                "  endarea\r\n"));
    }

    void doesNotFixNonMapBlocks()
    {
        const QString contents = QStringLiteral("survey cave\n"
                                                "  scrap s1\n"
                                                "  endscrap\n");

        const TherionSourceValidationResult result = TherionSourceValidator::validate(contents, basicCatalog());
        const TherionSourceDiagnostic diagnostic =
            diagnosticByCodeAtLine(result, QStringLiteral("unclosed-block"), 1);

        QCOMPARE(diagnostic.code, QStringLiteral("unclosed-block"));
        QVERIFY(!diagnostic.hasFix);
    }
};

int runTherionSourceValidatorFixTest(int argc, char **argv)
{
    TherionSourceValidatorFixTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceValidatorFixTest.moc"
