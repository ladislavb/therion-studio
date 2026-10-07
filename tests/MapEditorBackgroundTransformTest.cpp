// Geometric conformity of the shared background transform, against the
// composition Mapiah uses (lib/src/elements/mp_image_insert_config.dart).
//
// Mapiah scales the image from its `xx`/`yy` anchor and rotates the scaled
// image around its pivot. For anchor a and pivot c:
//
//     P_mapiah = a + S(c - a) + R(S(P - c))
//
// so with no rotation the anchor is a fixed point. Raster/SVG items use local
// origin as the anchor, while XVI can have a non-zero item-local anchor.
//
// These cases pin the mapping of the four corners rather than the bounding
// rectangle: with a uniform scale, or with only the bounding box compared, a
// wrong order between rotation and scaling stays invisible.

#include "../src/app/text_editor/map_editor/MapEditorBackgroundTransform.h"

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
MapiahBackgroundTransformInput makeInput(qreal rotationDeg,
                                        bool pivotSet,
                                        qreal pivotDx = 0.0,
                                        qreal pivotDy = 0.0,
                                        qreal anchorX = 0.0,
                                        qreal anchorY = 0.0)
{
    MapiahBackgroundTransformInput input;
    input.viewScaleX = 1.0;
    input.viewScaleY = 1.0;
    input.layerScaleX = kXScale;
    input.layerScaleY = kYScale;
    input.rotationDeg = rotationDeg;
    input.anchorLocalX = anchorX;
    input.anchorLocalY = anchorY;
    input.pivotSet = pivotSet;
    input.pivotLocalX = pivotDx;
    input.pivotLocalY = pivotDy;
    input.intrinsicWidth = kPixmapWidth;
    input.intrinsicHeight = kPixmapHeight;
    return input;
}

QPointF mapiahExpectedPoint(const QPointF &local,
                            const QPointF &anchor,
                            const QPointF &pivot,
                            qreal rotationDeg)
{
    const QPointF scaledPivotOffset((pivot.x() - anchor.x()) * kXScale,
                                    (pivot.y() - anchor.y()) * kYScale);
    const QPointF scaledOffset((local.x() - pivot.x()) * kXScale,
                               (local.y() - pivot.y()) * kYScale);

    const qreal radians = qDegreesToRadians(rotationDeg);
    const qreal cosine = std::cos(radians);
    const qreal sine = std::sin(radians);
    const QPointF rotated((scaledOffset.x() * cosine) - (scaledOffset.y() * sine),
                          (scaledOffset.x() * sine) + (scaledOffset.y() * cosine));

    return anchor + scaledPivotOffset + rotated;
}

QPointF defaultPivot()
{
    return QPointF(static_cast<qreal>(kPixmapWidth) / 2.0,
                   static_cast<qreal>(kPixmapHeight) / 2.0);
}
}

class MapEditorBackgroundTransformTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsAnchorFixedWithoutRotation();
    void rotatedCornersMatchMapiahComposition();
    void explicitPivotCornersMatchMapiahComposition();
    void nonZeroAnchorMatchesMapiahComposition();

private:
    void compareCorner(const QTransform &transform,
                       const QPointF &local,
                       const QPointF &anchor,
                       const QPointF &pivot,
                       qreal rotationDeg);
};

void MapEditorBackgroundTransformTest::compareCorner(const QTransform &transform,
                                                     const QPointF &local,
                                                     const QPointF &anchor,
                                                     const QPointF &pivot,
                                                     qreal rotationDeg)
{
    const QPointF actual = transform.map(local);
    const QPointF expected = mapiahExpectedPoint(local, anchor, pivot, rotationDeg);
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
void MapEditorBackgroundTransformTest::keepsAnchorFixedWithoutRotation()
{
    const QTransform transform = mapiahBackgroundLayerTransform(makeInput(0.0, false));

    const QPointF anchor = transform.map(QPointF(0.0, 0.0));
    QCOMPARE(anchor.x(), 0.0);
    QCOMPARE(anchor.y(), 0.0);

    const QPointF opposite = transform.map(QPointF(kPixmapWidth, kPixmapHeight));
    QCOMPARE(opposite.x(), kPixmapWidth * kXScale);
    QCOMPARE(opposite.y(), kPixmapHeight * kYScale);
}

void MapEditorBackgroundTransformTest::rotatedCornersMatchMapiahComposition()
{
    const QTransform transform = mapiahBackgroundLayerTransform(makeInput(kRotationDeg, false));

    compareCorner(transform, QPointF(0.0, 0.0), QPointF(), defaultPivot(), kRotationDeg);
    compareCorner(transform, QPointF(kPixmapWidth, 0.0), QPointF(), defaultPivot(), kRotationDeg);
    compareCorner(transform,
                  QPointF(kPixmapWidth, kPixmapHeight),
                  QPointF(),
                  defaultPivot(),
                  kRotationDeg);
    compareCorner(transform, QPointF(0.0, kPixmapHeight), QPointF(), defaultPivot(), kRotationDeg);
}

void MapEditorBackgroundTransformTest::explicitPivotCornersMatchMapiahComposition()
{
    constexpr qreal pivotDx = 5.0;
    constexpr qreal pivotDy = -3.0;
    const QTransform transform =
        mapiahBackgroundLayerTransform(makeInput(kRotationDeg, true, pivotDx, pivotDy));

    const QPointF pivot(pivotDx, pivotDy);
    compareCorner(transform, QPointF(0.0, 0.0), QPointF(), pivot, kRotationDeg);
    compareCorner(transform, QPointF(kPixmapWidth, kPixmapHeight), QPointF(), pivot, kRotationDeg);
}

void MapEditorBackgroundTransformTest::nonZeroAnchorMatchesMapiahComposition()
{
    const QPointF anchor(18.0, 9.0);
    const QPointF pivot(31.0, 4.0);
    const QTransform transform = mapiahBackgroundLayerTransform(
        makeInput(kRotationDeg, true, pivot.x(), pivot.y(), anchor.x(), anchor.y()));

    compareCorner(transform, anchor, anchor, pivot, kRotationDeg);
    compareCorner(transform, QPointF(0.0, 0.0), anchor, pivot, kRotationDeg);
    compareCorner(transform,
                  QPointF(kPixmapWidth, kPixmapHeight),
                  anchor,
                  pivot,
                  kRotationDeg);
}

int runMapEditorBackgroundTransformTest(int argc, char **argv)
{
    MapEditorBackgroundTransformTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorBackgroundTransformTest.moc"
