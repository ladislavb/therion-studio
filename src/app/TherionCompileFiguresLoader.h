#pragma once

#include "TherionCompileResult.h"

#include "reports/TherionCompileDatabaseFigures.h"

#include "../core/TherionXtherionData.h"

#include <QHash>
#include <QObject>

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
// Figures of the last good compile, from the .xtherion.dat and SQL database export it wrote.
struct TherionCompileFigures
{
    // The compile the figures come from; invalid before the first good compile.
    TherionCompileResult compile;
    QHash<QString, TherionXtherionSurvey> surveysByFullName;
    // Unavailable when the compile wrote no database export.
    TherionCompileDatabaseFigures database;
    // A later compile failed or wrote no survey tree, so the figures are older.
    bool laterCompileFailed = false;

    [[nodiscard]] bool isAvailable() const { return compile.isValid(); }
};

// Reads the compile's files off the UI thread after each compile and keeps the last good figures.
class TherionCompileFiguresLoader final : public QObject
{
    Q_OBJECT

public:
    explicit TherionCompileFiguresLoader(QObject *parent = nullptr);

    // An invalid result (project closed) clears the figures.
    void updateFromCompile(const TherionCompileResult &result);

    [[nodiscard]] const TherionCompileFigures &figures() const;

signals:
    void figuresChanged();

private:
    struct Loaded
    {
        quint64 generation = 0;
        TherionCompileResult compile;
        QVector<TherionXtherionSurvey> surveys;
        TherionCompileDatabaseFigures database;
    };

    void handleLoadFinished();
    void markLaterCompileFailed();

    quint64 generation_ = 0;
    TherionCompileFigures figures_;
    QFutureWatcher<Loaded> *loadWatcher_ = nullptr;
};
}
