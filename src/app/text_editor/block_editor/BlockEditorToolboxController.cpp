#include "BlockEditorToolboxController.h"

#include "BlockEditorCanvasItem.h"
#include "BlockEditorDirectiveRules.h"
#include "../TextEditorCommandMetadata.h"
#include "../TextEditorSourceSnapshotContext.h"

#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionFileTypes.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceSnapshotCache.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

void appendUnique(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}
}

namespace TherionStudio
{
namespace
{
enum class BlockToolboxDocumentKind
{
    Unknown,
    DataFile,
    ConfigFile,
};

BlockToolboxDocumentKind documentKindForPath(const QString &documentPath)
{
    const QString trimmedPath = documentPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return BlockToolboxDocumentKind::Unknown;
    }
    if (isTherionConfigFilePath(trimmedPath)) {
        return BlockToolboxDocumentKind::ConfigFile;
    }

    const QString suffix = QFileInfo(trimmedPath).suffix().trimmed().toLower();
    if (suffix == QStringLiteral("th") || suffix == QStringLiteral("th2")) {
        return BlockToolboxDocumentKind::DataFile;
    }
    return BlockToolboxDocumentKind::Unknown;
}

bool sourceFileBelongsToCreatingDataFilesChapter(const QString &sourceFile)
{
    return sourceFile.contains(QStringLiteral("therion/thbook/ch02.tex"), Qt::CaseInsensitive);
}

bool sourceFileBelongsToProcessingDataChapter(const QString &sourceFile)
{
    return sourceFile.contains(QStringLiteral("therion/thbook/ch03.tex"), Qt::CaseInsensitive);
}

bool isTh2MapObjectCommand(const QString &commandToken)
{
    const QString normalized = normalizeDirective(commandToken);
    return normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area");
}

bool isDataBodyOnlyToolboxCommand(const QString &contextToken, const QString &commandToken)
{
    return normalizeDirective(contextToken) == QStringLiteral("centerline")
        && normalizeDirective(commandToken) == QStringLiteral("extend");
}
}

BlockEditorToolboxController::BlockEditorToolboxController(BlockEditorToolboxContext context)
    : context_(std::move(context))
{
}

QLineEdit *BlockEditorToolboxController::filterEdit() const
{
    return context_.filterEdit != nullptr ? *context_.filterEdit : nullptr;
}

QListWidget *BlockEditorToolboxController::toolboxList() const
{
    return context_.toolboxList != nullptr ? *context_.toolboxList : nullptr;
}

QComboBox *BlockEditorToolboxController::scopeCombo() const
{
    return context_.scopeCombo != nullptr ? *context_.scopeCombo : nullptr;
}

QGraphicsScene *BlockEditorToolboxController::canvasScene() const
{
    return context_.canvasScene != nullptr ? *context_.canvasScene : nullptr;
}

QPlainTextEdit *BlockEditorToolboxController::editor() const
{
    return context_.editor != nullptr ? *context_.editor : nullptr;
}

QString BlockEditorToolboxController::normalizeCompletionContext(const QString &contextToken) const
{
    return context_.normalizeCompletionContext ? context_.normalizeCompletionContext(contextToken) : contextToken.trimmed().toLower();
}

bool BlockEditorToolboxController::isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const
{
    return context_.isCompatibleChildKindForBlocks ? context_.isCompatibleChildKindForBlocks(parentKind, childKind) : false;
}

