#include "MapEditorTab.h"

#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorCanvasEditController.h"
#include "MapEditorSceneSupport.h"
#include "../TextEditorSourceTransactionController.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionBackgroundMetadata.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceDocument.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../platform/DiagnosticLogging.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QPointer>
#include <QTimer>
#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
bool diagnosticMapInputLoggingEnabled()
{
    return TherionStudio::diagnosticLoggingEnabled();
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
}

QVector<TherionParsedLine> MapEditorTab::parsedLinesForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedParsedLinesValid_ && cachedParsedLinesRevision_ == currentRevision) {
        return cachedParsedLines_;
    }

    cachedParsedLines_ = TherionDocumentParser::parseTokenLines(textEditor_->text());
    cachedParsedLinesRevision_ = currentRevision;
    cachedParsedLinesValid_ = true;
    return cachedParsedLines_;
}

MapEditorLogicalSourceContext MapEditorTab::logicalSourceContext() const
{
    return MapEditorLogicalSourceContext{
        .logicalCommandsForCurrentDocument = [this]() {
            return logicalCommandsForCurrentDocument();
        },
        .geometryProjectionForCurrentDocument = [this]() {
            return geometryProjectionForCurrentDocument();
        },
    };
}

QVector<TherionSourceLogicalCommand> MapEditorTab::logicalCommandsForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedLogicalCommandsValid_ && cachedLogicalCommandsRevision_ == currentRevision) {
        return cachedLogicalCommands_;
    }

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.revisionId = currentRevision;
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromText(textEditor_->text(), metadata);
    cachedLogicalCommands_ = logicalDocument.commands();
    cachedLogicalCommandsRevision_ = currentRevision;
    cachedLogicalCommandsValid_ = true;
    return cachedLogicalCommands_;
}

Th2GeometryProjection MapEditorTab::geometryProjectionForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedGeometryProjectionValid_ && cachedGeometryProjectionRevision_ == currentRevision) {
        return cachedGeometryProjection_;
    }

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionMap;
    metadata.revisionId = currentRevision;
    const TherionSourceDocument sourceDocument =
        TherionSourceDocument::fromText(textEditor_->text(), metadata);
    const TherionSourceLogicalDocument logicalDocument =
        TherionSourceLogicalDocument::fromSourceDocument(sourceDocument);
    cachedLogicalCommands_ = logicalDocument.commands();
    cachedLogicalCommandsRevision_ = currentRevision;
    cachedLogicalCommandsValid_ = true;
    cachedGeometryProjection_ = Th2GeometryProjection::fromDocuments(sourceDocument, logicalDocument);
    cachedGeometryProjectionRevision_ = currentRevision;
    cachedGeometryProjectionValid_ = true;
    return cachedGeometryProjection_;
}

std::optional<MapEditorInteractiveLineControlHandleRef> MapEditorTab::interactiveLineControlAt(
    const QPointF &scenePosition,
    qreal sceneRadius) const
{
    return TherionStudio::interactiveLineControlAt(interactiveDrawState_.lineVertices_, scenePosition, sceneRadius);
}

QRectF MapEditorTab::mapSourceBoundsForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return QRectF();
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedMapSourceBoundsValid_ && cachedMapSourceBoundsRevision_ == currentRevision) {
        return cachedMapSourceBounds_;
    }

    const QString currentText = textEditor_->text();
    QRectF resolvedBounds;
    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(currentText);
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        resolvedBounds = areaAdjust.modelRect;
    } else {
        resolvedBounds = xtherionAutoAreaAdjustRect();
    }

    cachedMapSourceBoundsValid_ = true;
    cachedMapSourceBoundsRevision_ = currentRevision;
    cachedMapSourceBounds_ = resolvedBounds;
    return cachedMapSourceBounds_;
}

std::optional<QRectF> MapEditorTab::initialAreaAdjustRectForDraftInsertion() const
{
    if (textEditor_ == nullptr) {
        return std::nullopt;
    }

    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(textEditor_->text());
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        return std::nullopt;
    }

    const QRectF autoRect = xtherionAutoAreaAdjustRect();
    if (!autoRect.isValid() || autoRect.width() <= 0.0 || autoRect.height() <= 0.0) {
        return std::nullopt;
    }
    return autoRect;
}

QRectF MapEditorTab::sourceBoundsForInteractiveDraft() const
{
    if (const std::optional<QRectF> initialAreaAdjust = initialAreaAdjustRectForDraftInsertion();
        initialAreaAdjust.has_value()) {
        return *initialAreaAdjust;
    }
    return mapSourceBoundsForCurrentDocument();
}

QPointF MapEditorTab::sourcePointFromScenePosition(const QPointF &scenePosition) const
{
    if (textEditor_ == nullptr) {
        return scenePosition;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid()) {
        return scenePosition;
    }

    const QRectF sourceBounds = sourceBoundsForInteractiveDraft();
    if (!sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return scenePosition;
    }

    return previewToSourcePoint(scenePosition, sourceBounds, previewBounds);
}

bool MapEditorTab::hasCompletableInteractiveDrawSession() const
{
    if (interactiveDrawState_.lineExtensionActive_) {
        return interactiveDrawState_.mode_ == InteractiveDrawMode::Line && interactiveDrawState_.lineVertices_.size() >= 2;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::SmartArea) {
        return interactiveDrawState_.smartAreaPreviewActive_;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::Line) {
        return interactiveDrawState_.lineVertices_.size() >= 2;
    }
    if (interactiveDrawState_.mode_ == InteractiveDrawMode::Area) {
        return interactiveDrawState_.lineVertices_.size() >= 3;
    }
    return false;
}

QStringList MapEditorTab::lineCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::lineCoordinateRowsForInteractiveDraft(interactiveDrawState_.lineVertices_);
}

QStringList MapEditorTab::areaCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::areaCoordinateRowsForInteractiveDraft(interactiveDrawState_.lineVertices_);
}

void MapEditorTab::captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                                const std::optional<QPointF> &dragScenePoint)
{
    const QString pendingCommand = interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower();
    const TherionParsedLine pendingLineParsedLine =
        TherionDocumentParser::parseLine(QStringLiteral("line %1").arg(interactiveDrawState_.pendingInsertFields_.type.trimmed()));
    const bool pendingLinePointOrientationSupported = pendingCommand == QStringLiteral("line")
        && isOrientationSupportedForParsedLine(pendingLineParsedLine, orientationApplicabilityByCommand_);
    const bool pendingLinePointLeftSizeSupported = pendingCommand == QStringLiteral("line")
        && isLinePointLeftSizeSupportedForParsedLine(pendingLineParsedLine);
    TherionStudio::captureInteractiveLineAnchor(&interactiveDrawState_.lineVertices_,
                                                anchorScenePoint,
                                                sourcePointFromScenePosition(anchorScenePoint),
                                                dragScenePoint,
                                                interactiveDrawState_.nextLinePointSmooth_,
                                                interactiveDrawState_.nextLinePointIncomingControl_,
                                                interactiveDrawState_.nextLinePointOutgoingControl_,
                                                pendingInsertLinePointSegmentSubtype(),
                                                pendingLinePointOrientationSupported
                                                        && interactiveDrawState_.nextLinePointOrientationEnabled_
                                                    ? std::optional<qreal>(interactiveDrawState_.nextLinePointOrientationDegrees_)
                                                    : std::nullopt,
                                                pendingLinePointLeftSizeSupported
                                                        && interactiveDrawState_.nextLinePointLeftSizeEnabled_
                                                    ? std::optional<qreal>(interactiveDrawState_.nextLinePointLeftSize_)
                                                    : std::nullopt,
                                                [this](const QPointF &scenePoint) {
                                                    return sourcePointFromScenePosition(scenePoint);
                                                });
    if (pendingCommand == QStringLiteral("line") || pendingCommand == QStringLiteral("area")) {
        interactiveDrawState_.pendingInsertFieldsVisible_ = true;
    }
    updateInteractiveDrawPreview();
}

