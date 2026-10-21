#include "../src/app/text_editor/map_editor/MapEditorLineVertexHandles.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
// A line whose middle point has an incoming handle at (20 0) and the given outgoing handle, then the given rows.
QString lineText(const QString &outgoingHandle, const QString &middleRows)
{
    return QStringLiteral("scrap s1\n"
                          "line wall\n"
                          "  0 0\n"
                          "  10 10 20 0 30 0\n"
                          "%1"
                          "  %2 50 10 60 0\n"
                          "endline\n"
                          "endscrap\n")
        .arg(middleRows, outgoingHandle);
}

QStringList smoothRows(const QStringList &rows)
{
    QStringList result;
    for (const QString &row : rows) {
        if (row.trimmed().startsWith(QStringLiteral("smooth"))) {
            result.append(row.trimmed());
        }
    }
    return result;
}
}

// Line point smoothness as Therion reads it (thline.cxx; GitHub issue #36).
class MapEditorLinePointSmoothTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsSmoothnessLikeTherion_data()
    {
        QTest::addColumn<QString>("outgoingHandle");
        QTest::addColumn<QString>("middleRows");
        QTest::addColumn<bool>("smooth");

        QTest::newRow("handles in line, no smooth row") << QStringLiteral("40 0") << QString() << true;
        QTest::newRow("corner, no smooth row") << QStringLiteral("40 10") << QString() << false;
        QTest::newRow("corner, smooth on") << QStringLiteral("40 10") << QStringLiteral("  smooth on\n") << true;
        QTest::newRow("handles in line, smooth off") << QStringLiteral("40 0") << QStringLiteral("  smooth off\n") << false;
        QTest::newRow("corner, smooth auto") << QStringLiteral("40 10") << QStringLiteral("  smooth auto\n") << false;
    }

    void readsSmoothnessLikeTherion()
    {
        QFETCH(QString, outgoingHandle);
        QFETCH(QString, middleRows);
        QFETCH(bool, smooth);

        const std::optional<MapGeometryFeature> line = lineFeatureForLineNumber(lineText(outgoingHandle, middleRows), 2);
        QVERIFY(line.has_value());
        QCOMPARE(line->lineVertices.size(), 3);
        QCOMPARE(lineVertexIsSmooth(line->lineVertices.at(1)), smooth);
    }

    // Saving writes back the smooth rows the file had and adds none, so corners need no "smooth off".
    void writesBackOnlyTheSmoothRowsTheFileHad_data()
    {
        QTest::addColumn<QString>("middleRows");

        QTest::newRow("none") << QString();
        QTest::newRow("on") << QStringLiteral("  smooth on\n");
        QTest::newRow("off") << QStringLiteral("  smooth off\n");
        QTest::newRow("auto") << QStringLiteral("  smooth auto\n");
    }

    void writesBackOnlyTheSmoothRowsTheFileHad()
    {
        QFETCH(QString, middleRows);

        const std::optional<MapGeometryFeature> line = lineFeatureForLineNumber(lineText(QStringLiteral("40 10"), middleRows), 2);
        QVERIFY(line.has_value());
        QCOMPARE(smoothRows(coordinateRowsForLineVertices(line->lineVertices, line->closed)),
                 smoothRows(middleRows.split(QLatin1Char('\n'))));
    }

    // Unticking Smooth makes a corner but keeps both handles; ticking again lines them up keeping their lengths
    // (GitHub #48).
    void unticksAndReticksSmoothKeepingHandles()
    {
        // Smooth middle point: handles at (20 0) and (40 0), lengths 10 and 10.
        std::optional<MapGeometryFeature> line = lineFeatureForLineNumber(lineText(QStringLiteral("40 0"), QString()), 2);
        QVERIFY(line.has_value());
        QVector<MapGeometryFeature::TH2LineVertex> vertices = line->lineVertices;
        QVERIFY(lineVertexIsSmooth(vertices.at(1)));

        setLineVertexSmooth(&vertices, 1, line->closed, false);
        QCOMPARE(vertices.at(1).incomingControl, std::optional<QPointF>(QPointF(20.0, 0.0)));
        QCOMPARE(vertices.at(1).outgoingControl, std::optional<QPointF>(QPointF(40.0, 0.0)));
        QVERIFY(!lineVertexIsSmooth(vertices.at(1)));
        QCOMPARE(smoothRows(coordinateRowsForLineVertices(vertices, line->closed)),
                 QStringList({QStringLiteral("smooth off")}));

        // Now a corner: move the outgoing handle off line and further out, then tick Smooth again.
        vertices[1].outgoingControl = QPointF(30.0, 20.0);
        setLineVertexSmooth(&vertices, 1, line->closed, true);
        const MapGeometryFeature::TH2LineVertex &vertex = vertices.at(1);
        QVERIFY(lineVertexIsSmooth(vertex));
        QVERIFY(!vertex.smoothRow.has_value());
        const QPointF incoming = vertex.incomingControl.value() - vertex.anchor;
        const QPointF outgoing = vertex.outgoingControl.value() - vertex.anchor;
        QVERIFY(qFuzzyCompare(std::hypot(incoming.x(), incoming.y()), 10.0));
        QVERIFY(qFuzzyCompare(std::hypot(outgoing.x(), outgoing.y()), 20.0));
        QVERIFY(smoothRows(coordinateRowsForLineVertices(vertices, line->closed)).isEmpty());
    }
};

int runMapEditorLinePointSmoothTest()
{
    MapEditorLinePointSmoothTest test;
    return QTest::qExec(&test);
}

#include "MapEditorLinePointSmoothTest.moc"
