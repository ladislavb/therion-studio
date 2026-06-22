#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionBackgroundMetadata.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMouseEvent>
#include <QTabWidget>
#include <QThread>
#include <QTemporaryDir>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtMath>

#include <iostream>
#include <optional>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(1);
}

void pumpEventsFor(int milliseconds)
{
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + milliseconds;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
}

bool writeTextFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    return file.write(content) == content.size();
}

QString repositoryFilePath(const QString &relativePath)
{
    const QString fromCurrentDirectory = QDir::current().absoluteFilePath(relativePath);
    if (QFileInfo::exists(fromCurrentDirectory)) {
        return QFileInfo(fromCurrentDirectory).absoluteFilePath();
    }

    const QString fromBuildDirectory = QDir(QCoreApplication::applicationDirPath())
                                           .absoluteFilePath(QStringLiteral("../") + relativePath);
    return QFileInfo(fromBuildDirectory).absoluteFilePath();
}

bool waitForSingleRasterLayerReady(MapEditorTab *mapTab, const QSize &expectedSize)
{
    if (mapTab == nullptr) {
        return false;
    }

    QRectF lastBounds;
    int stableBoundsCount = 0;
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (mapTab->backgroundLayerCount() == 1
            && mapTab->backgroundLayerSourcePixelSize(0) == expectedSize) {
            const QRectF bounds = mapTab->backgroundLayerSceneBounds(0);
            if (bounds.isValid()) {
                if (nearlyEqual(bounds.left(), lastBounds.left())
                    && nearlyEqual(bounds.top(), lastBounds.top())
                    && nearlyEqual(bounds.width(), lastBounds.width())
                    && nearlyEqual(bounds.height(), lastBounds.height())) {
                    ++stableBoundsCount;
                    if (stableBoundsCount >= 3) {
                        return true;
                    }
                } else {
                    stableBoundsCount = 0;
                    lastBounds = bounds;
                }
            }
        }
        pumpEventsFor(10);
    }

    return mapTab->backgroundLayerCount() == 1
        && mapTab->backgroundLayerSourcePixelSize(0) == expectedSize
        && mapTab->backgroundLayerSceneBounds(0).isValid();
}

bool waitForRasterLayersReady(MapEditorTab *mapTab, const QVector<QSize> &expectedSizes)
{
    if (mapTab == nullptr || expectedSizes.isEmpty()) {
        return false;
    }

    QVector<QRectF> lastBounds(expectedSizes.size());
    int stableBoundsCount = 0;
    for (int attempt = 0; attempt < 400; ++attempt) {
        bool sizesMatch = mapTab->backgroundLayerCount() == expectedSizes.size();
        if (sizesMatch) {
            for (int index = 0; index < expectedSizes.size(); ++index) {
                if (mapTab->backgroundLayerSourcePixelSize(index) != expectedSizes.at(index)) {
                    sizesMatch = false;
                    break;
                }
            }
        }

        if (sizesMatch) {
            bool validBounds = true;
            bool unchangedBounds = true;
            for (int index = 0; index < expectedSizes.size(); ++index) {
                const QRectF bounds = mapTab->backgroundLayerSceneBounds(index);
                if (!bounds.isValid()) {
                    validBounds = false;
                    unchangedBounds = false;
                    break;
                }
                if (!nearlyEqual(bounds.left(), lastBounds.at(index).left())
                    || !nearlyEqual(bounds.top(), lastBounds.at(index).top())
                    || !nearlyEqual(bounds.width(), lastBounds.at(index).width())
                    || !nearlyEqual(bounds.height(), lastBounds.at(index).height())) {
                    unchangedBounds = false;
                }
                lastBounds[index] = bounds;
            }

            if (validBounds) {
                if (unchangedBounds) {
                    ++stableBoundsCount;
                    if (stableBoundsCount >= 3) {
                        return true;
                    }
                } else {
                    stableBoundsCount = 0;
                }
            }
        }

        pumpEventsFor(10);
    }

    if (mapTab->backgroundLayerCount() != expectedSizes.size()) {
        return false;
    }
    for (int index = 0; index < expectedSizes.size(); ++index) {
        if (mapTab->backgroundLayerSourcePixelSize(index) != expectedSizes.at(index)
            || !mapTab->backgroundLayerSceneBounds(index).isValid()) {
            return false;
        }
    }
    return true;
}

QTreeView *findBackgroundLayersTree(MapEditorTab *mapTab)
{
    if (mapTab == nullptr) {
        return nullptr;
    }

    const QList<QTreeView *> trees = mapTab->findChildren<QTreeView *>();
    for (QTreeView *tree : trees) {
        if (tree == nullptr || tree->model() == nullptr) {
            continue;
        }
        if (tree->model()->columnCount() == 3 && !tree->rootIsDecorated()) {
            return tree;
        }
    }

    return nullptr;
}

QDoubleSpinBox *findRequiredSpinBox(MapEditorTab *mapTab, const QString &objectName)
{
    if (mapTab == nullptr) {
        return nullptr;
    }

    return mapTab->findChild<QDoubleSpinBox *>(objectName);
}

bool selectBackgroundsInspectorTab(MapEditorTab *mapTab)
{
    if (mapTab == nullptr) {
        return false;
    }

    const QList<QTabWidget *> tabs = mapTab->findChildren<QTabWidget *>();
    for (QTabWidget *tabWidget : tabs) {
        if (tabWidget == nullptr) {
            continue;
        }
        for (int index = 0; index < tabWidget->count(); ++index) {
            if (tabWidget->tabText(index) == QStringLiteral("Backgrounds")) {
                tabWidget->setCurrentIndex(index);
                pumpEvents();
                return true;
            }
        }
    }

    return false;
}

