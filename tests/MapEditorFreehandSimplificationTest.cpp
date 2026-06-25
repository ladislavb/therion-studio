#include "../src/app/text_editor/map_editor/MapEditorInteractiveDrawLogic.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

namespace
{
class MapEditorFreehandSimplificationTest final : public QObject
{
    Q_OBJECT

private slots:
    void preservesMoreDetailForCurvedStrokes();
    void stillCollapsesNearlyStraightStrokes();
};

QVector<QPointF> sampledSineStroke(int samples, qreal length, qreal amplitude, qreal waves, qreal rise)
{
    QVector<QPointF> stroke;
    stroke.reserve(samples);
    for (int index = 0; index < samples; ++index) {
        const qreal t = static_cast<qreal>(index) / static_cast<qreal>(samples - 1);
        stroke.append(QPointF(t * length, std::sin(t * waves) * amplitude + t * rise));
    }
    return stroke;
}

void MapEditorFreehandSimplificationTest::preservesMoreDetailForCurvedStrokes()
{
    const QStringList mediumRows =
        bezierLineCoordinateRowsForFreehandStroke(sampledSineStroke(64, 120.0, 35.0, 8.0, 80.0));
    const QStringList detailedRows =
        bezierLineCoordinateRowsForFreehandStroke(sampledSineStroke(128, 260.0, 42.0, 16.0, 120.0));

    QVERIFY2(mediumRows.size() >= 10,
             qPrintable(QStringLiteral("Expected medium curved stroke to keep at least 10 rows, got %1.")
                            .arg(mediumRows.size())));
    QVERIFY2(detailedRows.size() >= 20,
             qPrintable(QStringLiteral("Expected detailed curved stroke to keep at least 20 rows, got %1.")
                            .arg(detailedRows.size())));
    QVERIFY(detailedRows.size() > mediumRows.size());
}

void MapEditorFreehandSimplificationTest::stillCollapsesNearlyStraightStrokes()
{
    QVector<QPointF> straightStroke;
    straightStroke.reserve(64);
    for (int index = 0; index < 64; ++index) {
        const qreal t = static_cast<qreal>(index) / 63.0;
        straightStroke.append(QPointF(t * 180.0, t * 12.0));
    }

    const QStringList rows = bezierLineCoordinateRowsForFreehandStroke(straightStroke);
    QVERIFY2(rows.size() <= 3,
             qPrintable(QStringLiteral("Expected straight freehand stroke to collapse to at most 3 rows, got %1.")
                            .arg(rows.size())));
}
}

int runMapEditorFreehandSimplificationTest(int argc, char **argv)
{
    MapEditorFreehandSimplificationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorFreehandSimplificationTest.moc"
