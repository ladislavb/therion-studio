#include "../src/app/text_editor/map_editor/MapEditorInputPolicy.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorInputPolicyTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesWheelAction();
    void resolvesTouchPanCandidate();
};
}

void MapEditorInputPolicyTest::resolvesWheelAction()
{
    QCOMPARE(resolveMapEditorWheelAction(false, false, false), MapEditorWheelAction::Zoom);
    QCOMPARE(resolveMapEditorWheelAction(false, true, false), MapEditorWheelAction::Pan);
    QCOMPARE(resolveMapEditorWheelAction(false, true, true), MapEditorWheelAction::Zoom);
    QCOMPARE(resolveMapEditorWheelAction(true, false, false), MapEditorWheelAction::Pan);
    QCOMPARE(resolveMapEditorWheelAction(true, true, false), MapEditorWheelAction::Pan);
    QCOMPARE(resolveMapEditorWheelAction(true, false, true), MapEditorWheelAction::Zoom);
}

void MapEditorInputPolicyTest::resolvesTouchPanCandidate()
{
    QVERIFY(shouldEnableTouchPanCandidate(true, false, false));
    QVERIFY(shouldEnableTouchPanCandidate(false, true, false));
    QVERIFY(!shouldEnableTouchPanCandidate(false, false, false));
    QVERIFY(!shouldEnableTouchPanCandidate(true, true, true));
}

int runMapEditorInputPolicyTest(int argc, char **argv)
{
    MapEditorInputPolicyTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorInputPolicyTest.moc"
