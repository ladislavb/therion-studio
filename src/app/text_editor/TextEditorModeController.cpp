#include "TextEditorModeController.h"

#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorSourceText.h"

#include "../../core/TherionFileTypes.h"
#include "../../core/TherionSourceDocument.h"

#include <QFileInfo>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>

#include <utility>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

TextEditorModeController::TextEditorModeController(TextEditorModeContext context)
    : context_(std::move(context))
{
}

bool TextEditorModeController::isBlocksModeSupportedForCurrentFile() const
{
    const QString documentName = context_.filePath != nullptr && !context_.filePath->trimmed().isEmpty()
        ? *context_.filePath
        : (context_.untitledDisplayName != nullptr ? *context_.untitledDisplayName : QString());
    if (documentName.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(documentName);
    const QString suffix = fileInfo.suffix().trimmed().toLower();
    const QString fileName = fileInfo.fileName();
    return suffix == QStringLiteral("th")
        || isTherionConfigFileName(fileName);
}

void TextEditorModeController::refreshBlocksModeAvailability()
{
    if (context_.blocksModeActive == nullptr) {
        return;
    }

    const bool supported = isBlocksModeSupportedForCurrentFile();
    if (context_.blocksModeButton != nullptr) {
        context_.blocksModeButton->setEnabled(supported);
    }
    if (!supported) {
        *context_.blocksModeActive = false;
    }
}

void TextEditorModeController::setBlocksModeActive(bool active)
{
    if (context_.blocksModeActive == nullptr) {
        return;
    }

    const bool targetActive = active && isBlocksModeSupportedForCurrentFile();
    if (*context_.blocksModeActive == targetActive) {
        refreshEditorModeUi();
        return;
    }

    *context_.blocksModeActive = targetActive;
    if (*context_.blocksModeActive) {
        if (context_.hideFindBar) {
            context_.hideFindBar();
        }
        if (context_.rebuildBlocksCanvasFromText) {
            context_.rebuildBlocksCanvasFromText();
        }
        if (context_.populateBlockToolbox) {
            context_.populateBlockToolbox();
        }
    } else if (context_.editor != nullptr) {
        context_.editor->setFocus();
    }
    refreshEditorModeUi();
    if (context_.editorModeChanged) {
        context_.editorModeChanged();
    }
}

bool TextEditorModeController::ensureEncodingRootDirectiveForBlocks()
{
    if (context_.editor == nullptr
        || context_.fileEncodingName == nullptr
        || context_.enforcingEncodingRootDirective == nullptr
        || *context_.enforcingEncodingRootDirective
        || !context_.replaceTextForSystemNormalization) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    if (lines.size() == 1 && lines.first().isEmpty()) {
        lines.clear();
    }

    QString encodingName = context_.fileEncodingName->trimmed();
    if (encodingName.isEmpty()) {
        encodingName = QStringLiteral("utf-8");
    }
    const QString desiredEncodingLine =
        QStringLiteral("encoding %1").arg(encodingName.toLower());

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    QStringList normalizedLines;
    normalizedLines.reserve(lines.size() + 1);
    normalizedLines.append(desiredEncodingLine);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(lineIndex + 1);
        if (sourceLine != nullptr && isEncodingDirective(sourceLine->sourceLine.parsed.directive)) {
            continue;
        }
        normalizedLines.append(lines.at(lineIndex));
    }

    const QString normalizedContents = blockEditorJoinSourceLines(contents, normalizedLines);
    if (normalizedContents == blockEditorJoinSourceLines(contents, lines)) {
        return false;
    }

    *context_.enforcingEncodingRootDirective = true;
    const bool applied = context_.replaceTextForSystemNormalization(normalizedContents);
    *context_.enforcingEncodingRootDirective = false;
    return applied;
}

void TextEditorModeController::refreshEditorModeUi()
{
    if (context_.blocksModeActive == nullptr
        || context_.rawModeButton == nullptr
        || context_.blocksModeButton == nullptr
        || context_.editorModeStack == nullptr) {
        return;
    }

    context_.rawModeButton->setChecked(!*context_.blocksModeActive);
    context_.blocksModeButton->setChecked(*context_.blocksModeActive);
    if (*context_.blocksModeActive && context_.blocksPanel != nullptr) {
        context_.editorModeStack->setCurrentWidget(context_.blocksPanel);
    } else if (context_.rawEditorPanel != nullptr) {
        context_.editorModeStack->setCurrentWidget(context_.rawEditorPanel);
    }
}
}