QGraphicsPathItem *findVisibleBackgroundPivotMarker(MapEditorTab *mapTab)
{
    if (mapTab == nullptr) {
        return nullptr;
    }

    auto *view = mapTab->findChild<QGraphicsView *>(QStringLiteral("mapCanvasView"));
    if (view == nullptr || view->scene() == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> items = view->scene()->items();
    for (QGraphicsItem *item : items) {
        auto *pathItem = qgraphicsitem_cast<QGraphicsPathItem *>(item);
        if (pathItem != nullptr
            && pathItem->isVisible()
            && nearlyEqual(pathItem->zValue(), 100000.0)) {
            return pathItem;
        }
    }

    return nullptr;
}

int runBackgroundVisibilityDoesNotDirtyDocumentTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background visibility test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("background.png"));
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(QColor(80, 120, 160, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary background image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_visibility.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for background visibility test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 -16 16 0\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1} {0 {}} background.png 0 {}\n"
        "\n"
        "scrap visibility-smoke -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    file.write(th2Contents.toUtf8());
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for background visibility test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer auto-loaded from xth_me_image_insert metadata.")) {
        return 1;
    }
    if (!expect(mapTab->isBackgroundLayerVisible(0),
                "Background layer should start visible from xth metadata.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Map tab should not be dirty after loading metadata background layer.")) {
        return 1;
    }

    const QSize expectedSize(16, 16);
    for (int attempt = 0; attempt < 200 && mapTab->backgroundLayerSourcePixelSize(0) != expectedSize; ++attempt) {
        pumpEvents();
    }
    if (!expect(mapTab->backgroundLayerSourcePixelSize(0) == expectedSize,
                "Expected metadata raster background to finish loading before idle refresh checks.")) {
        return 1;
    }
    pumpEventsFor(100);

    int stableBackgroundLayerRefreshCount = 0;
    QObject::connect(mapTab, &MapEditorTab::backgroundLayersChanged, mapTab, [&stableBackgroundLayerRefreshCount]() {
        ++stableBackgroundLayerRefreshCount;
    });
    pumpEventsFor(200);
    stableBackgroundLayerRefreshCount = 0;
    mapTab->triggerFit();
    pumpEventsFor(50);
    mapTab->triggerFit();
    pumpEventsFor(50);
    if (!expect(stableBackgroundLayerRefreshCount == 0,
                "Repeated fits with unchanged metadata should not refresh background layer models.")) {
        return 1;
    }

    const QString originalText = mapTab->text();
    mapTab->setSelectedBackgroundLayerIndex(0);
    mapTab->toggleSelectedBackgroundLayerVisibility();
    pumpEvents();

    if (!expect(!mapTab->isBackgroundLayerVisible(0), "Background layer should be hidden after toggle.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Hiding a background layer should not rewrite xth metadata or source text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Hiding a background layer should not dirty the TH2 document.")) {
        return 1;
    }

    mapTab->toggleSelectedBackgroundLayerVisibility();
    pumpEvents();

    if (!expect(mapTab->isBackgroundLayerVisible(0), "Background layer should be visible after second toggle.")) {
        return 1;
    }
    if (!expect(mapTab->text() == originalText,
                "Showing a background layer should not rewrite xth metadata or source text.")) {
        return 1;
    }
    if (!expect(!mapTab->isDirty(), "Showing a background layer should not dirty the TH2 document.")) {
        return 1;
    }

    return 0;
}

int runMapiahPercentEncodedXviSampleLoadsTest()
{
    const QString filePath =
        repositoryFilePath(QStringLiteral("sample_data/bruce/3-BulmerResurgencePlan.th2"));
    const QString xviPath =
        repositoryFilePath(QStringLiteral("sample_data/bruce/ptopo/3--BulmerResurgence_p.xvi"));
    if (!QFileInfo::exists(filePath) || !QFileInfo::exists(xviPath)) {
        std::cout << "SKIP: Bruce Mapiah sample TH2/XVI files are not available in this checkout.\n";
        return 0;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load Bruce Mapiah percent-encoded XVI sample.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEventsFor(250);

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer from percent-encoded Mapiah XVI sample path.")) {
        return 1;
    }
    QTreeView *backgroundLayersTree = findBackgroundLayersTree(mapTab);
    if (!expect(backgroundLayersTree != nullptr,
                "Expected Backgrounds inspector tree to exist for XVI sample.")) {
        return 1;
    }
    if (!expect(backgroundLayersTree->model()->rowCount() == 1,
                "Expected Backgrounds inspector to list the auto-loaded XVI layer.")) {
        return 1;
    }
    if (!expect(mapTab->backgroundLayerSceneBounds(0).isValid(),
                "Expected percent-encoded Mapiah XVI sample layer to have valid scene bounds.")) {
        return 1;
    }

    return 0;
}

int runRasterBackgroundKeepsFullResolutionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for raster resolution test.")) {
        return 1;
    }

    // A scan far larger than the preview canvas. Before the refactor the layer
    // pixmap was baked down to preview resolution; it must now stay full size.
    const int sourceWidth = 2000;
    const int sourceHeight = 1500;
    const QString imagePath = tempDir.filePath(QStringLiteral("scan.png"));
    QImage image(sourceWidth, sourceHeight, QImage::Format_ARGB32);
    image.fill(QColor(90, 130, 170, 255));
    if (!expect(image.save(imagePath), "Failed to create large temporary background image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_resolution.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 -1500 2000 0\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1} {0 {}} scan.png 0 {}\n"
        "\n"
        "scrap resolution-smoke -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create temporary TH2 file for raster resolution test.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for raster resolution test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one raster background layer auto-loaded for resolution test.")) {
        return 1;
    }

    // The source image loads asynchronously; wait for the full-resolution pixmap.
    const QSize expectedSize(sourceWidth, sourceHeight);
    for (int attempt = 0; attempt < 200 && mapTab->backgroundLayerSourcePixelSize(0) != expectedSize; ++attempt) {
        pumpEvents();
    }

    if (!expect(mapTab->backgroundLayerSourcePixelSize(0) == expectedSize,
                "Raster layer should keep the full source resolution instead of baking to preview size.")) {
        return 1;
    }

    // Geometry must still project correctly: scene bounds keep the source aspect.
    const QRectF bounds = mapTab->backgroundLayerSceneBounds(0);
    if (!expect(bounds.isValid() && bounds.height() > 0.0, "Expected valid raster scene bounds.")) {
        return 1;
    }
    const qreal sourceAspect = static_cast<qreal>(sourceWidth) / static_cast<qreal>(sourceHeight);
    if (!expect(nearlyEqual(bounds.width() / bounds.height(), sourceAspect, 0.02),
                "Raster scene bounds should preserve the source aspect ratio after the transform refactor.")) {
        return 1;
    }
    // The on-screen footprint must be the fitted preview size, not the source pixel size.
    if (!expect(bounds.width() < static_cast<qreal>(sourceWidth),
                "Raster scene footprint should be the projected preview size, not raw source pixels.")) {
        return 1;
    }

    return 0;
}

int runMapiahRasterBackgroundTransformTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for Mapiah raster test.")) {
        return 1;
    }

    const int sourceWidth = 120;
    const int sourceHeight = 60;
    const QString imagePath = tempDir.filePath(QStringLiteral("scan.png"));
    QImage image(sourceWidth, sourceHeight, QImage::Format_ARGB32);
    image.fill(QColor(120, 90, 160, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary Mapiah raster image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("mapiah_raster.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -128 -128 512 512\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##MAPIAH## image_insert_v1 {format=raster;filename=scan.png;xx=0;yy=0;xScale=1;yScale=1;rotationCenterDx=0;rotationCenterDy=0;rotationDeg=45;pivotSet=false}\n"
        "\n"
        "scrap mapiah-raster -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create temporary TH2 file for Mapiah raster test.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for Mapiah raster test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }

    const QSize expectedSize(sourceWidth, sourceHeight);
    for (int attempt = 0; attempt < 200 && mapTab->backgroundLayerSourcePixelSize(0) != expectedSize; ++attempt) {
        pumpEvents();
    }

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one Mapiah raster background layer to auto-load.")) {
        return 1;
    }
    if (!expect(mapTab->backgroundLayerSourcePixelSize(0) == expectedSize,
                "Expected Mapiah raster layer to load the referenced image.")) {
        return 1;
    }

    const QRectF bounds = mapTab->backgroundLayerSceneBounds(0);
    if (!expect(bounds.isValid() && bounds.width() > 0.0 && bounds.height() > 0.0,
                "Expected Mapiah raster layer to have valid transformed scene bounds.")) {
        return 1;
    }
    const qreal transformedAspect = bounds.width() / bounds.height();
    if (!expect(!nearlyEqual(transformedAspect, static_cast<qreal>(sourceWidth) / static_cast<qreal>(sourceHeight), 0.10),
                "Expected Mapiah raster rotation to affect the scene bounds aspect ratio.")) {
        return 1;
    }

    return 0;
}

int runMapiahRasterTransformIgnoresStaleSessionTransformTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for Mapiah stale session transform test.")) {
        return 1;
    }

    struct FixturePaths
    {
        QString imagePath;
        QString filePath;
    };

    const auto writeFixture = [&tempDir](const QString &name) -> std::optional<FixturePaths> {
        const QString fixtureRoot = tempDir.filePath(name);
        if (!QDir().mkpath(fixtureRoot)) {
            return std::nullopt;
        }

        const QString imagePath = QDir(fixtureRoot).filePath(QStringLiteral("scan.png"));
        QImage image(80, 40, QImage::Format_ARGB32);
        image.fill(QColor(64, 128, 192, 255));
        if (!image.save(imagePath)) {
            return std::nullopt;
        }

        const QString filePath = QDir(fixtureRoot).filePath(QStringLiteral("mapiah_stale_session_transform.th2"));
        const QByteArray th2Contents =
            "encoding utf-8\n"
            "##XTHERION## xth_me_area_adjust -100 -100 500 500\n"
            "##XTHERION## xth_me_area_zoom_to 100\n"
            "##MAPIAH## image_insert_v1 {format=raster;filename=scan.png;xx=10;yy=20;xScale=1.4;yScale=0.7;rotationCenterDx=5;rotationCenterDy=-3;rotationDeg=37;pivotSet=true}\n"
            "\n"
            "scrap stale-session-transform -projection plan\n"
            "line wall\n"
            "  0 0\n"
            "  100 0\n"
            "endline\n"
            "endscrap\n";
        if (!writeTextFile(filePath, th2Contents)) {
            return std::nullopt;
        }

        return FixturePaths{imagePath, filePath};
    };

    const std::optional<FixturePaths> cleanFixture = writeFixture(QStringLiteral("clean"));
    if (!expect(cleanFixture.has_value(), "Failed to write clean Mapiah stale session fixture.")) {
        return 1;
    }
    const std::optional<FixturePaths> restoredFixture = writeFixture(QStringLiteral("restored"));
    if (!expect(restoredFixture.has_value(), "Failed to write restored Mapiah stale session fixture.")) {
        return 1;
    }

    const auto boundsMatch = [](const QRectF &a, const QRectF &b) {
        return nearlyEqual(a.left(), b.left())
            && nearlyEqual(a.top(), b.top())
            && nearlyEqual(a.width(), b.width())
            && nearlyEqual(a.height(), b.height());
    };
    QtFileSystem fileSystem;
    QRectF cleanBounds;
    QPointF cleanPosition;
    {
        FakeSessionStore cleanSessionStore;
        QMainWindow hostWindow;
        hostWindow.resize(960, 720);
        auto *central = new QWidget(&hostWindow);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto *mapTab = new MapEditorTab(fileSystem, cleanSessionStore, CommandCatalogStore(), central);
        layout->addWidget(mapTab);
        hostWindow.setCentralWidget(central);
        hostWindow.show();
        pumpEvents();

        QString errorMessage;
        if (!expect(mapTab->loadFile(cleanFixture->filePath, &errorMessage),
                    "MapEditorTab failed to load clean Mapiah raster transform fixture.")) {
            if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << '\n';
            }
            return 1;
        }

        if (!expect(waitForSingleRasterLayerReady(mapTab, QSize(80, 40)),
                    "Clean Mapiah raster layer did not finish loading a stable source image.")) {
            return 1;
        }

        if (!expect(mapTab->backgroundLayerCount() == 1,
                    "Expected one clean Mapiah raster background layer.")) {
            return 1;
        }
        if (!expect(nearlyEqual(mapTab->backgroundLayerXScale(0), 1.4)
                        && nearlyEqual(mapTab->backgroundLayerYScale(0), 0.7)
                        && nearlyEqual(mapTab->backgroundLayerRotationDeg(0), 37.0),
                    "Clean Mapiah raster transform should come from metadata.")) {
            return 1;
        }

        cleanBounds = mapTab->backgroundLayerSceneBounds(0);
        cleanPosition = mapTab->backgroundLayerPosition(0);
        if (!expect(cleanBounds.isValid(), "Clean Mapiah raster layer should have valid transformed bounds.")) {
            return 1;
        }
    }

    FakeSessionStore staleSessionStore;
    const QString documentKey = QFileInfo(restoredFixture->filePath).canonicalFilePath();
    QJsonObject staleLayer;
    staleLayer.insert(QStringLiteral("path"), QFileInfo(restoredFixture->imagePath).absoluteFilePath());
    staleLayer.insert(QStringLiteral("visible"), true);
    staleLayer.insert(QStringLiteral("opacity"), 0.6);
    staleLayer.insert(QStringLiteral("gamma"), 1.0);
    staleLayer.insert(QStringLiteral("x_scale"), 0.25);
    staleLayer.insert(QStringLiteral("y_scale"), 2.5);
    staleLayer.insert(QStringLiteral("rotation_deg"), 180.0);
    staleLayer.insert(QStringLiteral("x"), cleanPosition.x() + 300.0);
    staleLayer.insert(QStringLiteral("y"), cleanPosition.y() - 200.0);
    QJsonArray layers;
    layers.append(staleLayer);
    QJsonObject root;
    root.insert(documentKey, layers);
    staleSessionStore.setTherionMapBackgroundLayers(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, staleSessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(restoredFixture->filePath, &errorMessage),
                "MapEditorTab failed to load stale-session Mapiah raster transform fixture.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }

    if (!expect(waitForSingleRasterLayerReady(mapTab, QSize(80, 40)),
                "Restored Mapiah raster layer did not finish loading a stable source image.")) {
        return 1;
    }

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one Mapiah raster background layer restored from stale session.")) {
        return 1;
    }
    if (!expect(nearlyEqual(mapTab->backgroundLayerXScale(0), 1.4)
                    && nearlyEqual(mapTab->backgroundLayerYScale(0), 0.7)
                    && nearlyEqual(mapTab->backgroundLayerRotationDeg(0), 37.0),
                "Metadata-backed Mapiah raster layer should ignore stale session transform values.")) {
        return 1;
    }
    const QPointF restoredPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(restoredPosition.x(), cleanPosition.x())
                    && nearlyEqual(restoredPosition.y(), cleanPosition.y()),
                "Metadata-backed Mapiah raster layer should ignore stale session scene position.")) {
        return 1;
    }

    const QRectF restoredBounds = mapTab->backgroundLayerSceneBounds(0);
    const bool restoredBoundsMatch = boundsMatch(restoredBounds, cleanBounds);
    if (!restoredBoundsMatch) {
        std::cerr << "Clean bounds: "
                  << cleanBounds.left() << ", " << cleanBounds.top() << ", "
                  << cleanBounds.width() << " x " << cleanBounds.height() << '\n';
        std::cerr << "Restored bounds: "
                  << restoredBounds.left() << ", " << restoredBounds.top() << ", "
                  << restoredBounds.width() << " x " << restoredBounds.height() << '\n';
    }
    if (!expect(restoredBoundsMatch,
                "Metadata-backed Mapiah raster layer should restore the same transformed bounds despite stale session data.")) {
        return 1;
    }

    return 0;
}

