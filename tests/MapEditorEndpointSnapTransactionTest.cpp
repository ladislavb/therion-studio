#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"
#include "FakeSessionStore.h"

#include <QDir>
#include <QComboBox>
#include <QFile>
#include <QGraphicsView>
#include <QPushButton>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
MapEditableGeometryVertexItem *anchor(QGraphicsScene *scene, int line, int index)
{
    for (QGraphicsItem *item : scene->items()) {
        auto *vertex = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertex != nullptr && vertex->geometryKind() == QStringLiteral("line")
            && vertex->vertexIndex() == index
            && vertex->data(kMapSceneLineNumberRole).toInt() == line) {
            return vertex;
        }
    }
    return nullptr;
}

QGraphicsPathItem *linePath(QGraphicsScene *scene, int line)
{
    for (QGraphicsItem *item : scene->items()) {
        auto *path = dynamic_cast<QGraphicsPathItem *>(item);
        if (path != nullptr && path->data(kMapItemRole).toInt() == kMapItemGeometryValue
            && path->data(kMapSceneLineNumberRole).toInt() == line
            && path->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeGeneric) {
            return path;
        }
    }
    return nullptr;
}

void mouse(QWidget *widget, QEvent::Type type, const QPoint &position,
           Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(position), QPointF(widget->mapToGlobal(position)),
                      button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

void click(QGraphicsView *view, const QPointF &position)
{
    view->centerOn(position);
    QCoreApplication::processEvents();
    const QPoint pixel = view->mapFromScene(position);
    mouse(view->viewport(), QEvent::MouseButtonPress, pixel, Qt::LeftButton, Qt::LeftButton);
    mouse(view->viewport(), QEvent::MouseButtonRelease, pixel, Qt::LeftButton, Qt::NoButton);
    QCoreApplication::processEvents();
}
}

class MapEditorEndpointSnapTransactionTest final : public QObject
{
    Q_OBJECT
private slots:
    void wallSnapWinsOverCloserNonWall_data()
    {
        QTest::addColumn<QString>("movingType");
        QTest::addColumn<bool>("newDraw");
        QTest::addColumn<bool>("wallSegment");
        QTest::addColumn<double>("wallDistancePixels");
        QTest::newRow("wall-drag") << QStringLiteral("wall") << false << false << 6.0;
        QTest::newRow("slope-drag-unchanged") << QStringLiteral("slope") << false << false << 6.0;
        QTest::newRow("wall-draw") << QStringLiteral("wall") << true << false << 6.0;
        QTest::newRow("slope-draw-unchanged") << QStringLiteral("slope") << true << false << 6.0;
        QTest::newRow("wall-segment-drag") << QStringLiteral("wall") << false << true << 6.0;
        QTest::newRow("wall-segment-draw") << QStringLiteral("wall") << true << true << 6.0;
        QTest::newRow("wall-outside-radius-drag") << QStringLiteral("wall") << false << false << 24.0;
    }

    void wallSnapWinsOverCloserNonWall()
    {
        QFETCH(QString, movingType);
        QFETCH(bool, newDraw);
        QFETCH(bool, wallSegment);
        QFETCH(double, wallDistancePixels);
        const bool preferWall = movingType == QStringLiteral("wall")
            && wallDistancePixels <= kMapEditorSnapTargetRadiusPixels;
        QVERIFY(QDir().mkpath(QStringLiteral(".verification")));
        QTemporaryDir directory(QDir::current().filePath(QStringLiteral(".verification/wall-snap-XXXXXX")));
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("wall-snap.th2"));
        QString original = QStringLiteral(
            "encoding utf-8\nscrap s1 -projection plan\n"
            "line %1 -id moving\n  0 0\n  90 0\nendline\nendscrap\n"
            "scrap s2 -projection plan\n"
            "line wall -id target\n  100 0\n  120 0\nendline\n"
            "line slope -id closer\n  99 0\n  119 0\nendline\n"
            "line border\n  98 0\n  118 0\nendline\n"
            "point 99.5 0 station -name P\nendscrap\n").arg(movingType);
        if (wallSegment) {
            original.replace(QStringLiteral("  100 0\n  120 0\n"),
                             QStringLiteral("  100 -20\n  100 20\n"));
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original.toUtf8());
        file.close();
        QtFileSystem files;
        FakeSessionStore prefs;
        MapEditorTab tab(files, prefs, CommandCatalogStore{});
        tab.resize(1280, 900);
        tab.show();
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        auto *view = tab.findChild<QGraphicsView *>();
        auto *stack = tab.findChild<QUndoStack *>();
        QVERIFY(view && stack);
        tab.goToLine(3);
        QCoreApplication::processEvents();
        auto *moving = anchor(view->scene(), 3, 1);
        auto *wall = anchor(view->scene(), 9, 0);
        auto *slope = anchor(view->scene(), 13, 0);
        QVERIFY(moving && wall && slope);
        const QPointF wallPosition = wallSegment
            ? (wall->scenePos() + anchor(view->scene(), 9, 1)->scenePos()) * 0.5
            : wall->scenePos();
        const QPointF wallGuidePosition = wall->scenePos();
        const QPointF slopePosition = slope->scenePos();
        const qreal scale = wallDistancePixels / QLineF(wallPosition, slopePosition).length();
        view->resetTransform();
        view->scale(scale, scale);
        view->centerOn(slopePosition);
        QCoreApplication::processEvents();
        QPoint end = view->mapFromScene(slopePosition);
        const QPointF expected = preferWall ? wallPosition : slopePosition;

