#include "../src/app/MainWindowStructureInfoPresenter.h"
#include "../src/core/ProjectStructureIndex.h"
#include "../src/app/reports/TherionCompileDatabaseFigures.h"
#include "../src/core/TherionXtherionData.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectStructureEntry makeEntry(const QString &category, const QString &name, const QString &namespacePath)
{
    ProjectStructureEntry entry;
    entry.category = category;
    entry.name = name;
    entry.namespacePath = namespacePath;
    entry.sourceFile = QStringLiteral("/project/cave/cave.th");
    entry.lineNumber = 12;
    return entry;
}

QString fieldValue(const MainWindowStructureInfoPresenter::Presentation &presentation, const QString &label)
{
    for (const MainWindowStructureInfoPresenter::Field &field : presentation.fields) {
        if (field.label == label) {
            return field.value;
        }
    }
    return QString();
}

class MainWindowStructureInfoPresenterTest : public QObject
{
    Q_OBJECT

private slots:
    void presentsSurvey();
    void presentsSurveyWithNamespaceOff();
    void presentsMapWithFullName();
    void presentsScrapAtTopLevel();
    void presentsUnnamedScrapWithoutFullName();
    void promptsWithoutSelection();
    void promptsForOtherRows();
    void omitsLocationWithoutLine();
    void htmlEscapesAndLinksSource();
    void notesSurveyStatisticsBeforeCompile();
    void presentsSurveyStatistics();
    void notesSurveyMissingFromCompile();
    void notesOutOfDateStatistics();
    void omitsStatisticsForMaps();
    void presentsMapFigures();
    void presentsScrapFigures();
    void notesMissingDatabaseExport();
    void notesMapMissingFromCompile();
    void offersDatabaseExportCompileOnlyWithoutExport();
};


MainWindowStructureInfoPresenter::CompileFiguresInput databaseInput()
{
    MainWindowStructureInfoPresenter::CompileFiguresInput input;
    input.available = true;
    input.databaseAvailable = true;
    return input;
}

void MainWindowStructureInfoPresenterTest::presentsMapFigures()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MC"), QStringLiteral("cave"));
    TherionMapFigures map;
    map.title = QStringLiteral("Cave plan");
    map.length = 18.0;
    map.depth = 5.394;
    map.items = {{4, true, QStringLiteral("MI"), QStringLiteral("inner.cave")},
                 {4, false, QStringLiteral("SC"), QStringLiteral("cave")},
                 {2, true, QStringLiteral("MI"), QStringLiteral("inner.cave")}};
    auto input = databaseInput();
    input.map = &map;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QString(), input);

    QCOMPARE(fieldValue(presentation, QStringLiteral("Title")), QStringLiteral("Cave plan"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Length")), QStringLiteral("18.00 m"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Depth")), QStringLiteral("5.39 m"));
    QCOMPARE(presentation.contents,
             QStringList({QStringLiteral("map MI@inner.cave"),
                          QStringLiteral("scrap SC@cave"),
                          QStringLiteral("map MI@inner.cave (preview above)")}));
    QVERIFY(presentation.notes.isEmpty());
    QVERIFY(MainWindowStructureInfoPresenter::html(presentation).contains(QStringLiteral("scrap SC@cave<br>")));
}

void MainWindowStructureInfoPresenterTest::presentsScrapFigures()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("SI"), QStringLiteral("inner.cave"));
    TherionScrapFigures scrap{2.75729, 30.77453};
    auto input = databaseInput();
    input.scrap = &scrap;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QString(), input);

    QCOMPARE(fieldValue(presentation, QStringLiteral("Average distortion")), QStringLiteral("2.76 %"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Maximum distortion")), QStringLiteral("30.77 %"));
    QVERIFY(presentation.contents.isEmpty());
}

void MainWindowStructureInfoPresenterTest::notesMissingDatabaseExport()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("SI"), QStringLiteral("inner.cave"));
    auto input = databaseInput();
    input.databaseAvailable = false;

    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList{QStringLiteral(
                 "Map and scrap details need a database export. Add export database -fmt sql to the config.")});
    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString()).notes,
             QStringList{QStringLiteral("Map and scrap details appear after a successful compile.")});
}

void MainWindowStructureInfoPresenterTest::notesMapMissingFromCompile()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MC"), QStringLiteral("cave"));
    auto input = databaseInput();
    input.sourcesChanged = true;

    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList({QStringLiteral("This map is not in the last successful compile."),
                          QStringLiteral("Out of date because sources changed after the last compile.")}));
}

MainWindowStructureInfoPresenter::CompileFiguresInput compiledInput(const TherionXtherionSurvey *survey)
{
    MainWindowStructureInfoPresenter::CompileFiguresInput input;
    input.available = true;
    input.survey = survey;
    return input;
}

void MainWindowStructureInfoPresenterTest::notesSurveyStatisticsBeforeCompile()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(presentation.statistics.isEmpty());
    QCOMPARE(presentation.notes, QStringList{QStringLiteral("Survey statistics appear after a successful compile.")});
}

void MainWindowStructureInfoPresenterTest::presentsSurveyStatistics()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    TherionXtherionSurvey survey;
    survey.title = QStringLiteral("Inner Series");
    survey.statistics = QStringLiteral("length: 15.00m\nnumber of shots: 2");
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"), compiledInput(&survey));

    QCOMPARE(fieldValue(presentation, QStringLiteral("Title")), QStringLiteral("Inner Series"));
    QCOMPARE(presentation.statistics, QStringList({QStringLiteral("length: 15.00m"), QStringLiteral("number of shots: 2")}));
    QVERIFY(presentation.notes.isEmpty());
    QVERIFY(MainWindowStructureInfoPresenter::html(presentation).contains(QStringLiteral("length: 15.00m<br>number of shots: 2")));
}

