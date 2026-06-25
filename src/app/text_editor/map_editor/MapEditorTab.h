#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSet>
#include <QVector>
#include <QPointer>
#include <QPersistentModelIndex>
#include <functional>
#include <memory>
#include <optional>

#include "MapEditorInspectorData.h"
#include "MapEditorInteractiveDrawLogic.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSmartAreaPlanner.h"
#include "MapEditorUndoArbitrationService.h"
#include "../../../core/CommandCatalogStore.h"
#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceValidator.h"

class QLabel;
class QFrame;
class QGraphicsScene;
class QGraphicsView;
class QLineEdit;
class QPlainTextEdit;
class QSplitter;
class QToolButton;
class QPushButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QListWidget;
class QSlider;
class QStandardItemModel;
class QTabWidget;
class QTreeView;
class QGraphicsRectItem;
class QGraphicsPixmapItem;
class QGraphicsPathItem;
class QUndoStack;
class QMainWindow;
class QGraphicsItem;
class QShortcut;
class QTimer;
class QModelIndex;
class QImage;
class QMenu;

namespace TherionStudio
{
struct TherionParsedLine;
enum class DraftGeometryKind;
enum class TextEditorSourceTransactionResult;
}

namespace TherionStudio
{
class TextEditorTab;
class DocumentFileInspector;
class DocumentInspectorPanel;
struct MapEditorCanvasEditContext;
struct MapEditorInteractiveDrawContext;
class MapEditorInspectorBackgroundController;
struct MapEditorInspectorBackgroundContext;
class MapEditorInspectorObjectController;
struct MapEditorInspectorObjectContext;
struct MapEditorObjectDetailsContext;
class MapEditorObjectDetailsEditController;
class MapEditorObjectDetailsPanelController;
class MapEditorMagnifierOverlay;
class MapEditorStylePreviewWidget;
struct MapEditorSceneLifecycleContext;
class MapEditorSceneLifecycleController;
struct MapEditorSceneRefreshContext;
class MapEditorSceneRefreshController;
struct MapEditorSelectionContext;
class IFileSystem;
class ISessionStore;
struct MapEditorViewportInputContext;

class MapEditorTab final : public QWidget
{
    Q_OBJECT

public:
    enum class WorkspaceMode
    {
        Visual,
        Raw
    };
    enum class InteractiveDrawMode
    {
        None,
        Point,
        Line,
        Freehand,
        Area,
        SmartArea
    };

    explicit MapEditorTab(IFileSystem &fileSystem,
                          ISessionStore &sessionStore,
                          CommandCatalogStore catalogStore,
                          QWidget *parent = nullptr);
    ~MapEditorTab() override;

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    void initializeNewDocument(const QString &suggestedFileName, const QString &contents);
    bool save(QString *errorMessage = nullptr);
    bool saveAs(const QString &filePath, QString *errorMessage = nullptr);
    void setProjectRootPath(const QString &projectRootPath);
    void showFindBar(bool replaceMode = false);
    void showFindBarWithText(const QString &findText,
                             bool replaceMode = false,
                             bool wholeWord = false,
                             bool matchCase = false);
    void hideFindBar();
    void goToLine(int lineNumber);
    QString filePath() const;
    QString displayName() const;
    bool isDirty() const;
    int currentLineNumber() const;
    QString text() const;
    QString statusPathText() const;
    QString statusEncodingText() const;
    QString statusModeText() const;
    QString statusHintText() const;
    int zoomPercent() const;
    MapEditorUndoOwner nextUndoOwner() const;
    MapEditorUndoOwner nextRedoOwner() const;
    bool isMagnifierEnabled() const;
    bool hasRightPanel() const;
    bool isRightPanelCollapsed() const;
    QString rightPanelLabel() const;
    bool hasContextHelpPanel() const;
    bool isContextHelpPanelCollapsed() const;
    bool canUndo() const;
    bool canRedo() const;
    InteractiveDrawMode interactiveDrawMode() const;
    bool canCompleteDraftAction() const;
    TherionSourceValidationResult validateDocument() const;
    void setProjectValidationDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics);
    bool applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes);
    void triggerUndo();
    void triggerRedo();
    void triggerZoomIn();
    void triggerZoomOut();
    void triggerFit();
    void triggerFitWithBackground();
    void triggerSelectMode();
    void triggerInsertScrap();
    void triggerCompleteDraft();
    void triggerAddPoint();
    void triggerAddLine();
    void triggerAddFreehandLine();
    void triggerAddArea();
    void triggerSmartArea();
