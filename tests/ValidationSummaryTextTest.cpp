#include "../src/app/ValidationSummaryText.h"
#include "../src/editor/ValidationSeverityStyle.h"

#include <QTextDocument>
#include <QtTest/QtTest>

#include <utility>

using namespace TherionStudio;

namespace
{
QVector<TherionSourceDiagnostic> diagnostics(int errors, int warnings, int notes)
{
    QVector<TherionSourceDiagnostic> result;
    for (const auto &[severity, count] : {std::pair{TherionSourceDiagnosticSeverity::Error, errors},
                                          std::pair{TherionSourceDiagnosticSeverity::Warning, warnings},
                                          std::pair{TherionSourceDiagnosticSeverity::Information, notes}}) {
        for (int index = 0; index < count; ++index) {
            TherionSourceDiagnostic diagnostic;
            diagnostic.severity = severity;
            result.append(diagnostic);
        }
    }
    return result;
}

QString plainText(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText();
}
}

class ValidationSummaryTextTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsCountsAndScope_data()
    {
        QTest::addColumn<int>("errors");
        QTest::addColumn<int>("warnings");
        QTest::addColumn<int>("notes");
        QTest::addColumn<int>("files");
        QTest::addColumn<QString>("expected");

        QTest::newRow("all kinds") << 12 << 7 << 234 << 405
                                   << QStringLiteral("12 errors · 7 warnings · 234 notes · 405 files searched");
        QTest::newRow("singular") << 1 << 1 << 1 << 1
                                  << QStringLiteral("1 error · 1 warning · 1 note · 1 file searched");
        QTest::newRow("zero counts left out") << 0 << 3 << 0 << 9 << QStringLiteral("3 warnings · 9 files searched");
        QTest::newRow("only notes") << 0 << 0 << 16 << 120
                                    << QStringLiteral("No problems · 16 notes · 120 files searched");
        QTest::newRow("nothing") << 0 << 0 << 0 << 120 << QStringLiteral("No problems · 120 files searched");
    }

    void listsCountsAndScope()
    {
        QFETCH(int, errors);
        QFETCH(int, warnings);
        QFETCH(int, notes);
        QFETCH(int, files);
        QFETCH(QString, expected);

        const QString html = ValidationSummaryText::html(diagnostics(errors, warnings, notes),
                                                         ValidationSummaryText::projectScope(files),
                                                         QPalette());
        QCOMPARE(plainText(html), expected);
        QVERIFY(Qt::mightBeRichText(html));
    }

    void colorsEachCountBySeverity()
    {
        const QPalette palette;
        const QString html = ValidationSummaryText::html(diagnostics(2, 1, 1), QString(), palette);
        for (const TherionSourceDiagnosticSeverity severity : {TherionSourceDiagnosticSeverity::Error,
                                                               TherionSourceDiagnosticSeverity::Warning,
                                                               TherionSourceDiagnosticSeverity::Information}) {
            QVERIFY(html.contains(validationSeverityForeground(severity, palette).name()));
        }
    }

    void escapesTheDocumentName()
    {
        const QString html = ValidationSummaryText::html(diagnostics(1, 0, 0),
                                                         ValidationSummaryText::documentScope(QStringLiteral("a<b>.th")),
                                                         QPalette());
        QCOMPARE(plainText(html), QStringLiteral("1 error in a<b>.th"));
    }
};

int runValidationSummaryTextTest(int argc, char **argv)
{
    ValidationSummaryTextTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ValidationSummaryTextTest.moc"