bool BlockEditorToolboxController::isCommandAllowedForDocument(const QString &commandToken) const
{
    if (context_.commandMetadata == nullptr) {
        return true;
    }

    const QString normalizedCommand = normalizeDirective(commandToken);
    if (normalizedCommand.isEmpty()) {
        return false;
    }

    const BlockToolboxDocumentKind documentKind = documentKindForPath(context_.documentPath ? context_.documentPath() : QString());
    if (normalizedCommand == QStringLiteral("comment")) {
        return true;
    }
    if (isMapObjectReferenceKind(normalizedCommand)) {
        return documentKind != BlockToolboxDocumentKind::ConfigFile;
    }
    if (documentKind == BlockToolboxDocumentKind::Unknown) {
        return true;
    }

    const QString sourceFile = context_.commandMetadata->commandSourceFileByToken.value(normalizedCommand);
    if (sourceFile.trimmed().isEmpty()) {
        return true;
    }

    switch (documentKind) {
    case BlockToolboxDocumentKind::DataFile:
        return sourceFileBelongsToCreatingDataFilesChapter(sourceFile)
            && !isTh2MapObjectCommand(normalizedCommand);
    case BlockToolboxDocumentKind::ConfigFile:
        return sourceFileBelongsToProcessingDataChapter(sourceFile);
    case BlockToolboxDocumentKind::Unknown:
        break;
    }
    return true;
}

QString BlockEditorToolboxController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorToolboxController", text);
}

