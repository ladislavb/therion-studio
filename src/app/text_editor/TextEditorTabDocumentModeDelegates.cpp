#include "TextEditorTab.h"

#include "TextEditorDocumentController.h"
#include "TextEditorModeController.h"
#include "TextEditorTabInteractionController.h"

#include "../../core/TherionFileTypes.h"
#include "../../editor/TherionSyntaxHighlighter.h"

#include <QLayout>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextCursor>

namespace TherionStudio
{
bool TextEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    untitledDisplayName_.clear();
    const bool loaded = documentController_ != nullptr
        && documentController_->loadFile(filePath, errorMessage);
    if (loaded) {
        refreshHighlighterSourceDocumentType();
    }
    return loaded;
}

void TextEditorTab::initializeNewDocument(const QString &suggestedFileName, const QString &contents)
{
    filePath_.clear();
    untitledDisplayName_ = suggestedFileName.trimmed().isEmpty() ? tr("Untitled.th") : suggestedFileName.trimmed();
    refreshHighlighterSourceDocumentType();
    fileEncodingName_ = QStringLiteral("UTF-8");
    fileEncodingLabel_ = QStringLiteral("UTF-8");
    cleanEncodingNameSnapshot_ = fileEncodingName_;
    encodingStatusNote_.clear();
    loading_ = true;
    if (editor_ != nullptr) {
        editor_->setPlainText(contents);
        editor_->moveCursor(QTextCursor::End);
        editor_->document()->setModified(true);
    }
    loading_ = false;
    cleanTextSnapshot_ = contents;
    currentLineNumber_ = 1;
    currentColumnNumber_ = 1;
    highlightedLineNumber_ = 1;
    blockDetailsSelectedLineNumber_ = 0;
    blockDetailsSelectedKind_.clear();
    dirty_ = true;
    refreshBlocksModeAvailability();
    setBlocksModeActive(false);
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    populateBlockToolbox();
    refreshEditorModeUi();
    refreshCurrentLineHighlight();
    refreshTitle();
    refreshStatus();
    updateContextHelp();
    emit dirtyStateChanged(true);
    emit documentTextChanged();
}

void TextEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    if (documentController_ != nullptr) {
        documentController_->setProjectRootPath(projectRootPath);
    }
}

bool TextEditorTab::save(QString *errorMessage)
{
    return documentController_ != nullptr
        && documentController_->save(errorMessage);
}

bool TextEditorTab::saveAs(const QString &filePath, QString *errorMessage)
{
    if (filePath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No save path was selected.");
        }
        return false;
    }

    const QString previousPath = filePath_;
    filePath_ = filePath;
    const bool saved = save(errorMessage);
    if (!saved) {
        filePath_ = previousPath;
        refreshHighlighterSourceDocumentType();
        refreshTitle();
        refreshStatus();
        return false;
    }

    untitledDisplayName_.clear();
    refreshHighlighterSourceDocumentType();
    refreshBlocksModeAvailability();
    refreshEditorModeUi();
    refreshTitle();
    refreshStatus();
    return true;
}

void TextEditorTab::setModeSelectorVisible(bool visible)
{
    modeSelectorRequestedVisible_ = visible;
    TextEditorTabInteractionController::ModeSelectorActions actions;
    if (modeRow_ != nullptr) {
        actions.setModeRowVisible = [this](bool rowVisible) {
            modeRow_->setVisible(rowVisible);
        };
        actions.setModeRowMaximumHeight = [this](int maximumHeight) {
            modeRow_->setMaximumHeight(maximumHeight);
        };
        actions.setModeRowMinimumHeight = [this](int minimumHeight) {
            modeRow_->setMinimumHeight(minimumHeight);
        };
    }

    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        actions.invalidateRootLayout = [rootLayout]() {
            rootLayout->invalidate();
        };
        actions.activateRootLayout = [rootLayout]() {
            rootLayout->activate();
        };
    }

    const int modeRowSizeHintHeight = modeRow_ == nullptr ? 0 : modeRow_->sizeHint().height();
    TextEditorTabInteractionController::applyModeSelectorVisibility(modeSelectorRequestedVisible_,
                                                                    modeRowSizeHintHeight,
                                                                    actions);
}

TextEditorTab::EditorMode TextEditorTab::editorMode() const
{
    return blocksModeActive_ ? EditorMode::Blocks : EditorMode::Raw;
}

