#include "../../src/core/Th2GeometryProjection.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class Th2GeometryProjectionTest : public QObject
{
    Q_OBJECT

private slots:
    void projectsCoreTh2GeometryFromLogicalDocument();
    void exposesStableObjectKeysAndLineLookups();
    void preservesPhysicalRangesForCrLfAndContinuationLines();
    void reportsBackgroundMetadataLines();
};
}

void Th2GeometryProjectionTest::projectsCoreTh2GeometryFromLogicalDocument()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan -scale [0 0 10 0 0 0 10 0 m]\n"
        "  point 1 2 station:name -name A1\n"
        "  line wall:bedrock -id border1 -close on\n"
        "    0 0\n"
        "    1 1 2 2 3 3\n"
        "    smooth off\n"
        "    subtype presumed\n"
        "  endline\n"
        "  area water -place top\n"
        "    border1\n"
        "  endarea\n"
        "endscrap\n");

    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(text);

    QCOMPARE(projection.blockObjects().size(), 1);
    QCOMPARE(projection.blockObjects().constFirst().kind, Th2GeometryObjectKind::Scrap);
    QCOMPARE(projection.blockObjects().constFirst().type, QStringLiteral("s1"));
    QCOMPARE(projection.blockObjects().constFirst().sourceRange.startLineNumber, 1);
    QCOMPARE(projection.blockObjects().constFirst().sourceRange.endLineNumber, 12);

    QCOMPARE(projection.points().size(), 1);
    const Th2PointObject &point = projection.points().constFirst();
    QCOMPARE(point.command.type, QStringLiteral("station"));
    QCOMPARE(point.command.subtype, QStringLiteral("name"));
    QCOMPARE(point.command.optionsByName.value(QStringLiteral("name")),
             QStringList({QStringLiteral("A1")}));
    QVERIFY(point.hasPosition);
    QCOMPARE(point.position, QPointF(1.0, 2.0));

    QCOMPARE(projection.lines().size(), 1);
    const Th2LineObject &line = projection.lines().constFirst();
    QCOMPARE(line.command.type, QStringLiteral("wall"));
    QCOMPARE(line.command.subtype, QStringLiteral("bedrock"));
    QCOMPARE(line.command.id, QStringLiteral("border1"));
    QVERIFY(line.closed);
    QCOMPARE(line.command.sourceRange.startLineNumber, 3);
    QCOMPARE(line.command.sourceRange.endLineNumber, 8);
    QCOMPARE(line.pointRows.size(), 4);
    QCOMPARE(line.pointRows.at(0).lineNumber, 4);
    QCOMPARE(line.pointRows.at(0).coordinatePoints, QVector<QPointF>({QPointF(0.0, 0.0)}));
    QCOMPARE(line.pointRows.at(1).coordinatePoints,
             QVector<QPointF>({QPointF(1.0, 1.0), QPointF(2.0, 2.0), QPointF(3.0, 3.0)}));
    QVERIFY(line.pointRows.at(2).smoothOff);
    QCOMPARE(line.pointRows.at(3).subtype, QStringLiteral("presumed"));

    QCOMPARE(projection.areas().size(), 1);
    const Th2AreaObject &area = projection.areas().constFirst();
    QCOMPARE(area.command.type, QStringLiteral("water"));
    QCOMPARE(area.command.optionsByName.value(QStringLiteral("place")),
             QStringList({QStringLiteral("top")}));
    QCOMPARE(area.borderReferences, QStringList({QStringLiteral("border1")}));
    QCOMPARE(area.command.sourceRange.startLineNumber, 9);
    QCOMPARE(area.command.sourceRange.endLineNumber, 11);
}

