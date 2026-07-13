#include "../src/app/ValidationResultsMarkdownExporter.h"

#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QTimeZone>

namespace
{
TherionStudio::ProjectValidationScanner::Finding finding(const QString &filePath,
                                                         TherionStudio::TherionSourceDiagnosticSeverity severity,
                                                         int lineNumber,
                                                         const QString &title,
                                                         const QString &message,
                                                         const QString &currentText,
                                                         const QString &suggestedText = QString())
{
    TherionStudio::TherionSourceDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.lineNumber = lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = currentText;
    diagnostic.suggestedText = suggestedText;
    diagnostic.hasFix = !suggestedText.isEmpty();
    return {filePath, diagnostic};
}
}

class ValidationResultsMarkdownExporterTest final : public QObject
{
    Q_OBJECT

private slots:
    void exportsGroupedProjectReport()
    {
        QTemporaryDir projectDirectory;
        QVERIFY(projectDirectory.isValid());
        const QString projectRoot = projectDirectory.path();
        const QDir projectRootDirectory(projectRoot);
        QVector<TherionStudio::ProjectValidationScanner::Finding> findings;
        findings.append(finding(projectRootDirectory.filePath(QStringLiteral("maps/a.th2")),
                                TherionStudio::TherionSourceDiagnosticSeverity::Warning,
                                12,
                                QStringLiteral("Portable path separator"),
                                QStringLiteral("Path uses backslash separators."),
                                QStringLiteral("input .\\data\\a.th"),
                                QStringLiteral("input ./data/a.th")));
        findings.append(finding(projectRootDirectory.filePath(QStringLiteral("thconfig")),
                                TherionStudio::TherionSourceDiagnosticSeverity::Error,
                                3,
                                QStringLiteral("Missing referenced source file"),
                                QStringLiteral("Referenced source file was not found."),
                                QStringLiteral("source missing.th")));

        TherionStudio::ValidationResultsMarkdownExporter::Options options;
        options.projectRootPath = projectRoot;
        options.scopeLabel = QStringLiteral("Project validation");
        options.generatedAt = QDateTime(QDate(2026, 7, 3),
                                        QTime(12, 30),
                                        QTimeZone(QByteArrayLiteral("UTC")));
        options.searchedFileCount = 2;
        options.limitReached = true;

        const QString markdown =
            TherionStudio::ValidationResultsMarkdownExporter::exportFindings(findings, options);

        QVERIFY(markdown.contains(QStringLiteral("# Validation Results")));
        QVERIFY(markdown.contains(QStringLiteral("- Findings: 2")));
        QVERIFY(markdown.contains(QStringLiteral("- Errors: 1")));
        QVERIFY(markdown.contains(QStringLiteral("- Warnings: 1")));
        QVERIFY(markdown.contains(QStringLiteral("Result limit was reached")));
        QVERIFY(markdown.contains(QStringLiteral("## %1").arg(QDir::toNativeSeparators(QStringLiteral("maps/a.th2")))));
        QVERIFY(markdown.contains(QStringLiteral("## %1").arg(QDir::toNativeSeparators(QStringLiteral("thconfig")))));
        QVERIFY(markdown.contains(QStringLiteral("### Line 12: Warning: Portable path separator")));
        QVERIFY(markdown.contains(QStringLiteral("```therion\ninput .\\data\\a.th\n```")));
        QVERIFY(markdown.contains(QStringLiteral("```therion\ninput ./data/a.th\n```")));
        QVERIFY(markdown.contains(QStringLiteral("### Line 3: Error: Missing referenced source file")));
    }

    void exportsEmptyReport()
    {
        TherionStudio::ValidationResultsMarkdownExporter::Options options;
        options.scopeLabel = QStringLiteral("Current document");
        options.searchedFileCount = 1;

        const QString markdown =
            TherionStudio::ValidationResultsMarkdownExporter::exportFindings({}, options);

        QVERIFY(markdown.contains(QStringLiteral("- Findings: 0")));
        QVERIFY(markdown.contains(QStringLiteral("- Errors: 0")));
        QVERIFY(markdown.contains(QStringLiteral("- Warnings: 0")));
        QVERIFY(markdown.contains(QStringLiteral("No validation problems found.")));
    }
};

QTEST_MAIN(ValidationResultsMarkdownExporterTest)

#include "ValidationResultsMarkdownExporterTest.moc"
