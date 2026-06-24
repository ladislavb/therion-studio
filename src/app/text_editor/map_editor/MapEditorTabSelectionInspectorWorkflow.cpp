#include "MapEditorTab.h"

#include "MapEditorSceneSupport.h"
#include "../TextEditorSourceTransactionController.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionBackgroundMetadata.h"
#include "../../../core/TherionSourceText.h"

#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QStringList>

namespace TherionStudio
{
namespace
{
QString insertedScrapIdentifier(const QString &text, int lineNumber, const QString &fallback)
{
    if (lineNumber <= 0) {
        return fallback;
    }

    const QStringList lines = TherionSourceText::splitTextLines(text);
    if (lineNumber > lines.size()) {
        return fallback;
    }

    const QStringList tokens = lines.at(lineNumber - 1).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.size() >= 2 && tokens.at(0).compare(QStringLiteral("scrap"), Qt::CaseInsensitive) == 0) {
        return tokens.at(1);
    }
    return fallback;
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

void MapEditorTab::handleAddPointTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Point);
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::Point) {
        return;
    }
    beginPendingInsertObject(QStringLiteral("point"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point. Esc cancels.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    interactiveDrawState_.lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::Line) {
        return;
    }
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Line mode: click to add vertices. Enter completes, Esc cancels.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    interactiveDrawState_.lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::Freehand) {
        return;
    }
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line; release inserts it. Esc cancels.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    interactiveDrawState_.lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::Area) {
        return;
    }
    beginPendingInsertObject(QStringLiteral("area"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Area mode: click to add vertices. Enter completes, Esc cancels.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSmartAreaTriggered()
{
    interactiveDrawState_.lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::SmartArea);
    beginPendingInsertObject(QStringLiteral("area"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Smart Area mode: click inside a closed boundary. Enter completes, Esc cancels.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
    interactiveDrawState_.lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::None);
    clearPendingInsertObject();
    refreshObjectDetailsPanel();
    selectModeActive_ = true;
    toolbarStatusNote_ = tr("Selection mode: click an object or vertex to select it.");
    if (mapView_ != nullptr) {
        mapView_->setDragMode(QGraphicsView::NoDrag);
    }
    refreshToolbarSummary();
}

void MapEditorTab::handleInsertScrapTriggered()
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Insert Scrap failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    interactiveDrawState_.lineExtensionActive_ = false;
    if (interactiveDrawState_.mode_ != InteractiveDrawMode::None) {
        setInteractiveDrawMode(InteractiveDrawMode::None);
    }
    selectModeActive_ = true;

    clearPendingInsertObject();

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QString scrapIdentifier;
    const QString scrapScaleOption = xtherionDefaultScrapScaleOption(mapSourceBoundsForCurrentDocument());
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::appendScrapBlockEdits(beforeText,
                                                      scrapIdentifier,
                                                      &sourceEdits,
                                                      &insertedLineNumber,
                                                      &errorMessage,
                                                      pendingScrapOptions(scrapScaleOption))) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert Scrap failed.")
            : tr("Insert Scrap failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    QString afterText = beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert Scrap failed.")
            : tr("Insert Scrap failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    const QString createdScrapIdentifier = insertedScrapIdentifier(afterText, insertedLineNumber, scrapIdentifier);
    clearPendingInsertObject();
    const TextEditorSourceTransactionResult transactionResult =
        applySourceTextChangeWithSnapshot(
            tr("Insert Scrap"),
            beforeText,
            afterText,
            insertedLineNumber,
            insertedLineNumber > 0
                ? std::function<void()>([this, insertedLineNumber]() {
                      rebuildInspectorObjectsTree();
                      goToLine(insertedLineNumber);
                      syncInspectorObjectSelectionToLine(insertedLineNumber, true);
                      activateSelectionInspector();
                  })
                : std::function<void()>());
    if (transactionResult != TextEditorSourceTransactionResult::Applied) {
        refreshObjectDetailsPanel();
        refreshToolbarSummary();
        return;
    }

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Created scrap \"%1\" at source line %2.").arg(createdScrapIdentifier, QString::number(insertedLineNumber))
        : tr("Created scrap \"%1\".").arg(createdScrapIdentifier);
    refreshObjectDetailsPanel();
    refreshToolbarSummary();
}

void MapEditorTab::handleCompleteDraftTriggered()
{
    if (commitInteractiveDrawSession()) {
        return;
    }

    auto *draftRectItem = selectedDraftGeometryItem();
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(draftRectItem);
    if (draftItem == nullptr || draftItem->isDraftComplete()) {
        return;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    const QVector<QPointF> vertices = sourceVerticesForDraft(draftItem);
    if (vertices.isEmpty()) {
        toolbarStatusNote_ = tr("Complete Draft failed: unable to resolve draft geometry coordinates.");
        refreshToolbarSummary();
        return;
    }

    QString geometryKind;
    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        geometryKind = QStringLiteral("point");
        break;
    case DraftGeometryKind::Line:
        geometryKind = QStringLiteral("line");
        break;
    case DraftGeometryKind::Area:
        geometryKind = QStringLiteral("area");
        break;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QString plannerSource = plannerSourceWithAreaAdjust(beforeText, initialAreaAdjustRectForDraftInsertion());
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::appendDraftGeometryEdits(plannerSource,
                                                         geometryKind,
                                                         vertices,
                                                         &sourceEdits,
                                                         &insertedLineNumber,
                                                         &errorMessage,
                                                         pendingDraftObjectOptions(geometryKind))) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    QString afterText = plannerSource;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, sourceEdits, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }

    const QString geometryLabel = draftGeometryLabel(draftItem->kind());
    recordDraftCompletion(draftRectItem, tr("Complete Draft"), beforeText, afterText, insertedLineNumber);
    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(geometryLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(geometryLabel);
    refreshToolbarSummary();
    updateHelpPanel();
}
}