void Th2GeometryProjectionTest::exposesStableObjectKeysAndLineLookups()
{
    const QString text = QStringLiteral(
        "##XTHERION## xth_me_image_insert {0 1 1.0} {1200 {}} scan.png 0 {}\n"
        "scrap s1\n"
        "  point 1 2 station -name A1\n"
        "  line wall -id border1\n"
        "    0 0\n"
        "    10 10\n"
        "  endline\n"
        "  area water\n"
        "    border1\n"
        "  endarea\n"
        "endscrap\n");

    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(text);

    QCOMPARE(projection.backgroundAtLineNumber(1)->command.stableKey,
             QStringLiteral("background:id:scan.png"));
    QCOMPARE(projection.pointAtLineNumber(3)->command.stableKey,
             QStringLiteral("point:id:A1"));
    QCOMPARE(projection.lineAtLineNumber(5)->command.stableKey,
             QStringLiteral("line:id:border1"));
    QCOMPARE(projection.areaAtLineNumber(9)->command.type, QStringLiteral("water"));

    const Th2GeometryObjectRef lineRef = projection.objectRefAtLineNumber(6);
    QVERIFY(lineRef.isValid());
    QCOMPARE(lineRef.kind, Th2GeometryObjectKind::Line);
    QCOMPARE(projection.commandForObjectRef(lineRef)->stableKey,
             QStringLiteral("line:id:border1"));

    const Th2GeometryObjectRef scrapRef = projection.objectRefAtLineNumber(11);
    QVERIFY(scrapRef.isValid());
    QCOMPARE(scrapRef.kind, Th2GeometryObjectKind::Scrap);
    QCOMPARE(projection.commandForObjectRef(scrapRef)->type, QStringLiteral("s1"));

    QVERIFY(!projection.objectRefAtLineNumber(99).isValid());
    QVERIFY(projection.commandForObjectRef({Th2GeometryObjectKind::Line, 99}) == nullptr);
}

void Th2GeometryProjectionTest::preservesPhysicalRangesForCrLfAndContinuationLines()
{
    const QString text = QStringLiteral(
        "scrap s1\r\n"
        "line wall \\\r\n"
        "  -id border1\r\n"
        "  10 20\r\n"
        "endline\r\n"
        "endscrap\r\n");
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(text);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);

    const Th2GeometryProjection projection =
        Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);

    QCOMPARE(projection.lines().size(), 1);
    const Th2LineObject &line = projection.lines().constFirst();
    QCOMPARE(line.command.id, QStringLiteral("border1"));
    QCOMPARE(line.command.sourceRange.startLineNumber, 2);
    QCOMPARE(line.command.sourceRange.endLineNumber, 5);
    QCOMPARE(line.pointRows.size(), 1);
    QCOMPARE(line.pointRows.constFirst().lineNumber, 4);
    QCOMPARE(line.pointRows.constFirst().sourceRange.startOffset,
             sourceDocument.lineAtLineNumber(4)->sourceLine.startOffset);
    QCOMPARE(sourceDocument.toText(), text);
}

void Th2GeometryProjectionTest::reportsBackgroundMetadataLines()
{
    const QString text = QStringLiteral(
        "##XTHERION## xth_me_area_adjust 0 0 100 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1.0} {1200 {}} scan.png 0 {}\n"
        "##MAPIAH## image_insert_v1 {format=raster;filename=background%20scan.png;xx=0;yy=0}\n"
        "scrap s1\n"
        "endscrap\n");

    const Th2GeometryProjection projection = Th2GeometryProjection::fromText(text);

    QCOMPARE(projection.backgrounds().size(), 2);
    QCOMPARE(projection.backgrounds().at(0).metadataFormat, QStringLiteral("xtherion"));
    QCOMPARE(projection.backgrounds().at(0).path, QStringLiteral("scan.png"));
    QCOMPARE(projection.backgrounds().at(0).command.sourceRange.startLineNumber, 2);
    QCOMPARE(projection.backgrounds().at(1).metadataFormat, QStringLiteral("mapiah"));
    QCOMPARE(projection.backgrounds().at(1).path, QStringLiteral("background scan.png"));
    QCOMPARE(projection.backgrounds().at(1).command.sourceRange.startLineNumber, 3);
}

int runTh2GeometryProjectionTest(int argc, char **argv)
{
    Th2GeometryProjectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "Th2GeometryProjectionTest.moc"
