#include "../src/app/text_editor/map_editor/MapEditorSceneGeneration.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorSceneGenerationTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsOnlyCurrentGeneration();
};

void MapEditorSceneGenerationTest::acceptsOnlyCurrentGeneration()
{
    MapEditorSceneGeneration generation;
    QCOMPARE(generation.current(), 0U);

    const quint64 first = generation.beginRefresh();
    QCOMPARE(first, 1U);
    QVERIFY(generation.isCurrent(first));

    const quint64 second = generation.beginRefresh();
    QCOMPARE(second, 2U);
    QVERIFY(!generation.isCurrent(first));
    QVERIFY(generation.isCurrent(second));
}
}

int runMapEditorSceneGenerationTest(int argc, char **argv)
{
    MapEditorSceneGenerationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorSceneGenerationTest.moc"
