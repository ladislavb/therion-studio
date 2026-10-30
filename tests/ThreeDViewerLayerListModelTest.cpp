#include "../src/app/three_d_viewer/ThreeDViewerLayerListModel.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

using namespace TherionStudio;

class ThreeDViewerLayerListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesLayerCountsAndVisibility();
    void omitsStationSubgroupsWhenOnlyOneStationClassExists();
};

void ThreeDViewerLayerListModelTest::exposesLayerCountsAndVisibility()
{
    ThreeDViewerSceneModel sceneModel;

    sceneModel.shots.append(ThreeDViewerShot{});
    sceneModel.shots.append(ThreeDViewerShot{0, 0, 0, {}, {}, 0.0, true});
    sceneModel.stations.append(ThreeDViewerStation{});
    ThreeDViewerStation entranceStation;
    entranceStation.entrance = true;
    sceneModel.stations.append(entranceStation);
    ThreeDViewerStation fixedStation;
    fixedStation.fixed = true;
    sceneModel.stations.append(fixedStation);
    sceneModel.meshGroups.append(ThreeDViewerMeshGroup{});
    sceneModel.surfaces.append(ThreeDViewerSurface{});
    ThreeDViewerScrapPolyline wallLine;
    wallLine.type = QStringLiteral("wall");
    wallLine.points = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    sceneModel.scrapLinework.append(wallLine);

    ThreeDViewerLayerListModel model;
    model.setSceneModel(sceneModel);

    QCOMPARE(model.rowCount(), 9);

    const QModelIndex centerlineIndex = model.index(0, 0);
    QCOMPARE(model.data(centerlineIndex, Qt::DisplayRole).toString(), QStringLiteral("Centerline"));
    QCOMPARE(model.data(centerlineIndex, ThreeDViewerLayerListModel::CountRole).toInt(), 2);
    QCOMPARE(model.data(centerlineIndex, Qt::CheckStateRole).toInt(), int(Qt::PartiallyChecked));
    QCOMPARE(model.layerVisible(0), true);
    QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("Surface"));
    QCOMPARE(model.data(model.index(2, 0), Qt::DisplayRole).toString(), QStringLiteral("Underground"));
    QCOMPARE(model.data(model.index(1, 0), Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QCOMPARE(model.data(model.index(2, 0), Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QCOMPARE(model.data(model.index(1, 0), ThreeDViewerLayerListModel::IndentRole).toInt(), 1);
    QCOMPARE(model.data(model.index(3, 0), Qt::DisplayRole).toString(), QStringLiteral("Stations"));
    QCOMPARE(model.data(model.index(4, 0), Qt::DisplayRole).toString(), QStringLiteral("Entrances"));
    QCOMPARE(model.data(model.index(5, 0), Qt::DisplayRole).toString(), QStringLiteral("Fixed Stations"));
    QCOMPARE(model.data(model.index(6, 0), Qt::DisplayRole).toString(), QStringLiteral("Other Stations"));
    QCOMPARE(model.data(model.index(7, 0), Qt::DisplayRole).toString(), QStringLiteral("Labels"));
    QCOMPARE(model.data(model.index(8, 0), Qt::DisplayRole).toString(), QStringLiteral("Walls"));
    QCOMPARE(model.data(model.index(8, 0), ThreeDViewerLayerListModel::CountRole).toInt(), 1);
    QCOMPARE(model.data(model.index(8, 0), Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QCOMPARE(model.data(model.index(3, 0), Qt::CheckStateRole).toInt(), int(Qt::PartiallyChecked));
    QCOMPARE(model.data(model.index(7, 0), Qt::CheckStateRole).toInt(), int(Qt::Checked));
    QVERIFY(model.layerVisible(2));

    QSignalSpy spy(&model, &ThreeDViewerLayerListModel::layerVisibilityChanged);
    QVERIFY(model.setData(centerlineIndex, Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(model.data(centerlineIndex, Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QCOMPARE(model.layerVisible(0), false);
    model.setLayerVisible(0, true);
    QCOMPARE(model.layerVisible(0), true);
    QCOMPARE(spy.count(), 2);
}

void ThreeDViewerLayerListModelTest::omitsStationSubgroupsWhenOnlyOneStationClassExists()
{
    ThreeDViewerSceneModel sceneModel;
    sceneModel.shots.append(ThreeDViewerShot{});
    sceneModel.stations.append(ThreeDViewerStation{});

    ThreeDViewerLayerListModel model;
    model.setSceneModel(sceneModel);

    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QStringLiteral("Centerline"));
    QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QStringLiteral("Underground"));
    QCOMPARE(model.data(model.index(2, 0), Qt::DisplayRole).toString(), QStringLiteral("Stations"));
    QCOMPARE(model.data(model.index(2, 0), ThreeDViewerLayerListModel::HasChildrenRole).toBool(), false);
    QCOMPARE(model.data(model.index(3, 0), Qt::DisplayRole).toString(), QStringLiteral("Labels"));
    QCOMPARE(model.data(model.index(4, 0), Qt::DisplayRole).toString(), QStringLiteral("Walls"));
    QVERIFY(model.layerVisible(2));
    QVERIFY(model.layerVisible(3));
}

int runThreeDViewerLayerListModelTest(int argc, char **argv)
{
    ThreeDViewerLayerListModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerLayerListModelTest.moc"
