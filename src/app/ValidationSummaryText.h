#pragma once

#include "../core/TherionSourceValidator.h"

#include <QCoreApplication>
#include <QPalette>
#include <QString>
#include <QVector>

namespace TherionStudio
{
// The Validation panel's summary line: errors, warnings and notes, each in its severity colour and left out when
// zero, followed by the scope.
class ValidationSummaryText final
{
public:
    Q_DECLARE_TR_FUNCTIONS(TherionStudio::ValidationSummaryText)

public:
    [[nodiscard]] static QString html(const QVector<TherionSourceDiagnostic> &diagnostics,
                                      const QString &scope,
                                      const QPalette &palette);
    // " · N files searched"
    [[nodiscard]] static QString projectScope(int searchedFileCount);
    // " in <file>"
    [[nodiscard]] static QString documentScope(const QString &documentLabel);
};
}