bool TextEditorTab::isBlocksModeAvailable() const
{
    return isBlocksModeSupportedForCurrentFile();
}

bool TextEditorTab::hasRightPanel() const
{
    if (editorMode() == EditorMode::Blocks) {
        return blockEditorSplitter_ != nullptr && blockDetailsPanel_ != nullptr;
    }
    return editorHelpSplitter_ != nullptr && helpPanel_ != nullptr;
}

bool TextEditorTab::isRightPanelCollapsed() const
{
    return editorMode() == EditorMode::Blocks
        ? blockInspectorCollapsed_
        : helpCollapsed_;
}

QString TextEditorTab::rightPanelLabel() const
{
    return editorMode() == EditorMode::Blocks
        ? tr("Block Inspector")
        : tr("Help");
}

void TextEditorTab::setRightPanelCollapsed(bool collapsed)
{
    if (editorMode() == EditorMode::Blocks) {
        setBlockInspectorCollapsed(collapsed);
        return;
    }

    setHelpCollapsed(collapsed);
}

void TextEditorTab::setInitialEditorMode(EditorMode mode)
{
    if (!filePath_.isEmpty()) {
        setEditorMode(mode);
        return;
    }

    blocksModeActive_ = mode == EditorMode::Blocks;
}

void TextEditorTab::setEditorMode(EditorMode mode)
{
    setBlocksModeActive(mode == EditorMode::Blocks);
}

bool TextEditorTab::isBlocksModeSupportedForCurrentFile() const
{
    return editorModeController_ != nullptr
        && editorModeController_->isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::refreshBlocksModeAvailability()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshBlocksModeAvailability();
    }
}

void TextEditorTab::refreshHighlighterSourceDocumentType()
{
    if (highlighter_ != nullptr) {
        highlighter_->setSourceDocumentType(sourceDocumentType());
    }
}

void TextEditorTab::setBlocksModeActive(bool active)
{
    if (editorModeController_ != nullptr) {
        editorModeController_->setBlocksModeActive(active);
    }
}

void TextEditorTab::setBlockInspectorCollapsed(bool collapsed)
{
    if (blockEditorSplitter_ == nullptr || blockDetailsPanel_ == nullptr) {
        return;
    }
    if (blockInspectorCollapsed_ == collapsed) {
        return;
    }

    const QList<int> sizes = blockEditorSplitter_->sizes();
    if (collapsed) {
        if (sizes.size() >= 3 && sizes.at(2) > 0) {
            blockInspectorPanelExtent_ = sizes.at(2);
        }
        blockInspectorCollapsed_ = true;
        blockEditorSplitter_->setSizes({sizes.value(0, 220), qMax(1, sizes.value(1, 980) + sizes.value(2, 0)), 0});
        return;
    }

    blockInspectorCollapsed_ = false;
    int totalWidth = 0;
    for (const int size : sizes) {
        totalWidth += size;
    }
    if (totalWidth <= 0) {
        blockEditorSplitter_->setSizes({220, 980, blockInspectorPanelExtent_});
        return;
    }

    const int inspectorWidth = qBound(blockDetailsPanel_->minimumWidth(),
                                      blockInspectorPanelExtent_,
                                      qMax(blockDetailsPanel_->minimumWidth(), totalWidth - 360));
    const int toolboxWidth = qMax(120, sizes.value(0, 220));
    const int canvasWidth = qMax(240, totalWidth - toolboxWidth - inspectorWidth);
    blockEditorSplitter_->setSizes({toolboxWidth, canvasWidth, inspectorWidth});
}

bool TextEditorTab::ensureEncodingRootDirectiveForBlocks()
{
    return editorModeController_ != nullptr
        && editorModeController_->ensureEncodingRootDirectiveForBlocks();
}

void TextEditorTab::refreshEditorModeUi()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshEditorModeUi();
    }
}

void TextEditorTab::handleRawModeRequested()
{
    TextEditorTabInteractionController::ModeRequestActions actions;
    actions.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    TextEditorTabInteractionController::handleModeRequest(false, actions);
}

void TextEditorTab::handleBlocksModeRequested()
{
    TextEditorTabInteractionController::ModeRequestActions actions;
    actions.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    TextEditorTabInteractionController::handleModeRequest(true, actions);
}
}