int runBackgroundTransformWritesMapiahMetadataTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background transform write test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("scan.png"));
    QImage image(80, 40, QImage::Format_ARGB32);
    image.fill(QColor(140, 110, 90, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary transform-write background image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_transform_write.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 -40 80 0\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1} {0 {}} scan.png 0 {}\n"
        "\n"
        "scrap transform-write -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create temporary TH2 file for background transform write test.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for background transform write test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer for transform write test.")) {
        return 1;
    }
    if (!expect(waitForSingleRasterLayerReady(mapTab, QSize(80, 40)),
                "Background transform-write test raster layer did not finish loading.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(0);
    if (!expect(selectBackgroundsInspectorTab(mapTab),
                "Expected Backgrounds inspector tab for background pivot test.")) {
        return 1;
    }
    QGraphicsPathItem *pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr,
                "Expected a visible background pivot marker after selecting the layer.")) {
        return 1;
    }

    const QPointF requestedPosition(120.0, 75.0);
    mapTab->setSelectedBackgroundLayerPosition(requestedPosition);
    pumpEvents();

    const QPointF movedPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(movedPosition.x(), requestedPosition.x())
                    && nearlyEqual(movedPosition.y(), requestedPosition.y()),
                "Moving a raster background should preserve the requested position value.")) {
        return 1;
    }

    QDoubleSpinBox *positionXSpin = findRequiredSpinBox(mapTab, QStringLiteral("mapBackgroundPosXSpin"));
    QDoubleSpinBox *positionYSpin = findRequiredSpinBox(mapTab, QStringLiteral("mapBackgroundPosYSpin"));
    if (!expect(positionXSpin != nullptr && positionYSpin != nullptr,
                "Expected background position spin boxes to be present.")) {
        return 1;
    }
    if (!expect(nearlyEqual(positionXSpin->value(), requestedPosition.x())
                    && nearlyEqual(positionYSpin->value(), requestedPosition.y()),
                "Background position spin boxes should reflect the selected layer immediately.")) {
        return 1;
    }
    pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr,
                "Expected a visible background pivot marker after moving the layer.")) {
        return 1;
    }
    QPointF pivotScenePosition = pivotMarker->scenePos();

    const qreal originalY = positionYSpin->value();
    positionYSpin->stepUp();
    pumpEvents();
    if (!expect(nearlyEqual(positionYSpin->value(), originalY + 1.0),
                "Background Y spin up should increase the scene Y value by one step.")) {
        return 1;
    }
    positionYSpin->stepDown();
    positionYSpin->stepDown();
    pumpEvents();
    if (!expect(nearlyEqual(positionYSpin->value(), originalY - 1.0),
                "Background Y spin down should decrease the scene Y value by one step.")) {
        return 1;
    }
    const QPointF editedPosition(positionXSpin->value(), positionYSpin->value());
    if (!expect(!mapTab->text().contains(QStringLiteral("##MAPIAH## image_insert_v1")),
                "Moving a raster background without scale/rotation edits should keep XTherion metadata.")) {
        return 1;
    }
    pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr,
                "Expected a visible background pivot marker after position step edits.")) {
        return 1;
    }
    pivotScenePosition = pivotMarker->scenePos();

    mapTab->setSelectedBackgroundLayerXScale(1.25);
    pumpEvents();
    pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr
                    && nearlyEqual(pivotMarker->scenePos().x(), pivotScenePosition.x())
                    && nearlyEqual(pivotMarker->scenePos().y(), pivotScenePosition.y()),
                "Scaling a raster background should keep the visible pivot marker fixed.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerYScale(0.75);
    pumpEvents();
    pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr
                    && nearlyEqual(pivotMarker->scenePos().x(), pivotScenePosition.x())
                    && nearlyEqual(pivotMarker->scenePos().y(), pivotScenePosition.y()),
                "Independent raster Y scaling should keep the visible pivot marker fixed.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerRotationDeg(-12.5);
    pumpEvents();
    pivotMarker = findVisibleBackgroundPivotMarker(mapTab);
    if (!expect(pivotMarker != nullptr
                    && nearlyEqual(pivotMarker->scenePos().x(), pivotScenePosition.x())
                    && nearlyEqual(pivotMarker->scenePos().y(), pivotScenePosition.y()),
                "Rotating a raster background should keep the visible pivot marker fixed.")) {
        return 1;
    }

    const QString updatedText = mapTab->text();
    if (!expect(updatedText.contains(QStringLiteral("##MAPIAH## image_insert_v1")),
                "Transforming a background layer should write Mapiah image metadata.")) {
        return 1;
    }
    if (!expect(!updatedText.contains(QStringLiteral("xth_me_image_insert")),
                "Transforming a background layer should replace the simple XTherion image metadata line.")) {
        return 1;
    }

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(updatedText, filePath);
    if (!expect(references.size() == 1,
                "Expected one background metadata reference after transform write.")) {
        return 1;
    }
    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.metadataFormat == TherionBackgroundMetadataFormat::Mapiah
                    && reference.layerFormat == TherionBackgroundLayerFormat::Raster,
                "Transform write should produce a Mapiah raster metadata reference.")) {
        return 1;
    }
    if (!expect(nearlyEqual(reference.xScale, 1.25)
                    && nearlyEqual(reference.yScale, 0.75)
                    && nearlyEqual(reference.rotationDeg, -12.5),
                "Mapiah metadata should preserve the edited scale and rotation values.")) {
        return 1;
    }
    if (!expect(reference.hasBasePosition,
                "Mapiah conversion should keep a raster base position.")) {
        return 1;
    }
    if (!expect(mapTab->canUndo(),
                "Background transform should leave map undo available immediately after the edit.")) {
        return 1;
    }

    mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Raw);
    pumpEventsFor(1000);
    if (!expect(mapTab->canUndo(),
                "Background transform undo should remain available after switching to Raw mode.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    const QString undoneText = mapTab->text();
    if (!expect(undoneText.contains(QStringLiteral("##MAPIAH## image_insert_v1")),
                "Undoing once should revert the last background transform snapshot, not remove all Mapiah metadata.")) {
        return 1;
    }
    if (!expect(undoneText.contains(QStringLiteral("rotationDeg=0")),
                "Undoing the rotation transform should restore the previous Mapiah rotation value.")) {
        return 1;
    }

    mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Visual);
    mapTab->beginSetSelectedBackgroundLayerPivot();
    mapTab->setSelectedBackgroundLayerRotationDeg(-6.0);
    pumpEventsFor(150);
    mapTab->resetSelectedBackgroundLayerPivot();
    pumpEvents();

    return 0;
}

