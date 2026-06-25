#include "MapEditorTab.h"

#include "../InspectorPanel.h"
#include "MapEditorMagnifierOverlay.h"
#include "MapEditorSceneThemePolicy.h"
#include "MapEditorStylePreviewWidget.h"
#include "MapEditorViewportInputController.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QListWidget>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QSvgRenderer>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <algorithm>
#include <functional>
#include <memory>

#include "../TextEditorTab.h"
#include "../../../core/CommandCatalogStore.h"
#include "../../../core/IFileSystem.h"
#include "../../../core/ISessionStore.h"

namespace TherionStudio
{
namespace
{
void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}
}

MapEditorTab::MapEditorTab(IFileSystem &fileSystem,
                           ISessionStore &sessionStore,
                           CommandCatalogStore catalogStore,
                           QWidget *parent)
    : QWidget(parent)
    , fileSystem_(&fileSystem)
    , sessionStore_(&sessionStore)
    , catalogStore_(std::move(catalogStore))
    , inspectorSymbolCatalog_(inspectorSymbolCatalogFromCommandCatalog(catalogStore_.catalogObject()))
    , orientationApplicabilityByCommand_(mapEditorOrientationApplicabilityFromCommandCatalog(catalogStore_.catalogObject()))
{
    initializeWorkspace();
}

void MapEditorTab::initializeWorkspace()
{
    undoStack_ = new QUndoStack(this);
    MapEditorUndoArbitrationService::resetOwnership(undoOwnershipState_, undoStack_->index());
    workspaceMode_ = WorkspaceMode::Visual;
    touchFriendlyControlsEnabled_ = false;
    magnifierEnabled_ = sessionStore_->therionMapMagnifierEnabled();
    objectsInspectorAutoCollapseExpandScrapsEnabled_ =
        sessionStore_->therionMapObjectsAutoCollapseExpandScrapsEnabled();
    restoreRecentPendingInsertQuickFieldsFromSession();
    sessionStore_->setTherionMapTouchFriendlyControlsEnabled(false);
    buildUi();
    connect(this, &MapEditorTab::zoomStatusChanged, this, [this](int) {
        refreshStatus();
    });
    connect(this, &MapEditorTab::modeStatusChanged, this, [this]() {
        refreshStatus();
    });
    updateWorkspaceVisibility();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
    }
#endif
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    handleApplicationAppearanceChanged();
}

MapEditorTab::~MapEditorTab()
{
    if (qApp != nullptr) {
        qApp->removeEventFilter(this);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        disconnect(styleHints, nullptr, this, nullptr);
    }
#endif

    const QList<QObject *> childObjects = findChildren<QObject *>();
    for (QObject *child : childObjects) {
        disconnect(child, nullptr, this, nullptr);
    }

    if (mapScene_ != nullptr) {
        disconnect(mapScene_, nullptr, this, nullptr);
    }
    mapScene_ = nullptr;
}