void BlockEditorToolboxController::populateBlockToolbox()
{
    if ((context_.tearingDown != nullptr && *context_.tearingDown) || toolboxList() == nullptr || context_.commandMetadata == nullptr) {
        return;
    }

    toolboxList()->clear();
    const QString filterText = filterEdit() != nullptr
        ? filterEdit()->text().trimmed().toLower()
        : QString();

    auto labelForCommand = [](const QString &commandToken) {
        QStringList parts = commandToken.split(QLatin1Char('-'), Qt::SkipEmptyParts);
        for (QString &part : parts) {
            if (!part.isEmpty()) {
                part[0] = part.at(0).toUpper();
            }
        }
        return parts.join(QLatin1Char(' '));
    };

    QString selectedScope = QStringLiteral("all");
    if (scopeCombo() != nullptr) {
        selectedScope = scopeCombo()->currentData().toString().trimmed().toLower();
    }

    QString effectiveScope = selectedScope;
    if (effectiveScope == QStringLiteral("__auto__")) {
        effectiveScope = selectedBlockInsertionContextToken();
    }
    if (effectiveScope != QStringLiteral("all")) {
        effectiveScope = normalizeCompletionContext(effectiveScope);
        if (effectiveScope.isEmpty()) {
            effectiveScope = QStringLiteral("none");
        }
    }

    QStringList contextsToShow;
    if (effectiveScope == QStringLiteral("all")) {
        appendUnique(contextsToShow, QStringLiteral("none"));
        QStringList remainingContexts;
        for (auto it = (*context_.commandMetadata).contextCommandTokens.cbegin(); it != (*context_.commandMetadata).contextCommandTokens.cend(); ++it) {
            const QString normalizedContext = normalizeCompletionContext(it.key());
            if (normalizedContext.isEmpty()
                || normalizedContext == QStringLiteral("all")
                || normalizedContext == QStringLiteral("none")) {
                continue;
            }
            appendUnique(remainingContexts, normalizedContext);
        }
        std::sort(remainingContexts.begin(), remainingContexts.end(), [](const QString &left, const QString &right) {
            return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
        });
        appendUniqueList(contextsToShow, remainingContexts);
    } else {
        appendUnique(contextsToShow, effectiveScope);
    }

    contextsToShow.erase(std::remove_if(contextsToShow.begin(),
                                        contextsToShow.end(),
                                        [this](const QString &contextToken) {
                                            if (contextToken == QStringLiteral("map")) {
                                                return false;
                                            }
                                            QStringList candidates = (*context_.commandMetadata).contextCommandTokens.value(contextToken);
                                            if (contextToken == QStringLiteral("none")) {
                                                appendUniqueList(candidates, (*context_.commandMetadata).contextCommandTokens.value(QStringLiteral("none")));
                                            }
                                            appendUniqueList(candidates, (*context_.commandMetadata).contextCommandTokens.value(QStringLiteral("all")));
                                            return candidates.isEmpty();
                                        }),
                         contextsToShow.end());

    int insertedRows = 0;
    for (const QString &contextToken : contextsToShow) {
        QStringList sectionCommands = (*context_.commandMetadata).contextCommandTokens.value(contextToken);
        if (contextToken == QStringLiteral("none")) {
            appendUniqueList(sectionCommands, (*context_.commandMetadata).contextCommandTokens.value(QStringLiteral("none")));
        }
        appendUniqueList(sectionCommands, (*context_.commandMetadata).contextCommandTokens.value(QStringLiteral("all")));
        appendUnique(sectionCommands, QStringLiteral("comment"));
        if (contextToken == QStringLiteral("map")) {
            appendUnique(sectionCommands, mapObjectReferenceKind());
        }

        QStringList visibleCommands;
        for (const QString &command : sectionCommands) {
            const QString normalizedCommand = normalizeDirective(command);
            if (normalizedCommand.isEmpty()) {
                continue;
            }
            if (normalizedCommand == QStringLiteral("all")
                || normalizedCommand == QStringLiteral("none")
                || normalizedCommand == QStringLiteral("encoding")
                || normalizedCommand.startsWith(QStringLiteral("end"))) {
                continue;
            }
            if (isDataBodyOnlyToolboxCommand(contextToken, normalizedCommand)) {
                continue;
            }
            const QString parentKind = contextToken == QStringLiteral("none") ? QString() : contextToken;
            if (!isCompatibleChildKindForBlocks(parentKind, normalizedCommand)) {
                continue;
            }
            if (!isCommandAllowedForDocument(normalizedCommand)) {
                continue;
            }
            const QString closingDirective = completionClosingDirectiveForOpening(normalizedCommand);
            const bool unsupportedContainer = !closingDirective.isEmpty()
                && !isContainerBlockDirective(normalizedCommand);
            if (unsupportedContainer) {
                continue;
            }

            const QString label = isMapObjectReferenceKind(normalizedCommand)
                ? tr("Object Reference")
                : labelForCommand(normalizedCommand);
            const QString searchable = normalizedCommand + QStringLiteral(" ") + label.toLower();
            if (!filterText.isEmpty() && !searchable.contains(filterText)) {
                continue;
            }
            appendUnique(visibleCommands, normalizedCommand);
        }

        std::sort(visibleCommands.begin(), visibleCommands.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });

        if (visibleCommands.isEmpty()) {
            continue;
        }

        auto *categoryItem =
            new QListWidgetItem(QStringLiteral("[%1]").arg(contextDisplayLabel(contextToken)), toolboxList());
        categoryItem->setFlags(Qt::ItemIsEnabled);
        ++insertedRows;

        for (const QString &commandToken : visibleCommands) {
            const QString label = isMapObjectReferenceKind(commandToken)
                ? tr("Object Reference")
                : labelForCommand(commandToken);
            auto *item = new QListWidgetItem(QStringLiteral("  %1").arg(label), toolboxList());
            item->setData(Qt::UserRole, commandToken);
            item->setToolTip(tr("Drag to canvas."));
            ++insertedRows;
        }
    }

    if (insertedRows == 0) {
        auto *emptyItem = new QListWidgetItem(tr("[No commands match filter]"), toolboxList());
        emptyItem->setFlags(Qt::ItemIsEnabled);
    }

    updateBlockToolboxScopeLabel();
}