int runLegacyMultiRasterAreaAdjustPlacementTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for legacy multi-raster placement test.")) {
        return 1;
    }

    const QString firstImagePath = tempDir.filePath(QStringLiteral("stara_mapa_1.png"));
    const QString secondImagePath = tempDir.filePath(QStringLiteral("stara_mapa_2.png"));
    QImage firstImage(723, 1024, QImage::Format_ARGB32);
    firstImage.fill(QColor(180, 180, 180, 255));
    QImage secondImage(1400, 96, QImage::Format_ARGB32);
    secondImage.fill(QColor(120, 120, 120, 255));
    if (!expect(firstImage.save(firstImagePath) && secondImage.save(secondImagePath),
                "Failed to create temporary raster images for legacy multi-raster placement test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("legacy_multi_raster.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -2588 -3632 2428 128\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1.0} {0 {}} stara_mapa_1.png 0 {}\n"
        "##XTHERION## xth_me_image_insert {-2460 1 1.0} {0 {}} stara_mapa_2.png 0 {}\n"
        "\n"
        "scrap legacy-raster -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create TH2 file for legacy multi-raster placement test.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for legacy multi-raster placement test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 2,
                "Expected two raster background layers in legacy multi-raster placement test.")) {
        return 1;
    }

    if (!expect(waitForRasterLayersReady(mapTab, {QSize(723, 1024), QSize(1400, 96)}),
                "Legacy multi-raster background layers did not finish loading stable source images.")) {
        return 1;
    }

    const QRectF firstBounds = mapTab->backgroundLayerSceneBounds(0);
    const QRectF secondBounds = mapTab->backgroundLayerSceneBounds(1);
    if (!expect(firstBounds.isValid() && secondBounds.isValid(),
                "Expected valid scene bounds for both legacy raster layers.")) {
        return 1;
    }
    if (!expect(qAbs(firstBounds.top() - secondBounds.top()) < 5.0,
                "Legacy XTherion raster layers sharing the same Y anchor should stay aligned vertically.")) {
        return 1;
    }
    if (!expect(secondBounds.right() < firstBounds.left(),
                "Legacy XTherion raster layer with negative X anchor should stay left of the first raster, not below it.")) {
        return 1;
    }

    return 0;
}

