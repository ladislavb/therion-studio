#pragma once

#include "ProjectValidationScanner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class ValidationResultsMarkdownExporter final
{
public:
    Q_DECLARE_TR_FUNCTIONS(TherionStudio::ValidationResultsMarkdownExporter)

public:
    struct Options
    {
        QString projectRootPath;
        QString scopeLabel;
        QDateTime generatedAt;
        int searchedFileCount = 0;
        bool limitReached = false;
    };

    [[nodiscard]] static QString exportFindings(const QVector<ProjectValidationScanner::Finding> &findings,
                                                const Options &options);
};
}
