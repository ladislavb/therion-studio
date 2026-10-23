#include "../../src/core/Th2CentrelineAlignment.h"
#include "../../src/core/TherionSourceLogicalDocument.h"
#include "../../src/core/TherionXviParser.h"

#include <QtMath>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool nearlyEqual(qreal left, qreal right, qreal tolerance = 1e-6)
{
    return qAbs(left - right) <= tolerance;
}

bool nearlyEqual(const QPointF &left, const QPointF &right, qreal tolerance = 1e-6)
{
    return nearlyEqual(left.x(), right.x(), tolerance) && nearlyEqual(left.y(), right.y(), tolerance);
}

TherionXviDocument xviDocument(const QString &text)
{
    TherionXviDocument document;
    if (!parseTherionXviDocumentText(text, &document)) {
        qWarning("XVI fixture did not parse");
    }
    return document;
}

const QString kXviText = QStringLiteral(
    "set XVIgrids {1 m}\n"
    "set XVIstations {\n"
    "  {0.0 0.0 1}\n"
    "  {78.74 0.0 2}\n"
    "  {78.74 157.48 3}\n"
    "}\n"
    "set XVIgrid {-100.0 -100.0 78.74 0.0 0.0 78.74 10 10}\n");
}

class Th2CentrelineAlignmentTest final : public QObject
{
    Q_OBJECT

private slots:
    void composesAndInvertsSimilarities()
    {
        const Th2SimilarityTransform rotateScale{0.0, 2.0, 1.0, 0.0}; // 90 degrees, scale 2, shift x
        QVERIFY(nearlyEqual(rotateScale.map(QPointF(1, 0)), QPointF(1, 2)));
        QVERIFY(nearlyEqual(rotateScale.scale(), 2.0));
        QVERIFY(nearlyEqual(rotateScale.inverted().map(QPointF(1, 2)), QPointF(1, 0)));
        const Th2SimilarityTransform shifted = rotateScale.then(Th2SimilarityTransform::fromTranslation(QPointF(0, 5)));
        QVERIFY(nearlyEqual(shifted.map(QPointF(1, 0)), QPointF(1, 7)));
        QVERIFY(nearlyEqual(rotateScale.then(rotateScale.inverted()).map(QPointF(3, 4)), QPointF(3, 4)));
    }

    void convertsTherionLengthUnits()
    {
        QCOMPARE(therionLengthUnitMetres(QStringLiteral("m")).value_or(0.0), 1.0);
        QCOMPARE(therionLengthUnitMetres(QStringLiteral("FT")).value_or(0.0), 0.3048);
        QVERIFY(!therionLengthUnitMetres(QStringLiteral("parsec")).has_value());
    }

    void readsXviGridScale()
    {
        const std::optional<qreal> metresPerUnit = xviMetresPerDrawingUnit(xviDocument(kXviText));
        QVERIFY(metresPerUnit.has_value());
        QVERIFY(nearlyEqual(*metresPerUnit, 1.0 / 78.74));
    }

    void recoversSimilarityFromStationPairs()
    {
        // drawing = 2 units per metre, rotated 30 degrees, shifted.
        const qreal angle = qDegreesToRadians(30.0);
        const Th2SimilarityTransform truth{0.5 * qCos(angle), 0.5 * qSin(angle), 5.0, -3.0};
        QVector<Th2StationFitPair> pairs;
        for (const QPointF &drawing : {QPointF(0, 0), QPointF(10, 0), QPointF(4, 12)}) {
            pairs.append(Th2StationFitPair{QString(), drawing, truth.map(drawing)});
        }
        qreal residual = -1.0;
        const std::optional<Th2SimilarityTransform> fit = similarityFit(pairs, &residual);
        QVERIFY(fit.has_value());
        QVERIFY(nearlyEqual(residual, 0.0));
        QVERIFY(nearlyEqual(fit->map(QPointF(7, -2)), truth.map(QPointF(7, -2))));
        QVERIFY(!similarityFit(pairs.mid(0, 1)).has_value());
    }