void MapEditorTab::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    workspaceModeRow_ = new QWidget(this);
    auto *workspaceModeLayout = new QHBoxLayout(workspaceModeRow_);
    workspaceModeLayout->setContentsMargins(12, 10, 12, 8);
    workspaceModeLayout->setSpacing(8);
    workspaceModeLayout->addWidget(new QLabel(tr("Mode:"), workspaceModeRow_));
    visualModeButton_ = new QPushButton(tr("Visual"), workspaceModeRow_);
    visualModeButton_->setCheckable(true);
    rawModeButton_ = new QPushButton(tr("Raw"), workspaceModeRow_);
    rawModeButton_->setCheckable(true);
    workspaceModeLayout->addWidget(visualModeButton_);
    workspaceModeLayout->addWidget(rawModeButton_);
    workspaceModeLayout->addStretch(1);

    connect(visualModeButton_, &QPushButton::clicked, this, [this]() {
        setWorkspaceMode(WorkspaceMode::Visual);
    });
    connect(rawModeButton_, &QPushButton::clicked, this, [this]() {
        setWorkspaceMode(WorkspaceMode::Raw);
    });
    workspaceModeRow_->setVisible(detachedPaneState_.inlineWorkspaceModeSelectorVisible_);
    workspaceModeRow_->setMaximumHeight(detachedPaneState_.inlineWorkspaceModeSelectorVisible_ ? QWIDGETSIZE_MAX : 0);

    toolbarStatusNote_ = tr("Ready");

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(splitter_, QStringLiteral("mapEditorWorkspaceSplitter"));

    textEditor_ = new TextEditorTab(*fileSystem_, catalogStore_, splitter_);
    textEditor_->setInlineStatusVisible(false);
    textEditor_->setModeSelectorVisible(false);
    sourceDrivenMapRefreshTimer_ = new QTimer(this);
    sourceDrivenMapRefreshTimer_->setSingleShot(true);
    sourceDrivenMapRefreshTimer_->setInterval(75);
    connect(sourceDrivenMapRefreshTimer_, &QTimer::timeout, this, &MapEditorTab::applySourceDrivenMapRefresh);
    connect(textEditor_, &TextEditorTab::titleChanged, this, &MapEditorTab::titleChanged);
    connect(textEditor_, &TextEditorTab::dirtyStateChanged, this, &MapEditorTab::dirtyStateChanged);
    connect(textEditor_, &TextEditorTab::currentLineChanged, this, &MapEditorTab::handleTextEditorCurrentLineChanged);
    connect(textEditor_, &TextEditorTab::cursorPositionChanged, this, &MapEditorTab::handleTextEditorCursorPositionChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::scheduleSourceDrivenMapRefresh);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::documentTextChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::handleDocumentTextChangedForUndoOwner);
    connect(this, &MapEditorTab::backgroundLayersChanged, this, &MapEditorTab::refreshInspectorBackgroundPanel);

    connect(undoStack_, &QUndoStack::canUndoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::canRedoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::handleMapUndoStackIndexChanged);

    mapPaneContainer_ = new QWidget(splitter_);
    mapPaneContainer_->setMinimumWidth(420);
    auto *mapPaneLayout = new QVBoxLayout(mapPaneContainer_);
    mapPaneLayout->setContentsMargins(0, 0, 0, 0);
    mapPaneLayout->setSpacing(0);

    mapPaneTopSeparator_ = new QFrame(mapPaneContainer_);
    mapPaneTopSeparator_->setObjectName(QStringLiteral("mapPaneTopSeparator"));
    mapPaneTopSeparator_->setFrameShape(QFrame::NoFrame);
    mapPaneTopSeparator_->setFixedHeight(1);
    mapPaneTopSeparator_->setStyleSheet(QStringLiteral(
        "QFrame#mapPaneTopSeparator {"
        " background-color: palette(mid);"
        " border: none;"
        "}"));

    mapDetailsSplitter_ = new QSplitter(Qt::Horizontal, mapPaneContainer_);
    mapDetailsSplitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(mapDetailsSplitter_, QStringLiteral("mapEditorDetailsSplitter"));

    mapView_ = new QGraphicsView(mapDetailsSplitter_);
    mapView_->setFrameShape(QFrame::NoFrame);
    mapView_->setObjectName(QStringLiteral("mapCanvasView"));
    mapView_->setStyleSheet(QStringLiteral(
        "QGraphicsView#mapCanvasView {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    mapView_->setDragMode(QGraphicsView::NoDrag);
    mapView_->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    mapView_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    mapView_->setCacheMode(QGraphicsView::CacheBackground);
    mapView_->setRenderHint(QPainter::Antialiasing, true);
    mapView_->setRenderHint(QPainter::SmoothPixmapTransform, true);
    mapView_->setBackgroundBrush(mapEditorCanvasViewportBackgroundColor());
    mapView_->setFocusPolicy(Qt::StrongFocus);
    mapView_->installEventFilter(this);
    if (mapView_->viewport() != nullptr) {
        mapView_->viewport()->setFocusPolicy(Qt::StrongFocus);
        mapView_->viewport()->installEventFilter(this);
        mapView_->viewport()->setMouseTracking(true);
        mapView_->viewport()->setCursor(Qt::CrossCursor);
        mapView_->viewport()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(mapView_->viewport(), &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
            if (mapView_ == nullptr || mapView_->viewport() == nullptr) {
                return;
            }
            MapEditorViewportInputController(viewportInputContext())
                .showContextMenuAtViewportPosition(position, mapView_->viewport()->mapToGlobal(position));
        });
        mapMagnifierOverlay_ = new MapEditorMagnifierOverlay(mapView_, mapView_);
        mapMagnifierOverlay_->updatePlacement();
        if (mapView_->horizontalScrollBar() != nullptr) {
            connect(mapView_->horizontalScrollBar(),
                    &QScrollBar::valueChanged,
                    this,
                    &MapEditorTab::refreshVisibleMagnifierOverlay);
        }
        if (mapView_->verticalScrollBar() != nullptr) {
            connect(mapView_->verticalScrollBar(),
                    &QScrollBar::valueChanged,
                    this,
                    &MapEditorTab::refreshVisibleMagnifierOverlay);
        }
    }

    buildMapScene();

    cancelDrawShortcut_ = new QShortcut(this);
    cancelDrawShortcut_->setKey(QKeySequence(Qt::Key_Escape));
    cancelDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cancelDrawShortcut_, &QShortcut::activated, this, [this]() {
        cancelInteractiveDrawingToSelectMode();
    });

    commitDrawShortcut_ = new QShortcut(this);
    commitDrawShortcut_->setKeys(QList<QKeySequence>{QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    commitDrawShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(commitDrawShortcut_, &QShortcut::activated, this, [this]() {
        commitInteractiveDrawSession();
    });

    previousSmartAreaCandidateShortcut_ = new QShortcut(this);
    previousSmartAreaCandidateShortcut_->setKey(QKeySequence(Qt::Key_BracketLeft));
    previousSmartAreaCandidateShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(previousSmartAreaCandidateShortcut_, &QShortcut::activated, this, [this]() {
        cycleSmartAreaCandidate(-1);
    });

    nextSmartAreaCandidateShortcut_ = new QShortcut(this);
    nextSmartAreaCandidateShortcut_->setKey(QKeySequence(Qt::Key_BracketRight));
    nextSmartAreaCandidateShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    connect(nextSmartAreaCandidateShortcut_, &QShortcut::activated, this, [this]() {
        cycleSmartAreaCandidate(1);
    });

    buildInspectorPanelUi();
    mapDetailsSplitter_->addWidget(mapView_);
    mapDetailsSplitter_->addWidget(objectDetailsPanel_);
    mapDetailsSplitter_->setCollapsible(1, true);
    mapDetailsSplitter_->setStretchFactor(0, 1);
    mapDetailsSplitter_->setStretchFactor(1, 0);
    mapDetailsSplitter_->setSizes(QList<int>{980, 320});
    mapPaneLayout->addWidget(mapPaneTopSeparator_);
    mapPaneLayout->addWidget(mapDetailsSplitter_, 1);
    splitter_->addWidget(mapPaneContainer_);
    splitter_->addWidget(textEditor_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes(QList<int>{700, 900});

    layout->addWidget(workspaceModeRow_);
    layout->addWidget(splitter_, 1);

    refreshMapScene();
    refreshWorkspaceModeUi();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshObjectDetailsPanel();
    updateCommandSurfaceState();
}

void MapEditorTab::updateMapInspectorLeftEdgeGeometry()
{
    auto *inspectorPanel = qobject_cast<InspectorPanel *>(objectDetailsPanel_);
    if (inspectorPanel == nullptr) {
        return;
    }

    inspectorPanel->updateLeftEdgeGeometry();
}

}