        if (newDraw) {
            tab.triggerAddLine();
            auto *typeCombo = tab.findChild<QComboBox *>(QStringLiteral("mapObjectQuickTypeCombo"));
            QVERIFY(typeCombo);
            typeCombo->setCurrentText(movingType);
            QVERIFY(QMetaObject::invokeMethod(typeCombo, "activated", Q_ARG(int, typeCombo->currentIndex())));
            QCoreApplication::processEvents();
            view->resetTransform();
            view->scale(scale, scale);
            view->centerOn(slopePosition);
            QCoreApplication::processEvents();
            end = view->mapFromScene(slopePosition);
            mouse(view->viewport(), QEvent::MouseMove, end, Qt::NoButton, Qt::NoButton);
        } else {
            const QPoint start = view->mapFromScene(moving->scenePos());
            mouse(view->viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
            for (int step = 1; step <= 6; ++step) {
                mouse(view->viewport(), QEvent::MouseMove, start + (end - start) * step / 6,
                      Qt::NoButton, Qt::LeftButton);
                QCoreApplication::processEvents();
            }
            QVERIFY2(QLineF(moving->scenePos(), expected).length() < 0.001,
                     "Wall drag must prefer the wall six pixels away over the slope directly under the pointer.");
        }
        if (preferWall) {
            bool wallGuideVisible = false;
            bool slopeGuideVisible = false;
            for (QGraphicsItem *item : view->scene()->items()) {
                auto *marker = dynamic_cast<QGraphicsEllipseItem *>(item);
                if (marker && marker->isVisible() && marker->acceptedMouseButtons() == Qt::NoButton
                    && qAbs(marker->zValue() - (newDraw ? 28.7 : 28.8)) < 0.001) {
                    wallGuideVisible |= QLineF(marker->scenePos(), wallGuidePosition).length() < 0.001;
                    slopeGuideVisible |= QLineF(marker->scenePos(), slopePosition).length() < 0.001;
                }
            }
            QVERIFY2(wallGuideVisible, "Snap guides must identify the preferred wall, not the closer slope.");
            QVERIFY2(!slopeGuideVisible, "A closer slope must not retain guides when a wall wins.");
        }
        if (newDraw) {
            mouse(view->viewport(), QEvent::MouseButtonPress, end, Qt::LeftButton, Qt::LeftButton);
            mouse(view->viewport(), QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
            const QPoint second = end + QPoint(-100, 80);
            mouse(view->viewport(), QEvent::MouseButtonPress, second, Qt::LeftButton, Qt::LeftButton);
            mouse(view->viewport(), QEvent::MouseButtonRelease, second, Qt::LeftButton, Qt::NoButton);
            tab.triggerCompleteDraft();
            QCoreApplication::processEvents();
            const auto features = collectGeometryFeatures(
                TherionDocumentParser::parseText(tab.text()));
            bool insertedLineStartsAtTarget = false;
            for (const auto &feature : features) {
                if (feature.kind == MapGeometryFeature::Kind::Line && feature.lineNumber != 3
                    && feature.optionValues.value(QStringLiteral("id")).isEmpty()
                    && feature.label == movingType && !feature.vertices.isEmpty()
                    && qAbs(feature.vertices.first().x() - (preferWall ? 100.0 : 99.5)) < 0.01
                    && qAbs(feature.vertices.first().y()) < (wallSegment ? 0.2 : 0.01)) {
                    insertedLineStartsAtTarget = true;
                }
            }
            QVERIFY2(insertedLineStartsAtTarget, qPrintable(tab.text()));
        } else {
            mouse(view->viewport(), QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
            QCoreApplication::processEvents();
            QVERIFY(QLineF(anchor(view->scene(), 3, 1)->scenePos(), expected).length() < 0.001);
            QVERIFY(tab.text().contains(preferWall
                ? QStringLiteral("  100 0\nendline\nendscrap")
                : QStringLiteral("  99 0\nendline\nendscrap")));
        }
        QCOMPARE(stack->count(), 1);
        tab.triggerUndo();
        QTRY_COMPARE(tab.text(), original);
    }

    void pointDragAtTenThousandPercentIsAtomic()
    {
        QTemporaryDir directory(QDir::current().filePath(QStringLiteral(".verification/point-zoom-XXXXXX")));
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("point.th2"));
        const QString original = QStringLiteral(
            "encoding utf-8\nscrap s -projection plan\npoint 100 100 station -name A # retained\nendscrap\n");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(original.toUtf8());
        file.close();
        QtFileSystem files;
        FakeSessionStore prefs;
        MapEditorTab tab(files, prefs, CommandCatalogStore{});
        tab.resize(1000, 700);
        tab.show();
        QString error;
        QVERIFY2(tab.loadFile(path, &error), qPrintable(error));
        auto *view = tab.findChild<QGraphicsView *>();
        auto *stack = tab.findChild<QUndoStack *>();
        QVERIFY(view && stack);
        const auto point = [&]() -> MapEditablePointItem * {
            for (QGraphicsItem *item : view->scene()->items()) {
                if (auto *candidate = dynamic_cast<MapEditablePointItem *>(item)) {
                    return candidate;
                }
            }
            return nullptr;
        };
        QTRY_VERIFY(point());
        tab.goToLine(3);
        QCoreApplication::processEvents();
        view->resetTransform();
        view->scale(100, 100);
        const QPointF before = point()->scenePos();
        click(view, before);
        QCOMPARE(view->transform().m11(), 100.0);
        const QPoint start = view->mapFromScene(before);
        const QPoint end = start + QPoint(24, 16);
        mouse(view->viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 6; ++step) {
            mouse(view->viewport(), QEvent::MouseMove, start + (end - start) * step / 6,
                  Qt::NoButton, Qt::LeftButton);
        }
        mouse(view->viewport(), QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        QTRY_VERIFY(tab.text() != original);
        QCOMPARE(stack->count(), 1);
        QVERIFY(tab.text().contains(QStringLiteral("# retained")));
        QVERIFY(QLineF(point()->scenePos(), before + QPointF(0.24, 0.16)).length() < 0.02);
        const QString after = tab.text();
        tab.triggerUndo();
        QTRY_COMPARE(tab.text(), original);
        tab.triggerRedo();
        QTRY_COMPARE(tab.text(), after);
    }

    void snapThenDeleteUnrelatedPoint_data()
    {
        QTest::addColumn<QString>("initialEndpoint");
        QTest::addColumn<QString>("snappedEndpoint");
        QTest::addColumn<double>("zoom");
        QTest::newRow("settled-selection") << QStringLiteral("90") << QStringLiteral("100") << 1.0;
        QTest::newRow("close-gap-zoomed") << QStringLiteral("99.85") << QStringLiteral("100.00") << 40.0;
        QTest::newRow("close-gap-10000-percent") << QStringLiteral("99.94") << QStringLiteral("100.00") << 100.0;
        QTest::newRow("close-gap-20000-percent") << QStringLiteral("99.97") << QStringLiteral("100.00") << 200.0;
    }

    void snapThenDeleteUnrelatedPoint()
    {
        QFETCH(QString, initialEndpoint);
        QFETCH(QString, snappedEndpoint);
        QFETCH(double, zoom);
        QVERIFY(QDir().mkpath(QStringLiteral(".verification")));
        QTemporaryDir directory(QDir::current().filePath(QStringLiteral(".verification/snap-XXXXXX")));
        QVERIFY(directory.isValid());
        const QString filePath = directory.filePath(QStringLiteral("three-walls.th2"));
        const QString original = QStringLiteral(
            "encoding utf-8\nscrap s -projection plan\n"
            "line wall -id first\n  0 0\n  %1 0\nendline\n"
            "line wall -id second\n  100 0\n  200 0\nendline\n"
            "line wall -id third\n  0 -100\n  100 -100\n  200 -100\nendline\nendscrap\n").arg(initialEndpoint);
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original.toUtf8()), original.toUtf8().size());
        file.close();

        QtFileSystem fileSystem;
        FakeSessionStore sessionStore;
        MapEditorTab tab(fileSystem, sessionStore, CommandCatalogStore());
        tab.resize(1280, 900);
        tab.show();
        QString error;
        QVERIFY2(tab.loadFile(filePath, &error), qPrintable(error));
        auto *view = tab.findChild<QGraphicsView *>();
        auto *stack = tab.findChild<QUndoStack *>();
        QVERIFY(view != nullptr);
        QVERIFY(stack != nullptr);
        QTRY_VERIFY(view->scene() != nullptr);
        tab.goToLine(3);
        QTRY_VERIFY(anchor(view->scene(), 3, 1) != nullptr);
        QCoreApplication::processEvents();
        auto *moving = anchor(view->scene(), 3, 1);
        auto *target = anchor(view->scene(), 7, 0);
        QVERIFY(moving != nullptr);
        QVERIFY(target != nullptr);
        QVERIFY(moving->isVisible());
        const QPointF targetPosition = target->scenePos();
        view->resetTransform();
        view->scale(zoom, zoom);
        QCOMPARE(view->transform().m11(), zoom);
        view->centerOn(targetPosition);
        QCoreApplication::processEvents();
        click(view, moving->scenePos());
        QCOMPARE(tab.text(), original);
        QCOMPARE(stack->count(), 0);
        const QPoint start = view->mapFromScene(moving->scenePos());
        const QPoint end = view->mapFromScene(targetPosition);
        QVERIFY(start != end);
        QVERIFY(view->viewport()->rect().contains(start));
        QVERIFY(view->viewport()->rect().contains(end));
        mouse(view->viewport(), QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 6; ++step) {
            mouse(view->viewport(), QEvent::MouseMove, start + (end - start) * step / 6,
                  Qt::NoButton, Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        mouse(view->viewport(), QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        QString expectedSnap = original;
        expectedSnap.replace(QStringLiteral("  %1 0\n").arg(initialEndpoint),
                             QStringLiteral("  %1 0\n").arg(snappedEndpoint));
        const QString sourceAfterSnap = tab.text();
        const int undoCountAfterSnap = stack->count();
        QVERIFY(QLineF(anchor(view->scene(), 3, 1)->scenePos(), targetPosition).length() < 0.001);
        QVERIFY(linePath(view->scene(), 3) != nullptr);
        QVERIFY(QLineF(linePath(view->scene(), 3)->path().currentPosition(),
                       targetPosition).length() < 0.001);
        QTest::qWait(250);
        tab.goToLine(11);
        QTRY_VERIFY(anchor(view->scene(), 11, 1) != nullptr);
        click(view, anchor(view->scene(), 11, 1)->scenePos());
        const QString sourceAfterSelection = tab.text();
        QPushButton *deleteButton = nullptr;
        for (auto *button : tab.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Delete Point")) {
                deleteButton = button;
                break;
            }
        }
        QVERIFY(deleteButton != nullptr);
        QTRY_VERIFY(deleteButton->isEnabled());
        deleteButton->click();
        QString expectedDelete = expectedSnap;
        expectedDelete.remove(QStringLiteral("  100 -100\n"));
        expectedDelete.replace(QStringLiteral("  0 -100\n"), QStringLiteral("  0.0 -100.0\n"));
        expectedDelete.replace(QStringLiteral("  200 -100\n"), QStringLiteral("  200.0 -100.0\n"));
        QTRY_VERIFY(tab.text().contains(QStringLiteral("  0.0 -100.0\n  200.0 -100.0\n")));
        QTRY_VERIFY(anchor(view->scene(), 3, 1) != nullptr);
        QCOMPARE(tab.text(), expectedDelete);
        QCOMPARE(sourceAfterSnap, expectedSnap);
        QCOMPARE(sourceAfterSelection, expectedSnap);
        QCOMPARE(undoCountAfterSnap, 1);
        QCOMPARE(stack->count(), 2);
        QVERIFY(QLineF(anchor(view->scene(), 3, 1)->scenePos(),
                       anchor(view->scene(), 7, 0)->scenePos()).length() < 0.001);
        QVERIFY(linePath(view->scene(), 3) != nullptr);
        QVERIFY(QLineF(linePath(view->scene(), 3)->path().currentPosition(),
                       anchor(view->scene(), 7, 0)->scenePos()).length() < 0.001);
        stack->undo();
        QCOMPARE(tab.text(), expectedSnap);
        stack->undo();
        QCOMPARE(tab.text(), original);
        stack->redo();
        QCOMPARE(tab.text(), expectedSnap);
        stack->redo();
        QCOMPARE(tab.text(), expectedDelete);
        QVERIFY2(tab.save(&error), qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()), expectedDelete);
        file.close();
        QVERIFY2(tab.loadFile(filePath, &error), qPrintable(error));
        QTRY_VERIFY(anchor(view->scene(), 3, 1) != nullptr);
        QCOMPARE(tab.text(), expectedDelete);
        QVERIFY(QLineF(anchor(view->scene(), 3, 1)->scenePos(),
                       anchor(view->scene(), 7, 0)->scenePos()).length() < 0.001);
    }
};

int runMapEditorEndpointSnapTransactionTest(int argc, char **argv)
{
    MapEditorEndpointSnapTransactionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorEndpointSnapTransactionTest.moc"
