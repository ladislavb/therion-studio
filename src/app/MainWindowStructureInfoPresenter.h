#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct ProjectStructureEntry;
struct TherionXtherionSurvey;
struct TherionMapFigures;
struct TherionScrapFigures;

// Text of the info box under the Survey and Map Structure trees.
class MainWindowStructureInfoPresenter final
{
    Q_DECLARE_TR_FUNCTIONS(MainWindowStructureInfoPresenter)

public:
    struct Field
    {
        QString label;
        QString value;
    };

    struct Presentation
    {
        QString heading;
        QVector<Field> fields;
        QString sourceLocation;
        // Survey statistics lines as Therion writes them.
        QStringList statistics;
        // Map contents, one map or scrap per line.
        QStringList contents;
        QStringList notes;
        // Offer a compile that adds a database export without changing the config.
        bool offerDatabaseExportCompile = false;
        QString prompt;
    };

    // Compile figures of the selected row; the default means no good compile yet.
    struct CompileFiguresInput
    {
        bool available = false;
        // The compile wrote a SQL database export.
        bool databaseAvailable = false;
        // Each is null when the selected object is not in the compile.
        const TherionXtherionSurvey *survey = nullptr;
        const TherionMapFigures *map = nullptr;
        const TherionScrapFigures *scrap = nullptr;
        bool sourcesChanged = false;
        bool laterCompileFailed = false;
    };

    // Link targets in html(): the definition location and the database export compile.
    static QString sourceLinkTarget();
    static QString databaseExportCompileLinkTarget();

    // entry is null when no survey, map or scrap row is selected.
    static Presentation present(const ProjectStructureEntry *entry,
                                const QString &sourceDisplayPath,
                                const CompileFiguresInput &figures);
    static Presentation present(const ProjectStructureEntry *entry, const QString &sourceDisplayPath);
    static QString html(const Presentation &presentation);
};

// Name shown for a structure entry; unnamed scraps get a translated label.
QString structureEntryDisplayName(const ProjectStructureEntry &entry);
}