bool MapEditorTab::setInteractiveLineControlScenePoint(const MapEditorInteractiveLineControlHandleRef &handle,
                                                       const QPointF &scenePoint)
{
    return TherionStudio::setInteractiveLineControlScenePoint(&interactiveDrawState_.lineVertices_,
                                                              handle,
                                                              scenePoint,
                                                              [this](const QPointF &scenePointToMap) {
                                                                  return sourcePointFromScenePosition(scenePointToMap);
                                                              });
}

bool MapEditorTab::commitInteractiveDrawVertices(const QString &geometryKind,
                                                 const QVector<QPointF> &vertices,
                                                 const QString &successLabel)
{
    QElapsedTimer totalTimer;
    QElapsedTimer stageTimer;
    const bool logCommitTiming = diagnosticMapInputLoggingEnabled();
    if (logCommitTiming) {
        totalTimer.start();
        stageTimer.start();
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        return false;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const bool lineGeometry = geometryKind.trimmed().compare(QStringLiteral("line"), Qt::CaseInsensitive) == 0;
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRectForDraftInsertion());
    const qint64 prepareMs = logCommitTiming ? stageTimer.elapsed() : 0;
    if (logCommitTiming) {
        stageTimer.restart();
    }
    QVector<TherionSourceTextEdit> sourceEdits;
    const bool planned = lineGeometry
        ? TherionDocumentEditor::appendDraftLineGeometryEdits(plannerSource,
                                                              bezierLineCoordinateRowsForFreehandStroke(vertices),
                                                              &sourceEdits,
                                                              &insertedLineNumber,
                                                              &errorMessage,
                                                              QString(),
                                                              pendingDraftObjectOptions(QStringLiteral("line")))
        : TherionDocumentEditor::appendDraftGeometryEdits(plannerSource,
                                                          geometryKind,
                                                          vertices,
                                                          &sourceEdits,
                                                          &insertedLineNumber,
                                                          &errorMessage,
                                                          pendingDraftObjectOptions(geometryKind));
    const qint64 planMs = logCommitTiming ? stageTimer.elapsed() : 0;
    if (!planned) {
        if (logCommitTiming) {
            qInfo().noquote()
                << QStringLiteral("map-commit geometry=%1 result=plan-failed vertices=%2 prepare_ms=%3 plan_ms=%4 total_ms=%5")
                       .arg(geometryKind)
                       .arg(vertices.size())
                       .arg(prepareMs)
                       .arg(planMs)
                       .arg(totalTimer.elapsed());
        }
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }

    if (logCommitTiming) {
        stageTimer.restart();
    }
    QString afterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        if (logCommitTiming) {
            qInfo().noquote()
                << QStringLiteral("map-commit geometry=%1 result=apply-edits-failed vertices=%2 edits=%3 prepare_ms=%4 plan_ms=%5 apply_edits_ms=%6 total_ms=%7")
                       .arg(geometryKind)
                       .arg(vertices.size())
                       .arg(sourceEdits.size())
                       .arg(prepareMs)
                       .arg(planMs)
                       .arg(stageTimer.elapsed())
                       .arg(totalTimer.elapsed());
        }
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }
    const qint64 applyEditsMs = logCommitTiming ? stageTimer.elapsed() : 0;

    if (logCommitTiming) {
        stageTimer.restart();
    }
    const int sourceEditCount = sourceEdits.size();
    auto deferredProjectionRefresh = [guarded = QPointer<MapEditorTab>(this)]() {
        QTimer::singleShot(0, guarded, [guarded]() {
            if (guarded == nullptr) {
                return;
            }
            guarded->flushPendingMapSceneRefreshAfterCommand();
        });
    };
    MapEditorCanvasEditController canvasEditController(canvasEditContext());
    const TextEditorSourceTransactionResult transactionResult =
        canvasEditController.applySourceEditsWithSnapshotDeferredProjection(
            tr("Complete Draft"),
            beforeText,
            std::move(sourceEdits),
            insertedLineNumber,
            std::move(deferredProjectionRefresh),
            TextEditorSourceSelectionRestorePolicy::CustomHook,
            [this, successLabel, insertedLineNumber]() {
                toolbarStatusNote_ = insertedLineNumber > 0
                    ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(successLabel, QString::number(insertedLineNumber))
                    : tr("Complete Draft wrote %1 geometry to source.").arg(successLabel);
            });
    const qint64 transactionMs = logCommitTiming ? stageTimer.elapsed() : 0;
    if (logCommitTiming) {
        qInfo().noquote()
            << QStringLiteral(
                   "map-commit geometry=%1 result=%2 vertices=%3 edits=%4 inserted_line=%5 before_chars=%6 after_chars=%7 "
                   "prepare_ms=%8 plan_ms=%9 apply_edits_ms=%10 transaction_ms=%11 total_ms=%12")
                   .arg(geometryKind)
                   .arg(transactionResult == TextEditorSourceTransactionResult::Applied ? QStringLiteral("applied")
                                                                                        : QStringLiteral("not-applied"))
                   .arg(vertices.size())
                   .arg(sourceEditCount)
                   .arg(insertedLineNumber)
                   .arg(beforeText.size())
                   .arg(afterText.size())
                   .arg(prepareMs)
                   .arg(planMs)
                   .arg(applyEditsMs)
                   .arg(transactionMs)
                   .arg(totalTimer.elapsed());
    }
    return transactionResult == TextEditorSourceTransactionResult::Applied;
}