#ifdef THERION_STUDIO_TESTING
    bool testBeginLineExtensionFromSelection(int lineNumber, int sourceVertexIndex, bool prepend)
    {
        return beginLineExtensionFromSelection(lineNumber, sourceVertexIndex, prepend);
    }
    void testAppendInteractiveDrawLineVertex(const MapEditorInteractiveLineDraftVertex &vertex)
    {
        interactiveDrawState_.lineVertices_.append(vertex);
    }
    bool testCommitLineExtensionSession()
    {
        return commitLineExtensionSession();
    }
    bool testLineExtensionActive() const
    {
        return interactiveDrawState_.lineExtensionActive_;
    }
    InteractiveDrawMode testInteractiveDrawMode() const
    {
        return interactiveDrawState_.mode_;
    }
    int testInteractiveDrawLineVertexCount() const
    {
        return interactiveDrawState_.lineVertices_.size();
    }
    QPointF testScenePointFromSourcePosition(const QPointF &sourcePosition) const
    {
        return scenePointFromSourcePosition(sourcePosition);
    }
    bool testHasMapSceneItemForLine(int lineNumber) const
    {
        return mapItemsByLine_.contains(lineNumber);
    }
#endif
    void setMagnifierEnabled(bool enabled);
    void setRightPanelCollapsed(bool collapsed);
    void setContextHelpPanelCollapsed(bool collapsed);
    bool isInsertModeActive() const;
    bool isMapPaneDetached() const;
    QString mapPaneWindowActionText() const;
    QString mapPaneWindowActionToolTip() const;
    WorkspaceMode workspaceMode() const;
    void setInlineWorkspaceModeSelectorVisible(bool visible);
    int backgroundLayerCount() const;
    QString backgroundLayerLabel(int index) const;
    bool isBackgroundLayerVisible(int index) const;
    qreal backgroundLayerOpacity(int index) const;
    qreal backgroundLayerGamma(int index) const;
    qreal backgroundLayerXScale(int index) const;
    qreal backgroundLayerYScale(int index) const;
    qreal backgroundLayerRotationDeg(int index) const;
    bool backgroundLayerSupportsGamma(int index) const;
    bool backgroundLayerSupportsTransformEditing(int index) const;
    bool backgroundLayerSupportsPositionEditing(int index) const;
    QPointF backgroundLayerPosition(int index) const;
    QRectF backgroundLayerSceneBounds(int index) const;
    // Pixel size of the displayed pixmap for a raster layer. Reflects the full
    // source resolution (bounded by the display cap), independent of zoom.
    QSize backgroundLayerSourcePixelSize(int index) const;
    int selectedBackgroundLayerIndex() const;
    void setSelectedBackgroundLayerIndex(int index);
    void browseAndAddBackgroundImages();
#ifdef THERION_STUDIO_TESTING
    bool addSvgBackgroundImageForTest(const QString &imagePath) { return addSvgBackgroundImage(imagePath); }
    bool backgroundLayerPaintsVisiblePixelsForTest(int index) const { return backgroundLayerPaintsVisiblePixels(index); }
#endif
    void removeSelectedBackgroundLayer();
    void moveSelectedBackgroundLayerUp();
    void moveSelectedBackgroundLayerDown();
    void toggleSelectedBackgroundLayerVisibility();
    void setSelectedBackgroundLayerOpacity(qreal opacity);
    void resetSelectedBackgroundLayerOpacity();
    void setSelectedBackgroundLayerGamma(qreal gamma);
    void resetSelectedBackgroundLayerGamma();
    void setSelectedBackgroundLayerPosition(const QPointF &position);
    void setSelectedBackgroundLayerXScale(qreal scale);
    void setSelectedBackgroundLayerYScale(qreal scale);
    void setSelectedBackgroundLayerRotationDeg(qreal rotationDeg);
    void beginSetSelectedBackgroundLayerPivot();
    void resetSelectedBackgroundLayerPivot();
    void nudgeSelectedBackgroundLayer(const QPointF &delta);

public slots:
    void setWorkspaceMode(WorkspaceMode mode);
    void toggleMapPaneWindow();

signals:
    void titleChanged();
    void dirtyStateChanged(bool dirty);
    void currentLineChanged(int lineNumber);
    void documentTextChanged();
    void backgroundLayersChanged();
    void backgroundLayerPropertiesChanged();
    void modeStatusChanged();
    void statusHintChanged();
    void workspaceModeChanged(TherionStudio::MapEditorTab::WorkspaceMode mode);
    void mapPaneDetachStateChanged(bool detached);
    void zoomStatusChanged(int zoomPercent);
    void commandSurfaceStateChanged();
    void openDedicatedWindowRequested(TherionStudio::MapEditorTab *tab);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void handleTextEditorCurrentLineChanged(int lineNumber);
    void handleTextEditorCursorPositionChanged(int lineNumber, int columnNumber);
    void handleMapSceneSelectionChanged();
    void handleAddPointTriggered();
    void handleAddLineTriggered();
    void handleAddFreehandLineTriggered();
    void handleAddAreaTriggered();
    void handleSmartAreaTriggered();
    void handleSelectModeTriggered();
    void handleInsertScrapTriggered();
    void handleCompleteDraftTriggered();
    void handleUndoTriggered();
    void handleRedoTriggered();
    void handleZoomInTriggered();
    void handleZoomOutTriggered();
    void handleFitTriggered();
    void handleFitWithBackgroundTriggered();
    void updateCommandSurfaceState();
    void handleDocumentTextChangedForUndoOwner();
    void handleMapUndoStackIndexChanged(int index);
    void resetUndoOwnerState();

