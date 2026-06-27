#include "MapEditorInteractiveDrawController.h"

#include <QCoreApplication>

#include "../TextEditorTab.h"
#include "MapEditorSceneSupport.h"
#include "../../../core/TherionBackgroundMetadata.h"

#include <QBrush>
#include <QColor>
#include <QGraphicsEllipseItem>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QWidget>
#include <QScopedValueRollback>

#include <utility>

namespace TherionStudio
{
namespace
{
std::optional<QRectF> draftInitialAreaAdjustRect(const MapEditorInteractiveDrawContext &context)
{
    return context.initialAreaAdjustRectForDraftInsertion
        ? context.initialAreaAdjustRectForDraftInsertion()
        : std::optional<QRectF>{};
}

qreal previewDevicePixelRatio(const MapEditorInteractiveDrawContext &context)
{
    if (context.view != nullptr && context.view->viewport() != nullptr) {
        return qMax<qreal>(1.0, context.view->viewport()->devicePixelRatioF());
    }
    return 1.0;
}

qreal previewCosmeticWidth(const MapEditorInteractiveDrawContext &context, qreal logicalWidth)
{
    return qMax<qreal>(0.1, logicalWidth) * previewDevicePixelRatio(context);
}

TherionDraftObjectOptions draftObjectOptionsFor(const MapEditorInteractiveDrawContext &context, const QString &commandKind)
{
    return context.draftObjectOptions
        ? context.draftObjectOptions(commandKind)
        : TherionDraftObjectOptions{};
}

QString plannerSourceWithAreaAdjust(const QString &beforeText, const std::optional<QRectF> &initialAreaAdjustRect)
{
    QString plannerSource = beforeText;
    if (initialAreaAdjustRect.has_value()
        && initialAreaAdjustRect->isValid()
        && !parseTherionAreaAdjust(plannerSource).valid) {
        plannerSource = upsertTherionAreaAdjustMetadata(plannerSource, *initialAreaAdjustRect);
    }
    return plannerSource;
}

bool planPointInsert(const QString &beforeText,
                     const QPointF &sourcePoint,
                     const TherionDraftObjectOptions &objectOptions,
                     const std::optional<QRectF> &initialAreaAdjustRect,
                     QString *afterText,
                     int *insertedLineNumber,
                     QString *errorMessage)
{
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRect);
    QVector<TherionSourceTextEdit> sourceEdits;
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftGeometryEdits(plannerSource,
                                                          QStringLiteral("point"),
                                                          {sourcePoint},
                                                          &sourceEdits,
                                                          &resolvedLineNumber,
                                                          errorMessage,
                                                          objectOptions)) {
        return false;
    }

    QString resolvedAfterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&resolvedAfterText, sourceEdits, errorMessage)) {
        return false;
    }

    if (afterText != nullptr) {
        *afterText = resolvedAfterText;
    }
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool planLineInsert(const QString &beforeText,
                    const QStringList &coordinateRows,
                    const QString &lineOptions,
                    const TherionDraftObjectOptions &objectOptions,
                    const std::optional<QRectF> &initialAreaAdjustRect,
                    QString *afterText,
                    int *insertedLineNumber,
                    QString *errorMessage)
{
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRect);
    QVector<TherionSourceTextEdit> sourceEdits;
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftLineGeometryEdits(plannerSource,
                                                              coordinateRows,
                                                              &sourceEdits,
                                                              &resolvedLineNumber,
                                                              errorMessage,
                                                              lineOptions,
                                                              objectOptions)) {
        return false;
    }

    QString resolvedAfterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&resolvedAfterText, sourceEdits, errorMessage)) {
        return false;
    }

    if (afterText != nullptr) {
        *afterText = resolvedAfterText;
    }
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool planAreaInsert(const QString &beforeText,
                    const QStringList &coordinateRows,
                    const TherionDraftObjectOptions &objectOptions,
                    const std::optional<QRectF> &initialAreaAdjustRect,
                    QString *afterText,
                    int *insertedLineNumber,
                    QString *errorMessage)
{
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRect);
    QVector<TherionSourceTextEdit> sourceEdits;
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftAreaGeometryEdits(plannerSource,
                                                              coordinateRows,
                                                              &sourceEdits,
                                                              &resolvedLineNumber,
                                                              errorMessage,
                                                              objectOptions)) {
        return false;
    }

    QString resolvedAfterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&resolvedAfterText, sourceEdits, errorMessage)) {
        return false;
    }

    if (afterText != nullptr) {
        *afterText = resolvedAfterText;
    }
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

enum class SourceApplyResult
{
    Applied,
    NotApplied,
    Unavailable,
};

SourceApplyResult applyInsertWithSnapshot(const MapEditorInteractiveDrawContext &context,
                                          const QString &label,
                                          const QString &beforeText,
                                          const QString &afterText,
                                          int insertedLineNumber,
                                          std::function<void()> afterDeferredProjection = {})
{
    if (context.applySourceTextChangeWithSnapshotDeferredProjection) {
        const TextEditorSourceTransactionResult result =
            context.applySourceTextChangeWithSnapshotDeferredProjection(label,
                                                                        beforeText,
                                                                        afterText,
                                                                        insertedLineNumber,
                                                                        std::move(afterDeferredProjection));
        if (result == TextEditorSourceTransactionResult::Applied
            || (result == TextEditorSourceTransactionResult::NoChange
                && context.textEditor != nullptr
                && context.textEditor->text() == afterText)) {
            return SourceApplyResult::Applied;
        }
        if (result == TextEditorSourceTransactionResult::Unavailable) {
            return SourceApplyResult::Unavailable;
        }
        return SourceApplyResult::NotApplied;
    }

    if (!context.applySourceTextChangeWithSnapshot) {
        return SourceApplyResult::Unavailable;
    }

    const TextEditorSourceTransactionResult result =
        context.applySourceTextChangeWithSnapshot(label, beforeText, afterText, insertedLineNumber, {});
    if (result == TextEditorSourceTransactionResult::Applied
        || (result == TextEditorSourceTransactionResult::NoChange
            && context.textEditor != nullptr
            && context.textEditor->text() == afterText)) {
        return SourceApplyResult::Applied;
    }
    if (result == TextEditorSourceTransactionResult::Unavailable) {
        return SourceApplyResult::Unavailable;
    }
    return SourceApplyResult::NotApplied;
}
}

QString MapEditorInteractiveDrawController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorInteractiveDrawController", text);
}

MapEditorInteractiveDrawMode MapEditorInteractiveDrawController::mode() const
{
    return context_.drawMode ? context_.drawMode() : MapEditorInteractiveDrawMode::None;
}

void MapEditorInteractiveDrawController::setMode(MapEditorInteractiveDrawMode mode)
{
    if (context_.setDrawMode) {
        context_.setDrawMode(mode);
    }
}

bool MapEditorInteractiveDrawController::canPlanDraftInsertionForMode(MapEditorInteractiveDrawMode mode,
                                                                      QString *errorMessage) const
{
    if (mode == MapEditorInteractiveDrawMode::None
        || mode == MapEditorInteractiveDrawMode::SmartArea) {
        return true;
    }
    if (context_.textEditor == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Drawing failed: no active TH2 text editor.");
        }
        return false;
    }

    const QString plannerSource =
        plannerSourceWithAreaAdjust(context_.textEditor->text(), draftInitialAreaAdjustRect(context_));
    QVector<TherionSourceTextEdit> sourceEdits;
    int insertedLineNumber = 0;
    if (mode == MapEditorInteractiveDrawMode::Point) {
        return TherionDocumentEditor::appendDraftGeometryEdits(plannerSource,
                                                               QStringLiteral("point"),
                                                               QVector<QPointF>{QPointF(0.0, 0.0)},
                                                               &sourceEdits,
                                                               &insertedLineNumber,
                                                               errorMessage,
                                                               draftObjectOptionsFor(context_, QStringLiteral("point")));
    }
    if (mode == MapEditorInteractiveDrawMode::Line
        || mode == MapEditorInteractiveDrawMode::Freehand) {
        return TherionDocumentEditor::appendDraftLineGeometryEdits(plannerSource,
                                                                   QStringList{
                                                                       QStringLiteral("0 0"),
                                                                       QStringLiteral("1 1"),
                                                                   },
                                                                   &sourceEdits,
                                                                   &insertedLineNumber,
                                                                   errorMessage,
                                                                   QString(),
                                                                   draftObjectOptionsFor(context_, QStringLiteral("line")));
    }
    if (mode == MapEditorInteractiveDrawMode::Area) {
        return TherionDocumentEditor::appendDraftAreaGeometryEdits(plannerSource,
                                                                   QStringList{
                                                                       QStringLiteral("0 0"),
                                                                       QStringLiteral("1 0"),
                                                                       QStringLiteral("1 1"),
                                                                   },
                                                                   &sourceEdits,
                                                                   &insertedLineNumber,
                                                                   errorMessage,
                                                                   draftObjectOptionsFor(context_, QStringLiteral("area")));
    }

    return true;
}

MapEditorInteractiveDrawController::MapEditorInteractiveDrawController(MapEditorInteractiveDrawContext context)
    : context_(std::move(context))
{
}

void MapEditorInteractiveDrawController::setInteractiveDrawMode(MapEditorInteractiveDrawMode mode)
{
    QString errorMessage;
    if (!canPlanDraftInsertionForMode(mode, &errorMessage)) {
        clearInteractiveDrawSession(true);
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Drawing failed: no active TH2 text editor.")
            : errorMessage;
        context_.refreshToolbarSummary();
        context_.updateHelpPanel();
        context_.updateCommandSurfaceState();
        return;
    }

    const MapEditorInteractiveDrawMode previousMode = this->mode();
    clearInteractiveDrawSession(false);
    setMode(mode);
    (*context_.selectModeActive) = mode == MapEditorInteractiveDrawMode::None;
    if (previousMode != this->mode()) {
        context_.emitModeStatusChanged();
    }
    context_.updateCommandSurfaceState();
}

bool MapEditorInteractiveDrawController::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    if (mode() == MapEditorInteractiveDrawMode::None) {
        return false;
    }

    if (context_.textEditor == nullptr) {
        (*context_.toolbarStatusNote) = tr("Drawing failed: no active TH2 text editor.");
        context_.refreshToolbarSummary();
        return true;
    }

    if (mode() == MapEditorInteractiveDrawMode::Point) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QString beforeText = context_.textEditor->text();
        QString afterText;
        const TherionDraftObjectOptions objectOptions = draftObjectOptionsFor(context_, QStringLiteral("point"));
        if (!planPointInsert(beforeText,
                             context_.sourcePointFromScenePosition(scenePosition),
                             objectOptions,
                             draftInitialAreaAdjustRect(context_),
                             &afterText,
                             &insertedLineNumber,
                             &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Point insert failed.")
                : tr("Point insert failed: %1").arg(errorMessage);
        } else {
            const SourceApplyResult applyResult =
                applyInsertWithSnapshot(context_, tr("Insert Point"), beforeText, afterText, insertedLineNumber);
            if (applyResult == SourceApplyResult::Unavailable) {
                (*context_.toolbarStatusNote) = tr("Point insert failed: source transaction callback is unavailable.");
            } else if (applyResult == SourceApplyResult::Applied) {
                if (context_.recordCommittedDraftObjectOptions) {
                    context_.recordCommittedDraftObjectOptions(QStringLiteral("point"), objectOptions);
                }
                context_.refreshObjectDetailsPanel();
                (*context_.toolbarStatusNote) = insertedLineNumber > 0
                    ? tr("Inserted point at source line %1.").arg(insertedLineNumber)
                    : tr("Inserted point.");
            }
        }
        context_.refreshToolbarSummary();
        return true;
    }

    if (mode() == MapEditorInteractiveDrawMode::SmartArea) {
        if (context_.previewSmartAreaAt != nullptr && context_.previewSmartAreaAt(scenePosition)) {
            (*context_.toolbarStatusNote) = context_.hasSmartAreaPreview != nullptr && context_.hasSmartAreaPreview()
                ? tr("Smart Area: preview ready. Press [ or ] for alternatives, Enter or Complete Draft to insert, Esc to cancel.")
                : tr("Smart Area: no closed area was found at the click position.");
        } else {
            (*context_.toolbarStatusNote) = tr("Smart Area: no closed area was found at the click position.");
        }
        context_.refreshToolbarSummary();
        context_.updateCommandSurfaceState();
        return true;
    }

    if (mode() == MapEditorInteractiveDrawMode::Area) {
        context_.captureInteractiveLineAnchor(scenePosition, std::nullopt);
        (*context_.hoverActive) = false;
        (*context_.toolbarStatusNote) = tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                                 .arg((*context_.lineVertices).size());
        context_.refreshToolbarSummary();
        context_.updateCommandSurfaceState();
        return true;
    }

    return false;
}

bool MapEditorInteractiveDrawController::commitInteractiveDrawSession(bool closeLineDraft)
{
    const MapEditorInteractiveDrawMode modeAtCommit = mode();
    if (modeAtCommit == MapEditorInteractiveDrawMode::SmartArea) {
        if (context_.commitSmartAreaPreview != nullptr && context_.commitSmartAreaPreview()) {
            clearInteractiveDrawSession(true);
            (*context_.toolbarStatusNote) = tr("Smart Area inserted. Selection mode is active.");
        } else if (context_.hasSmartAreaPreview != nullptr && context_.hasSmartAreaPreview()) {
            // Keep existing toolbar status (for example stale-edit feedback) when preview exists but apply did not succeed.
        } else {
            (*context_.toolbarStatusNote) = tr("Smart Area needs a preview before insertion.");
        }
        context_.refreshToolbarSummary();
        context_.updateHelpPanel();
        context_.updateCommandSurfaceState();
        return true;
    }

    if (modeAtCommit != MapEditorInteractiveDrawMode::Line
        && modeAtCommit != MapEditorInteractiveDrawMode::Area) {
        return false;
    }

    if (context_.textEditor == nullptr) {
        (*context_.toolbarStatusNote) = tr("Complete Draft failed: no active TH2 text editor.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int minVertexCount = modeAtCommit == MapEditorInteractiveDrawMode::Line ? 2 : 3;
    const int capturedVertices = (*context_.lineVertices).size();
    if (capturedVertices < minVertexCount) {
        (*context_.toolbarStatusNote) = modeAtCommit == MapEditorInteractiveDrawMode::Line
            ? tr("Line mode needs at least 2 vertices before completion.")
            : tr("Area mode needs at least 3 vertices before completion.");
        context_.refreshToolbarSummary();
        return true;
    }

    int committedDraftLineNumber = 0;
    if (modeAtCommit == MapEditorInteractiveDrawMode::Line) {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QStringList coordinateRows = closeLineDraft
            ? closedLineCoordinateRowsForInteractiveDraft((*context_.lineVertices))
            : context_.lineCoordinateRowsForInteractiveDraft();
        const QString lineOptions = closeLineDraft ? QStringLiteral("-close on") : QString();
        const QString beforeText = context_.textEditor->text();
        QString afterText;
        const TherionDraftObjectOptions objectOptions = draftObjectOptionsFor(context_, QStringLiteral("line"));
        if (!planLineInsert(beforeText,
                            coordinateRows,
                            lineOptions,
                            objectOptions,
                            draftInitialAreaAdjustRect(context_),
                            &afterText,
                            &insertedLineNumber,
                            &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            context_.refreshToolbarSummary();
            return true;
        }
        std::function<void()> afterDeferredProjection;
        if (closeLineDraft && insertedLineNumber > 0 && context_.selectCommittedDraftObject) {
            afterDeferredProjection = [selectCommittedDraftObject = context_.selectCommittedDraftObject,
                                       insertedLineNumber]() {
                selectCommittedDraftObject(insertedLineNumber);
            };
        }
        const SourceApplyResult applyResult =
            applyInsertWithSnapshot(context_,
                                    tr("Insert Line"),
                                    beforeText,
                                    afterText,
                                    insertedLineNumber,
                                    std::move(afterDeferredProjection));
        if (applyResult == SourceApplyResult::Unavailable) {
            (*context_.toolbarStatusNote) = tr("Complete Draft failed: source transaction callback is unavailable.");
            context_.refreshToolbarSummary();
            return true;
        }
        if (applyResult == SourceApplyResult::NotApplied) {
            context_.refreshToolbarSummary();
            context_.updateCommandSurfaceState();
            return true;
        }
        if (context_.recordCommittedDraftObjectOptions) {
            context_.recordCommittedDraftObjectOptions(QStringLiteral("line"), objectOptions);
        }
        committedDraftLineNumber = insertedLineNumber;
    } else {
        QString errorMessage;
        int insertedLineNumber = 0;
        const QString beforeText = context_.textEditor->text();
        QString afterText;
        const TherionDraftObjectOptions objectOptions = draftObjectOptionsFor(context_, QStringLiteral("area"));
        if (!planAreaInsert(beforeText,
                            context_.areaCoordinateRowsForInteractiveDraft(),
                            objectOptions,
                            draftInitialAreaAdjustRect(context_),
                            &afterText,
                            &insertedLineNumber,
                            &errorMessage)) {
            (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                ? tr("Complete Draft failed.")
                : tr("Complete Draft failed: %1").arg(errorMessage);
            context_.refreshToolbarSummary();
            return true;
        }
        const SourceApplyResult applyResult =
            applyInsertWithSnapshot(context_, tr("Insert Area"), beforeText, afterText, insertedLineNumber);
        if (applyResult == SourceApplyResult::Unavailable) {
            (*context_.toolbarStatusNote) = tr("Complete Draft failed: source transaction callback is unavailable.");
            context_.refreshToolbarSummary();
            return true;
        }
        if (applyResult == SourceApplyResult::NotApplied) {
            context_.refreshToolbarSummary();
            context_.updateCommandSurfaceState();
            return true;
        }
        if (context_.recordCommittedDraftObjectOptions) {
            context_.recordCommittedDraftObjectOptions(QStringLiteral("area"), objectOptions);
        }
        committedDraftLineNumber = insertedLineNumber;
    }

    if (modeAtCommit == MapEditorInteractiveDrawMode::Line && closeLineDraft) {
        clearInteractiveDrawSession(true);
        if (committedDraftLineNumber > 0
            && context_.selectCommittedDraftObject
            && !context_.applySourceTextChangeWithSnapshotDeferredProjection) {
            context_.selectCommittedDraftObject(committedDraftLineNumber);
        }
        (*context_.toolbarStatusNote) = tr("Selection mode: draft committed.");
        context_.refreshToolbarSummary();
        context_.updateHelpPanel();
        context_.updateCommandSurfaceState();
        return true;
    }

    clearInteractiveDrawSession(false);
    setMode(modeAtCommit);
    (*context_.selectModeActive) = false;
    if (modeAtCommit == MapEditorInteractiveDrawMode::Line) {
        (*context_.toolbarStatusNote) = closeLineDraft
            ? tr("Line committed as closed loop. Line mode is still active for the next object.")
            : tr("Line committed. Line mode is still active for the next object.");
    } else {
        (*context_.toolbarStatusNote) = tr("Area committed. Area mode is still active for the next object.");
    }
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
    context_.updateHelpPanel();
    context_.updateCommandSurfaceState();
    return true;
}

void MapEditorInteractiveDrawController::clearInteractiveDrawSession(bool clearMode)
{
    (*context_.sourceVertices).clear();
    (*context_.sceneVertices).clear();
    (*context_.lineVertices).clear();
    (*context_.strokeActive) = false;
    (*context_.anchorPressActive) = false;
    (*context_.anchorDragActive) = false;
    (*context_.controlDragActive) = false;
    (*context_.hoverActive) = false;
    if (context_.previewSmartAreaAt != nullptr) {
        context_.previewSmartAreaAt(QPointF(std::numeric_limits<qreal>::quiet_NaN(),
                                            std::numeric_limits<qreal>::quiet_NaN()));
    }
    if (context_.hoverSnapActive != nullptr) {
        (*context_.hoverSnapActive) = false;
    }
    if (context_.hoverSnapGuideScenePoints != nullptr) {
        context_.hoverSnapGuideScenePoints->clear();
    }
    if (context_.view != nullptr && context_.view->viewport() != nullptr && !(*context_.panActive)) {
        context_.view->viewport()->unsetCursor();
    }

    if (context_.scene != nullptr) {
        if ((*context_.previewPath) != nullptr) {
            context_.scene->removeItem((*context_.previewPath));
            delete (*context_.previewPath);
            (*context_.previewPath) = nullptr;
        }
        for (QGraphicsItem *marker : std::as_const((*context_.previewMarkers))) {
            if (marker != nullptr) {
                context_.scene->removeItem(marker);
                delete marker;
            }
        }
    }
    (*context_.previewMarkers).clear();
    (*context_.previewPath) = nullptr;

    if (clearMode) {
        const bool modeChanged = mode() != MapEditorInteractiveDrawMode::None;
        setMode(MapEditorInteractiveDrawMode::None);
        (*context_.selectModeActive) = true;
        if (context_.clearPendingInsertObject != nullptr) {
            context_.clearPendingInsertObject();
        }
        if (context_.refreshObjectDetailsPanel != nullptr) {
            context_.refreshObjectDetailsPanel();
        }
        if (modeChanged) {
            context_.emitModeStatusChanged();
        }
    }
}

void MapEditorInteractiveDrawController::updateInteractiveDrawPreview()
{
    if (context_.scene == nullptr) {
        return;
    }

    auto clearPreviewMarkers = [this]() {
        for (QGraphicsItem *marker : std::as_const((*context_.previewMarkers))) {
            if (marker != nullptr) {
                if (marker->scene() != nullptr) {
                    marker->scene()->removeItem(marker);
                }
                delete marker;
            }
        }
        (*context_.previewMarkers).clear();
    };
    auto hidePreviewPath = [this]() {
        if ((*context_.previewPath) != nullptr) {
            (*context_.previewPath)->setPath(QPainterPath());
            (*context_.previewPath)->setVisible(false);
        }
    };

    auto removePreviewMarkerAt = [this](int markerIndex) {
        if (markerIndex < 0 || markerIndex >= (*context_.previewMarkers).size()) {
            return;
        }
        QGraphicsItem *marker = (*context_.previewMarkers).at(markerIndex);
        if (marker != nullptr) {
            if (marker->scene() != nullptr) {
                marker->scene()->removeItem(marker);
            }
            delete marker;
        }
        (*context_.previewMarkers)[markerIndex] = nullptr;
    };
    auto ensurePreviewMarkerSlot = [this](int markerIndex) {
        while ((*context_.previewMarkers).size() <= markerIndex) {
            (*context_.previewMarkers).append(nullptr);
        }
    };
    auto ensurePreviewEllipseMarker = [this, &ensurePreviewMarkerSlot, &removePreviewMarkerAt](
                                          int markerIndex,
                                          const QRectF &rect) {
        ensurePreviewMarkerSlot(markerIndex);
        auto *marker = dynamic_cast<QGraphicsEllipseItem *>((*context_.previewMarkers).at(markerIndex));
        if (marker == nullptr) {
            removePreviewMarkerAt(markerIndex);
            marker = new QGraphicsEllipseItem(rect);
            marker->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            marker->setAcceptedMouseButtons(Qt::NoButton);
            (*context_.previewMarkers)[markerIndex] = marker;
        } else {
            marker->setRect(rect);
        }
        if (marker->scene() != nullptr && marker->scene() != context_.scene) {
            marker->scene()->removeItem(marker);
        }
        if (marker->scene() != context_.scene) {
            context_.scene->addItem(marker);
        }
        return marker;
    };
    auto ensurePreviewLineMarker = [this, &ensurePreviewMarkerSlot, &removePreviewMarkerAt](int markerIndex) {
        ensurePreviewMarkerSlot(markerIndex);
        auto *marker = dynamic_cast<QGraphicsLineItem *>((*context_.previewMarkers).at(markerIndex));
        if (marker == nullptr) {
            removePreviewMarkerAt(markerIndex);
            marker = new QGraphicsLineItem();
            marker->setAcceptedMouseButtons(Qt::NoButton);
            (*context_.previewMarkers)[markerIndex] = marker;
        }
        if (marker->scene() != nullptr && marker->scene() != context_.scene) {
            marker->scene()->removeItem(marker);
        }
        if (marker->scene() != context_.scene) {
            context_.scene->addItem(marker);
        }
        return marker;
    };
    auto trimPreviewMarkers = [this, &removePreviewMarkerAt](int usedMarkerCount) {
        for (int markerIndex = (*context_.previewMarkers).size() - 1; markerIndex >= usedMarkerCount; --markerIndex) {
            removePreviewMarkerAt(markerIndex);
        }
        (*context_.previewMarkers).resize(usedMarkerCount);
    };

    if (mode() != MapEditorInteractiveDrawMode::Line
        && mode() != MapEditorInteractiveDrawMode::Point
        && mode() != MapEditorInteractiveDrawMode::Area
        && mode() != MapEditorInteractiveDrawMode::Freehand
        && mode() != MapEditorInteractiveDrawMode::SmartArea) {
        clearPreviewMarkers();
        hidePreviewPath();
        return;
    }

    if (mode() == MapEditorInteractiveDrawMode::SmartArea) {
        clearPreviewMarkers();
        if (context_.hasSmartAreaPreview == nullptr || !context_.hasSmartAreaPreview()) {
            hidePreviewPath();
            return;
        }
        if ((*context_.previewPath) == nullptr) {
            (*context_.previewPath) = new QGraphicsPathItem();
            (*context_.previewPath)->setZValue(28.0);
            (*context_.previewPath)->setAcceptedMouseButtons(Qt::NoButton);
            context_.scene->addItem((*context_.previewPath));
        } else if ((*context_.previewPath)->scene() != context_.scene) {
            context_.scene->addItem((*context_.previewPath));
        }
        QPen previewPen(QColor(QStringLiteral("#0077cc")),
                        previewCosmeticWidth(context_, 3.2),
                        Qt::DashLine,
                        Qt::RoundCap,
                        Qt::RoundJoin);
        previewPen.setCosmetic(true);
        QColor fillColor(QStringLiteral("#28a8ff"));
        fillColor.setAlpha(90);
        (*context_.previewPath)->setPen(previewPen);
        (*context_.previewPath)->setBrush(QBrush(fillColor));
        (*context_.previewPath)->setVisible(true);
        return;
    }

    QColor accent;
    if (mode() == MapEditorInteractiveDrawMode::Line) {
        accent = QColor(QStringLiteral("#ff9a2f"));
    } else if (mode() == MapEditorInteractiveDrawMode::Area) {
        accent = QColor(QStringLiteral("#ff7f8f"));
    } else {
        accent = QColor(QStringLiteral("#66d38f"));
    }
    accent.setAlpha(220);
    QColor controlColor = QColor(QStringLiteral("#33a8ff"));
    controlColor.setAlpha(225);

    QPainterPath path;
    QVector<QPointF> anchorMarkers;
    QVector<QPointF> snapGuideMarkers = context_.hoverSnapGuideScenePoints != nullptr
        ? (*context_.hoverSnapGuideScenePoints)
        : QVector<QPointF>();
    QVector<QPointF> controlMarkers;
    QVector<QLineF> controlConnectors;
    std::optional<QPointF> hoverSnapTargetMarker;
    if (mode() == MapEditorInteractiveDrawMode::Line
        || mode() == MapEditorInteractiveDrawMode::Area) {
        struct DraftPreviewVertex
        {
            QPointF anchorScene;
            std::optional<QPointF> incomingControlScene;
            std::optional<QPointF> outgoingControlScene;
        };

        QVector<DraftPreviewVertex> previewVertices;
        previewVertices.reserve((*context_.lineVertices).size() + 1);
        for (const MapEditorInteractiveLineDraftVertex &vertex : std::as_const((*context_.lineVertices))) {
            DraftPreviewVertex previewVertex;
            previewVertex.anchorScene = vertex.anchorScene;
            previewVertex.incomingControlScene = vertex.incomingControlScene;
            previewVertex.outgoingControlScene = vertex.outgoingControlScene;
            previewVertices.append(previewVertex);
            anchorMarkers.append(vertex.anchorScene);
        }

        auto applyDraggedControlsToCurrentVertex = [&previewVertices](const QPointF &dragScenePoint) {
            if (previewVertices.isEmpty()) {
                return;
            }
            DraftPreviewVertex &current = previewVertices[previewVertices.size() - 1];
            current.outgoingControlScene = dragScenePoint;
            current.incomingControlScene = current.anchorScene - (dragScenePoint - current.anchorScene);
        };

        if ((*context_.anchorPressActive)) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = (*context_.anchorPressScenePoint);
            previewVertices.append(candidate);
            anchorMarkers.append(candidate.anchorScene);
            if ((*context_.anchorDragActive)) {
                applyDraggedControlsToCurrentVertex((*context_.anchorDragScenePoint));
            }
        } else if ((*context_.hoverActive)) {
            DraftPreviewVertex candidate;
            candidate.anchorScene = (*context_.hoverScenePoint);
            previewVertices.append(candidate);
            anchorMarkers.append(candidate.anchorScene);
            if (context_.hoverSnapActive != nullptr && (*context_.hoverSnapActive)) {
                const QPointF snapPoint = (context_.hoverSnapScenePoint != nullptr)
                    ? (*context_.hoverSnapScenePoint)
                    : candidate.anchorScene;
                hoverSnapTargetMarker = snapPoint;
            }
        }

        if (!previewVertices.isEmpty()) {
            path.moveTo(previewVertices.first().anchorScene);
            for (int index = 1; index < previewVertices.size(); ++index) {
                const DraftPreviewVertex &previous = previewVertices.at(index - 1);
                const DraftPreviewVertex &current = previewVertices.at(index);
                const QPointF control1 = previous.outgoingControlScene.value_or(previous.anchorScene);
                const QPointF control2 = current.incomingControlScene.value_or(current.anchorScene);
                const bool curved = previous.outgoingControlScene.has_value() || current.incomingControlScene.has_value();
                if (curved) {
                    path.cubicTo(control1, control2, current.anchorScene);
                } else {
                    path.lineTo(current.anchorScene);
                }
            }

            for (const DraftPreviewVertex &vertex : std::as_const(previewVertices)) {
                if (vertex.incomingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.incomingControlScene.value()));
                    controlMarkers.append(vertex.incomingControlScene.value());
                }
                if (vertex.outgoingControlScene.has_value()) {
                    controlConnectors.append(QLineF(vertex.anchorScene, vertex.outgoingControlScene.value()));
                    controlMarkers.append(vertex.outgoingControlScene.value());
                }
            }
        }
        if (mode() == MapEditorInteractiveDrawMode::Area && previewVertices.size() >= 3) {
            const DraftPreviewVertex &first = previewVertices.first();
            const DraftPreviewVertex &last = previewVertices.last();

            std::optional<QPointF> closingOutgoing = last.outgoingControlScene;
            if (!closingOutgoing.has_value() && last.incomingControlScene.has_value()) {
                closingOutgoing = mirroredSmoothControlPoint(last.anchorScene,
                                                             last.incomingControlScene.value(),
                                                             std::nullopt);
            }

            std::optional<QPointF> closingIncoming = first.incomingControlScene;
            if (!closingIncoming.has_value() && first.outgoingControlScene.has_value()) {
                closingIncoming = mirroredSmoothControlPoint(first.anchorScene,
                                                             first.outgoingControlScene.value(),
                                                             std::nullopt);
            }

            const bool curvedClose = closingOutgoing.has_value() || closingIncoming.has_value();
            if (curvedClose) {
                path.cubicTo(closingOutgoing.value_or(last.anchorScene),
                             closingIncoming.value_or(first.anchorScene),
                             first.anchorScene);
                if (closingOutgoing.has_value()) {
                    controlConnectors.append(QLineF(last.anchorScene, closingOutgoing.value()));
                    controlMarkers.append(closingOutgoing.value());
                }
                if (closingIncoming.has_value()) {
                    controlConnectors.append(QLineF(first.anchorScene, closingIncoming.value()));
                    controlMarkers.append(closingIncoming.value());
                }
            } else {
                path.lineTo(first.anchorScene);
            }
        }
    } else if (mode() == MapEditorInteractiveDrawMode::Point) {
        if ((*context_.hoverActive)) {
            anchorMarkers.append((*context_.hoverScenePoint));
            if (context_.hoverSnapActive != nullptr && (*context_.hoverSnapActive)) {
                const QPointF snapPoint = (context_.hoverSnapScenePoint != nullptr)
                    ? (*context_.hoverSnapScenePoint)
                    : (*context_.hoverScenePoint);
                hoverSnapTargetMarker = snapPoint;
            }
        }
    } else {
        if ((*context_.sceneVertices).isEmpty()) {
            clearPreviewMarkers();
            hidePreviewPath();
            return;
        }
        QVector<QPointF> displayVertices = (*context_.sceneVertices);
        if ((*context_.hoverActive)) {
            displayVertices.append((*context_.hoverScenePoint));
        }

        path = QPainterPath(displayVertices.first());
        for (int index = 1; index < displayVertices.size(); ++index) {
            path.lineTo(displayVertices.at(index));
        }
        if (mode() == MapEditorInteractiveDrawMode::Area && displayVertices.size() >= 3) {
            path.closeSubpath();
        }
    }

    const bool usesPathPreview = mode() != MapEditorInteractiveDrawMode::Point;
    if (usesPathPreview && (*context_.previewPath) == nullptr) {
        (*context_.previewPath) = new QGraphicsPathItem();
        (*context_.previewPath)->setBrush(Qt::NoBrush);
        (*context_.previewPath)->setZValue(28.0);
        (*context_.previewPath)->setAcceptedMouseButtons(Qt::NoButton);
        context_.scene->addItem((*context_.previewPath));
    } else if (usesPathPreview && (*context_.previewPath)->scene() != context_.scene) {
        context_.scene->addItem((*context_.previewPath));
    }

    if (usesPathPreview) {
        const Qt::PenStyle previewPenStyle = mode() == MapEditorInteractiveDrawMode::Freehand
            ? Qt::SolidLine
            : Qt::DashLine;
        QPen previewPen(accent,
                        previewCosmeticWidth(context_, 4.4),
                        previewPenStyle,
                        Qt::RoundCap,
                        Qt::RoundJoin);
        previewPen.setCosmetic(true);
        (*context_.previewPath)->setPen(previewPen);
        (*context_.previewPath)->setPath(path);
        (*context_.previewPath)->setVisible(true);
    } else {
        hidePreviewPath();
    }

    int previewMarkerIndex = 0;
    for (const QPointF &guidePoint : std::as_const(snapGuideMarkers)) {
        QGraphicsEllipseItem *marker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-8.0, -8.0, 16.0, 16.0));
        marker->setPos(guidePoint);
        QColor guideColor(QStringLiteral("#00e5ff"));
        guideColor.setAlpha(235);
        QPen guidePen(guideColor, previewCosmeticWidth(context_, 2.4));
        guidePen.setCosmetic(true);
        QColor guideFill(QStringLiteral("#00e5ff"));
        guideFill.setAlpha(45);
        marker->setPen(guidePen);
        marker->setBrush(QBrush(guideFill));
        marker->setZValue(28.7);
    }

    for (const QPointF &vertex : std::as_const(anchorMarkers)) {
        QGraphicsEllipseItem *marker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-5.0, -5.0, 10.0, 10.0));
        marker->setPos(vertex);
        QPen markerPen(accent.darker(130), previewCosmeticWidth(context_, 1.6));
        markerPen.setCosmetic(true);
        marker->setPen(markerPen);
        marker->setBrush(QBrush(accent));
        marker->setZValue(28.5);
    }

    if (hoverSnapTargetMarker.has_value()) {
        QGraphicsEllipseItem *snapMarker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-8.5, -8.5, 17.0, 17.0));
        snapMarker->setPos(hoverSnapTargetMarker.value());
        QPen snapMarkerPen(QColor(QStringLiteral("#ffe16a")), previewCosmeticWidth(context_, 2.4));
        snapMarkerPen.setCosmetic(true);
        snapMarker->setPen(snapMarkerPen);
        snapMarker->setBrush(Qt::NoBrush);
        snapMarker->setZValue(28.8);
    }

    for (const QLineF &connector : std::as_const(controlConnectors)) {
        QGraphicsLineItem *connectorItem = ensurePreviewLineMarker(previewMarkerIndex++);
        connectorItem->setLine(connector);
        QPen connectorPen(controlColor.lighter(115),
                          previewCosmeticWidth(context_, 2.2),
                          Qt::DashLine,
                          Qt::RoundCap,
                          Qt::RoundJoin);
        connectorPen.setCosmetic(true);
        connectorItem->setPen(connectorPen);
        connectorItem->setZValue(28.4);
    }

    for (const QPointF &control : std::as_const(controlMarkers)) {
        QGraphicsEllipseItem *marker = ensurePreviewEllipseMarker(previewMarkerIndex++, QRectF(-5.2, -5.2, 10.4, 10.4));
        marker->setPos(control);
        QPen markerPen(controlColor.darker(150), previewCosmeticWidth(context_, 1.7));
        markerPen.setCosmetic(true);
        marker->setPen(markerPen);
        marker->setBrush(QBrush(controlColor));
        marker->setZValue(28.6);
    }
    trimPreviewMarkers(previewMarkerIndex);
}

bool MapEditorInteractiveDrawController::cancelInteractiveDrawingToSelectMode()
{
    if (mode() == MapEditorInteractiveDrawMode::None) {
        return false;
    }

    const MapEditorInteractiveDrawMode modeAtCancel = mode();
    if (modeAtCancel == MapEditorInteractiveDrawMode::SmartArea) {
        clearInteractiveDrawSession(true);
        (*context_.toolbarStatusNote) = tr("Selection mode: Smart Area canceled.");
        context_.refreshToolbarSummary();
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
        return true;
    }
    const bool isLineOrArea = modeAtCancel == MapEditorInteractiveDrawMode::Line
        || modeAtCancel == MapEditorInteractiveDrawMode::Area;
    bool committedLineOrAreaDraft = false;
    int committedDraftLineNumber = 0;
    if (isLineOrArea && context_.hasCompletableInteractiveDrawSession()) {
        if (modeAtCancel == MapEditorInteractiveDrawMode::Line) {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = context_.textEditor->text();
            QString afterText;
            const TherionDraftObjectOptions objectOptions = draftObjectOptionsFor(context_, QStringLiteral("line"));
            if (!planLineInsert(beforeText,
                                context_.lineCoordinateRowsForInteractiveDraft(),
                                QString(),
                                objectOptions,
                                draftInitialAreaAdjustRect(context_),
                                &afterText,
                                &insertedLineNumber,
                                &errorMessage)) {
                (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            const SourceApplyResult applyResult =
                applyInsertWithSnapshot(context_, tr("Insert Line"), beforeText, afterText, insertedLineNumber);
            if (applyResult == SourceApplyResult::Unavailable) {
                (*context_.toolbarStatusNote) = tr("Complete Draft failed: source transaction callback is unavailable.");
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            if (applyResult == SourceApplyResult::NotApplied) {
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            if (context_.recordCommittedDraftObjectOptions) {
                context_.recordCommittedDraftObjectOptions(QStringLiteral("line"), objectOptions);
            }
            committedDraftLineNumber = insertedLineNumber;
        } else {
            QString errorMessage;
            int insertedLineNumber = 0;
            const QString beforeText = context_.textEditor->text();
            QString afterText;
            const TherionDraftObjectOptions objectOptions = draftObjectOptionsFor(context_, QStringLiteral("area"));
            if (!planAreaInsert(beforeText,
                                context_.areaCoordinateRowsForInteractiveDraft(),
                                objectOptions,
                                draftInitialAreaAdjustRect(context_),
                                &afterText,
                                &insertedLineNumber,
                                &errorMessage)) {
                (*context_.toolbarStatusNote) = errorMessage.isEmpty()
                    ? tr("Complete Draft failed.")
                    : tr("Complete Draft failed: %1").arg(errorMessage);
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            const SourceApplyResult applyResult =
                applyInsertWithSnapshot(context_, tr("Insert Area"), beforeText, afterText, insertedLineNumber);
            if (applyResult == SourceApplyResult::Unavailable) {
                (*context_.toolbarStatusNote) = tr("Complete Draft failed: source transaction callback is unavailable.");
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            if (applyResult == SourceApplyResult::NotApplied) {
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                context_.updateHelpPanel();
                return false;
            }
            if (context_.recordCommittedDraftObjectOptions) {
                context_.recordCommittedDraftObjectOptions(QStringLiteral("area"), objectOptions);
            }
            committedDraftLineNumber = insertedLineNumber;
        }
        committedLineOrAreaDraft = true;
    }

    clearInteractiveDrawSession(true);
    (*context_.strokeActive) = false;
    if (context_.view != nullptr) {
        context_.view->setDragMode(QGraphicsView::NoDrag);
    }

    if (committedLineOrAreaDraft) {
        if (committedDraftLineNumber > 0 && context_.selectCommittedDraftObject) {
            context_.selectCommittedDraftObject(committedDraftLineNumber);
        }
        (*context_.toolbarStatusNote) = tr("Selection mode: draft committed.");
    } else if (isLineOrArea) {
        (*context_.toolbarStatusNote) = tr("Selection mode: draft canceled.");
    } else {
        (*context_.toolbarStatusNote) = tr("Selection mode: drawing canceled.");
    }
    context_.refreshToolbarSummary();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    return true;
}
}
