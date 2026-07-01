#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorDetailsPaneController.h"
#include "block_editor/BlockEditorSelectionDetailsController.h"
#include "block_editor/BlockEditorSourceController.h"
#include "block_editor/BlockEditorToolboxDetailsController.h"

#include <QPlainTextEdit>

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsSelectionDetailsController()
{
    blockDetailsSelectionDetailsController_ =
        std::make_unique<BlockEditorSelectionDetailsController>(blockEditorSelectionDetailsContext());
}

BlockEditorSelectionDetailsContext TextEditorTab::blockEditorSelectionDetailsContext()
{
    BlockEditorSelectionDetailsContext context;
    context.tearingDown = &tearingDown_;
    context.detailsPopulating = &blockDetailsPopulating_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.selectedKind = &blockDetailsSelectedKind_;
    context.baseStatusText = &blockDetailsBaseStatusText_;
    context.commentMarker = &blockDetailsCommentMarker_;
    context.editPanel = blockDetailsEditPanel_;
    context.titleLabel = blockDetailsTitleLabel_;
    context.statusLabel = blockDetailsStatusLabel_;
    context.primaryFieldLabel = blockDetailsPrimaryFieldLabel_;
    context.secondaryFieldLabel = blockDetailsSecondaryFieldLabel_;
    context.commentFieldLabel = blockDetailsCommentFieldLabel_;
    context.optionsLabel = blockDetailsOptionsLabel_;
    context.optionArgsLabel = blockDetailsOptionArgsLabel_;
    context.primaryFieldStack = blockDetailsPrimaryFieldStack_;
    context.idEdit = blockDetailsIdEdit_;
    context.readOnlyValueLabel = blockDetailsReadOnlyValueLabel_;
    context.additionalPositionalEdit = blockDetailsAdditionalPositionalEdit_;
    context.commentEdit = blockDetailsCommentEdit_;
    context.readingsTagEditor = blockDetailsReadingsTagEditor_;
    context.secondaryFieldStack = blockDetailsSecondaryFieldStack_;
    context.optionsTable = blockDetailsOptionsTable_;
    context.optionArgsPanel = blockDetailsOptionArgsPanel_;
    context.addOptionButton = blockDetailsAddOptionButton_;
    context.removeOptionButton = blockDetailsRemoveOptionButton_;
    context.dataRowsButton = blockDetailsDataRowsButton_;
    context.helpBrowser = blockDetailsHelpBrowser_;
    context.commandMetadata = &commandMetadata_;
    context.loadNormalizedLines = [this](QStringList *lines) {
        const BlockEditorSourceController source(blockEditorSourceContext());
        if (!source.hasEditor() || lines == nullptr) {
            return false;
        }
        *lines = source.normalizedLines();
        return true;
    };
    context.loadSourceSnapshot = [this](QString *sourceText, int *revisionId) {
        const BlockEditorSourceContext sourceContext = blockEditorSourceContext();
        const BlockEditorSourceController source(sourceContext);
        if (!source.hasEditor() || sourceText == nullptr || revisionId == nullptr) {
            return false;
        }
        *sourceText = source.text();
        *revisionId = sourceContext.editor != nullptr && sourceContext.editor->document() != nullptr
            ? sourceContext.editor->document()->revision()
            : 0;
        return true;
    };
    context.clearDetailsPane = [this]() {
        clearBlockDetailsPane();
    };
    context.normalizedDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    context.supportsDetailsPaneForKind = [this](const QString &kind) {
        return supportsDetailsPaneForKind(kind);
    };
    context.isContainerBlockDirectiveForBlocks = [this](const QString &directive) {
        return isContainerBlockDirectiveForBlocks(directive);
    };
    context.commandSupportsInlineIdField = [this](const QString &commandToken) {
        return commandSupportsInlineIdField(commandToken);
    };
    context.commandHasRequiredIdArgument = [this](const QString &commandToken) {
        return commandHasRequiredIdArgument(commandToken);
    };
    context.commandArgumentSignaturesFor = [this](const QString &commandToken) {
        return commandArgumentSignaturesFor(commandToken);
    };
    context.setDetailsMode = [this](BlockEditorSelectionDetailsMode mode) {
        switch (mode) {
        case BlockEditorSelectionDetailsMode::StructuredOptions:
            blockDetailsMode_ = BlockDetailsMode::StructuredOptions;
            break;
        case BlockEditorSelectionDetailsMode::SimpleValue:
            blockDetailsMode_ = BlockDetailsMode::SimpleValue;
            break;
        case BlockEditorSelectionDetailsMode::DataHeader:
            blockDetailsMode_ = BlockDetailsMode::DataHeader;
            break;
        case BlockEditorSelectionDetailsMode::Unsupported:
            blockDetailsMode_ = BlockDetailsMode::Unsupported;
            break;
        }
    };
    context.setReadingsTagEditor = [this](const QString &placeholderText,
                                          const QStringList &suggestions,
                                          const QStringList &tokens) {
        setBlockDetailsReadingsTagEditor(placeholderText, suggestions, tokens);
    };
    context.installLineEditCompleter = [this](QLineEdit *lineEdit, const QStringList &values) {
        installBlockDetailsLineEditCompleter(lineEdit, values);
    };
    context.refreshOptionArgumentEditors = [this]() {
        refreshBlockDetailsOptionArgumentEditors();
    };
    context.updateHelpForCurrentFocus = [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    };
    context.refreshApplyState = [this]() {
        refreshBlockDetailsApplyState();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

void TextEditorTab::buildBlockDetailsPaneController()
{
    blockDetailsPaneController_ =
        std::make_unique<BlockEditorDetailsPaneController>(blockEditorDetailsPaneContext());
}

BlockEditorDetailsPaneContext TextEditorTab::blockEditorDetailsPaneContext()
{
    BlockEditorDetailsPaneContext context;
    context.tearingDown = &tearingDown_;
    context.editPanel = blockDetailsEditPanel_;
    context.titleLabel = blockDetailsTitleLabel_;
    context.statusLabel = blockDetailsStatusLabel_;
    context.primaryFieldStack = blockDetailsPrimaryFieldStack_;
    context.idEdit = blockDetailsIdEdit_;
    context.readOnlyValueLabel = blockDetailsReadOnlyValueLabel_;
    context.additionalPositionalEdit = blockDetailsAdditionalPositionalEdit_;
    context.secondaryFieldStack = blockDetailsSecondaryFieldStack_;
    context.commentEdit = blockDetailsCommentEdit_;
    context.primaryFieldLabel = blockDetailsPrimaryFieldLabel_;
    context.secondaryFieldLabel = blockDetailsSecondaryFieldLabel_;
    context.commentFieldLabel = blockDetailsCommentFieldLabel_;
    context.optionsLabel = blockDetailsOptionsLabel_;
    context.optionsTable = blockDetailsOptionsTable_;
    context.optionArgsLabel = blockDetailsOptionArgsLabel_;
    context.optionArgsPanel = blockDetailsOptionArgsPanel_;
    context.addOptionButton = blockDetailsAddOptionButton_;
    context.removeOptionButton = blockDetailsRemoveOptionButton_;
    context.dataRowsButton = blockDetailsDataRowsButton_;
    context.helpBrowser = blockDetailsHelpBrowser_;
    context.resetDetailsState = [this](bool teardownOnly) {
        blockDetailsMode_ = BlockDetailsMode::None;
        blockDetailsSelectedLineNumber_ = 0;
        blockDetailsSelectedKind_.clear();
        if (!teardownOnly) {
            blockDetailsBaseStatusText_.clear();
        }
    };
    context.setDetailsPopulating = [this](bool populating) {
        blockDetailsPopulating_ = populating;
    };
    context.clearReadingsTagEditor = [this]() {
        setBlockDetailsReadingsTagEditor(QString(), {}, {});
    };
    context.refreshOptionArgumentEditors = [this]() {
        refreshBlockDetailsOptionArgumentEditors();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

void TextEditorTab::buildBlockDetailsToolboxDetailsController()
{
    blockDetailsToolboxDetailsController_ =
        std::make_unique<BlockEditorToolboxDetailsController>(blockEditorToolboxDetailsContext());
}

BlockEditorToolboxDetailsContext TextEditorTab::blockEditorToolboxDetailsContext()
{
    BlockEditorToolboxDetailsContext context;
    context.tearingDown = &tearingDown_;
    context.scene = blockCanvasScene_;
    context.editPanel = blockDetailsEditPanel_;
    context.statusLabel = blockDetailsStatusLabel_;
    context.primaryFieldLabel = blockDetailsPrimaryFieldLabel_;
    context.secondaryFieldLabel = blockDetailsSecondaryFieldLabel_;
    context.commentFieldLabel = blockDetailsCommentFieldLabel_;
    context.idEdit = blockDetailsIdEdit_;
    context.additionalPositionalEdit = blockDetailsAdditionalPositionalEdit_;
    context.secondaryFieldStack = blockDetailsSecondaryFieldStack_;
    context.commentEdit = blockDetailsCommentEdit_;
    context.optionsTable = blockDetailsOptionsTable_;
    context.addOptionButton = blockDetailsAddOptionButton_;
    context.removeOptionButton = blockDetailsRemoveOptionButton_;
    context.optionArgsLabel = blockDetailsOptionArgsLabel_;
    context.optionArgsPanel = blockDetailsOptionArgsPanel_;
    context.helpBrowser = blockDetailsHelpBrowser_;
    context.helpInspector = blockDetailsHelpInspector_;
    context.commandMetadata = &commandMetadata_;
    context.normalizeDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    context.beginToolboxCommandDetails = [this](const QString &normalizedCommand, const QString &statusText) {
        blockDetailsPopulating_ = true;
        blockDetailsMode_ = BlockDetailsMode::Unsupported;
        blockDetailsSelectedLineNumber_ = 0;
        blockDetailsSelectedKind_ = normalizedCommand;
        blockDetailsBaseStatusText_ = statusText;
    };
    context.setDetailsPopulating = [this](bool populating) {
        blockDetailsPopulating_ = populating;
    };
    context.refreshOptionArgumentEditors = [this]() {
        refreshBlockDetailsOptionArgumentEditors();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

void TextEditorTab::buildBlockDetailsCanvasSelectionController()
{
    blockDetailsCanvasSelectionController_ =
        std::make_unique<BlockEditorCanvasSelectionController>(blockEditorCanvasSelectionContext());
}

BlockEditorCanvasSelectionContext TextEditorTab::blockEditorCanvasSelectionContext()
{
    BlockEditorCanvasSelectionContext context;
    context.tearingDown = &tearingDown_;
    context.scene = blockCanvasScene_;
    context.view = blockCanvasView_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.selectedKind = &blockDetailsSelectedKind_;
    context.clearDetailsPane = [this]() {
        clearBlockDetailsPane();
    };
    context.loadDetailsForSelection = [this](const QString &kind, int lineNumber) {
        return loadBlockDetailsForSelection(kind, lineNumber);
    };
    context.setDetailsSelectionFallback = [this](int lineNumber, const QString &kind) {
        blockDetailsSelectedLineNumber_ = lineNumber;
        blockDetailsSelectedKind_ = normalizedDirectiveToken(kind);
    };
    return context;
}
}