void MainWindowStructureInfoPresenterTest::notesSurveyMissingFromCompile()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"), compiledInput(nullptr));

    QVERIFY(presentation.statistics.isEmpty());
    QCOMPARE(presentation.notes, QStringList{QStringLiteral("This survey is not in the last successful compile.")});
}

void MainWindowStructureInfoPresenterTest::notesOutOfDateStatistics()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    TherionXtherionSurvey survey;
    survey.statistics = QStringLiteral("length: 15.00m");
    auto input = compiledInput(&survey);
    input.sourcesChanged = true;
    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList{QStringLiteral("Out of date because sources changed after the last compile.")});

    input.laterCompileFailed = true;
    QCOMPARE(MainWindowStructureInfoPresenter::present(&entry, QString(), input).notes,
             QStringList{QStringLiteral("The last compile failed, so these figures are from an earlier compile.")});
}

void MainWindowStructureInfoPresenterTest::omitsStatisticsForMaps()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QStringLiteral("cave"));
    TherionXtherionSurvey survey;
    survey.statistics = QStringLiteral("length: 15.00m");
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QString(), compiledInput(&survey));

    QVERIFY(presentation.statistics.isEmpty());
    QVERIFY(!presentation.notes.contains(QStringLiteral("Survey statistics appear after a successful compile.")));
}

void MainWindowStructureInfoPresenterTest::presentsSurvey()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("inner"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QCOMPARE(presentation.heading, QStringLiteral("Survey inner"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("inner.cave"));
    QCOMPARE(presentation.sourceLocation, QStringLiteral("cave/cave.th, line 12"));
    QVERIFY(presentation.prompt.isEmpty());
}

void MainWindowStructureInfoPresenterTest::presentsSurveyWithNamespaceOff()
{
    ProjectStructureEntry entry = makeEntry(QStringLiteral("Surveys"), QStringLiteral("flat"), QStringLiteral("cave"));
    entry.createsNamespace = false;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    // Therion still names a -namespace off survey child.parent.
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("flat.cave"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Namespace")), QStringLiteral("off"));
}

void MainWindowStructureInfoPresenterTest::presentsMapWithFullName()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QStringLiteral("inner.cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QCOMPARE(presentation.heading, QStringLiteral("Map MP1"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("MP1@inner.cave"));
}

void MainWindowStructureInfoPresenterTest::presentsScrapAtTopLevel()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("SP1"), QString());
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave.th2"));

    QCOMPARE(presentation.heading, QStringLiteral("Scrap SP1"));
    QCOMPARE(fieldValue(presentation, QStringLiteral("Full name")), QStringLiteral("SP1"));
}

void MainWindowStructureInfoPresenterTest::presentsUnnamedScrapWithoutFullName()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Scraps"), QStringLiteral("Unnamed Scrap"), QStringLiteral("cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave.th2"));

    QCOMPARE(presentation.heading, QStringLiteral("Scrap Unnamed Scrap"));
    QVERIFY(presentation.fields.isEmpty());
}

void MainWindowStructureInfoPresenterTest::promptsWithoutSelection()
{
    const auto presentation = MainWindowStructureInfoPresenter::present(nullptr, QString());

    QVERIFY(!presentation.prompt.isEmpty());
    QVERIFY(presentation.heading.isEmpty());
    QVERIFY(presentation.fields.isEmpty());
}

void MainWindowStructureInfoPresenterTest::promptsForOtherRows()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Stations"), QStringLiteral("1"), QStringLiteral("cave"));
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(!presentation.prompt.isEmpty());
    QVERIFY(presentation.heading.isEmpty());
}

void MainWindowStructureInfoPresenterTest::omitsLocationWithoutLine()
{
    ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MP1"), QString());
    entry.lineNumber = 0;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th"));

    QVERIFY(presentation.sourceLocation.isEmpty());
    QVERIFY(!MainWindowStructureInfoPresenter::html(presentation).contains(QStringLiteral("<a ")));
}

void MainWindowStructureInfoPresenterTest::htmlEscapesAndLinksSource()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("a<b"), QString());
    const QString html = MainWindowStructureInfoPresenter::html(
        MainWindowStructureInfoPresenter::present(&entry, QStringLiteral("cave/cave.th")));

    QVERIFY(html.contains(QStringLiteral("a&lt;b")));
    QVERIFY(!html.contains(QStringLiteral("a<b")));
    QVERIFY(html.contains(QStringLiteral("href=\"%1\"").arg(MainWindowStructureInfoPresenter::sourceLinkTarget())));
    QVERIFY(html.contains(QStringLiteral("cave/cave.th, line 12")));
}
void MainWindowStructureInfoPresenterTest::offersDatabaseExportCompileOnlyWithoutExport()
{
    const ProjectStructureEntry entry = makeEntry(QStringLiteral("Maps"), QStringLiteral("MC"), QStringLiteral("cave"));
    auto input = databaseInput();
    QVERIFY(!MainWindowStructureInfoPresenter::present(&entry, QString(), input).offerDatabaseExportCompile);

    input.databaseAvailable = false;
    const auto presentation = MainWindowStructureInfoPresenter::present(&entry, QString(), input);
    QVERIFY(presentation.offerDatabaseExportCompile);
    QVERIFY(MainWindowStructureInfoPresenter::html(presentation).contains(
        QStringLiteral("href=\"%1\"").arg(MainWindowStructureInfoPresenter::databaseExportCompileLinkTarget())));
    QVERIFY(!MainWindowStructureInfoPresenter::present(&entry, QString()).offerDatabaseExportCompile);
}

}

int runMainWindowStructureInfoPresenterTest(int argc, char **argv)
{
    MainWindowStructureInfoPresenterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowStructureInfoPresenterTest.moc"
