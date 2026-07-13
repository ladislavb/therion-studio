#include "../../src/core/ThreeDViewerLoxLoader.h"

#include <QDir>
#include <QFile>
#include <QtTest/QtTest>
#include <QtEndian>

#include <cstring>

using namespace TherionStudio;

namespace
{
QString committedLoxPath()
{
    return QDir(QStringLiteral(THERION_STUDIO_TEST_FIXTURE_ROOT))
        .filePath(QStringLiteral("three_d_viewer/1302.lox"));
}

void appendUInt32(QByteArray *bytes, quint32 value)
{
    const quint32 littleEndian = qToLittleEndian(value);
    bytes->append(reinterpret_cast<const char *>(&littleEndian), sizeof(littleEndian));
}

void appendDouble(QByteArray *bytes, double value)
{
    quint64 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const quint64 littleEndian = qToLittleEndian(bits);
    bytes->append(reinterpret_cast<const char *>(&littleEndian), sizeof(littleEndian));
}

void appendDataPtr(QByteArray *bytes, quint32 position, quint32 size)
{
    appendUInt32(bytes, position);
    appendUInt32(bytes, size);
}

void appendChunk(QByteArray *bytes, quint32 type, quint32 recordCount, const QByteArray &records, const QByteArray &data)
{
    appendUInt32(bytes, type);
    appendUInt32(bytes, quint32(records.size()));
    appendUInt32(bytes, recordCount);
    appendUInt32(bytes, quint32(data.size()));
    bytes->append(records);
    bytes->append(data);
}

QByteArray surfaceFixtureBytes()
{
    QByteArray bytes;

    QByteArray surfaceData;
    appendDouble(&surfaceData, 10.0);
    appendDouble(&surfaceData, 11.0);
    appendDouble(&surfaceData, 12.0);
    appendDouble(&surfaceData, 13.0);

    QByteArray surfaceRecord;
    appendUInt32(&surfaceRecord, 42);
    appendUInt32(&surfaceRecord, 2);
    appendUInt32(&surfaceRecord, 2);
    appendDataPtr(&surfaceRecord, 0, quint32(surfaceData.size()));
    appendDouble(&surfaceRecord, 100.0);
    appendDouble(&surfaceRecord, 200.0);
    appendDouble(&surfaceRecord, 5.0);
    appendDouble(&surfaceRecord, 0.0);
    appendDouble(&surfaceRecord, 0.0);
    appendDouble(&surfaceRecord, 7.0);
    appendChunk(&bytes, 5, 1, surfaceRecord, surfaceData);

    QByteArray bitmapData("png-bytes");
    QByteArray bitmapRecord;
    appendUInt32(&bitmapRecord, 42);
    appendUInt32(&bitmapRecord, 1);
    appendDataPtr(&bitmapRecord, 0, quint32(bitmapData.size()));
    appendDouble(&bitmapRecord, 100.0);
    appendDouble(&bitmapRecord, 200.0);
    appendDouble(&bitmapRecord, 5.0);
    appendDouble(&bitmapRecord, 0.0);
    appendDouble(&bitmapRecord, 0.0);
    appendDouble(&bitmapRecord, 7.0);
    appendChunk(&bytes, 6, 1, bitmapRecord, bitmapData);

    return bytes;
}

class ThreeDViewerLoxLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsCommittedLoxScene();
    void loadsTerrainSurfaceChunks();
    void rejectsEmptyInput();
    void rejectsTruncatedInput();
};

void ThreeDViewerLoxLoaderTest::loadsCommittedLoxScene()
{
    const QString path = committedLoxPath();
    QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("Committed .lox fixture is missing: %1").arg(path)));

    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadFile(path);

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.scene.surveys.size(), 3);
    QCOMPARE(result.scene.stations.size(), 10);
    QCOMPARE(result.scene.shots.size(), 9);
    QCOMPARE(result.scene.meshGroups.size(), 1);

    const ThreeDViewerSurvey &survey = result.scene.surveys.at(2);
    QCOMPARE(survey.id, quint32(2));
    QCOMPARE(survey.parentId, quint32(1));
    QCOMPARE(survey.name, QStringLiteral("hp"));
    QCOMPARE(survey.title, QStringLiteral("Hlavní polygon"));

    const ThreeDViewerStation &station = result.scene.stations.first();
    QCOMPARE(station.id, quint32(0));
    QCOMPARE(result.scene.stationQualifiedName(station), QStringLiteral("0@hp.1302"));

    const ThreeDViewerShot &shot = result.scene.shots.first();
    QCOMPARE(shot.fromStationId, quint32(0));
    QCOMPARE(shot.toStationId, quint32(1));
    QCOMPARE(shot.surveyId, quint32(2));

    const ThreeDViewerBounds bounds = result.scene.bounds();
    QVERIFY(bounds.valid);
    QCOMPARE(bounds.minimum.x, -5.511378785446);
    QCOMPARE(bounds.minimum.y, -15.48373914206);
    QCOMPARE(bounds.minimum.z, -17.59977728999);
    QCOMPARE(bounds.maximum.x, 0.827920906564);
    QCOMPARE(bounds.maximum.y, 0.0);
    QCOMPARE(bounds.maximum.z, 0.9730335954936);
}

void ThreeDViewerLoxLoaderTest::loadsTerrainSurfaceChunks()
{
    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadBytes(surfaceFixtureBytes());

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.scene.surfaces.size(), 1);

    const ThreeDViewerSurface &surface = result.scene.surfaces.first();
    QCOMPARE(surface.id, quint32(42));
    QCOMPARE(surface.width, quint32(2));
    QCOMPARE(surface.height, quint32(2));
    QCOMPARE(surface.elevations.size(), 4);
    QCOMPARE(surface.elevations.at(0), 10.0);
    QCOMPARE(surface.elevations.at(3), 13.0);

    const ThreeDViewerBounds bounds = result.scene.bounds();
    QVERIFY(bounds.valid);
    QCOMPARE(bounds.minimum.x, 100.0);
    QCOMPARE(bounds.minimum.y, 200.0);
    QCOMPARE(bounds.minimum.z, 10.0);
    QCOMPARE(bounds.maximum.x, 105.0);
    QCOMPARE(bounds.maximum.y, 207.0);
    QCOMPARE(bounds.maximum.z, 13.0);
}

void ThreeDViewerLoxLoaderTest::rejectsEmptyInput()
{
    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadBytes({});

    QVERIFY(!result.ok());
    QVERIFY(result.error.contains(QStringLiteral("empty")));
}

void ThreeDViewerLoxLoaderTest::rejectsTruncatedInput()
{
    const ThreeDViewerLoxLoader loader;
    const QByteArray truncatedHeader(8, '\0');
    const ThreeDViewerLoxLoader::Result result = loader.loadBytes(truncatedHeader);

    QVERIFY(!result.ok());
    QVERIFY(result.error.contains(QStringLiteral("truncated")));
}
}

int runThreeDViewerLoxLoaderTest(int argc, char **argv)
{
    ThreeDViewerLoxLoaderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerLoxLoaderTest.moc"