bool MapEditorTab::previewSmartAreaAt(const QPointF &scenePosition)
{
    interactiveDrawState_.smartAreaPreviewActive_ = false;
    interactiveDrawState_.smartAreaCandidates_.clear();
    interactiveDrawState_.smartAreaCandidate_ = {};
    interactiveDrawState_.smartAreaCandidateIndex_ = 0;

    if (std::isnan(scenePosition.x()) || std::isnan(scenePosition.y())) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return false;
    }

    const QPointF sourcePoint = sourcePointFromScenePosition(scenePosition);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLinesForCurrentDocument());
    const QVector<MapEditorSmartAreaCandidate> candidates = mapEditorSmartAreaCandidatesAt(features, sourcePoint);
    if (candidates.isEmpty()) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return false;
    }

    interactiveDrawState_.smartAreaCandidates_ = candidates;
    interactiveDrawState_.smartAreaCandidate_ = interactiveDrawState_.smartAreaCandidates_.first();
    interactiveDrawState_.smartAreaPreviewActive_ = true;

    updateSmartAreaPreviewPath();
    return true;
}

void MapEditorTab::updateSmartAreaPreviewPath()
{
    if (!interactiveDrawState_.smartAreaPreviewActive_
        || interactiveDrawState_.smartAreaCandidates_.isEmpty()
        || interactiveDrawState_.smartAreaCandidateIndex_ < 0
        || interactiveDrawState_.smartAreaCandidateIndex_ >= interactiveDrawState_.smartAreaCandidates_.size()) {
        if (interactiveDrawState_.previewPath_ != nullptr) {
            interactiveDrawState_.previewPath_->setPath(QPainterPath());
            interactiveDrawState_.previewPath_->setVisible(false);
        }
        return;
    }

    interactiveDrawState_.smartAreaCandidate_ =
        interactiveDrawState_.smartAreaCandidates_.at(interactiveDrawState_.smartAreaCandidateIndex_);

    QPainterPath path;
    const QVector<QPointF> &vertices = interactiveDrawState_.smartAreaCandidate_.vertices;
    if (!vertices.isEmpty()) {
        path.moveTo(scenePointFromSourcePosition(vertices.first()));
        for (int index = 1; index < vertices.size(); ++index) {
            path.lineTo(scenePointFromSourcePosition(vertices.at(index)));
        }
        path.closeSubpath();
    }

    if (interactiveDrawState_.previewPath_ == nullptr) {
        interactiveDrawState_.previewPath_ = new QGraphicsPathItem();
        interactiveDrawState_.previewPath_->setAcceptedMouseButtons(Qt::NoButton);
        interactiveDrawState_.previewPath_->setZValue(28.0);
        if (mapScene_ != nullptr) {
            mapScene_->addItem(interactiveDrawState_.previewPath_);
        }
    } else if (mapScene_ != nullptr && interactiveDrawState_.previewPath_->scene() != mapScene_) {
        mapScene_->addItem(interactiveDrawState_.previewPath_);
    }
    interactiveDrawState_.previewPath_->setPath(path);
    interactiveDrawState_.previewPath_->setVisible(true);
    updateInteractiveDrawPreview();
}

bool MapEditorTab::hasSmartAreaPreview() const
{
    return interactiveDrawState_.smartAreaPreviewActive_;
}

bool MapEditorTab::cycleSmartAreaCandidate(int delta)
{
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::SmartArea
        || !interactiveDrawState_.smartAreaPreviewActive_
        || interactiveDrawState_.smartAreaCandidates_.size() <= 1) {
        return false;
    }

    const int count = interactiveDrawState_.smartAreaCandidates_.size();
    interactiveDrawState_.smartAreaCandidateIndex_ =
        (interactiveDrawState_.smartAreaCandidateIndex_ + delta + count) % count;
    updateSmartAreaPreviewPath();
    toolbarStatusNote_ = tr("Smart Area candidate %1 of %2 selected. Press Enter or Complete Draft to insert.")
        .arg(interactiveDrawState_.smartAreaCandidateIndex_ + 1)
        .arg(count);
    refreshToolbarSummary();
    updateCommandSurfaceState();
    return true;
}

bool MapEditorTab::commitSmartAreaPreview()
{
    if (textEditor_ == nullptr || !interactiveDrawState_.smartAreaPreviewActive_) {
        return false;
    }

    QVector<TherionReferencedAreaBoundaryLine> boundaryLines;
    boundaryLines.reserve(interactiveDrawState_.smartAreaCandidate_.boundaryLines.size());
    for (const MapEditorSmartAreaBoundaryLine &line : std::as_const(interactiveDrawState_.smartAreaCandidate_.boundaryLines)) {
        boundaryLines.append(TherionReferencedAreaBoundaryLine{line.lineNumber, line.identifier});
    }

    const QString beforeText = textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    int insertedLineNumber = 0;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::appendReferencedAreaEdits(beforeText,
                                                         interactiveDrawState_.smartAreaCandidate_.scrapLineNumber,
                                                         boundaryLines,
                                                         &sourceEdits,
                                                         &insertedLineNumber,
                                                         &errorMessage,
                                                         pendingDraftObjectOptions(QStringLiteral("area")))
        || !TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Smart Area insert failed.")
            : tr("Smart Area insert failed: %1").arg(errorMessage);
        return false;
    }

    const TextEditorSourceTransactionResult transactionResult =
        applySourceTextChangeWithSnapshot(
            tr("Insert Smart Area"),
            beforeText,
            afterText,
            insertedLineNumber,
            [this, insertedLineNumber]() {
                toolbarStatusNote_ = insertedLineNumber > 0
                    ? tr("Smart Area inserted at source line %1.").arg(insertedLineNumber)
                    : tr("Smart Area inserted.");
            });
    return transactionResult == TextEditorSourceTransactionResult::Applied;
}

QPointF MapEditorTab::scenePointFromSourcePosition(const QPointF &sourcePosition) const
{
    const QRectF previewBounds = mapPreviewBounds();
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    if (!previewBounds.isValid()
        || !sourceBounds.isValid()
        || sourceBounds.width() < 0.001
        || sourceBounds.height() < 0.001) {
        return sourcePosition;
    }
    return mapGeometryPointToPreview(sourcePosition, sourceBounds, previewBounds);
}
}
