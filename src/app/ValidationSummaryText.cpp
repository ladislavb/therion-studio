#include "ValidationSummaryText.h"

#include "../editor/ValidationSeverityStyle.h"

#include <QStringList>

namespace TherionStudio
{
QString ValidationSummaryText::html(const QVector<TherionSourceDiagnostic> &diagnostics,
                                    const QString &scope,
                                    const QPalette &palette)
{
    int errors = 0;
    int warnings = 0;
    int notes = 0;
    for (const TherionSourceDiagnostic &diagnostic : diagnostics) {
        switch (diagnostic.severity) {
        case TherionSourceDiagnosticSeverity::Error:
            ++errors;
            break;
        case TherionSourceDiagnosticSeverity::Warning:
            ++warnings;
            break;
        case TherionSourceDiagnosticSeverity::Information:
            ++notes;
            break;
        }
    }

    auto coloured = [&palette](TherionSourceDiagnosticSeverity severity, const QString &text) {
        return QStringLiteral("<span style=\"color:%1; font-weight:600\">%2</span>")
            .arg(validationSeverityForeground(severity, palette).name(), text.toHtmlEscaped());
    };
    QStringList parts;
    if (errors == 0 && warnings == 0) {
        parts.append(tr("No problems").toHtmlEscaped());
    }
    if (errors > 0) {
        parts.append(coloured(TherionSourceDiagnosticSeverity::Error,
                              errors == 1 ? tr("1 error") : tr("%n errors", nullptr, errors)));
    }
    if (warnings > 0) {
        parts.append(coloured(TherionSourceDiagnosticSeverity::Warning,
                              warnings == 1 ? tr("1 warning") : tr("%n warnings", nullptr, warnings)));
    }
    if (notes > 0) {
        parts.append(coloured(TherionSourceDiagnosticSeverity::Information,
                              notes == 1 ? tr("1 note") : tr("%n notes", nullptr, notes)));
    }
    // One outer span so a label recognises rich text even when the line starts with plain words.
    return QStringLiteral("<span>%1%2</span>").arg(parts.join(QStringLiteral(" · ")), scope.toHtmlEscaped());
}

QString ValidationSummaryText::projectScope(int searchedFileCount)
{
    return QStringLiteral(" · ")
        + (searchedFileCount == 1 ? tr("1 file searched") : tr("%n files searched", nullptr, searchedFileCount));
}

QString ValidationSummaryText::documentScope(const QString &documentLabel)
{
    return QLatin1Char(' ') + tr("in %1").arg(documentLabel);
}
}