int runLegacyMultiRasterSessionRestoreIgnoresStalePlacementTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for legacy session restore test.")) {
        return 1;
    }

    const QString firstImagePath = tempDir.filePath(QStringLiteral("stara_mapa_1.png"));
    const QString secondImagePath = tempDir.filePath(QStringLiteral("stara_mapa_2.png"));
    QImage firstImage(723, 1024, QImage::Format_ARGB32);
    firstImage.fill(QColor(180, 180, 180, 255));
    QImage secondImage(1400, 96, QImage::Format_ARGB32);
    secondImage.fill(QColor(120, 120, 120, 255));
    if (!expect(firstImage.save(firstImagePath) && secondImage.save(secondImagePath),
                "Failed to create temporary raster images for legacy session restore test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("legacy_multi_raster_session.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -2588 -3632 2428 128\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1.0} {0 {}} stara_mapa_1.png 0 {}\n"
        "##XTHERION## xth_me_image_insert {-2460 1 1.0} {0 {}} stara_mapa_2.png 0 {}\n"
        "\n"
        "scrap legacy-raster -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create TH2 file for legacy session restore test.")) {
        return 1;
    }

    FakeSessionStore sessionStore;
    const QString documentKey = QFileInfo(filePath).canonicalFilePath();
    QJsonArray layersArray;
    {
        QJsonObject firstLayer;
        firstLayer.insert(QStringLiteral("path"), QFileInfo(firstImagePath).absoluteFilePath());
        firstLayer.insert(QStringLiteral("visible"), true);
        firstLayer.insert(QStringLiteral("opacity"), 0.5);
        firstLayer.insert(QStringLiteral("gamma"), 1.0);
        firstLayer.insert(QStringLiteral("x_scale"), 1.0);
        firstLayer.insert(QStringLiteral("y_scale"), 1.0);
        firstLayer.insert(QStringLiteral("rotation_deg"), 0.0);
        firstLayer.insert(QStringLiteral("x"), 8000.0);
        firstLayer.insert(QStringLiteral("y"), 9000.0);
        layersArray.append(firstLayer);
    }
    {
        QJsonObject secondLayer;
        secondLayer.insert(QStringLiteral("path"), QFileInfo(secondImagePath).absoluteFilePath());
        secondLayer.insert(QStringLiteral("visible"), true);
        secondLayer.insert(QStringLiteral("opacity"), 0.5);
        secondLayer.insert(QStringLiteral("gamma"), 1.0);
        secondLayer.insert(QStringLiteral("x_scale"), 1.0);
        secondLayer.insert(QStringLiteral("y_scale"), 1.0);
        secondLayer.insert(QStringLiteral("rotation_deg"), 0.0);
        secondLayer.insert(QStringLiteral("x"), -4000.0);
        secondLayer.insert(QStringLiteral("y"), -7000.0);
        layersArray.append(secondLayer);
    }
    QJsonObject rootObject;
    rootObject.insert(documentKey, layersArray);
    sessionStore.setTherionMapBackgroundLayers(QString::fromUtf8(QJsonDocument(rootObject).toJson(QJsonDocument::Compact)));

    QtFileSystem fileSystem;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for legacy session restore test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }

    if (!expect(waitForRasterLayersReady(mapTab, {QSize(723, 1024), QSize(1400, 96)}),
                "Legacy session-restore raster layers did not finish loading stable source images.")) {
        return 1;
    }

    const QRectF firstBounds = mapTab->backgroundLayerSceneBounds(0);
    const QRectF secondBounds = mapTab->backgroundLayerSceneBounds(1);
    if (!expect(firstBounds.isValid() && secondBounds.isValid(),
                "Expected valid scene bounds for both restored legacy raster layers.")) {
        return 1;
    }
    if (!expect(qAbs(firstBounds.top() - secondBounds.top()) < 5.0,
                "Restored legacy XTherion raster layers should stay aligned vertically after metadata reprojection.")) {
        return 1;
    }
    if (!expect(secondBounds.right() < firstBounds.left(),
                "Restored legacy XTherion raster layer with negative X anchor should stay left of the first raster.")) {
        return 1;
    }

    return 0;
}

int runBackgroundRotationUndoSurvivesRawModeSwitchTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background rotate undo test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("scan.png"));
    QImage image(80, 40, QImage::Format_ARGB32);
    image.fill(QColor(90, 120, 150, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary rotate-undo background image.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_rotate_undo.th2"));
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 -40 80 0\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {0 1 1} {0 {}} scan.png 0 {}\n"
        "\n"
        "scrap rotate-undo -projection plan\n"
        "point 0 0 station -name A\n"
        "endscrap\n");
    if (!expect(writeTextFile(filePath, th2Contents.toUtf8()),
                "Failed to create temporary TH2 file for background rotate undo test.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for background rotate undo test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    mapTab->setSelectedBackgroundLayerIndex(0);
    mapTab->setSelectedBackgroundLayerRotationDeg(10.0);
    pumpEvents();
    if (!expect(mapTab->canUndo(), "Background rotation should make map undo available.")) {
        return 1;
    }

    mapTab->setWorkspaceMode(MapEditorTab::WorkspaceMode::Raw);
    pumpEventsFor(1000);
    if (!expect(mapTab->canUndo(), "Background rotation undo should remain available in Raw mode.")) {
        return 1;
    }

    mapTab->triggerUndo();
    pumpEvents();
    if (!expect(!mapTab->text().contains(QStringLiteral("rotationDeg=10")),
                "Undoing background rotation from Raw mode should remove the rotated Mapiah metadata.")) {
        return 1;
    }

    return 0;
}

int runBackgroundGammaPreservesPlacementTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for background gamma test.")) {
        return 1;
    }

    const QString imagePath = tempDir.filePath(QStringLiteral("background.png"));
    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(160, 120, 80, 255));
    if (!expect(image.save(imagePath), "Failed to create temporary background image for gamma test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_gamma.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for background gamma test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {245.0 1.0} 822.0 background.png 0 {}\n"
        "\n"
        "scrap gamma-smoke -projection plan\n"
        "point 500 500 station -name A\n"
        "endscrap\n");
    file.write(th2Contents.toUtf8());
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for background gamma test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one background layer auto-loaded for gamma test.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(0);
    const QPointF originalPosition = mapTab->backgroundLayerPosition(0);
    mapTab->setSelectedBackgroundLayerGamma(1.5);
    pumpEvents();

    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {245.0 1.0 1.5} 822.0 background.png 0 {}")),
                "Changing background gamma should preserve the existing X/Y metadata tokens and update only the gamma token.")) {
        return 1;
    }

    const QPointF updatedPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(updatedPosition.x(), originalPosition.x())
                    && nearlyEqual(updatedPosition.y(), originalPosition.y()),
                "Changing background gamma should not move the scene layer.")) {
        return 1;
    }

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(mapTab->text(), filePath);
    if (!expect(references.size() == 1,
                "Expected gamma-updated metadata to keep one background reference.")) {
        return 1;
    }
    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.hasBasePosition
                    && nearlyEqual(reference.basePosition.x(), 245.0)
                    && nearlyEqual(reference.basePosition.y(), 822.0),
                "Changing background gamma should preserve XTherion X/Y placement metadata.")) {
        return 1;
    }
    if (!expect(reference.hasImageScale && nearlyEqual(reference.imageScale, 1.5),
                "Changing background gamma should update only the metadata gamma value.")) {
        return 1;
    }

    return 0;
}

int runBackgroundGammaPreservesSelectedLayerTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for multi-layer gamma test.")) {
        return 1;
    }

    const QString firstImagePath = tempDir.filePath(QStringLiteral("zab_ch2.png"));
    const QString secondImagePath = tempDir.filePath(QStringLiteral("zab_dom1.png"));
    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(100, 140, 180, 255));
    if (!expect(image.save(firstImagePath) && image.save(secondImagePath),
                "Failed to create temporary background images for multi-layer gamma test.")) {
        return 1;
    }

    const QString filePath = tempDir.filePath(QStringLiteral("background_gamma_selection.th2"));
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for multi-layer gamma test.")) {
        return 1;
    }

    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {126.6 1.0} 304.1 zab_ch2.png 0 {}\n"
        "##XTHERION## xth_me_image_insert {820.4 1.0} 213.1 zab_dom1.png 0 {}\n"
        "\n"
        "scrap gamma-selection-smoke -projection plan\n"
        "point 500 500 station -name A\n"
        "endscrap\n");
    file.write(th2Contents.toUtf8());
    file.close();

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for multi-layer gamma test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 2,
                "Expected two background layers auto-loaded for multi-layer gamma test.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(0);
    const QPointF firstOriginalPosition = mapTab->backgroundLayerPosition(0);
    mapTab->setSelectedBackgroundLayerGamma(0.8);
    pumpEvents();

    if (!expect(mapTab->selectedBackgroundLayerIndex() == 0,
                "Changing gamma on the first layer should preserve the selected background layer.")) {
        return 1;
    }
    if (!expect(nearlyEqual(mapTab->backgroundLayerPosition(0).x(), firstOriginalPosition.x())
                    && nearlyEqual(mapTab->backgroundLayerPosition(0).y(), firstOriginalPosition.y()),
                "Changing gamma on the first layer should not move that layer.")) {
        return 1;
    }
    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {126.6 1.0 0.8} 304.1 zab_ch2.png 0 {}"))
                    && mapTab->text().contains(QStringLiteral("xth_me_image_insert {820.4 1.0} 213.1 zab_dom1.png 0 {}")),
                "Changing gamma on the first layer should update only the first layer metadata.")) {
        return 1;
    }

    mapTab->setSelectedBackgroundLayerIndex(1);
    const QPointF secondOriginalPosition = mapTab->backgroundLayerPosition(1);
    mapTab->setSelectedBackgroundLayerGamma(1.4);
    pumpEvents();

    if (!expect(mapTab->selectedBackgroundLayerIndex() == 1,
                "Changing gamma on the second layer should preserve the selected background layer.")) {
        return 1;
    }
    if (!expect(nearlyEqual(mapTab->backgroundLayerPosition(1).x(), secondOriginalPosition.x())
                    && nearlyEqual(mapTab->backgroundLayerPosition(1).y(), secondOriginalPosition.y()),
                "Changing gamma on the second layer should not move that layer.")) {
        return 1;
    }
    if (!expect(mapTab->text().contains(QStringLiteral("xth_me_image_insert {126.6 1.0 0.8} 304.1 zab_ch2.png 0 {}"))
                    && mapTab->text().contains(QStringLiteral("xth_me_image_insert {820.4 1.0 1.4} 213.1 zab_dom1.png 0 {}")),
                "Changing gamma on the second layer should update only the second layer metadata.")) {
        return 1;
    }

    return 0;
}

int runMetadataXviPlacementIgnoresSessionScenePositionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for XVI placement test.")) {
        return 1;
    }

    const QString xviPath = tempDir.filePath(QStringLiteral("placed.xvi"));
    QFile xviFile(xviPath);
    if (!expect(xviFile.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary XVI file for placement test.")) {
        return 1;
    }
    xviFile.write(
        "set XVIgrid {-2651.57480315 -4006.2992126 78.7401574803 0.0 0.0 78.7401574803 76.0 98.0}\n"
        "set XVIstations {\n"
        "  {-701.54 -2508.75 1.4}\n"
        "  {0.0 0.0 1.0}\n"
        "}\n"
        "set XVIshots {\n"
        "  {-701.54 -2508.75 0.0 0.0}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black -701.54 -2508.75 -640.0 -2450.0}\n"
        "}\n");
    xviFile.close();

    const QString filePath = tempDir.filePath(QStringLiteral("metadata_xvi_restore.th2"));
    QFile th2File(filePath);
    if (!expect(th2File.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create temporary TH2 file for XVI placement test.")) {
        return 1;
    }
    const QString th2Contents = QStringLiteral(
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust -128.0 -128.0 6033.5118110225 7797.7952755891\n"
        "##XTHERION## xth_me_area_zoom_to 25\n"
        "##XTHERION## xth_me_image_insert {1950.0348031500002 1 1.0} {1497.5492126 1.4} placed.xvi 0 {}\n"
        "\n"
        "scrap xvi-restore -projection plan -scale [0 0 787.4 787.4 0 0 10 10 m]\n"
        "point 1950.0348031500002 1497.5492126 station -name 1.4\n"
        "point 2651.57480315 4006.2992126 station -name 1.0\n"
        "endscrap\n");
    th2File.write(th2Contents.toUtf8());
    th2File.close();

    QtFileSystem fileSystem;
    FakeSessionStore cleanSessionStore;
    QPointF metadataPosition;
    {
        QMainWindow hostWindow;
        hostWindow.resize(960, 720);
        auto *central = new QWidget(&hostWindow);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto *mapTab = new MapEditorTab(fileSystem, cleanSessionStore, CommandCatalogStore(), central);
        layout->addWidget(mapTab);
        hostWindow.setCentralWidget(central);
        hostWindow.show();
        pumpEvents();

        QString errorMessage;
        if (!expect(mapTab->loadFile(filePath, &errorMessage),
                    "MapEditorTab failed to load TH2 file for clean XVI placement test.")) {
            if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << '\n';
            }
            return 1;
        }
        pumpEvents();

        if (!expect(mapTab->backgroundLayerCount() == 1,
                    "Expected one metadata XVI layer in clean placement test.")) {
            return 1;
        }
        metadataPosition = mapTab->backgroundLayerPosition(0);
    }

    FakeSessionStore staleSessionStore;
    const QString documentKey = QFileInfo(filePath).canonicalFilePath();
    QJsonObject staleLayer;
    staleLayer.insert(QStringLiteral("path"), QFileInfo(xviPath).absoluteFilePath());
    staleLayer.insert(QStringLiteral("visible"), true);
    staleLayer.insert(QStringLiteral("opacity"), 0.58);
    staleLayer.insert(QStringLiteral("gamma"), 1.0);
    staleLayer.insert(QStringLiteral("x"), metadataPosition.x() + 197.0);
    staleLayer.insert(QStringLiteral("y"), metadataPosition.y() + 42.0);
    QJsonArray layers;
    layers.append(staleLayer);
    QJsonObject root;
    root.insert(documentKey, layers);
    staleSessionStore.setTherionMapBackgroundLayers(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, staleSessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to load TH2 file for stale-session XVI placement test.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one metadata XVI layer restored from stale session.")) {
        return 1;
    }
    const QPointF restoredPosition = mapTab->backgroundLayerPosition(0);
    if (!expect(nearlyEqual(restoredPosition.x(), metadataPosition.x())
                    && nearlyEqual(restoredPosition.y(), metadataPosition.y()),
                "Metadata-backed XVI layer should ignore stale session scene position and keep XTherion placement.")) {
        return 1;
    }

    return 0;
}

int runXviCacheReloadsSameTimestampContentChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory for XVI cache reload test.")) {
        return 1;
    }

    const QString xviPath = tempDir.filePath(QStringLiteral("cached.xvi"));
    const QByteArray firstXvi =
        "set XVIgrid {0 0}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black 0 0 100 100}\n"
        "}\n";
    if (!expect(writeTextFile(xviPath, firstXvi), "Failed to write initial XVI cache test file.")) {
        return 1;
    }
    const QDateTime originalModified = QFileInfo(xviPath).lastModified();

    const QString filePath = tempDir.filePath(QStringLiteral("xvi_cache_reload.th2"));
    const QByteArray th2Contents =
        "encoding utf-8\n"
        "##XTHERION## xth_me_area_adjust 0 0 1000 1000\n"
        "##XTHERION## xth_me_area_zoom_to 100\n"
        "##XTHERION## xth_me_image_insert {100 1 1.0} {100 root} cached.xvi 0 {}\n"
        "\n"
        "scrap cache-reload -projection plan\n"
        "point 100 100 station -name root\n"
        "endscrap\n";
    if (!expect(writeTextFile(filePath, th2Contents), "Failed to write TH2 cache reload test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    QRectF firstBounds;
    {
        FakeSessionStore sessionStore;
        QMainWindow hostWindow;
        hostWindow.resize(960, 720);
        auto *central = new QWidget(&hostWindow);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
        layout->addWidget(mapTab);
        hostWindow.setCentralWidget(central);
        hostWindow.show();
        pumpEvents();

        QString errorMessage;
        if (!expect(mapTab->loadFile(filePath, &errorMessage),
                    "MapEditorTab failed to load initial XVI cache reload file.")) {
            if (!errorMessage.isEmpty()) {
                std::cerr << errorMessage.toStdString() << '\n';
            }
            return 1;
        }
        pumpEvents();

        if (!expect(mapTab->backgroundLayerCount() == 1,
                    "Expected one metadata XVI layer before cache reload.")) {
            return 1;
        }
        firstBounds = mapTab->backgroundLayerSceneBounds(0);
        if (!expect(firstBounds.isValid(), "Expected valid initial XVI background bounds.")) {
            return 1;
        }
    }

    const QByteArray secondXvi =
        "set XVIgrid {0 0}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {black 800 800 900 900}\n"
        "}\n";
    if (!expect(writeTextFile(xviPath, secondXvi), "Failed to rewrite XVI cache test file.")) {
        return 1;
    }
    QFile timestampFile(xviPath);
    if (!expect(timestampFile.open(QIODevice::ReadWrite),
                "Failed to reopen XVI cache test file for timestamp reset.")) {
        return 1;
    }
    if (!expect(timestampFile.setFileTime(originalModified, QFileDevice::FileModificationTime),
                "Failed to restore original XVI cache test modification timestamp.")) {
        return 1;
    }
    timestampFile.close();

    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(960, 720);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    if (!expect(mapTab->loadFile(filePath, &errorMessage),
                "MapEditorTab failed to reload TH2 after same-timestamp XVI content change.")) {
        if (!errorMessage.isEmpty()) {
            std::cerr << errorMessage.toStdString() << '\n';
        }
        return 1;
    }
    pumpEvents();

    if (!expect(mapTab->backgroundLayerCount() == 1,
                "Expected one metadata XVI layer after same-timestamp cache reload.")) {
        return 1;
    }
    const QRectF secondBounds = mapTab->backgroundLayerSceneBounds(0);
    if (!expect(secondBounds.isValid(), "Expected valid reloaded XVI background bounds.")) {
        return 1;
    }
    if (!expect(!nearlyEqual(secondBounds.left(), firstBounds.left())
                    || !nearlyEqual(secondBounds.top(), firstBounds.top())
                    || !nearlyEqual(secondBounds.width(), firstBounds.width())
                    || !nearlyEqual(secondBounds.height(), firstBounds.height()),
                "XVI cache should reload changed content even when the file timestamp is unchanged.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runBackgroundVisibilityDoesNotDirtyDocumentTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMapiahPercentEncodedXviSampleLoadsTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runRasterBackgroundKeepsFullResolutionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMapiahRasterBackgroundTransformTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMapiahRasterTransformIgnoresStaleSessionTransformTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBackgroundTransformWritesMapiahMetadataTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLegacyMultiRasterAreaAdjustPlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLegacyMultiRasterSessionRestoreIgnoresStalePlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBackgroundRotationUndoSurvivesRawModeSwitchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBackgroundGammaPreservesPlacementTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBackgroundGammaPreservesSelectedLayerTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMetadataXviPlacementIgnoresSessionScenePositionTest(); rc != 0) {
        return rc;
    }
    return runXviCacheReloadsSameTimestampContentChangeTest();
}
