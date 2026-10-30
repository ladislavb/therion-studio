#include "TextEditorTab.h"
#include "block_editor/BlockEditorApplyExecutor.h"
#include "block_editor/BlockEditorApplyStateController.h"
#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasItem.h"
#include "block_editor/BlockEditorCanvasView.h"
#include "block_editor/BlockEditorCanvasRebuildController.h"
#include "block_editor/BlockEditorConfigureController.h"
#include "block_editor/BlockEditorDeleteExecutor.h"
#include "block_editor/BlockEditorDetailsSupport.h"
#include "block_editor/BlockEditorDetailsHelpController.h"
#include "block_editor/BlockEditorDetailsPaneController.h"
#include "block_editor/BlockEditorDocumentOutlineBuilder.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorInsertionController.h"
#include "block_editor/BlockEditorLineBuildService.h"
#include "block_editor/BlockEditorMoveController.h"
#include "block_editor/BlockEditorMovePreviewController.h"
#include "block_editor/BlockEditorOptionArgsController.h"
#include "block_editor/BlockEditorSelectionDetailsController.h"
#include "block_editor/BlockEditorSourceController.h"
#include "block_editor/BlockEditorSourceText.h"
#include "block_editor/BlockEditorToolboxController.h"
#include "block_editor/BlockEditorTokenTagEditor.h"
#include "block_editor/BlockEditorToolboxDetailsController.h"
#include "raw_editor/RawEditorCompletionController.h"
#include "raw_editor/RawEditorFindController.h"
#include "raw_editor/RawEditorTextEdit.h"
#include "TextEditorAppearanceController.h"
#include "TextEditorContextHelpController.h"
#include "TextEditorCursorController.h"
#include "TextEditorDocumentController.h"
#include "TextEditorEncodingController.h"
#include "TextEditorModeController.h"
#include "TextEditorTabInteractionController.h"
#include "TextEditorSourceRewriteController.h"
#include "TextEditorStatusController.h"
#include "TextEditorSurfaceStyler.h"

#include <QLineEdit>

#include <utility>

#include "../../core/IFileSystem.h"
#include "../../core/TherionCommandSyntax.h"
#include "../../editor/TherionSyntaxHighlighter.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(IFileSystem &fileSystem, CommandCatalogStore catalogStore, QWidget *parent)
    : QWidget(parent)
    , fileSystem_(&fileSystem)
    , catalogStore_(std::move(catalogStore))
{
    buildAll();
}

TextEditorTab::~TextEditorTab()
{
    tearingDown_ = true;
    if (auto *typedCanvasView = dynamic_cast<BlockEditorCanvasView *>(blockCanvasView_); typedCanvasView != nullptr) {
        typedCanvasView->onDropBlock = {};
        typedCanvasView->onDragPreview = {};
        typedCanvasView->blockLineAtScenePosition = {};
        typedCanvasView->onBlockMovePreview = {};
        typedCanvasView->onBlockMoveRequest = {};
    }
    if (blockCanvasScene_ != nullptr) {
        disconnect(blockCanvasScene_, nullptr, this, nullptr);
    }
}

void TextEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void TextEditorTab::handleApplicationAppearanceChanged()
{
    if (appearanceController_ != nullptr) {
        appearanceController_->handleApplicationAppearanceChanged();
    }
    if (highlighter_ != nullptr) {
        highlighter_->reloadPaletteForApplicationAppearance();
    }
}

QString TextEditorTab::filePath() const
{
    return filePath_;
}

void TextEditorTab::handleTextChanged()
{
    if (highlighter_ != nullptr && highlighter_->applyingFormatsOnly()) {
        return;
    }

    TextEditorTabInteractionController::TextChangedActions actions;
    actions.rebuildBlocksCanvasFromText = [this]() {
        rebuildBlocksCanvasFromText();
    };
    actions.applyDirtyStateFromCurrentState = [this]() {
        applyDirtyStateFromCurrentState();
    };
    actions.emitDocumentTextChanged = [this]() {
        emit documentTextChanged();
    };
    TextEditorTabInteractionController::handleTextChanged(loading_, actions);
}

}