    void fitsScrapStationsToXviCentreline()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap other\n"
            "point 9 9 station -name 1\n"
            "endscrap\n"
            "scrap s1 -projection plan\n"
            "point 10 20 station -name 1\n"
            "point 88.74 20 station -name 2\n"
            "point 40 40 label -text 2\n"
            "point 88.74 177.48 station -name 3\n"
            "point 0 0 station -name unknown\n"
            "endscrap\n"));
        const QVector<Th2ScrapStationPoint> points = th2ScrapStationPoints(th2.commands(), 4);
        QCOMPARE(points.size(), 4);
        QCOMPARE(points.first().position, QPointF(10, 20));
        QCOMPARE(points.first().lineNumber, 5);

        const TherionXviDocument xvi = xviDocument(kXviText);
        const QVector<Th2StationFitPair> pairs = xviStationFitPairs(points, xvi);
        QCOMPARE(pairs.size(), 3);
        const Th2CentrelineFit fit = fitTh2ScrapToCentreline(pairs, xviGridTransform(xvi));
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(fit.stationCount, 3);
        QVERIFY(nearlyEqual(fit.rmsResidualMetres, 0.0));
        QVERIFY(nearlyEqual(fit.drawingToMetres.map(QPointF(88.74, 177.48)), QPointF(1.0, 2.0)));
    }

    void fitsScrapContainingLineWithBestXvi()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap first\n"
            "point 0 0 station -name 9\n"
            "endscrap\n"
            "scrap s1 -scale [1 m]\n"
            "point 10 20 station -name 1\n"
            "point 88.74 20 station -name 2\n"
            "endscrap\n"));
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 6), 4);
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 2), 1);
        QCOMPARE(th2ScrapStartLineContaining(th2.commands(), 8), 0);
        QCOMPARE(th2StationPointAtLine(th2.commands(), 6)->position, QPointF(88.74, 20));
        QVERIFY(!th2StationPointAtLine(th2.commands(), 4).has_value());

        const TherionXviDocument unrelated = xviDocument(QStringLiteral(
            "set XVIgrids {1 m}\nset XVIstations {\n  {0 0 77}\n}\nset XVIgrid {0 0 10 0 0 10 1 1}\n"));
        const Th2CentrelineFit fit = fitTh2ScrapContainingLine(th2.commands(), 5, {unrelated, xviDocument(kXviText)});
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviStations);
        QCOMPARE(fit.stationCount, 2);
        QVERIFY(nearlyEqual(fit.drawingToMetres.scale(), 1.0 / 78.74));

        // The scrap -scale never places a drawing (D5).
        QVERIFY(!fitTh2ScrapContainingLine(th2.commands(), 5, {}).isValid());
        QVERIFY(!fitTh2ScrapContainingLine(th2.commands(), 8, {}).isValid());
    }

    void neverUsesScrapScale()
    {
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap one -scale [2 cm]\n"
            "point 10 20 station -name 1\n"
            "endscrap\n"));
        QVERIFY(!fitTh2ScrapContainingLine(th2.commands(), 2, {}).isValid());
    }

    void usesXviGridWithFewerThanTwoStations()
    {
        // A grid turned 90 degrees: its x axis points north in the drawing, 2 units per metre.
        const TherionXviDocument turned = xviDocument(QStringLiteral(
            "set XVIgrids {1 m}\nset XVIstations {\n  {0 0 77}\n}\nset XVIgrid {0 0 0 2 -2 0 1 1}\n"));
        const std::optional<Th2SimilarityTransform> grid = xviGridTransform(turned);
        QVERIFY(grid.has_value());
        QVERIFY(nearlyEqual(grid->map(QPointF(0, 2)), QPointF(1, 0)));

        const QVector<Th2StationFitPair> pairs = {{QStringLiteral("1"), QPointF(10, 20), QPointF(3, 4)}};
        const Th2CentrelineFit fit = fitTh2ScrapToCentreline(pairs, grid);
        QCOMPARE(fit.source, Th2CentrelineFitSource::XviGrid);
        QCOMPARE(fit.stationCount, 1);
        QVERIFY(nearlyEqual(fit.drawingToMetres.scale(), 0.5));
        QVERIFY(!fitTh2ScrapToCentreline({}, std::nullopt).isValid());

        // A scrap whose stations are not in the XVI is still placed by its grid.
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap s\n"
            "point 0 0 station -name 5\n"
            "point 9 9 station -name 6\n"
            "endscrap\n"));
        QCOMPARE(fitTh2ScrapContainingLine(th2.commands(), 2, {turned}).source, Th2CentrelineFitSource::XviGrid);
    }

    void linksPartnerDrawingAtJoinStation()
    {
        // This drawing: 78.74 units per metre. Partner: 19.685 units per metre, its own origin.
        Th2CentrelineFit thisFit;
        thisFit.source = Th2CentrelineFitSource::XviStations;
        thisFit.drawingToMetres = Th2SimilarityTransform::fromScale(1.0 / 78.74);
        Th2CentrelineFit partnerFit;
        partnerFit.source = Th2CentrelineFitSource::XviStations;
        partnerFit.drawingToMetres = Th2SimilarityTransform::fromScale(1.0 / 19.685)
                                         .then(Th2SimilarityTransform::fromTranslation(QPointF(-50, 7)));

        const QPointF thisJoin(-1832.02, 848.59);
        const QPointF partnerJoin(43.85, 12.99);
        const Th2SimilarityTransform partnerToThis = joinPartnerToThisTransform(thisFit, thisJoin, partnerFit, partnerJoin);
        QVERIFY(nearlyEqual(partnerToThis.map(partnerJoin), thisJoin, 1e-6));
        // One metre in the partner drawing is one metre in this drawing.
        const QPointF partnerMetreEast = partnerJoin + QPointF(19.685, 0.0);
        QVERIFY(nearlyEqual(partnerToThis.map(partnerMetreEast), thisJoin + QPointF(78.74, 0.0), 1e-6));

        // Two stations 2 m apart (a nosurvey leg): the partner's lands 2 m north of this drawing's.
        const Th2SimilarityTransform apart =
            joinPartnerToThisTransform(thisFit, thisJoin, partnerFit, partnerJoin, QPointF(0.0, 2.0));
        QVERIFY(nearlyEqual(apart.map(partnerJoin), thisJoin + QPointF(0.0, 157.48), 1e-6));
    }

    void projectsStationPositionsIntoScrapViews()
    {
        const Th2StationPosition position{3.0, 4.0, 5.0};
        QCOMPARE(*projectedStationMetres(position, QStringLiteral("plan")), QPointF(3.0, 4.0));
        QVERIFY(nearlyEqual(*projectedStationMetres(position, QStringLiteral("elevation 0")), QPointF(3.0, 5.0)));
        // Looking east (90): north is to the left.
        QVERIFY(nearlyEqual(*projectedStationMetres(position, QStringLiteral("elevation 90")), QPointF(-4.0, 5.0)));
        // Looking north-west (315): north-east is to the right.
        QVERIFY(nearlyEqual(*projectedStationMetres(position, QStringLiteral("elevation 315")),
                            QPointF((3.0 + 4.0) * qSqrt(0.5), 5.0)));
        QVERIFY(!projectedStationMetres(position, QStringLiteral("extended")).has_value());
        QVERIFY(!projectedStationMetres(position, QStringLiteral("elevation")).has_value());
        QVERIFY(!projectedStationMetres(position, QStringLiteral("elevation 100 grad")).has_value());
    }

    void fitsScrapToMeasuredLegsAsDrawn()
    {
        // Drawn at 10 units per metre and turned 90 degrees anticlockwise: east is drawn up the page.
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap a\n"
            "point 0 0 station -name 1\n"
            "point 0 100 station -name 2\n"
            "point -50 100 station -name 3\n"
            "point -53 100 station -name 4\n"
            "point 0 0 station -name 9\n"
            "endscrap\n"));
        const QHash<int, QString> keys = {{2, QStringLiteral("1@a")},
                                          {3, QStringLiteral("2@a")},
                                          {4, QStringLiteral("3@a")},
                                          {5, QStringLiteral("4@a")},
                                          {6, QStringLiteral("9@a")}};
        const QVector<Th2MeasuredLeg> legs = {
            {QStringLiteral("1@a"), QStringLiteral("2@a"), {10.0, 0.0, 1.0}},
            {QStringLiteral("2@a"), QStringLiteral("3@a"), {0.0, 5.0, 0.0}},
            // 0.3 m: too short to trust, and drawn badly.
            {QStringLiteral("3@a"), QStringLiteral("4@a"), {0.3, 0.3, 0.0}},
            // One end not drawn in this scrap.
            {QStringLiteral("2@a"), QStringLiteral("7@a"), {40.0, 0.0, 0.0}},
        };
        const Th2CentrelineFit fit = fitTh2ScrapToMeasuredLegs(th2.commands(), 3, keys, legs, QStringLiteral("plan"));
        QCOMPARE(fit.source, Th2CentrelineFitSource::MeasuredLegs);
        QCOMPARE(fit.stationCount, 2);
        QVERIFY(nearlyEqual(fit.rmsResidualMetres, 0.0));
        QVERIFY(nearlyEqual(fit.drawingToMetres.map(QPointF(0, 100)) - fit.drawingToMetres.map(QPointF()),
                            QPointF(10.0, 0.0)));

        // Too few usable legs, or a projection with no fixed frame, gives no fit.
        QVERIFY(!fitTh2ScrapToMeasuredLegs(th2.commands(), 3, keys, legs.mid(2), QStringLiteral("plan")).isValid());
        QVERIFY(!fitTh2ScrapToMeasuredLegs(th2.commands(), 3, keys, legs, QStringLiteral("none")).isValid());
    }

    void leavesOutDuplicatedAndMisdrawnStations()
    {
        // 10 units per metre along x. Station 5 is drawn 3 m off its leg; station 6 is drawn twice.
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap a\n"
            "point 0 0 station -name 1\n"
            "point 50 0 station -name 2\n"
            "point 100 0 station -name 3\n"
            "point 150 0 station -name 4\n"
            "point 200 30 station -name 5\n"
            "point 0 200 station -name 6\n"
            "point 0 -900 station -name 6\n"
            "endscrap\n"));
        QHash<int, QString> keys;
        for (int line = 2; line <= 8; ++line) {
            keys.insert(line, QStringLiteral("%1@a").arg(line == 8 ? 6 : line - 1));
        }
        const QVector<Th2MeasuredLeg> legs = {
            {QStringLiteral("1@a"), QStringLiteral("2@a"), {5.0, 0.0, 0.0}},
            {QStringLiteral("2@a"), QStringLiteral("3@a"), {5.0, 0.0, 0.0}},
            {QStringLiteral("3@a"), QStringLiteral("4@a"), {5.0, 0.0, 0.0}},
            {QStringLiteral("4@a"), QStringLiteral("5@a"), {5.0, 0.0, 0.0}},
            {QStringLiteral("1@a"), QStringLiteral("6@a"), {0.0, 50.0, 0.0}},
        };
        const Th2CentrelineFit fit = fitTh2ScrapToMeasuredLegs(th2.commands(), 2, keys, legs, QStringLiteral("plan"));
        QCOMPARE(fit.stationCount, 3);
        QVERIFY(nearlyEqual(fit.drawingToMetres.scale(), 0.1));
        QVERIFY(nearlyEqual(fit.rmsResidualMetres, 0.0));
    }

    void dropsLegsSeenEndOnInElevation()
    {
        // Elevation looking north (0): a leg running north is seen end on.
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap e -projection [elevation 0]\n"
            "point 0 0 station -name 1\n"
            "point 40 0 station -name 2\n"
            "point 0 30 station -name 3\n"
            "endscrap\n"));
        const QHash<int, QString> keys = {{2, QStringLiteral("1@e")}, {3, QStringLiteral("2@e")}, {4, QStringLiteral("3@e")}};
        const QVector<Th2MeasuredLeg> legs = {
            {QStringLiteral("1@e"), QStringLiteral("2@e"), {4.0, 0.0, 0.0}},
            {QStringLiteral("1@e"), QStringLiteral("3@e"), {0.0, 20.0, 0.2}},
        };
        const Th2CentrelineFit fit =
            fitTh2ScrapToMeasuredLegs(th2.commands(), 2, keys, legs, QStringLiteral("elevation 0"));
        QCOMPARE(fit.stationCount, 1);
        QVERIFY(nearlyEqual(fit.drawingToMetres.scale(), 0.1));
    }

    void fitsExtendedScaleOnlyAsDrawn()
    {
        // Drawn going left at 10 units per metre, with a pitch down.
        const TherionSourceLogicalDocument th2 = TherionSourceLogicalDocument::fromText(QStringLiteral(
            "scrap x -projection extended\n"
            "point 0 0 station -name 1\n"
            "point -50 0 station -name 2\n"
            "point -50 -80 station -name 3\n"
            "endscrap\n"));
        const QHash<int, QString> keys = {{2, QStringLiteral("1@x")}, {3, QStringLiteral("2@x")}, {4, QStringLiteral("3@x")}};
        const QVector<Th2MeasuredLeg> legs = {
            {QStringLiteral("1@x"), QStringLiteral("2@x"), {3.0, 4.0, 0.0}},
            {QStringLiteral("2@x"), QStringLiteral("3@x"), {0.0, 0.0, -8.0}},
        };
        const Th2CentrelineFit fit = fitTh2ScrapToMeasuredLegs(th2.commands(), 2, keys, legs, QStringLiteral("extended"));
        QCOMPARE(fit.stationCount, 2);
        QVERIFY(nearlyEqual(fit.drawingToMetres.b, 0.0));
        QVERIFY(nearlyEqual(fit.drawingToMetres.a, 0.1));
        QVERIFY(nearlyEqual(fit.rmsResidualMetres, 0.0));
    }

    void comparesFitScales()
    {
        Th2CentrelineFit legs;
        legs.drawingToMetres = Th2SimilarityTransform::fromScale(0.1);
        Th2CentrelineFit xvi;
        xvi.drawingToMetres = Th2SimilarityTransform{0.0, 0.105, 0.0, 0.0};
        QVERIFY(!th2FitScalesDisagree(legs, xvi));
        xvi.drawingToMetres = Th2SimilarityTransform::fromScale(0.2);
        QVERIFY(th2FitScalesDisagree(legs, xvi));
    }

    void anchorsDrawingsAtTheClosestComputedStations()
    {
        const QVector<Th2ScrapStationPoint> thisPoints = {{QStringLiteral("1"), QPointF(0, 0), 2},
                                                          {QStringLiteral("2"), QPointF(100, 0), 3}};
        const QVector<Th2ScrapStationPoint> partnerPoints = {{QStringLiteral("5"), QPointF(7, 7), 4},
                                                             {QStringLiteral("6"), QPointF(9, 9), 5}};
        const QHash<int, QString> thisKeys = {{2, QStringLiteral("1@x")}, {3, QStringLiteral("2@x")}};
        const QHash<int, QString> partnerKeys = {{4, QStringLiteral("5@y")}, {5, QStringLiteral("6@y")}};
        const QHash<QString, Th2StationPosition> positions = {{QStringLiteral("1@x"), {0.0, 0.0, 0.0}},
                                                             {QStringLiteral("2@x"), {10.0, 0.0, 0.0}},
                                                             {QStringLiteral("5@y"), {11.0, 2.0, 0.0}},
                                                             {QStringLiteral("6@y"), {30.0, 0.0, 0.0}}};
        const std::optional<Th2JoinAnchor> anchor = closestComputedStationPair(
            thisPoints, thisKeys, partnerPoints, partnerKeys, positions, QStringLiteral("plan"));
        QVERIFY(anchor.has_value());
        QCOMPARE(anchor->thisPoint, QPointF(100, 0));
        QCOMPARE(anchor->partnerPoint, QPointF(7, 7));
        QVERIFY(nearlyEqual(anchor->partnerOffsetMetres, QPointF(1.0, 2.0)));

        // Extended has no computed frame, so drawings that share no station cannot be anchored.
        QVERIFY(!closestComputedStationPair(thisPoints, thisKeys, partnerPoints, partnerKeys, positions,
                                            QStringLiteral("extended"))
                     .has_value());
        QVERIFY(!computedStationOffsetMetres(QStringLiteral("1@x"), QStringLiteral("missing@y"), positions,
                                             QStringLiteral("plan"))
                     .has_value());
    }
};

int runTh2CentrelineAlignmentTest(int argc, char **argv)
{
    Th2CentrelineAlignmentTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "Th2CentrelineAlignmentTest.moc"
