// Geometric conformity of the raster background transform, against the
// composition Mapiah uses (lib/src/elements/mp_image_insert_config.dart).
//
// Mapiah scales the image from its `xx`/`yy` anchor and rotates the scaled
// image around its centre:
//
//     P_mapiah = S(c) + R(S(P - c))
//
// so with no rotation the anchor is a fixed point and P_mapiah = S(P).
//
// These cases pin the mapping of the four corners rather than the bounding
// rectangle: with a uniform scale, or with only the bounding box compared, a
// wrong order between rotation and scaling stays invisible.

#include "../src/app/text_editor/map_editor/MapEditorRasterBackgroundTransform.h"

#include <QObject>
#include <QPointF>
#include <QTest>
#include <QTransform>
#include <QtMath>

#include <cmath>

using namespace TherionStudio;

namespace
{
constexpr qreal kEpsilon = 1e-6;

constexpr int kPixmapWidth = 80;
constexpr int kPixmapHeight = 40;

// Deliberately non-uniform: with xScale == yScale a swapped rotation/scale
// order still maps every corner to the right place.
constexpr qreal kXScale = 1.4;
constexpr qreal kYScale = 0.7;

constexpr qreal kRotationDeg = 37.0;

// One preview unit is one image pixel, which keeps the expected values
// readable: the view scale is then the identity and the only transform left is
// the Mapiah one.
RasterBackgroundTransformInput makeInput(qreal rotationDeg,
                                         bool pivotSet,
                                         qreal pivotDx = 0.0,
                                         qreal pivotDy = 0.0)
{
    RasterBackgroundTransformInput input;
    input.viewScaleX = 1.0;
    input.viewScaleY = 1.0;
    input.layerScaleX = kXScale;
    input.layerScaleY = kYScale;
    input.rotationDeg = rotationDeg;
    input.pivotSet = pivotSet;
    input.pivotLocalX = pivotDx;
    input.pivotLocalY = pivotDy;
    input.intrinsicWidth = kPixmapWidth;
    input.intrinsicHeight = kPixmapHeight;
    return input;
}

QPointF mapiahExpectedPoint(const QPointF &local, const QPointF &pivot, qreal rotationDeg)
{
    const QPointF scaledPivot(pivot.x() * kXScale, pivot.y() * kYScale);
    const QPointF scaledOffset((local.x() - pivot.x()) * kXScale,
                               (local.y() - pivot.y()) * kYScale);

    const qreal radians = qDegreesToRadians(rotationDeg);
    const qreal cosine = std::cos(radians);
    const qreal sine = std::sin(radians);
    const QPointF rotated((scaledOffset.x() * cosine) - (scaledOffset.y() * sine),
                          (scaledOffset.x() * sine) + (scaledOffset.y() * cosine));

    return scaledPivot + rotated;
}

QPointF defaultPivot()
{
    return QPointF(static_cast<qreal>(kPixmapWidth) / 2.0,
                   static_cast<qreal>(kPixmapHeight) / 2.0);
}
}

class MapEditorRasterBackgroundTransformTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsAnchorFixedWithoutRotation();
    void rotatedCornersMatchMapiahComposition();
    void explicitPivotCornersMatchMapiahComposition();

private:
    void compareCorner(const QTransform &transform,
                       const QPointF &local,
                       const QPointF &pivot,
                       qreal rotationDeg);
};

void MapEditorRasterBackgroundTransformTest::compareCorner(const QTransform &transform,
                                                           const QPointF &local,
                                                           const QPointF &pivot,
                                                           qreal rotationDeg)
{
    const QPointF actual = transform.map(local);
    const QPointF expected = mapiahExpectedPoint(local, pivot, rotationDeg);
    QVERIFY2(std::abs(actual.x() - expected.x()) < kEpsilon
                 && std::abs(actual.y() - expected.y()) < kEpsilon,
             qPrintable(QStringLiteral("corner (%1, %2): expected (%3, %4) but got (%5, %6)")
                            .arg(local.x())
                            .arg(local.y())
                            .arg(expected.x())
                            .arg(expected.y())
                            .arg(actual.x())
                            .arg(actual.y())));
}

// The case reported in ladislavb/therion-studio#30: with xScale == yScale == 4.7
// and no rotation the image keeps the right size but its anchor drifts by
// (1 - s) * w / 2, so the background no longer lines up with the survey.
void MapEditorRasterBackgroundTransformTest::keepsAnchorFixedWithoutRotation()
{
    const QTransform transform = rasterBackgroundLayerTransform(makeInput(0.0, false));

    const QPointF anchor = transform.map(QPointF(0.0, 0.0));
    QCOMPARE(anchor.x(), 0.0);
    QCOMPARE(anchor.y(), 0.0);

    const QPointF opposite = transform.map(QPointF(kPixmapWidth, kPixmapHeight));
    QCOMPARE(opposite.x(), kPixmapWidth * kXScale);
    QCOMPARE(opposite.y(), kPixmapHeight * kYScale);
}

void MapEditorRasterBackgroundTransformTest::rotatedCornersMatchMapiahComposition()
{
    const QTransform transform = rasterBackgroundLayerTransform(makeInput(kRotationDeg, false));

    compareCorner(transform, QPointF(0.0, 0.0), defaultPivot(), kRotationDeg);
    compareCorner(transform, QPointF(kPixmapWidth, 0.0), defaultPivot(), kRotationDeg);
    compareCorner(transform, QPointF(kPixmapWidth, kPixmapHeight), defaultPivot(), kRotationDeg);
    compareCorner(transform, QPointF(0.0, kPixmapHeight), defaultPivot(), kRotationDeg);
}

void MapEditorRasterBackgroundTransformTest::explicitPivotCornersMatchMapiahComposition()
{
    constexpr qreal pivotDx = 5.0;
    constexpr qreal pivotDy = -3.0;
    const QTransform transform =
        rasterBackgroundLayerTransform(makeInput(kRotationDeg, true, pivotDx, pivotDy));

    const QPointF pivot(pivotDx, pivotDy);
    compareCorner(transform, QPointF(0.0, 0.0), pivot, kRotationDeg);
    compareCorner(transform, QPointF(kPixmapWidth, kPixmapHeight), pivot, kRotationDeg);
}

int runMapEditorRasterBackgroundTransformTest(int argc, char **argv)
{
    MapEditorRasterBackgroundTransformTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorRasterBackgroundTransformTest.moc"
