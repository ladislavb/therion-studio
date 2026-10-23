#include "../../src/core/TherionXtherionData.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

// Lines in the form Therion 6.4 writes them (thselector.cxx).
class TherionXtherionDataTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesNestedSurveys();
    void skipsUnnamedTopLevelNode();
    void decodesTitleEscapes();
    void keepsSurveyWithoutStations();
    void ignoresOtherCommandsAndBrokenLines();
    void acceptsWindowsLineEndings();
};

void TherionXtherionDataTest::parsesNestedSurveys()
{
    const QString text = QStringLiteral(
        "xth_cp_data_tree_insert 16 1 1 cave cave \"Cave\" \"Cave\" \"length: 18.00m (surface 0.00m, duplicated 0.00m)\\nnumber of shots: 3\"\n"
        "xth_cp_data_tree_insert 18 16 2 inner inner.cave \"\" \"\" \"length: 15.00m (surface 0.00m, duplicated 0.00m)\\nvertical range: 0.00m (from 1@inner.cave at 0.00m to 1@inner.cave at 0.00m)\"\n");
    const QVector<TherionXtherionSurvey> surveys = parseTherionXtherionSurveys(text);

    QCOMPARE(surveys.size(), 2);
    QCOMPARE(surveys.at(0).name, QStringLiteral("cave"));
    QCOMPARE(surveys.at(0).fullName, QStringLiteral("cave"));
    QCOMPARE(surveys.at(0).title, QStringLiteral("Cave"));
    QCOMPARE(surveys.at(0).statistics,
             QStringLiteral("length: 18.00m (surface 0.00m, duplicated 0.00m)\nnumber of shots: 3"));
    QCOMPARE(surveys.at(1).name, QStringLiteral("inner"));
    QCOMPARE(surveys.at(1).fullName, QStringLiteral("inner.cave"));
    QVERIFY(surveys.at(1).title.isEmpty());
    QVERIFY(surveys.at(1).statistics.contains(QStringLiteral("from 1@inner.cave at 0.00m")));
}

void TherionXtherionDataTest::skipsUnnamedTopLevelNode()
{
    const QString text = QStringLiteral(
        "xth_cp_data_tree_insert 1 {} 0   \"\" \"\" \"length: 18.00m (surface 0.00m, duplicated 0.00m)\"\n");

    QVERIFY(parseTherionXtherionSurveys(text).isEmpty());
}

void TherionXtherionDataTest::decodesTitleEscapes()
{
    const QString text = QStringLiteral(
        "xth_cp_data_tree_insert 16 1 1 cave cave \"Cave \\\"Q\\\" 50\\% \\{x\\} back\\\\\\\\slash\" \"plain\" \"length: 1.00m\"\n");
    const QVector<TherionXtherionSurvey> surveys = parseTherionXtherionSurveys(text);

    QCOMPARE(surveys.size(), 1);
    QCOMPARE(surveys.at(0).title, QStringLiteral("Cave \"Q\" 50% {x} back\\\\slash"));
}

void TherionXtherionDataTest::keepsSurveyWithoutStations()
{
    const QString text = QStringLiteral(
        "xth_cp_data_tree_insert 22 16 2 empty empty.cave \"\" \"\" \"length: 0.00m (surface 0.00m, duplicated 0.00m)\"\n");
    const QVector<TherionXtherionSurvey> surveys = parseTherionXtherionSurveys(text);

    QCOMPARE(surveys.size(), 1);
    QCOMPARE(surveys.at(0).statistics, QStringLiteral("length: 0.00m (surface 0.00m, duplicated 0.00m)"));
}

void TherionXtherionDataTest::ignoresOtherCommandsAndBrokenLines()
{
    const QString text = QStringLiteral(
        "set xth(cp,files) {}\n"
        "xth_cp_data_tree_create\n"
        "xth_cp_map_tree_insert projection 0 p1 {} 0 plan {} {} {}\n"
        "xth_cp_data_tree_insert 18 16 2 inner\n");

    QVERIFY(parseTherionXtherionSurveys(text).isEmpty());
}

void TherionXtherionDataTest::acceptsWindowsLineEndings()
{
    const QString text = QStringLiteral(
        "xth_cp_data_tree_insert 16 1 1 cave cave \"\" \"\" \"length: 1.00m\"\r\n"
        "xth_cp_data_tree_insert 18 16 2 inner inner.cave \"\" \"\" \"length: 1.00m\"\r\n");
    const QVector<TherionXtherionSurvey> surveys = parseTherionXtherionSurveys(text);

    QCOMPARE(surveys.size(), 2);
    QCOMPARE(surveys.at(0).statistics, QStringLiteral("length: 1.00m"));
}

int runTherionXtherionDataTest(int argc, char **argv)
{
    TherionXtherionDataTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionXtherionDataTest.moc"
