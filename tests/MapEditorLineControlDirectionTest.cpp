#include "../src/app/text_editor/map_editor/MapEditorLineControlDirection.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorLineControlDirectionTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsDirectionWhileChangingLength();
    void ignoresMovementAcrossTheHandle();
    void stopsShortOfTheAnchor();
    void leavesZeroLengthHandleUnchanged();
    void detectsUnchangedDirection();
};

void MapEditorLineControlDirectionTest::keepsDirectionWhileChangingLength()
{
    const QPointF anchor(10.0, 10.0);
    const QPointF control(13.0, 14.0);
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, QPointF(16.0, 18.0), 0.5), QPointF(16.0, 18.0));
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, QPointF(11.5, 12.0), 0.5), QPointF(11.5, 12.0));
}

void MapEditorLineControlDirectionTest::ignoresMovementAcrossTheHandle()
{
    const QPointF anchor(0.0, 0.0);
    const QPointF control(10.0, 0.0);
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, QPointF(15.0, 7.0), 0.5), QPointF(15.0, 0.0));
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, QPointF(4.0, -3.0), 0.5), QPointF(4.0, 0.0));
}

void MapEditorLineControlDirectionTest::stopsShortOfTheAnchor()
{
    const QPointF anchor(0.0, 0.0);
    const QPointF control(0.0, 10.0);
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, QPointF(0.0, -20.0), 0.5), QPointF(0.0, 0.5));
    QCOMPARE(lengthOnlyLineControlPoint(anchor, control, anchor, 0.5), QPointF(0.0, 0.5));
}

void MapEditorLineControlDirectionTest::leavesZeroLengthHandleUnchanged()
{
    const QPointF anchor(3.0, 4.0);
    QCOMPARE(lengthOnlyLineControlPoint(anchor, anchor, QPointF(9.0, 9.0), 0.5), anchor);
}

void MapEditorLineControlDirectionTest::detectsUnchangedDirection()
{
    const QPointF anchor(10.0, 10.0);
    const QPointF control(13.0, 14.0);
    QVERIFY(lineControlDirectionUnchanged(anchor, control, QPointF(16.0, 18.0)));
    QVERIFY(lineControlDirectionUnchanged(anchor, control, QPointF(11.5, 12.0)));
    QVERIFY(!lineControlDirectionUnchanged(anchor, control, QPointF(7.0, 6.0)));
    QVERIFY(!lineControlDirectionUnchanged(anchor, control, QPointF(13.0, 15.0)));
    QVERIFY(!lineControlDirectionUnchanged(anchor, control, anchor));
    QVERIFY(!lineControlDirectionUnchanged(anchor, anchor, control));
}
} // namespace

int runMapEditorLineControlDirectionTest(int argc, char **argv)
{
    MapEditorLineControlDirectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorLineControlDirectionTest.moc"