void BlockEditorToolboxController::populateBlockToolboxScopeCombo()
{
    if (scopeCombo() == nullptr) {
        return;
    }

    const QString previousScope = scopeCombo()->currentData().toString().trimmed().toLower();
    const QSignalBlocker scopeSignalBlocker(scopeCombo());
    scopeCombo()->clear();
    scopeCombo()->addItem(tr("Auto (selected block)"), QStringLiteral("__auto__"));
    scopeCombo()->addItem(tr("All"), QStringLiteral("all"));
    scopeCombo()->addItem(tr("Top-level"), QStringLiteral("none"));

    QStringList dynamicContexts;
    for (auto contextIterator = (*context_.commandMetadata).contextCommandTokens.cbegin(); contextIterator != (*context_.commandMetadata).contextCommandTokens.cend(); ++contextIterator) {
        const QString normalizedContext = normalizeCompletionContext(contextIterator.key());
        if (normalizedContext.isEmpty()
            || normalizedContext == QStringLiteral("all")
            || normalizedContext == QStringLiteral("none")) {
            continue;
        }
        appendUnique(dynamicContexts, normalizedContext);
    }
    std::sort(dynamicContexts.begin(), dynamicContexts.end(), [this](const QString &left, const QString &right) {
        return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
    });

    for (const QString &contextToken : dynamicContexts) {
        scopeCombo()->addItem(contextDisplayLabel(contextToken), contextToken);
    }

    int restoreIndex = 0;
    if (!previousScope.isEmpty()) {
        restoreIndex = scopeCombo()->findData(previousScope);
    }
    if (restoreIndex < 0) {
        restoreIndex = 0;
    }
    scopeCombo()->setCurrentIndex(restoreIndex);
}

QString BlockEditorToolboxController::selectedBlockInsertionContextToken() const
{
    if (canvasScene() == nullptr) {
        return QStringLiteral("none");
    }

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };
    BlockCanvasItem *selectedBlock = nullptr;
    if (!canvasScene()->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockFromItem(canvasScene()->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = resolveBlockFromItem(canvasScene()->focusItem());
    }
    if (selectedBlock == nullptr) {
        return QStringLiteral("none");
    }

    const QString selectedKind = normalizeDirective(selectedBlock->kind());
    const QString selectedContext = normalizeCompletionContext(selectedKind);
    if (selectedBlock->isContainerOpen() && !selectedContext.isEmpty()) {
        return selectedContext;
    }

    if (editor() == nullptr || selectedBlock->lineNumber() <= 0) {
        return QStringLiteral("none");
    }

    const TherionStudio::TextEditorSourceSnapshotContext snapshotContext =
        TherionStudio::TextEditorSourceSnapshotContext::fromEditor(editor());
    const TherionSourceLogicalDocument &logicalDocument = snapshotContext.logicalDocument(sourceSnapshotCache_);

    QStringList stack;
    const int lastLine = selectedBlock->lineNumber();
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.startLineNumber >= lastLine) {
            break;
        }

        const QString directive = normalizeDirective(command.parsed.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            for (int stackIndex = stack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (stack.at(stackIndex) == openingDirective) {
                    stack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (isContainerDirectiveInstance(directive, command.parsed)) {
            stack.append(directive);
        }
    }

    if (stack.isEmpty()) {
        return QStringLiteral("none");
    }

    const QString parentContext = normalizeCompletionContext(stack.constLast());
    return parentContext.isEmpty() ? QStringLiteral("none") : parentContext;
}

void BlockEditorToolboxController::updateBlockToolboxScopeLabel()
{
    if (scopeCombo() == nullptr) {
        return;
    }

    QString selectedScope = QStringLiteral("all");
    selectedScope = scopeCombo()->currentData().toString().trimmed().toLower();

    if (selectedScope == QStringLiteral("__auto__")) {
        const QString contextToken = selectedBlockInsertionContextToken();
        scopeCombo()->setToolTip(tr("Auto scope currently resolves to: %1.")
                                                .arg(contextDisplayLabel(contextToken)));
        return;
    }

    if (selectedScope == QStringLiteral("all")) {
        scopeCombo()->setToolTip(tr("Shows commands from all supported contexts."));
        return;
    }

    const QString normalizedScope = normalizeCompletionContext(selectedScope);
    const QString labelText = contextDisplayLabel(normalizedScope.isEmpty() ? QStringLiteral("none") : normalizedScope);
    scopeCombo()->setToolTip(tr("Shows commands for: %1.").arg(labelText));
}

}
