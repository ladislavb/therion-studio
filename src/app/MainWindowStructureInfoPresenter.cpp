#include "MainWindowStructureInfoPresenter.h"

#include "../core/ProjectStructureIndex.h"
#include "reports/TherionCompileDatabaseFigures.h"

#include "../core/TherionXtherionData.h"

namespace TherionStudio
{
namespace
{
bool isUnnamedScrap(const ProjectStructureEntry &entry)
{
    return entry.category == QStringLiteral("Scraps") && entry.name == QStringLiteral("Unnamed Scrap");
}

// Therion full name: innermost to outermost, objects as name@survey.parent.
QString therionFullName(const ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Surveys")) {
        return ProjectStructureIndex::surveyFullName(entry);
    }
    if (entry.namespacePath.isEmpty()) {
        return entry.name;
    }
    return QStringLiteral("%1@%2").arg(entry.name, entry.namespacePath);
}

using Presenter = MainWindowStructureInfoPresenter;

// Therion keyword of a map item type (thdb2dmi.h), empty for a normal item.
QString mapItemTypeKeyword(int type)
{
    switch (type) {
    case 1:
        return QStringLiteral("preview below");
    case 2:
        return QStringLiteral("preview above");
    case 3:
        return QStringLiteral("beside");
    case 5:
        return QStringLiteral("none");
    default:
        return QString();
    }
}

QString metres(double value)
{
    return MainWindowStructureInfoPresenter::tr("%1 m").arg(QString::number(value, 'f', 2));
}

QString percent(double value)
{
    return MainWindowStructureInfoPresenter::tr("%1 %").arg(QString::number(value, 'f', 2));
}

void appendFigureNotes(Presenter::Presentation *result, const Presenter::CompileFiguresInput &figures)
{
    if (figures.laterCompileFailed) {
        result->notes.append(MainWindowStructureInfoPresenter::tr(
            "The last compile failed, so these figures are from an earlier compile."));
    } else if (figures.sourcesChanged) {
        result->notes.append(MainWindowStructureInfoPresenter::tr(
            "Out of date because sources changed after the last compile."));
    }
}

void appendSurveyFigures(Presenter::Presentation *result, const Presenter::CompileFiguresInput &figures)
{
    if (!figures.available) {
        result->notes.append(MainWindowStructureInfoPresenter::tr("Survey statistics appear after a successful compile."));
        return;
    }
    if (figures.survey == nullptr) {
        result->notes.append(MainWindowStructureInfoPresenter::tr("This survey is not in the last successful compile."));
    } else {
        if (!figures.survey->title.isEmpty()) {
            result->fields.append({MainWindowStructureInfoPresenter::tr("Title"), figures.survey->title});
        }
        result->statistics = figures.survey->statistics.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    appendFigureNotes(result, figures);
}

// Returns false when the database figures are missing, after adding a note that says why.
bool hasDatabaseFigures(Presenter::Presentation *result,
                        const Presenter::CompileFiguresInput &figures,
                        bool objectFound,
                        const QString &missingNote)
{
    if (!figures.available) {
        result->notes.append(MainWindowStructureInfoPresenter::tr("Map and scrap details appear after a successful compile."));
        return false;
    }
    if (!figures.databaseAvailable) {
        result->notes.append(MainWindowStructureInfoPresenter::tr(
            "Map and scrap details need a database export. Add export database -fmt sql to the config."));
        result->offerDatabaseExportCompile = true;
        return false;
    }
    if (!objectFound) {
        result->notes.append(missingNote);
        appendFigureNotes(result, figures);
        return false;
    }
    return true;
}

void appendMapFigures(Presenter::Presentation *result, const Presenter::CompileFiguresInput &figures)
{
    if (!hasDatabaseFigures(result, figures, figures.map != nullptr,
                            MainWindowStructureInfoPresenter::tr("This map is not in the last successful compile."))) {
        return;
    }
    const TherionMapFigures &map = *figures.map;
    if (!map.title.isEmpty()) {
        result->fields.append({MainWindowStructureInfoPresenter::tr("Title"), map.title});
    }
    if (map.length) {
        result->fields.append({MainWindowStructureInfoPresenter::tr("Length"), metres(*map.length)});
    }
    if (map.depth) {
        result->fields.append({MainWindowStructureInfoPresenter::tr("Depth"), metres(*map.depth)});
    }
    // Therion object kinds and keywords stay untranslated.
    for (const TherionMapItemFigures &item : map.items) {
        QString line = QStringLiteral("%1 %2").arg(item.isMap ? QStringLiteral("map") : QStringLiteral("scrap"),
                                                   therionCompiledObjectKey(item.name, item.surveyFullName));
        const QString keyword = mapItemTypeKeyword(item.type);
        if (!keyword.isEmpty()) {
            line += QStringLiteral(" (%1)").arg(keyword);
        }
        result->contents.append(line);
    }
    appendFigureNotes(result, figures);
}

void appendScrapFigures(Presenter::Presentation *result, const Presenter::CompileFiguresInput &figures)
{
    if (!hasDatabaseFigures(result, figures, figures.scrap != nullptr,
                            MainWindowStructureInfoPresenter::tr("This scrap is not in the last successful compile."))) {
        return;
    }
    if (figures.scrap->averageDistortion) {
        result->fields.append({MainWindowStructureInfoPresenter::tr("Average distortion"), percent(*figures.scrap->averageDistortion)});
    }
    if (figures.scrap->maximumDistortion) {
        result->fields.append({MainWindowStructureInfoPresenter::tr("Maximum distortion"), percent(*figures.scrap->maximumDistortion)});
    }
    appendFigureNotes(result, figures);
}
}

QString structureEntryDisplayName(const ProjectStructureEntry &entry)
{
    return isUnnamedScrap(entry) ? QObject::tr("Unnamed Scrap") : entry.name;
}

QString MainWindowStructureInfoPresenter::sourceLinkTarget()
{
    return QStringLiteral("structure-info:source");
}

QString MainWindowStructureInfoPresenter::databaseExportCompileLinkTarget()
{
    return QStringLiteral("structure-info:compile-database-export");
}

MainWindowStructureInfoPresenter::Presentation MainWindowStructureInfoPresenter::present(
    const ProjectStructureEntry *entry,
    const QString &sourceDisplayPath)
{
    return present(entry, sourceDisplayPath, CompileFiguresInput());
}

MainWindowStructureInfoPresenter::Presentation MainWindowStructureInfoPresenter::present(
    const ProjectStructureEntry *entry,
    const QString &sourceDisplayPath,
    const CompileFiguresInput &figures)
{
    Presentation result;
    const bool supported = entry != nullptr
        && (entry->category == QStringLiteral("Surveys")
            || entry->category == QStringLiteral("Maps")
            || entry->category == QStringLiteral("Scraps"));
    if (!supported) {
        result.prompt = tr("Select a survey, map or scrap to see its details.");
        return result;
    }

    const QString name = structureEntryDisplayName(*entry);
    if (entry->category == QStringLiteral("Surveys")) {
        result.heading = tr("Survey %1").arg(name);
        result.fields.append({tr("Full name"), therionFullName(*entry)});
        if (!entry->createsNamespace) {
            result.fields.append({tr("Namespace"), tr("off")});
        }
        appendSurveyFigures(&result, figures);
    } else {
        result.heading = entry->category == QStringLiteral("Maps")
            ? tr("Map %1").arg(name)
            : tr("Scrap %1").arg(name);
        if (!isUnnamedScrap(*entry)) {
            result.fields.append({tr("Full name"), therionFullName(*entry)});
        }
        if (entry->category == QStringLiteral("Maps")) {
            appendMapFigures(&result, figures);
        } else {
            appendScrapFigures(&result, figures);
        }
    }

    if (!sourceDisplayPath.isEmpty() && entry->lineNumber > 0) {
        result.sourceLocation = tr("%1, line %2").arg(sourceDisplayPath).arg(entry->lineNumber);
    }
    return result;
}

QString MainWindowStructureInfoPresenter::html(const Presentation &presentation)
{
    if (!presentation.prompt.isEmpty()) {
        return QStringLiteral("<p>%1</p>").arg(presentation.prompt.toHtmlEscaped());
    }

    QString rows;
    const auto appendRow = [&rows](const QString &label, const QString &valueHtml) {
        rows += QStringLiteral("<tr><td style=\"padding-right:8px\">%1</td><td>%2</td></tr>")
                    .arg(label.toHtmlEscaped(), valueHtml);
    };
    for (const Field &field : presentation.fields) {
        appendRow(field.label, field.value.toHtmlEscaped());
    }
    if (!presentation.sourceLocation.isEmpty()) {
        appendRow(tr("Defined in"),
                  QStringLiteral("<a href=\"%1\">%2</a>")
                      .arg(sourceLinkTarget(), presentation.sourceLocation.toHtmlEscaped()));
    }

    QString html = QStringLiteral("<p><b>%1</b></p><table>%2</table>")
                       .arg(presentation.heading.toHtmlEscaped(), rows);
    for (const QStringList &block : {presentation.statistics, presentation.contents}) {
        if (block.isEmpty()) {
            continue;
        }
        QStringList lines;
        for (const QString &line : block) {
            lines.append(line.toHtmlEscaped());
        }
        html += QStringLiteral("<p>%1</p>").arg(lines.join(QStringLiteral("<br>")));
    }
    for (const QString &note : presentation.notes) {
        html += QStringLiteral("<p><i>%1</i></p>").arg(note.toHtmlEscaped());
    }
    if (presentation.offerDatabaseExportCompile) {
        html += QStringLiteral("<p><a href=\"%1\">%2</a></p>")
                    .arg(databaseExportCompileLinkTarget(),
                         tr("Compile once with a database export, without changing the config").toHtmlEscaped());
    }
    return html;
}
}
