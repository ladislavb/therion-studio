#include "BlockEditorCanvasRebuildController.h"

#include <QCoreApplication>

#include "BlockEditorCanvasItem.h"
#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"

#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceSnapshotCache.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStringList>

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        if (!target.contains(value, Qt::CaseInsensitive)) {
            target.append(value);
        }
    }
}
}

namespace TherionStudio
{
BlockEditorCanvasRebuildController::BlockEditorCanvasRebuildController(BlockEditorCanvasRebuildContext context)
    : context_(std::move(context))
{
}

QString BlockEditorCanvasRebuildController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorCanvasRebuildController", text);
}

void BlockEditorCanvasRebuildController::rebuildBlocksCanvasFromText()
{
    if (context_.scene == nullptr
        || context_.movePreviewLine == nullptr
        || context_.containerBoundaryGuideItems == nullptr
        || context_.containerBoundaryEndYByLine == nullptr
        || context_.commandMetadata == nullptr
        || context_.sourceContext == nullptr
        || context_.handleBlockDeleteRequest == nullptr
        || context_.handleBlockMoveRequest == nullptr
        || context_.updateBlockMovePreview == nullptr
        || context_.clearBlockMovePreview == nullptr
        || context_.refreshBlockDetailsSelectionFromScene == nullptr
        || (context_.tearingDown != nullptr && *context_.tearingDown)) {
        return;
    }

    const BlockEditorSourceContext sourceContext = context_.sourceContext();
    const BlockEditorSourceController source(sourceContext);
    if (!source.hasEditor()) {
        return;
    }

    const int preferredSelectedLine = context_.selectedLineNumber != nullptr ? *context_.selectedLineNumber : 0;
    (*context_.movePreviewLine) = nullptr;
    (*context_.containerBoundaryGuideItems).clear();
    (*context_.containerBoundaryEndYByLine).clear();
    {
        const QSignalBlocker sceneSignalBlocker(context_.scene);
        context_.scene->clear();
    }

    if (!(context_.isBlocksModeSupportedForCurrentFile != nullptr && context_.isBlocksModeSupportedForCurrentFile())) {
        auto *note = context_.scene->addText(
            tr("Blocks mode is currently available only for .th and Therion config files."));
        note->setPos(16.0, 16.0);
        return;
    }

    const QString sourceText = source.text();
    const QStringList lines = blockEditorNormalizedSourceLines(sourceText);
    const QVector<BlockEditorLogicalLine> logicalLines = blockEditorBuildLogicalLines(lines);
    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata sourceMetadata;
    sourceMetadata.revisionId = sourceContext.editor != nullptr && sourceContext.editor->document() != nullptr
        ? sourceContext.editor->document()->revision()
        : 0;
    const TherionSourceLogicalDocument &logicalDocument =
        sourceSnapshotCache.logicalDocument(sourceText, sourceMetadata);
    QHash<int, const TherionSourceLogicalCommand *> logicalCommandsByStartLine;
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        logicalCommandsByStartLine.insert(command.startLineNumber, &command);
    }

    struct StackEntry
    {
        QString directive;
        BlockCanvasItem *item = nullptr;
    };

    QVector<StackEntry> stack;
    QVector<BlockCanvasItem *> roots;
    QVector<BlockCanvasItem *> allItems;

    auto isCommandDirectiveInScope = [this](const QString &commandToken, const QString &scope) {
        QStringList candidates = context_.commandMetadata->contextCommandTokens.value(scope);
        appendUniqueList(candidates, context_.commandMetadata->contextCommandTokens.value(QStringLiteral("all")));
        if (scope == QStringLiteral("none")) {
            appendUniqueList(candidates, context_.commandMetadata->contextCommandTokens.value(QStringLiteral("none")));
        }
        return candidates.contains(commandToken, Qt::CaseInsensitive);
    };

    auto nearestOpenContainerLine = [&stack](const QString &directive) {
        for (int index = stack.size() - 1; index >= 0; --index) {
            if (stack.at(index).directive == directive && stack.at(index).item != nullptr) {
                return stack.at(index).item->lineNumber();
            }
        }
        return 0;
    };

    auto dataEntryEndLine = [&](int dataLineNumber, int fallbackEndLine) {
        if (context_.resolveScopeForCommandAtLine == nullptr) {
            return fallbackEndLine;
        }

        const QString dataScope =
            context_.resolveScopeForCommandAtLine(QStringLiteral("data"), lines, dataLineNumber);
        const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
        const int dataScopeParentLine = nearestOpenContainerLine(dataScope);

        int dataScopeEndLine = lines.size() + 1;
        if (dataScopeParentLine > 0 && !dataScopeClosing.isEmpty()) {
            const int resolvedEndLine = findMatchingBlockEndLine(lines,
                                                                 dataScopeParentLine,
                                                                 dataScope,
                                                                 dataScopeClosing);
            if (resolvedEndLine > dataScopeParentLine) {
                dataScopeEndLine = resolvedEndLine;
            }
        }

        int bodyLastLine = dataScopeEndLine - 1;
        for (int scanLine = dataLineNumber + 1; scanLine <= dataScopeEndLine - 1; ++scanLine) {
            const TherionSourceLogicalCommand *scanLogicalCommand =
                logicalDocument.commandAtPhysicalLine(scanLine);
            if (scanLogicalCommand != nullptr && scanLogicalCommand->startLineNumber != scanLine) {
                continue;
            }
            const TherionParsedLine scanParsedLine = scanLogicalCommand != nullptr
                ? scanLogicalCommand->parsed
                : TherionDocumentParser::parseLine(lines.at(scanLine - 1), scanLine);
            const QString scanDirective = normalizeDirective(scanParsedLine.directive);
            if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                continue;
            }
            if (scanDirective == QStringLiteral("data")
                || (!dataScopeClosing.isEmpty() && scanDirective == dataScopeClosing)
                || (!dataScope.isEmpty() && isCommandDirectiveInScope(scanDirective, dataScope))) {
                bodyLastLine = scanLine - 1;
                break;
            }
        }
        return qMax(fallbackEndLine, bodyLastLine);
    };

    int activeDataEntryEndLine = 0;

    for (const BlockEditorLogicalLine &logicalLine : logicalLines) {
        if (logicalLine.startLine <= activeDataEntryEndLine) {
            continue;
        }
        const TherionSourceLogicalCommand *logicalCommand =
            logicalCommandsByStartLine.value(logicalLine.startLine, nullptr);
        const TherionParsedLine parsedLine = logicalCommand != nullptr
            ? logicalCommand->parsed
            : TherionDocumentParser::parseLine(logicalLine.text, logicalLine.startLine);
        if (isFullLineComment(parsedLine)) {
            BlockCanvasItem *parentItem = nullptr;
            if (!stack.isEmpty()) {
                parentItem = stack.last().item;
            }

            auto *item = new BlockCanvasItem(QStringLiteral("comment"),
                                             parsedLine.commentText.trimmed(),
                                             QString(),
                                             parsedLine.lineNumber,
                                             true,
                                             true,
                                             false,
                                             parentItem);
            item->onDelete = [this](int lineNumber) {
                context_.handleBlockDeleteRequest(lineNumber);
            };
            item->onMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
                context_.handleBlockMoveRequest(lineNumber, scenePos);
            };
            item->onMovePreview = [this](int sourceLineNumber, const QPointF &scenePos, bool active) {
                if (active) {
                    context_.updateBlockMovePreview(sourceLineNumber, scenePos);
                } else {
                    context_.clearBlockMovePreview();
                }
            };
            if (parentItem == nullptr) {
                roots.append(item);
                context_.scene->addItem(item);
            }
            allItems.append(item);
            continue;
        }

        QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            while (!stack.isEmpty()) {
                const QString openingDirective = stack.last().directive;
                const QString expectedClosing = closingDirectiveFor(openingDirective);
                stack.removeLast();
                if (expectedClosing == directive) {
                    break;
                }
            }
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!stack.isEmpty()) {
            activeScope = normalizeDirective(stack.last().directive);
        }

        const bool commandDirective = !isBlockOpeningDirective(directive)
            && isCommandDirectiveInScope(directive, activeScope);
        const bool mapObjectReference =
            isMapObjectReferenceCandidateLine(activeScope, parsedLine, commandDirective);
        bool renderAsUnrecognized = false;
        if (!isBlockOpeningDirective(directive) && !commandDirective && !mapObjectReference) {
            const QString trimmedRawLine = parsedLine.rawText.trimmed();
            const QString firstToken = parsedLine.tokens.isEmpty() ? QString() : normalizeDirective(parsedLine.tokens.first());
            const bool firstTokenLooksLikeDirective = !firstToken.isEmpty()
                && firstToken.at(0).isLetter()
                && !isBlockClosingDirective(firstToken);
            const bool rawLineStartsLikeDirective = !trimmedRawLine.isEmpty() && trimmedRawLine.at(0).isLetter();
            renderAsUnrecognized = firstTokenLooksLikeDirective || rawLineStartsLikeDirective;
            if (!renderAsUnrecognized) {
                continue;
            }
        }

        BlockCanvasItem *parentItem = nullptr;
        if (!stack.isEmpty()) {
            parentItem = stack.last().item;
        }

        if (mapObjectReference) {
            directive = mapObjectReferenceKind();
        } else if (renderAsUnrecognized) {
            directive = unrecognizedKind();
        }

        const QString name = blockDisplayNameForKind(directive, parsedLine);
        const QString inlineComment = parsedLine.commentStart >= 0 ? parsedLine.commentText.trimmed() : QString();
        const bool encodingDirective = isEncodingDirective(directive);
        const bool unrecognizedDirective = isUnrecognizedKind(directive);
        const bool isContainerInstance = isContainerDirectiveInstance(directive, parsedLine);
        auto *item = new BlockCanvasItem(directive,
                                         name,
                                         inlineComment,
                                         parsedLine.lineNumber,
                                         !encodingDirective,
                                         !encodingDirective && !unrecognizedDirective,
                                         isContainerInstance,
                                         parentItem);
        item->onDelete = [this](int lineNumber) {
            context_.handleBlockDeleteRequest(lineNumber);
        };
        item->onMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
            context_.handleBlockMoveRequest(lineNumber, scenePos);
        };
        item->onMovePreview = [this](int sourceLineNumber, const QPointF &scenePos, bool active) {
            if (active) {
                context_.updateBlockMovePreview(sourceLineNumber, scenePos);
            } else {
                context_.clearBlockMovePreview();
            }
        };
        if (parentItem == nullptr) {
            roots.append(item);
            context_.scene->addItem(item);
        }
        allItems.append(item);

        if (!commandDirective && isContainerInstance) {
            stack.append(StackEntry{directive, item});
        } else if (!commandDirective && directive == QStringLiteral("data")) {
            activeDataEntryEndLine = dataEntryEndLine(logicalLine.startLine, logicalLine.endLine);
        }
    }

    if (allItems.isEmpty()) {
        auto *note = context_.scene->addText(tr("No Therion directives found."));
        note->setPos(16.0, 16.0);
        return;
    }

    const bool hasEncodingRoot = std::any_of(roots.cbegin(), roots.cend(), [](const BlockCanvasItem *item) {
        return item != nullptr && isEncodingDirective(item->kind());
    });

    qreal y = 16.0;
    std::function<void(BlockCanvasItem *, int)> layoutTree = [&](BlockCanvasItem *item, int depth) {
        if (item == nullptr) {
            return;
        }
        int visualDepth = depth;
        if (hasEncodingRoot && !isEncodingDirective(item->kind())) {
            // Keep an existing `encoding` directive as visual document root.
            ++visualDepth;
        }
        const QPointF scenePosition(24.0 + (visualDepth * 28.0), y);
        if (QGraphicsItem *parent = item->parentItem(); parent != nullptr) {
            item->setPos(parent->mapFromScene(scenePosition));
        } else {
            item->setPos(scenePosition);
        }
        y += 52.0;

        QList<BlockCanvasItem *> childBlocks;
        const QList<QGraphicsItem *> children = item->childItems();
        for (QGraphicsItem *child : children) {
            auto *childBlock = dynamic_cast<BlockCanvasItem *>(child);
            if (childBlock != nullptr) {
                childBlocks.append(childBlock);
            }
        }
        std::sort(childBlocks.begin(), childBlocks.end(), [](BlockCanvasItem *left, BlockCanvasItem *right) {
            return left->lineNumber() < right->lineNumber();
        });
        for (BlockCanvasItem *childBlock : childBlocks) {
            layoutTree(childBlock, depth + 1);
        }
    };

    for (BlockCanvasItem *root : roots) {
        layoutTree(root, 0);
    }

    QStringList sourceLines = blockEditorNormalizedSourceLines(source.text());

    auto visualSubtreeBottomForSpan = [&allItems](int startLine, int endLine, qreal fallbackBottom) {
        qreal bottom = fallbackBottom;
        for (BlockCanvasItem *blockItem : allItems) {
            if (blockItem == nullptr) {
                continue;
            }
            const int blockLine = blockItem->lineNumber();
            if (blockLine < startLine || blockLine > endLine) {
                continue;
            }
            bottom = qMax(bottom, blockItem->sceneBoundingRect().bottom());
        }
        return bottom;
    };

    struct ContainerBoundary
    {
        BlockCanvasItem *item = nullptr;
        int closingLine = 0;
        qreal minEndY = 0.0;
        qreal endY = 0.0;
        qreal maxEndY = std::numeric_limits<qreal>::max();
        int depth = 0;
    };

    QVector<ContainerBoundary> boundaries;
    boundaries.reserve(allItems.size());

    // Keep closure lines on the same visible rhythm as block-to-block spacing.
    // Because closure lines are thick, include half stroke width in spacing math.
    constexpr qreal kClosureMarkerStrokeWidth = 3.4;
    constexpr qreal kCardToCardGap = 10.0;
    constexpr qreal kClosurePadAboveSubtree = kCardToCardGap + (kClosureMarkerStrokeWidth * 0.5);
    constexpr qreal kClosurePadBeforeNextBlock = kCardToCardGap + (kClosureMarkerStrokeWidth * 0.5);
    constexpr qreal kClosurePreferredOffset = kClosurePadAboveSubtree;

    auto subtreeBottomForItem = [](BlockCanvasItem *root) {
        if (root == nullptr) {
            return 0.0;
        }
        qreal bottom = root->sceneBoundingRect().bottom();
        QList<QGraphicsItem *> stack;
        stack.append(root);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            bottom = qMax(bottom, current->sceneBoundingRect().bottom());
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return bottom;
    };

    auto compactImmediateChildGaps = [&allItems]() {
        constexpr qreal kGapEpsilon = 0.5;
        for (BlockCanvasItem *parent : allItems) {
            if (parent == nullptr) {
                continue;
            }

            QList<BlockCanvasItem *> childBlocks;
            const QList<QGraphicsItem *> children = parent->childItems();
            for (QGraphicsItem *child : children) {
                auto *childBlock = dynamic_cast<BlockCanvasItem *>(child);
                if (childBlock != nullptr) {
                    childBlocks.append(childBlock);
                }
            }
            std::sort(childBlocks.begin(), childBlocks.end(), [](BlockCanvasItem *left, BlockCanvasItem *right) {
                return left->lineNumber() < right->lineNumber();
            });
            if (childBlocks.isEmpty()) {
                continue;
            }

            BlockCanvasItem *firstChild = childBlocks.first();
            if (firstChild == nullptr) {
                continue;
            }
            const qreal expectedTop = parent->sceneBoundingRect().bottom() + kCardToCardGap;
            const qreal actualTop = firstChild->sceneBoundingRect().top();
            if (actualTop > expectedTop + kGapEpsilon) {
                const qreal deltaY = actualTop - expectedTop;
                // Keep sibling spacing stable by shifting all immediate child subtrees
                // of this parent by the same delta.
                for (BlockCanvasItem *childBlock : childBlocks) {
                    if (childBlock == nullptr) {
                        continue;
                    }
                    childBlock->moveBy(0.0, -deltaY);
                }
            }
        }
    };

    auto nextBlockTopAfterLine = [&allItems](int lineNumber) {
        qreal nextTop = std::numeric_limits<qreal>::max();
        for (BlockCanvasItem *candidate : allItems) {
            if (candidate == nullptr || candidate->lineNumber() <= lineNumber) {
                continue;
            }
            nextTop = qMin(nextTop, candidate->sceneBoundingRect().top());
        }
        return nextTop;
    };
    auto shiftBlocksAfterLine = [&allItems, &compactImmediateChildGaps](int lineNumber, qreal deltaY) {
        if (deltaY <= 0.0) {
            return;
        }
        for (BlockCanvasItem *candidate : allItems) {
            if (candidate == nullptr || candidate->lineNumber() <= lineNumber) {
                continue;
            }
            candidate->moveBy(0.0, deltaY);
        }
        compactImmediateChildGaps();
    };
    auto maxDescendantLineNumber = [](BlockCanvasItem *root) {
        if (root == nullptr) {
            return 0;
        }
        int maxLine = root->lineNumber();
        QList<QGraphicsItem *> stack;
        stack.append(root);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(current)) {
                maxLine = qMax(maxLine, blockItem->lineNumber());
            }
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return maxLine;
    };

    struct ContainerLayoutTarget
    {
        BlockCanvasItem *item = nullptr;
        int openingLine = 0;
        int closingLine = 0;
        int depth = 0;
    };

    QVector<ContainerLayoutTarget> targets;
    targets.reserve(allItems.size());

    for (BlockCanvasItem *item : allItems) {
        if (item == nullptr || !item->isContainerOpen()) {
            continue;
        }

        int closureLine = item->lineNumber();
        const QString openingDirective = normalizeDirective(item->kind());
        const QString closingDirective = closingDirectiveFor(openingDirective);
        if (!openingDirective.isEmpty() && !closingDirective.isEmpty()) {
            const int openingLine = item->lineNumber();
            const int closingLine = findMatchingBlockEndLine(sourceLines,
                                                             openingLine,
                                                             openingDirective,
                                                             closingDirective);
            if (closingLine > openingLine) {
                closureLine = closingLine;
            }
        }
        closureLine = qMax(closureLine, maxDescendantLineNumber(item));

        int depth = 0;
        for (QGraphicsItem *parent = item->parentItem(); parent != nullptr; parent = parent->parentItem()) {
            if (dynamic_cast<BlockCanvasItem *>(parent) != nullptr) {
                ++depth;
            }
        }

        targets.append(ContainerLayoutTarget{item, item->lineNumber(), closureLine, depth});
    }

    std::sort(targets.begin(), targets.end(), [](const ContainerLayoutTarget &left, const ContainerLayoutTarget &right) {
        if (left.closingLine != right.closingLine) {
            return left.closingLine < right.closingLine;
        }
        return left.depth > right.depth;
    });

    for (const ContainerLayoutTarget &target : std::as_const(targets)) {
        BlockCanvasItem *item = target.item;
        if (item == nullptr) {
            continue;
        }

        qreal subtreeBottom = visualSubtreeBottomForSpan(target.openingLine,
                                                         target.closingLine,
                                                         item->sceneBoundingRect().bottom());
        const qreal minEndY = subtreeBottom + kClosurePadAboveSubtree;
        qreal preferredEndY = qMax(subtreeBottom + kClosurePreferredOffset, minEndY);

        qreal maxEndY = std::numeric_limits<qreal>::max();
        const qreal nextTop = nextBlockTopAfterLine(target.closingLine);
        if (nextTop != std::numeric_limits<qreal>::max()) {
            maxEndY = nextTop - kClosurePadBeforeNextBlock;
        }

        if (maxEndY < minEndY) {
            const qreal deltaY = minEndY - maxEndY;
            shiftBlocksAfterLine(target.closingLine, deltaY);
            maxEndY += deltaY;
        }

        boundaries.append(ContainerBoundary{item,
                                            target.closingLine,
                                            minEndY,
                                            qMin(preferredEndY, maxEndY),
                                            maxEndY,
                                            target.depth});
    }

    std::sort(boundaries.begin(), boundaries.end(), [](const ContainerBoundary &left, const ContainerBoundary &right) {
        if (!qFuzzyCompare(left.endY + 1.0, right.endY + 1.0)) {
            return left.endY < right.endY;
        }
        return left.depth > right.depth;
    });

    // Keep nested closure markers visually distinct even when container spans end
    // at the same rendered subtree bottom (for example `endcenterline` + `endsurvey`).
    // Respect each boundary's [minEndY, maxEndY] corridor and back-propagate when
    // a lower marker is clamped by available space.
    constexpr qreal kMinClosureMarkerGap = 10.0;
    for (int i = 0; i < boundaries.size(); ++i) {
        ContainerBoundary &boundary = boundaries[i];
        if (i > 0) {
            const qreal minAllowedY = boundaries.at(i - 1).endY + kMinClosureMarkerGap;
            if (boundary.endY < minAllowedY) {
                boundary.endY = minAllowedY;
            }
        }
        boundary.endY = qMax(boundary.endY, boundary.minEndY);
        if (boundary.endY > boundary.maxEndY) {
            boundary.endY = boundary.maxEndY;
            for (int j = i - 1; j >= 0; --j) {
                ContainerBoundary &previous = boundaries[j];
                const qreal maxAllowedY = boundaries.at(j + 1).endY - kMinClosureMarkerGap;
                if (previous.endY <= maxAllowedY) {
                    break;
                }
                previous.endY = qMax(previous.minEndY, maxAllowedY);
            }
        }
    }

    // If constraints are still too tight for exact nested spacing, expand the
    // layout by pushing subsequent blocks down so closure lines never collapse.
    for (int i = 1; i < boundaries.size(); ++i) {
        const qreal requiredY = boundaries.at(i - 1).endY + kMinClosureMarkerGap;
        if (boundaries.at(i).endY >= requiredY) {
            continue;
        }

        const qreal deltaY = requiredY - boundaries.at(i).endY;
        shiftBlocksAfterLine(boundaries.at(i).closingLine, deltaY);

        for (int j = i; j < boundaries.size(); ++j) {
            ContainerBoundary &shifted = boundaries[j];
            shifted.minEndY += deltaY;
            shifted.endY += deltaY;
            if (shifted.maxEndY < std::numeric_limits<qreal>::max()) {
                shifted.maxEndY += deltaY;
            }
        }
    }

    // Compaction/shift steps above can move cards after a boundary was initially
    // solved. Re-clamp each closure marker against current subtree/next-card
    // geometry so end-caps never ride into the following card.
    const qreal kInfinity = std::numeric_limits<qreal>::max();
    for (ContainerBoundary &boundary : boundaries) {
        BlockCanvasItem *item = boundary.item;
        if (item == nullptr) {
            continue;
        }

        const qreal subtreeBottom = visualSubtreeBottomForSpan(item->lineNumber(),
                                                               boundary.closingLine,
                                                               item->sceneBoundingRect().bottom());
        boundary.minEndY = subtreeBottom + kClosurePadAboveSubtree;

        const qreal nextTop = nextBlockTopAfterLine(boundary.closingLine);
        boundary.maxEndY = nextTop != kInfinity
            ? (nextTop - kClosurePadBeforeNextBlock)
            : kInfinity;

        // Prefer keeping distance from the next card over preserving a stale
        // lower endY, even when constraints are tight.
        if (boundary.maxEndY != kInfinity && boundary.maxEndY < boundary.minEndY) {
            boundary.endY = boundary.maxEndY;
            continue;
        }

        boundary.endY = qMax(boundary.endY, boundary.minEndY);
        if (boundary.maxEndY != kInfinity) {
            boundary.endY = qMin(boundary.endY, boundary.maxEndY);
        }
    }

    const QPalette palette = QApplication::palette();
    QPen connectorPen(palette.color(QPalette::Mid));
    connectorPen.setWidthF(1.2);
    connectorPen.setStyle(Qt::SolidLine);

    constexpr qreal kConnectorInsetX = 2.0;

    for (const ContainerBoundary &boundary : std::as_const(boundaries)) {
        BlockCanvasItem *item = boundary.item;
        if (item == nullptr) {
            continue;
        }

        const QRectF itemRect = item->sceneBoundingRect();
        const qreal guideX = itemRect.left() + kConnectorInsetX;
        const qreal startY = itemRect.bottom();
        const qreal endY = boundary.endY;

        auto *vertical = context_.scene->addLine(QLineF(guideX, startY, guideX, endY), connectorPen);
        vertical->setOpacity(0.38);
        vertical->setZValue(-100.0);
        (*context_.containerBoundaryGuideItems).append(vertical);

        // Keep the closure marker aligned with the opening card width.
        const qreal endCapStartX = itemRect.left();
        const qreal endCapEndX = itemRect.right();

        const QColor baseColor = blockEditorCanvasBaseColorForDirective(item->kind());
        QColor closeColor = baseColor.lightnessF() < 0.5
            ? baseColor.lighter(155)
            : baseColor.darker(130);
        closeColor.setAlpha(245);
        QPen closePen(closeColor);
        closePen.setWidthF(kClosureMarkerStrokeWidth);
        closePen.setStyle(Qt::SolidLine);
        auto *endCap = context_.scene->addLine(QLineF(endCapStartX, endY, endCapEndX, endY), closePen);
        endCap->setOpacity(0.95);
        endCap->setZValue(-99.0);
        endCap->setData(kBlockEditorCanvasEndHintContainerLineDataRole, item->lineNumber());
        (*context_.containerBoundaryGuideItems).append(endCap);
        (*context_.containerBoundaryEndYByLine).insert(item->lineNumber(), endY);

    }

    QRectF sceneRect = context_.scene->itemsBoundingRect();
    if (!sceneRect.isValid() || sceneRect.isEmpty()) {
        sceneRect = QRectF(0.0, 0.0, 1.0, 1.0);
    } else {
        sceneRect = sceneRect.adjusted(-16.0, -12.0, 20.0, 24.0);
    }
    context_.scene->setSceneRect(sceneRect);

    if (preferredSelectedLine > 0) {
        for (BlockCanvasItem *item : allItems) {
            if (item == nullptr || item->lineNumber() != preferredSelectedLine) {
                continue;
            }
            context_.scene->clearSelection();
            item->setSelected(true);
            context_.scene->setFocusItem(item);
            break;
        }
    }

    context_.refreshBlockDetailsSelectionFromScene();

}
}