private:
    struct DetachedPaneState
    {
        QPointer<QMainWindow> window_;
        bool detached_ = false;
        bool inlineWorkspaceModeSelectorVisible_ = true;
        bool reattaching_ = false;
    };

    struct SelectionSyncState
    {
        bool textNavigationInProgress_ = false;
        int lastCursorSyncedLine_ = -1;
        int lastCursorSyncedColumn_ = -1;
        bool pendingClickSelection_ = false;
        QPointF pendingClickScenePosition_;
        QElapsedTimer pendingClickElapsed_;
        int pendingClickLineNumber_ = 0;
        int pendingClickSourceVertexIndex_ = -1;
        QString pendingClickGeometryKind_;
        int pendingNavigationLineNumber_ = 0;
        int sceneRefreshSelectionLineNumber_ = 0;
        int sceneRefreshSelectionVertexIndex_ = -1;
        QString sceneRefreshSelectionGeometryKind_;
    };

    struct InteractiveDrawState
    {
        InteractiveDrawMode mode_ = InteractiveDrawMode::None;
        InspectorObjectQuickFields pendingInsertFields_;
        QHash<QString, QVector<InspectorObjectQuickFields>> recentPendingInsertFieldsByCommand_;
        QString pendingTargetScrapIdentifier_;
        QString lastPendingStationName_;
        bool pendingInsertFieldsVisible_ = false;
        bool nextLinePointSmooth_ = true;
        bool nextLinePointIncomingControl_ = true;
        bool nextLinePointOutgoingControl_ = true;
        QString currentLinePointSegmentSubtype_;
        bool nextLinePointOrientationEnabled_ = false;
        qreal nextLinePointOrientationDegrees_ = 0.0;
        bool nextLinePointLeftSizeEnabled_ = false;
        qreal nextLinePointLeftSize_ = 40.0;
        bool lineExtensionActive_ = false;
        int lineExtensionLineNumber_ = 0;
        bool lineExtensionPrepend_ = false;
        QVector<QPointF> sourceVertices_;
        QVector<QPointF> sceneVertices_;
        QVector<MapEditorInteractiveLineDraftVertex> lineVertices_;
        bool strokeActive_ = false;
        bool anchorPressActive_ = false;
        QPointF anchorPressScenePoint_;
        bool anchorDragActive_ = false;
        QPointF anchorDragScenePoint_;
        bool controlDragActive_ = false;
        MapEditorInteractiveLineControlHandleRef controlDragHandle_;
        bool hoverActive_ = false;
        QPointF hoverScenePoint_;
        bool hoverSnapActive_ = false;
        QPointF hoverSnapScenePoint_;
        QVector<QPointF> hoverSnapGuideScenePoints_;
        QGraphicsPathItem *previewPath_ = nullptr;
        QVector<QGraphicsItem *> previewMarkers_;
        bool smartAreaPreviewActive_ = false;
        QVector<MapEditorSmartAreaCandidate> smartAreaCandidates_;
        MapEditorSmartAreaCandidate smartAreaCandidate_;
        int smartAreaCandidateIndex_ = 0;
    };

    struct ObjectSelectionState
    {
        int selectedObjectLineNumber_ = 0;
        int selectedObjectVertexIndex_ = -1;
        int selectedLineSegmentStartVertexIndex_ = -1;
        int selectedLineSegmentEndVertexIndex_ = -1;
        QString selectedObjectKind_;
        std::optional<QPointF> selectedObjectCoordinate_;
    };

    struct ObjectDetailsUiState
    {
        QLabel *objectDetailsSelectionLabel_ = nullptr;
        QWidget *objectDetailsEmptySelectionSection_ = nullptr;
        QWidget *objectSelectionSection_ = nullptr;
        QLabel *objectSelectionTitleLabel_ = nullptr;
        QWidget *vertexSelectionSection_ = nullptr;
        QLabel *vertexSelectionTitleLabel_ = nullptr;
        QWidget *linePointActionsSection_ = nullptr;
        QWidget *geometrySelectionSection_ = nullptr;
        QWidget *advancedSelectionSection_ = nullptr;
        QPushButton *objectDeleteButton_ = nullptr;
        QLabel *objectAreaReferenceLabel_ = nullptr;
        QWidget *objectQuickFieldsEditor_ = nullptr;
        QLabel *objectQuickIdentifierLabel_ = nullptr;
        QLabel *objectQuickNameLabel_ = nullptr;
        QLabel *objectQuickTextLabel_ = nullptr;
        QLabel *objectQuickValueLabel_ = nullptr;
        QLabel *objectQuickProjectionLabel_ = nullptr;
        QLabel *objectQuickTypeLabel_ = nullptr;
        QLabel *objectQuickSubtypeLabel_ = nullptr;
        QLabel *objectQuickRecentLabel_ = nullptr;
        QLabel *objectQuickTargetScrapLabel_ = nullptr;
        QLabel *objectStylePreviewLabel_ = nullptr;
        QComboBox *objectQuickTypeCombo_ = nullptr;
        QComboBox *objectQuickSubtypeCombo_ = nullptr;
        QComboBox *objectQuickProjectionCombo_ = nullptr;
        QComboBox *objectQuickTargetScrapCombo_ = nullptr;
        QLineEdit *objectQuickIdentifierEdit_ = nullptr;
        QWidget *objectQuickNameEditor_ = nullptr;
        QWidget *objectQuickTextEditor_ = nullptr;
        QWidget *objectQuickValueEditor_ = nullptr;
        QWidget *objectQuickRecentEditor_ = nullptr;
        QVector<QPushButton *> objectQuickRecentButtons_;
        QLineEdit *objectQuickNameEdit_ = nullptr;
        QLineEdit *objectQuickTextEdit_ = nullptr;
        QLineEdit *objectQuickValueEdit_ = nullptr;
        MapEditorStylePreviewWidget *objectStylePreview_ = nullptr;
        QString objectQuickCommandKind_;
        QWidget *vertexActionsEditor_ = nullptr;
        QPushButton *vertexInsertBeforeButton_ = nullptr;
        QPushButton *vertexInsertAfterButton_ = nullptr;
        QPushButton *vertexDeleteButton_ = nullptr;
        QPushButton *vertexSplitButton_ = nullptr;
        QCheckBox *linePointPreviousControlCheck_ = nullptr;
        QCheckBox *linePointSmoothCheck_ = nullptr;
        QCheckBox *linePointNextControlCheck_ = nullptr;
        QLabel *objectDetailsMetadataLabel_ = nullptr;
        QWidget *lineOptionsEditor_ = nullptr;
        QWidget *scrapOptionsEditor_ = nullptr;
        QLabel *scrapProjectionLabel_ = nullptr;
        QWidget *scrapProjectionEditor_ = nullptr;
        QWidget *objectQuickProjectionEditor_ = nullptr;
        QCheckBox *lineClosedCheck_ = nullptr;
        QCheckBox *lineReversedCheck_ = nullptr;
        QCheckBox *objectClipDisabledCheck_ = nullptr;
        QWidget *pointAlignEditor_ = nullptr;
        QLabel *pointAlignLabel_ = nullptr;
        QComboBox *pointAlignCombo_ = nullptr;
        QWidget *scrapScaleEditor_ = nullptr;
        QDoubleSpinBox *scrapScaleSourceX1Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleSourceY1Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleSourceX2Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleSourceY2Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleRealX1Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleRealY1Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleRealX2Spin_ = nullptr;
        QDoubleSpinBox *scrapScaleRealY2Spin_ = nullptr;
        QComboBox *scrapScaleUnitCombo_ = nullptr;
        QPushButton *scrapScaleUseBoundsButton_ = nullptr;
        QPushButton *scrapScaleApplyButton_ = nullptr;
        QWidget *objectOrientationEditor_ = nullptr;
        QCheckBox *objectOrientationEnabledCheck_ = nullptr;
        QDoubleSpinBox *objectOrientationSpin_ = nullptr;
        QCheckBox *linePointLeftSizeEnabledCheck_ = nullptr;
        QDoubleSpinBox *linePointLeftSizeSpin_ = nullptr;
        QLabel *linePointSegmentSubtypeLabel_ = nullptr;
        QComboBox *linePointSegmentSubtypeCombo_ = nullptr;
        QCheckBox *linePointAltitudeAutoCheck_ = nullptr;
        QWidget *linePointFlagsEditor_ = nullptr;
        QPlainTextEdit *linePointFlagsEdit_ = nullptr;
        bool linePointFlagsDirty_ = false;
        QWidget *objectConfigureButtonRow_ = nullptr;
        QPushButton *objectConfigureButton_ = nullptr;
        bool updatingObjectDetailsUi_ = false;
    };

    void initializeWorkspace();
    void buildUi();
    void buildInspectorPanelUi();
    void buildInspectorBackgroundTab(DocumentInspectorPanel *inspectorPanel);
    void buildMapScene();
    MapEditorViewportInputContext viewportInputContext();
    void refreshMapScene();
    void refreshMapScenePreservingUndoStack();
    void flushPendingMapSceneRefreshAfterCommand();
    void applyPendingNavigationSelection(bool consume);
    void scheduleSourceDrivenMapRefresh();
    void applySourceDrivenMapRefresh();
    MapEditorSceneRefreshContext sceneRefreshContext();
    void clearMapScene();
    MapEditorSelectionContext selectionContext();
    MapEditorCanvasEditContext canvasEditContext();
    void clearDraftGeometryItems();
    void clearBackgroundImageItems();
    void restoreDraftGeometryItems();
    void restoreBackgroundImageItems();
    void selectMapLine(int lineNumber, bool centerOnSelection = true);
    void selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection = true);
    void setInteractiveDrawMode(InteractiveDrawMode mode);
    MapEditorInteractiveDrawContext interactiveDrawContext();
    bool handleInteractiveDrawClick(const QPointF &scenePosition);
    bool commitInteractiveDrawSession(bool closeLineDraft = false);
    void clearInteractiveDrawSession(bool clearMode);
    void updateInteractiveDrawPreview();
    bool hasUndoableInteractiveDrawStep() const;
    bool undoInteractiveDrawStep();
    QVector<TherionParsedLine> parsedLinesForCurrentDocument() const;
    QRectF mapSourceBoundsForCurrentDocument() const;
    std::optional<QRectF> initialAreaAdjustRectForDraftInsertion() const;
    QRectF sourceBoundsForInteractiveDraft() const;
    QPointF sourcePointFromScenePosition(const QPointF &scenePosition) const;
    bool hasCompletableInteractiveDrawSession() const;
    bool commitInteractiveDrawVertices(const QString &geometryKind,
                                       const QVector<QPointF> &vertices,
                                       const QString &successLabel);
    bool previewSmartAreaAt(const QPointF &scenePosition);
    bool hasSmartAreaPreview() const;
    bool cycleSmartAreaCandidate(int delta);
    bool commitSmartAreaPreview();
    void updateSmartAreaPreviewPath();
    bool beginLineExtensionFromSelection(int lineNumber, int sourceVertexIndex, bool prepend);
    bool commitLineExtensionSession();
    QPointF scenePointFromSourcePosition(const QPointF &sourcePosition) const;
    bool cancelInteractiveDrawingToSelectMode();
    QStringList lineCoordinateRowsForInteractiveDraft() const;
    QStringList areaCoordinateRowsForInteractiveDraft() const;
    void captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                      const std::optional<QPointF> &dragScenePoint);
    std::optional<MapEditorInteractiveLineControlHandleRef> interactiveLineControlAt(const QPointF &scenePosition,
                                                                                     qreal sceneRadius) const;
    bool setInteractiveLineControlScenePoint(const MapEditorInteractiveLineControlHandleRef &handle,
                                             const QPointF &scenePoint);
    void fitMapToView(bool includeBackgroundImages = false);
    void fitMapToViewAfterViewportResize(bool includeBackgroundImages = false);
    void syncZoomFactorFromView();
    void applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition);
    void refreshToolbarSummary();
    void adjustMapZoom(qreal factor);
    void updateMagnifierOverlayGeometry();
    void updateMagnifierOverlayFromViewportPosition(const QPoint &viewportPosition, bool active);
    void scheduleMagnifierOverlayUpdateFromViewportPosition(const QPoint &viewportPosition);
    void refreshVisibleMagnifierOverlay();
    void hideMagnifierOverlay();
    QString magnifierCoordinateTextForScenePosition(const QPointF &scenePosition) const;
    QRectF mapGeometryFitBounds() const;
    QRectF mapBackgroundFitBounds() const;
    QRectF mapPreviewBounds() const;
    MapEditorSceneLifecycleContext sceneLifecycleContext() const;
    void addBackgroundImage(const QString &imagePath, bool writeXtherionMetadata = false);
    void addBackgroundImageAsync(const QString &imagePath, bool writeXtherionMetadata = false);
    bool addSvgBackgroundImage(const QString &imagePath);
    QGraphicsPixmapItem *addBackgroundImagePlaceholder(const QString &imagePath);
    bool addBackgroundImageFromSourceImage(const QString &imagePath,
                                           const QImage &image,
                                           bool writeXtherionMetadata = false);
    void loadBackgroundImageSourceAsync(QGraphicsPixmapItem *item);
    void refreshBackgroundLayerControls();
    void refreshBackgroundLayerPropertyControls();
    void applyBackgroundLayerStackingOrder();
    void saveBackgroundLayersToSession() const;
    void loadBackgroundLayersFromSession();
    void loadBackgroundLayersFromDocumentMetadata();
    void syncAutoBackgroundLayersFromCurrentDocument();
    void reprojectMetadataBackgroundLayersForCurrentDocument();
    QRectF xtherionAutoAreaAdjustRect() const;
    void syncBackgroundLayerXtherionMetadata(QGraphicsPixmapItem *item, const QString &label, bool preserveExistingPlacement = false);
    bool syncBackgroundLayerXtherionGammaMetadata(QGraphicsPixmapItem *item, const QString &label);
    void syncBackgroundLayerMapiahMetadata(QGraphicsPixmapItem *item,
                                           const QString &label,
                                           bool preserveExistingPlacement = false);
    void removeBackgroundLayerXtherionMetadata(const QString &layerPath, const QString &label);
    void invalidateBackgroundLayerRasterJobs(QGraphicsPixmapItem *item);
    void invalidateBackgroundRasterJobs();
    QGraphicsPixmapItem *backgroundLayerItemAt(int index) const;
    QGraphicsPixmapItem *selectedBackgroundLayerItem() const;
    bool backgroundLayerPaintsVisiblePixels(int index) const;
    qreal backgroundLayerGammaValue(const QGraphicsPixmapItem *item) const;
    qreal backgroundLayerXScaleValue(const QGraphicsPixmapItem *item) const;
    qreal backgroundLayerYScaleValue(const QGraphicsPixmapItem *item) const;
    qreal backgroundLayerRotationDegValue(const QGraphicsPixmapItem *item) const;
    QPointF backgroundLayerBaseModelPosition(QGraphicsPixmapItem *item) const;
    QPointF backgroundLayerPivotScenePosition(QGraphicsPixmapItem *item) const;
    void refreshBackgroundPivotMarkerVisibility();
    void setSelectedBackgroundLayerPivotAtScenePosition(const QPointF &scenePosition);
    void cancelBackgroundPivotPickMode();
    bool handleBackgroundPivotPickViewportEvent(QEvent *event);
    void ensureBackgroundPivotMarker();
    void showBackgroundPivotMarkerAtScenePosition(const QPointF &scenePosition);
    void hideBackgroundPivotMarker();
    void applyBackgroundLayerTransform(QGraphicsPixmapItem *item);
    void applyBackgroundLayerGamma(QGraphicsPixmapItem *item, qreal gamma);
    void setSelectedBackgroundLayerIndexInternal(int index);
    QVector<QPointF> sourceVerticesForDraft(const QGraphicsRectItem *item) const;
    QPointF previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const;
    QString canonicalDocumentSessionKey() const;
    void recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition);
    void recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible);
    void recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint);
    void recordLineAreaVertexMove(int lineNumber,
                                  const QString &kind,
                                  int vertexIndex,
                                  const QPointF &oldPoint,
                                  const QPointF &newPoint);
    void recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees);
    void recordLinePointLeftHandleChange(int lineNumber,
                                         int sourceVertexIndex,
                                         qreal orientationDegrees,
                                         qreal leftSize);
    void restorePointSelection(int lineNumber);
    void restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex);
    TextEditorSourceTransactionResult recordSourceTextSnapshot(const QString &label,
                                                               const QString &beforeText,
                                                               const QString &afterText,
                                                               int insertedLineNumber);
    TextEditorSourceTransactionResult applySourceTextChangeWithSnapshot(
        const QString &label,
        const QString &beforeText,
        const QString &afterText,
        int insertedLineNumber,
        std::function<void()> selectionRestoreHook = {});
    bool insertLineVertexFromSelection(bool before);
    bool insertLineVertexAtSelectionCoordinate();
    bool splitLineAtSelection();
    bool removeLineVertexFromSelection();
    bool toggleLineVertexSmoothFromSelection();
    bool setLineVertexSmoothForSelection(bool smooth);
    bool setLineVertexControlHandleForSelection(bool incoming, bool enabled);
    void recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition);
    void recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible);
    void recordDraftCompletion(QGraphicsRectItem *item,
                               const QString &label,
                               const QString &beforeText,
                               const QString &afterText,
                               int insertedLineNumber);
    QGraphicsRectItem *selectedDraftGeometryItem() const;
    QGraphicsRectItem *createDraftGeometryItem(DraftGeometryKind kind);
    void addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position);
    void removeDraftGeometryItem(QGraphicsRectItem *item);
    void updateHelpPanel();
    void updateWorkspaceVisibility();
    void updateGeometrySelectionPresentation();
    void updateMapInspectorLeftEdgeGeometry();
    bool isMapEditorEventReceiver(QObject *receiver) const;
    bool handleMapEditorEscapeKeyEvent(QObject *receiver, QEvent *event);
    bool handleMapEditorDrawingShortcutKeyEvent(QObject *receiver, QEvent *event);
    void syncMapSelectionFromTextCursor(int lineNumber, int columnNumber);
    void detachMapPaneToWindow();
    void reattachMapPaneFromWindow();
    void focusDetachedMapPaneWindow();
    void refreshTitle();
    void refreshStatus();
    QString displayPath() const;
    void handleApplicationAppearanceChanged();
    void refreshWorkspaceModeUi();
    MapEditorInspectorObjectContext inspectorObjectContext();
    void rebuildInspectorObjectsTree();
    QModelIndex findInspectorObjectIndexForLine(int lineNumber) const;
    void syncInspectorObjectSelectionToLine(int lineNumber);
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection);
    void syncInspectorObjectSelectionToLineExpanding(int lineNumber, bool scrollToSelection);
    void syncInspectorObjectSelectionToLineForNavigation(int lineNumber, bool scrollToSelection = true);
    void setInspectorObjectCurrentIndex(const QModelIndex &index);
    void configureInspectorObjectTreeColumns();
    void clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers = {});
    void handleInspectorObjectSelectionChanged(const QModelIndex &current);
    void handleInspectorObjectClicked(const QModelIndex &index);
    void setObjectsInspectorAutoCollapseExpandScrapsEnabled(bool enabled);
    std::optional<bool> handleInspectorObjectViewportEvent(QEvent *event);
    void applyInspectorObjectVisibility();
    void configureInspectorBackgroundLayerTreeColumns();
    void handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current);
    void handleInspectorBackgroundLayerClicked(const QModelIndex &index);
    MapEditorInspectorBackgroundContext inspectorBackgroundContext();
    void refreshInspectorBackgroundPanel();
    void refreshInspectorBackgroundSelectionControls();
    MapEditorObjectDetailsContext objectDetailsContext();
    void refreshObjectDetailsPanel();
    void beginPendingInsertObject(const QString &commandKind);
    void clearPendingInsertObject();
    std::optional<InspectorObjectQuickFields> pendingInsertQuickFields() const;
    void setPendingInsertQuickFields(const InspectorObjectQuickFields &fields);
    QVector<InspectorObjectQuickFields> recentPendingInsertQuickFields(const QString &commandKind) const;
    bool pendingInsertLinePointAvailable() const;
    bool pendingInsertLinePointSmooth() const;
    void setPendingInsertLinePointSmooth(bool smooth);
    bool pendingInsertLinePointIncomingControl() const;
    bool pendingInsertLinePointOutgoingControl() const;
    void setPendingInsertLinePointControl(bool incoming, bool enabled);
    QString pendingInsertLinePointSegmentSubtype() const;
    void setPendingInsertLinePointSegmentSubtype(const QString &subtype);
    bool pendingInsertLinePointOrientationEnabled() const;
    qreal pendingInsertLinePointOrientationDegrees() const;
    void setPendingInsertLinePointOrientation(bool enabled, qreal degrees);
    bool pendingInsertLinePointLeftSizeEnabled() const;
    qreal pendingInsertLinePointLeftSize() const;
    void setPendingInsertLinePointLeftSize(bool enabled, qreal leftSize);
    std::optional<InspectorScrapContext> pendingInsertTargetScrapContext() const;
    void setPendingInsertTargetScrapIdentifier(const QString &identifier);
    TherionDraftObjectOptions pendingDraftObjectOptions(const QString &commandKind) const;
    void recordCommittedPendingDraftObjectOptions(const QString &commandKind,
                                                  const TherionDraftObjectOptions &options);
    void applyRecentPendingInsertQuickFields(int index);
    void restoreRecentPendingInsertQuickFieldsFromSession();
    void persistRecentPendingInsertQuickFieldsToSession() const;
    QString pendingScrapPreferredName() const;
    QString pendingScrapOptions(const QString &scaleOption) const;
    void activateSelectionInspector();
    void handleObjectOrientationValueChanged(double value);
    void handleLinePointLeftSizeValueChanged(double value);
    void deleteSelectedObjectFromSelection();
    void applyObjectQuickFieldEdits();
    void applyScrapProjectionEdit();
    void updateObjectQuickSubtypeChoices();
    void insertVertexBeforeFromSelectionPanel();
    void insertVertexAfterFromSelectionPanel();
    void splitLineFromSelectionPanel();
    void deleteVertexFromSelectionPanel();
    void handleLinePointPreviousControlToggled(bool checked);
    void handleLinePointSmoothToggled(bool checked);
    void handleLinePointNextControlToggled(bool checked);
    void handleLinePointSegmentSubtypeChanged();
    void handleLinePointAltitudeAutoToggled(bool checked);
    void applyLinePointFlagsEdits();
    void populateScrapScaleFromSourceBounds();
    void applyScrapScaleEdits();
    void handleConfigureObjectSettingsTriggered();
    void handleObjectOrientationEnabledToggled(bool checked);
    void handleLinePointLeftSizeEnabledToggled(bool checked);
    void handleLineClosedToggled(bool checked);
    void handleLineReversedToggled(bool checked);
    void handleObjectClipDisabledToggled(bool checked);
    void handlePointAlignChanged();
    void showMapSelectionContextMenu(const QPoint &globalPosition);

    QWidget *workspaceModeRow_ = nullptr;
    QPushButton *visualModeButton_ = nullptr;
    QPushButton *rawModeButton_ = nullptr;
    TextEditorTab *textEditor_ = nullptr;
    QGraphicsView *mapView_ = nullptr;
    MapEditorMagnifierOverlay *mapMagnifierOverlay_ = nullptr;
    QPoint magnifierPendingViewportPosition_;
    QPoint magnifierLastViewportPosition_;
    QElapsedTimer magnifierLastUpdateElapsed_;
    bool magnifierUpdatePending_ = false;
    bool magnifierHasViewportPosition_ = false;
    bool magnifierThrottleActive_ = false;
    bool magnifierEnabled_ = true;
    bool mapInspectorCollapsed_ = false;
    int mapInspectorPanelExtent_ = 320;
    QGraphicsScene *mapScene_ = nullptr;
    QWidget *mapPaneContainer_ = nullptr;
    QFrame *mapPaneTopSeparator_ = nullptr;
    QSplitter *mapDetailsSplitter_ = nullptr;
    QFrame *objectDetailsPanel_ = nullptr;
    DocumentFileInspector *mapFileInspector_ = nullptr;
    QTabWidget *mapInspectorTabs_ = nullptr;
    int mapInspectorBackgroundTabIndex_ = -1;
    QFrame *mapInspectorLeftEdge_ = nullptr;
    QCheckBox *mapObjectsAutoCollapseExpandScrapsCheck_ = nullptr;
    QTreeView *mapObjectsTree_ = nullptr;
    QStandardItemModel *mapObjectsModel_ = nullptr;
    QTreeView *mapBackgroundLayersTree_ = nullptr;
    QStandardItemModel *mapBackgroundLayersModel_ = nullptr;
    QToolButton *mapBackgroundAddButton_ = nullptr;
    QPushButton *mapBackgroundMoveUpButton_ = nullptr;
    QPushButton *mapBackgroundMoveDownButton_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosXSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundPosYSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundScaleXSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundScaleYSpin_ = nullptr;
    QDoubleSpinBox *mapBackgroundRotationSpin_ = nullptr;
    QCheckBox *mapBackgroundLockScaleCheck_ = nullptr;
    QPushButton *mapBackgroundSetPivotButton_ = nullptr;
    QPushButton *mapBackgroundResetPivotButton_ = nullptr;
    QSlider *mapBackgroundOpacitySlider_ = nullptr;
    QSlider *mapBackgroundGammaSlider_ = nullptr;
    QPushButton *mapBackgroundOpacityResetButton_ = nullptr;
    QPushButton *mapBackgroundGammaResetButton_ = nullptr;
    bool updatingMapInspectorBackgroundUi_ = false;
    bool backgroundPivotPickActive_ = false;
    QGraphicsPathItem *backgroundPivotMarker_ = nullptr;
    bool updatingMapInspectorObjectSelection_ = false;
    QSet<int> hiddenInspectorObjectLines_;
    QPersistentModelIndex inspectorObjectPressedNameIndex_;
    bool inspectorObjectPressedWasSelected_ = false;
    QPointer<QWidget> inspectorObjectDropIndicator_;
    int lastInspectorClickedObjectLineNumber_ = 0;
    QSet<int> suppressedInspectorAutoReselectLineNumbers_;
    bool objectsInspectorAutoCollapseExpandScrapsEnabled_ = false;
    ObjectDetailsUiState objectDetailsUiState_;
    ObjectSelectionState objectSelectionState_;
    QSplitter *splitter_ = nullptr;
    QShortcut *cancelDrawShortcut_ = nullptr;
    QShortcut *commitDrawShortcut_ = nullptr;
    QShortcut *previousSmartAreaCandidateShortcut_ = nullptr;
    QShortcut *nextSmartAreaCandidateShortcut_ = nullptr;
    QHash<int, QGraphicsItem *> mapItemsByLine_;
    QVector<QGraphicsRectItem *> draftGeometryItems_;
    QVector<QGraphicsPixmapItem *> backgroundImageItems_;
    quint64 backgroundRasterJobGeneration_ = 0;
    QUndoStack *undoStack_ = nullptr;
    int nextDraftGeometryId_ = 1;
    WorkspaceMode workspaceMode_ = WorkspaceMode::Visual;
    QVector<TherionSourceDiagnostic> projectValidationDiagnostics_;
    QString projectRootPath_;
    QString toolbarStatusNote_;
    bool updatingSelection_ = false;
    bool updatingBackgroundLayerControls_ = false;
    bool autoFitEnabled_ = true;
    qreal zoomFactor_ = 1.0;
    bool fitBackgroundRequested_ = false;
    mutable bool cachedMapSourceBoundsValid_ = false;
    mutable int cachedMapSourceBoundsRevision_ = -1;
    mutable QRectF cachedMapSourceBounds_;
    mutable bool cachedParsedLinesValid_ = false;
    mutable int cachedParsedLinesRevision_ = -1;
    mutable QVector<TherionParsedLine> cachedParsedLines_;
    bool mapPanActive_ = false;
    bool mapPanMoved_ = false;
    QPoint mapPanStartPosition_;
    QPoint mapPanLastPosition_;
    QPointer<QMenu> mapSelectionContextMenu_;
    bool primaryPointerInteractionActive_ = false;
    bool selectModeActive_ = true;
    bool touchPanCandidate_ = false;
    bool touchPanActive_ = false;
    QPointF touchPanStartPosition_;
    QPointF touchPanLastPosition_;
    QDateTime lastTabletInteractionUtc_;
    bool nativeZoomGestureActive_ = false;
    QDateTime lastNativeZoomGestureUtc_;
    IFileSystem *fileSystem_ = nullptr;
    ISessionStore *sessionStore_ = nullptr;
    CommandCatalogStore catalogStore_;
    InspectorSymbolCatalog inspectorSymbolCatalog_;
    MapEditorOrientationApplicabilityByCommand orientationApplicabilityByCommand_;
    bool touchFriendlyControlsEnabled_ = false;
    int selectedBackgroundLayerIndex_ = -1;
    bool mapCommandApplyInProgress_ = false;
    MapEditorUndoOwnershipState undoOwnershipState_;
    bool preserveNextSourceDrivenMapRefresh_ = false;
    int preserveMapUndoForSourceRevision_ = 0;
    bool mapSceneRefreshPending_ = false;
    bool mapSceneRefreshWhenVisiblePending_ = false;
    quint64 lineVertexSelectionRestoreGeneration_ = 0;
    QTimer *sourceDrivenMapRefreshTimer_ = nullptr;
    DetachedPaneState detachedPaneState_;
    SelectionSyncState selectionSyncState_;
    InteractiveDrawState interactiveDrawState_;
};
}
