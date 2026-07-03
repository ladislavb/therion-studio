#include "ValidationResultsMarkdownExporter.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>

#include <algorithm>

namespace
{
QString severityLabel(TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    switch (severity) {
    case TherionStudio::TherionSourceDiagnosticSeverity::Warning:
        return TherionStudio::ValidationResultsMarkdownExporter::tr("Warning");
    case TherionStudio::TherionSourceDiagnosticSeverity::Error:
        return TherionStudio::ValidationResultsMarkdownExporter::tr("Error");
    }
    return TherionStudio::ValidationResultsMarkdownExporter::tr("Warning");
}

QString displayPath(const QString &projectRootPath, const QString &filePath)
{
    if (filePath.isEmpty()) {
        return TherionStudio::ValidationResultsMarkdownExporter::tr("Untitled document");
    }

    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    if (!projectRootPath.trimmed().isEmpty()) {
        const QDir rootDir(projectRootPath);
        const QString relativePath = rootDir.relativeFilePath(absolutePath);
        if (!relativePath.startsWith(QStringLiteral("../"))
            && relativePath != QStringLiteral("..")
            && !QDir::isAbsolutePath(relativePath)) {
            return QDir::toNativeSeparators(relativePath);
        }
    }
    return QDir::toNativeSeparators(absolutePath);
}

QString markdownInlineCode(QString text)
{
    if (text.isEmpty()) {
        text = QStringLiteral(" ");
    }

    int longestBacktickRun = 0;
    int currentRun = 0;
    for (const QChar character : std::as_const(text)) {
        if (character == QLatin1Char('`')) {
            ++currentRun;
            longestBacktickRun = qMax(longestBacktickRun, currentRun);
        } else {
            currentRun = 0;
        }
    }

    const QString delimiter(longestBacktickRun + 1, QLatin1Char('`'));
    if (text.startsWith(QLatin1Char('`')) || text.endsWith(QLatin1Char('`'))) {
        text = QLatin1Char(' ') + text + QLatin1Char(' ');
    }
    return delimiter + text + delimiter;
}

QString markdownHeadingText(QString text)
{
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text = text.simplified();
    if (text.isEmpty()) {
        return TherionStudio::ValidationResultsMarkdownExporter::tr("Untitled");
    }
    return text;
}

QString fencedCodeBlock(const QString &text, const QString &language = QString())
{
    int longestBacktickRun = 0;
    int currentRun = 0;
    for (const QChar character : text) {
        if (character == QLatin1Char('`')) {
            ++currentRun;
            longestBacktickRun = qMax(longestBacktickRun, currentRun);
        } else {
            currentRun = 0;
        }
    }

    const QString fence(qMax(3, longestBacktickRun + 1), QLatin1Char('`'));
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (!normalized.endsWith(QLatin1Char('\n'))) {
        normalized.append(QLatin1Char('\n'));
    }

    return fence + language + QLatin1Char('\n') + normalized + fence + QLatin1String("\n");
}

bool diagnosticLess(const TherionStudio::ProjectValidationScanner::Finding &left,
                    const TherionStudio::ProjectValidationScanner::Finding &right)
{
    if (left.filePath != right.filePath) {
        return left.filePath < right.filePath;
    }
    if (left.diagnostic.lineNumber != right.diagnostic.lineNumber) {
        return left.diagnostic.lineNumber < right.diagnostic.lineNumber;
    }
    if (left.diagnostic.columnNumber != right.diagnostic.columnNumber) {
        return left.diagnostic.columnNumber < right.diagnostic.columnNumber;
    }
    return left.diagnostic.title < right.diagnostic.title;
}
}

namespace TherionStudio
{
QString ValidationResultsMarkdownExporter::exportFindings(const QVector<ProjectValidationScanner::Finding> &findings,
                                                          const Options &options)
{
    const QDateTime generatedAt = options.generatedAt.isValid()
        ? options.generatedAt
        : QDateTime::currentDateTime();

    int errorCount = 0;
    int warningCount = 0;
    for (const ProjectValidationScanner::Finding &finding : findings) {
        switch (finding.diagnostic.severity) {
        case TherionSourceDiagnosticSeverity::Error:
            ++errorCount;
            break;
        case TherionSourceDiagnosticSeverity::Warning:
            ++warningCount;
            break;
        }
    }

    QString markdown;
    markdown += QStringLiteral("# %1\n\n").arg(ValidationResultsMarkdownExporter::tr("Validation Results"));
    markdown += QStringLiteral("- %1: %2\n")
                    .arg(ValidationResultsMarkdownExporter::tr("Generated"),
                         QLocale().toString(generatedAt, QLocale::ShortFormat));
    if (!options.scopeLabel.trimmed().isEmpty()) {
        markdown += QStringLiteral("- %1: %2\n")
                        .arg(ValidationResultsMarkdownExporter::tr("Scope"),
                             markdownInlineCode(options.scopeLabel.trimmed()));
    }
    if (!options.projectRootPath.trimmed().isEmpty()) {
        markdown += QStringLiteral("- %1: %2\n")
                        .arg(ValidationResultsMarkdownExporter::tr("Project"),
                             markdownInlineCode(
                                 QDir::toNativeSeparators(QFileInfo(options.projectRootPath).absoluteFilePath())));
    }
    if (options.searchedFileCount > 0) {
        markdown += QStringLiteral("- %1: %2\n")
                        .arg(ValidationResultsMarkdownExporter::tr("Searched files"))
                        .arg(options.searchedFileCount);
    }
    markdown += QStringLiteral("- %1: %2\n").arg(ValidationResultsMarkdownExporter::tr("Findings")).arg(findings.size());
    markdown += QStringLiteral("- %1: %2\n").arg(ValidationResultsMarkdownExporter::tr("Errors")).arg(errorCount);
    markdown += QStringLiteral("- %1: %2\n").arg(ValidationResultsMarkdownExporter::tr("Warnings")).arg(warningCount);
    if (options.limitReached) {
        markdown += QStringLiteral("- %1\n")
                        .arg(ValidationResultsMarkdownExporter::tr(
                            "Result limit was reached; only visible findings are included."));
    }
    markdown += QLatin1Char('\n');

    if (findings.isEmpty()) {
        markdown += ValidationResultsMarkdownExporter::tr("No validation problems found.");
        markdown += QStringLiteral("\n");
        return markdown;
    }

    QVector<ProjectValidationScanner::Finding> sortedFindings = findings;
    std::stable_sort(sortedFindings.begin(), sortedFindings.end(), diagnosticLess);

    QString currentFilePath;
    for (const ProjectValidationScanner::Finding &finding : std::as_const(sortedFindings)) {
        if (finding.filePath != currentFilePath) {
            currentFilePath = finding.filePath;
            markdown += QStringLiteral("## %1\n\n").arg(markdownHeadingText(displayPath(options.projectRootPath, currentFilePath)));
        }

        const TherionSourceDiagnostic &diagnostic = finding.diagnostic;
        markdown += QStringLiteral("### %1 %2: %3: %4\n\n")
                        .arg(ValidationResultsMarkdownExporter::tr("Line"))
                        .arg(qMax(1, diagnostic.lineNumber))
                        .arg(severityLabel(diagnostic.severity),
                             markdownHeadingText(diagnostic.title));
        if (!diagnostic.message.trimmed().isEmpty()) {
            markdown += diagnostic.message.trimmed();
            markdown += QStringLiteral("\n\n");
        }
        if (!diagnostic.currentText.isEmpty()) {
            markdown += QStringLiteral("**%1**\n\n").arg(ValidationResultsMarkdownExporter::tr("Current source"));
            markdown += fencedCodeBlock(diagnostic.currentText, QStringLiteral("therion"));
            markdown += QLatin1Char('\n');
        }
        if (diagnostic.hasFix && !diagnostic.suggestedText.isEmpty()) {
            markdown += QStringLiteral("**%1**\n\n").arg(ValidationResultsMarkdownExporter::tr("Automatic fix preview"));
            markdown += fencedCodeBlock(diagnostic.suggestedText, QStringLiteral("therion"));
            markdown += QLatin1Char('\n');
        }
    }

    return markdown;
}
}
