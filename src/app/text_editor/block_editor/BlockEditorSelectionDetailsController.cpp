#include "BlockEditorSelectionDetailsController.h"

#include "BlockEditorDetailsSupport.h"
#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"
#include "../ContextHelpController.h"
#include "../TextEditorCommandMetadata.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceSnapshotCache.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <optional>
#include <utility>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorSelectionDetailsController::BlockEditorSelectionDetailsController(BlockEditorSelectionDetailsContext context)
    : context_(std::move(context))
{
}

bool BlockEditorSelectionDetailsController::loadSelectionDetails(const QString &kind, int lineNumber)
{
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("TherionStudio::BlockEditorSelectionDetailsController", text);
    };
    if (context_.tearingDown != nullptr && *context_.tearingDown) {
        return false;
    }
    if (context_.detailsPopulating == nullptr
        || context_.selectedLineNumber == nullptr
        || context_.selectedKind == nullptr
        || context_.baseStatusText == nullptr
        || context_.commentMarker == nullptr
        || context_.commandMetadata == nullptr
        || !context_.setReadingsTagEditor
        || !context_.installLineEditCompleter) {
        return false;
    }
    if (lineNumber <= 0 || !context_.loadNormalizedLines) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return false;
    }

    QStringList lines;
    if (!context_.loadNormalizedLines(&lines)) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return false;
    }
    if (lineNumber > lines.size()) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return false;
    }

    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, lineNumber, &logicalLine)) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return false;
    }

    const QString normalizedKind = context_.normalizedDirectiveToken ? context_.normalizedDirectiveToken(kind) : kind.trimmed().toLower();
    TherionParsedLine parsedLine;
    if (context_.loadSourceSnapshot) {
        QString sourceText;
        int revisionId = 0;
        if (context_.loadSourceSnapshot(&sourceText, &revisionId)) {
            TherionSourceDocumentMetadata sourceMetadata;
            sourceMetadata.revisionId = revisionId;
            TherionSourceSnapshotCache sourceSnapshotCache;
            const TherionSourceLogicalDocument &logicalDocument =
                sourceSnapshotCache.logicalDocument(sourceText, sourceMetadata);
            if (const TherionSourceLogicalCommand *logicalCommand =
                    logicalDocument.commandAtPhysicalLine(logicalLine.startLine);
                logicalCommand != nullptr && logicalCommand->startLineNumber == logicalLine.startLine) {
                parsedLine = logicalCommand->parsed;
            }
        }
    }
    if (parsedLine.lineNumber <= 0) {
        parsedLine = TherionDocumentParser::parseLine(logicalLine.text, logicalLine.startLine);
    }
    const bool commentOnlyLine = normalizedKind == QStringLiteral("comment") && isFullLineComment(parsedLine);
    const bool unrecognizedLineKind = isUnrecognizedKind(normalizedKind);
    if (parsedLine.tokens.isEmpty() && !commentOnlyLine) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return false;
    }
    const QString inlineComment = parsedLine.commentStart >= 0 ? parsedLine.commentText.trimmed() : QString();
    const QString markerSourceLine = lines.at(logicalLine.startLine - 1);
    if (parsedLine.commentStart >= 0 && parsedLine.commentStart < markerSourceLine.size()) {
        *context_.commentMarker = markerSourceLine.at(parsedLine.commentStart);
    } else {
        *context_.commentMarker = QLatin1Char('#');
    }

    *context_.selectedLineNumber = logicalLine.startLine;
    *context_.selectedKind = normalizedKind;
    if (context_.editPanel != nullptr) {
        context_.editPanel->setVisible(true);
    }

    *context_.detailsPopulating = true;
    std::optional<QSignalBlocker> idEditSignalBlocker;
    std::optional<QSignalBlocker> additionalPositionalSignalBlocker;
    std::optional<QSignalBlocker> commentEditSignalBlocker;
    std::optional<QSignalBlocker> optionsTableSignalBlocker;
    if (context_.idEdit != nullptr) {
        idEditSignalBlocker.emplace(context_.idEdit);
    }
    if (context_.additionalPositionalEdit != nullptr) {
        additionalPositionalSignalBlocker.emplace(context_.additionalPositionalEdit);
    }
    if (context_.commentEdit != nullptr) {
        commentEditSignalBlocker.emplace(context_.commentEdit);
    }
    if (context_.optionsTable != nullptr) {
        optionsTableSignalBlocker.emplace(context_.optionsTable);
    }

    const bool supported = context_.supportsDetailsPaneForKind ? context_.supportsDetailsPaneForKind(normalizedKind) : false;
    const bool hasCatalogOptions = !(*context_.commandMetadata).commandOptionTokens.value(normalizedKind).isEmpty();
    const bool structuredOptionsMode =
        (context_.isContainerBlockDirectiveForBlocks ? context_.isContainerBlockDirectiveForBlocks(normalizedKind) : false)
        || hasCatalogOptions;
    const bool simpleValueMode = !structuredOptionsMode && normalizedKind != QStringLiteral("data");
    const bool dataHeaderMode = normalizedKind == QStringLiteral("data");
    if (!supported) {
        if (context_.setDetailsMode) { context_.setDetailsMode(BlockEditorSelectionDetailsMode::Unsupported); }
    } else if (structuredOptionsMode) {
        if (context_.setDetailsMode) { context_.setDetailsMode(BlockEditorSelectionDetailsMode::StructuredOptions); }
    } else if (simpleValueMode) {
        if (context_.setDetailsMode) { context_.setDetailsMode(BlockEditorSelectionDetailsMode::SimpleValue); }
    } else if (dataHeaderMode) {
        if (context_.setDetailsMode) { context_.setDetailsMode(BlockEditorSelectionDetailsMode::DataHeader); }
    } else {
        if (context_.setDetailsMode) { context_.setDetailsMode(BlockEditorSelectionDetailsMode::Unsupported); }
    }

    const QString titleText = isMapObjectReferenceKind(normalizedKind)
                                  ? tr("Object Reference")
                                  : (isUnrecognizedKind(normalizedKind)
                                         ? tr("Unrecognized command")
                                         : normalizedKind);
    const QString sourceLineText = tr("Source line %1").arg(logicalLine.startLine);
    if (context_.titleLabel != nullptr) {
        context_.titleLabel->setText(titleText);
        context_.titleLabel->setVisible(true);
    }
    *context_.baseStatusText = sourceLineText;
    if (context_.statusLabel != nullptr) {
        context_.statusLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
        context_.statusLabel->setText(*context_.baseStatusText);
    }

    if (!supported) {
        if (context_.editPanel != nullptr) {
            context_.editPanel->setVisible(false);
        }
        if (context_.primaryFieldLabel != nullptr) {
            context_.primaryFieldLabel->setVisible(false);
        }
        if (context_.secondaryFieldLabel != nullptr) {
            context_.secondaryFieldLabel->setVisible(false);
        }
        if (context_.commentFieldLabel != nullptr) {
            context_.commentFieldLabel->setVisible(false);
        }
        if (context_.idEdit != nullptr) {
            context_.idEdit->clear();
            context_.idEdit->setEnabled(false);
            context_.idEdit->setVisible(false);
        }
        if (context_.readOnlyValueLabel != nullptr) {
            context_.readOnlyValueLabel->clear();
            context_.readOnlyValueLabel->setVisible(false);
        }
        if (context_.primaryFieldStack != nullptr) {
            context_.primaryFieldStack->setVisible(false);
            if (context_.idEdit != nullptr) {
                context_.primaryFieldStack->setCurrentWidget(context_.idEdit);
            }
        }
        if (context_.additionalPositionalEdit != nullptr) {
            context_.additionalPositionalEdit->clear();
            context_.additionalPositionalEdit->setEnabled(false);
            context_.additionalPositionalEdit->setVisible(false);
        }
        if (context_.secondaryFieldStack != nullptr) {
            context_.secondaryFieldStack->setVisible(false);
            if (context_.additionalPositionalEdit != nullptr) {
                context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
            }
        }
        context_.setReadingsTagEditor(QString(), {}, {});
        if (context_.commentEdit != nullptr) {
            context_.commentEdit->clear();
            context_.commentEdit->setEnabled(false);
            context_.commentEdit->setVisible(false);
        }
        if (context_.optionsLabel != nullptr) {
            context_.optionsLabel->setVisible(false);
        }
        if (context_.optionsTable != nullptr) {
            context_.optionsTable->clearSelection();
            context_.optionsTable->setEnabled(false);
            context_.optionsTable->setRowCount(0);
            context_.optionsTable->setVisible(false);
        }
        if (context_.addOptionButton != nullptr) {
            context_.addOptionButton->setEnabled(false);
            context_.addOptionButton->setVisible(false);
        }
        if (context_.removeOptionButton != nullptr) {
            context_.removeOptionButton->setEnabled(false);
            context_.removeOptionButton->setVisible(false);
        }
        if (context_.dataRowsButton != nullptr) {
            context_.dataRowsButton->setEnabled(false);
            context_.dataRowsButton->setVisible(false);
        }
        if (context_.optionArgsLabel != nullptr) {
            context_.optionArgsLabel->setVisible(false);
        }
        if (context_.optionArgsPanel != nullptr) {
            context_.optionArgsPanel->setVisible(false);
        }
        *context_.detailsPopulating = false;
        if (context_.refreshOptionArgumentEditors) {
            context_.refreshOptionArgumentEditors();
        }
        if (context_.updateHelpForCurrentFocus) {
            context_.updateHelpForCurrentFocus();
        }
        if (context_.refreshApplyState) {
            context_.refreshApplyState();
        }
        return true;
    }

    if (context_.primaryFieldLabel != nullptr) {
        context_.primaryFieldLabel->setVisible(true);
    }
    if (context_.secondaryFieldLabel != nullptr) {
        context_.secondaryFieldLabel->setVisible(true);
    }
    if (context_.commentFieldLabel != nullptr) {
        context_.commentFieldLabel->setText(tr("Comment"));
        context_.commentFieldLabel->setVisible(true);
    }
    if (context_.idEdit != nullptr) {
        context_.idEdit->setVisible(true);
        context_.idEdit->setReadOnly(false);
        context_.installLineEditCompleter(context_.idEdit, {});
    }
    if (context_.readOnlyValueLabel != nullptr) {
        context_.readOnlyValueLabel->clear();
        context_.readOnlyValueLabel->setVisible(false);
    }
    if (context_.primaryFieldStack != nullptr) {
        context_.primaryFieldStack->setVisible(true);
        if (context_.idEdit != nullptr) {
            context_.primaryFieldStack->setCurrentWidget(context_.idEdit);
        }
    }
    if (context_.additionalPositionalEdit != nullptr) {
        context_.additionalPositionalEdit->setVisible(true);
        context_.additionalPositionalEdit->setReadOnly(false);
        context_.installLineEditCompleter(context_.additionalPositionalEdit, {});
    }
    if (context_.secondaryFieldStack != nullptr) {
        context_.secondaryFieldStack->setVisible(true);
        if (context_.additionalPositionalEdit != nullptr) {
            context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
        }
    }
    if (context_.commentEdit != nullptr) {
        context_.commentEdit->setVisible(true);
        context_.commentEdit->setReadOnly(false);
        context_.commentEdit->setEnabled(true);
        context_.commentEdit->setPlaceholderText(tr("optional"));
        context_.commentEdit->setText(inlineComment);
    }
    if (context_.optionsLabel != nullptr) {
        context_.optionsLabel->setVisible(false);
    }
    if (context_.optionsTable != nullptr) {
        context_.optionsTable->setEnabled(false);
        context_.optionsTable->setRowCount(0);
        context_.optionsTable->setVisible(false);
    }
    if (context_.optionArgsLabel != nullptr) {
        context_.optionArgsLabel->setVisible(false);
    }
    if (context_.optionArgsPanel != nullptr) {
        context_.optionArgsPanel->setVisible(false);
    }
    if (context_.addOptionButton != nullptr) {
        context_.addOptionButton->setEnabled(false);
        context_.addOptionButton->setVisible(false);
    }
    if (context_.removeOptionButton != nullptr) {
        context_.removeOptionButton->setEnabled(false);
        context_.removeOptionButton->setVisible(false);
    }
    if (context_.dataRowsButton != nullptr) {
        context_.dataRowsButton->setVisible(dataHeaderMode);
        context_.dataRowsButton->setEnabled(dataHeaderMode);
    }

    if (structuredOptionsMode) {
        const bool explicitIdMode = context_.commandSupportsInlineIdField ? context_.commandSupportsInlineIdField(normalizedKind) : false;
        const bool requiresId = context_.commandHasRequiredIdArgument ? context_.commandHasRequiredIdArgument(normalizedKind) : false;

        const ParsedCommandOptions parsedOptions =
            parseCommandOptions(normalizedKind,
                                           parsedLine.tokens,
                                           (*context_.commandMetadata).commandOptionFixedArityByKey,
                                           true);

        if (context_.primaryFieldLabel != nullptr) {
            if (explicitIdMode) {
                context_.primaryFieldLabel->setText(tr("ID"));
            } else {
                const QStringList argumentSignatures = context_.commandArgumentSignaturesFor ? context_.commandArgumentSignaturesFor(normalizedKind) : QStringList();
                if (!argumentSignatures.isEmpty()) {
                    context_.primaryFieldLabel->setText(blockArgumentLabelFromSignature(argumentSignatures.first()));
                } else {
                    context_.primaryFieldLabel->setText(tr("Value"));
                }
            }
        }
        if (context_.idEdit != nullptr) {
            context_.idEdit->setEnabled(true);
            if (explicitIdMode) {
                context_.idEdit->setPlaceholderText(requiresId ? tr("required")
                                                                           : tr("optional"));
            } else {
                context_.idEdit->setPlaceholderText(tr("optional"));
            }
            context_.idEdit->setText(parsedOptions.leadingValue);
        }
        const bool showExtraArguments = !parsedOptions.extraPositionalTokens.isEmpty();
        if (context_.secondaryFieldLabel != nullptr) {
            context_.secondaryFieldLabel->setText(tr("Extra Arguments (Advanced)"));
            context_.secondaryFieldLabel->setVisible(showExtraArguments);
        }
        if (context_.additionalPositionalEdit != nullptr) {
            context_.additionalPositionalEdit->setEnabled(showExtraArguments);
            context_.additionalPositionalEdit->setVisible(showExtraArguments);
            context_.additionalPositionalEdit->setText(showExtraArguments
                                                                       ? parsedOptions.extraPositionalTokens.join(QLatin1Char(' '))
                                                                       : QString());
        }
        if (context_.secondaryFieldStack != nullptr) {
            context_.secondaryFieldStack->setVisible(showExtraArguments);
            if (context_.additionalPositionalEdit != nullptr) {
                context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
            }
        }
        context_.setReadingsTagEditor(QString(), {}, {});
        const bool showOptionsSection = hasCatalogOptions || !parsedOptions.optionEntries.isEmpty();
        if (context_.optionsLabel != nullptr) {
            context_.optionsLabel->setVisible(showOptionsSection);
        }
        if (context_.optionsTable != nullptr) {
            context_.optionsTable->setVisible(showOptionsSection);
            context_.optionsTable->setEnabled(showOptionsSection);
            context_.optionsTable->setRowCount(showOptionsSection ? parsedOptions.optionEntries.size() : 0);
            if (showOptionsSection) {
                for (int row = 0; row < parsedOptions.optionEntries.size(); ++row) {
                    context_.optionsTable->setItem(row, 0, new QTableWidgetItem(parsedOptions.optionEntries.at(row).key));
                    context_.optionsTable->setItem(row, 1, new QTableWidgetItem(parsedOptions.optionEntries.at(row).value));
                }
            }
            if (showOptionsSection && parsedOptions.optionEntries.size() > 0) {
                context_.optionsTable->setCurrentCell(0, 0);
            }
        }
        if (context_.addOptionButton != nullptr) {
            context_.addOptionButton->setVisible(showOptionsSection);
            context_.addOptionButton->setEnabled(showOptionsSection);
        }
        if (context_.removeOptionButton != nullptr) {
            context_.removeOptionButton->setVisible(showOptionsSection);
            context_.removeOptionButton->setEnabled(showOptionsSection && !parsedOptions.optionEntries.isEmpty());
        }
    } else if (simpleValueMode) {
        const bool readOnlyRootEncoding = normalizedKind == QStringLiteral("encoding");
        const TherionHelpEntry helpEntry = (*context_.commandMetadata).helpEntries.value(normalizedKind);
        QStringList argumentSignatures;
        for (const QString &argumentLine : helpEntry.arguments) {
            const QString signature = blockArgumentSignatureFromHelpLine(argumentLine);
            if (!signature.isEmpty()) {
                argumentSignatures.append(signature);
            }
        }
        const bool noValueCommand = !commentOnlyLine
            && !unrecognizedLineKind
            && !isMapObjectReferenceKind(normalizedKind)
            && argumentSignatures.isEmpty()
            && (*context_.commandMetadata).commandArgumentSignaturesByToken.value(normalizedKind).isEmpty();
        const bool hasSecondaryArgument = argumentSignatures.size() > 1;

        QString currentValue = unrecognizedLineKind
            ? lines.at(lineNumber - 1)
            : (commentOnlyLine
            ? parsedLine.commentText.trimmed()
            : (isMapObjectReferenceKind(normalizedKind)
            ? parsedLine.tokens.value(0)
            : (parsedLine.tokens.size() > 1 ? parsedLine.tokens.at(1) : QString())));
        QString secondaryValue;
        if (!commentOnlyLine && !unrecognizedLineKind && !isMapObjectReferenceKind(normalizedKind) && hasSecondaryArgument && parsedLine.tokens.size() > 2) {
            secondaryValue = parsedLine.tokens.mid(2).join(QLatin1Char(' '));
        } else if (!commentOnlyLine && !unrecognizedLineKind && !isMapObjectReferenceKind(normalizedKind) && !hasSecondaryArgument) {
            currentValue = parsedLine.tokens.size() > 1
                ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
                : QString();
        }

        if (context_.primaryFieldLabel != nullptr) {
            if (unrecognizedLineKind) {
                context_.primaryFieldLabel->setText(tr("Raw line"));
                context_.primaryFieldLabel->setVisible(true);
            } else if (commentOnlyLine) {
                context_.primaryFieldLabel->setText(tr("Comment"));
                context_.primaryFieldLabel->setVisible(true);
            } else if (isMapObjectReferenceKind(normalizedKind)) {
                context_.primaryFieldLabel->setText(tr("Target"));
            } else if (readOnlyRootEncoding) {
                context_.primaryFieldLabel->setText(tr("Value"));
            } else if (noValueCommand) {
                context_.primaryFieldLabel->setVisible(false);
            } else if ((*context_.commandMetadata).commandPrimaryValueIsPerson.value(normalizedKind, false)) {
                context_.primaryFieldLabel->setText(tr("Person"));
            } else if (!argumentSignatures.isEmpty()) {
                context_.primaryFieldLabel->setText(blockArgumentLabelFromSignature(argumentSignatures.first()));
            } else {
                context_.primaryFieldLabel->setText(tr("Value"));
            }
        }
        if (context_.secondaryFieldLabel != nullptr) {
            if (commentOnlyLine || unrecognizedLineKind) {
                context_.secondaryFieldLabel->setVisible(false);
            } else if (hasSecondaryArgument) {
                if (argumentSignatures.size() > 1) {
                    context_.secondaryFieldLabel->setText(blockArgumentLabelFromSignature(argumentSignatures.at(1)));
                } else {
                    context_.secondaryFieldLabel->setText(tr("Value 2"));
                }
                context_.secondaryFieldLabel->setVisible(true);
            } else {
                context_.secondaryFieldLabel->setVisible(false);
            }
        }
        if (context_.additionalPositionalEdit != nullptr) {
            if (!commentOnlyLine && !unrecognizedLineKind && hasSecondaryArgument) {
                context_.additionalPositionalEdit->setEnabled(true);
                context_.additionalPositionalEdit->setVisible(true);
                context_.additionalPositionalEdit->setPlaceholderText(tr("optional"));
                context_.additionalPositionalEdit->setText(secondaryValue);
            } else {
                context_.additionalPositionalEdit->clear();
                context_.additionalPositionalEdit->setEnabled(false);
                context_.additionalPositionalEdit->setVisible(false);
            }
        }
        if (context_.secondaryFieldStack != nullptr) {
            context_.secondaryFieldStack->setVisible(!commentOnlyLine && !unrecognizedLineKind && hasSecondaryArgument);
            if (context_.additionalPositionalEdit != nullptr) {
                context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
            }
        }
        context_.setReadingsTagEditor(QString(), {}, {});
        if (context_.idEdit != nullptr) {
            context_.idEdit->setEnabled(unrecognizedLineKind || commentOnlyLine || !noValueCommand);
            context_.idEdit->setVisible(true);
            context_.idEdit->setReadOnly(readOnlyRootEncoding);
            context_.idEdit->setPlaceholderText((unrecognizedLineKind || commentOnlyLine || !noValueCommand) ? tr("required") : QString());
            context_.idEdit->setText(currentValue);
        }
        if (context_.readOnlyValueLabel != nullptr) {
            context_.readOnlyValueLabel->setText(readOnlyRootEncoding ? currentValue : QString());
            context_.readOnlyValueLabel->setVisible(readOnlyRootEncoding);
        }
        if (context_.primaryFieldStack != nullptr) {
            context_.primaryFieldStack->setVisible(unrecognizedLineKind || commentOnlyLine || !noValueCommand);
            if (readOnlyRootEncoding && context_.readOnlyValueLabel != nullptr) {
                context_.primaryFieldStack->setCurrentWidget(context_.readOnlyValueLabel);
            } else if (context_.idEdit != nullptr) {
                context_.primaryFieldStack->setCurrentWidget(context_.idEdit);
            }
        }
        if (readOnlyRootEncoding) {
            if (context_.commentFieldLabel != nullptr) {
                context_.commentFieldLabel->setVisible(false);
            }
            if (context_.commentEdit != nullptr) {
                context_.commentEdit->clear();
                context_.commentEdit->setReadOnly(true);
                context_.commentEdit->setEnabled(false);
                context_.commentEdit->setVisible(false);
            }
        }
        if (commentOnlyLine || unrecognizedLineKind) {
            if (context_.commentFieldLabel != nullptr) {
                context_.commentFieldLabel->setVisible(false);
            }
            if (context_.commentEdit != nullptr) {
                context_.commentEdit->clear();
                context_.commentEdit->setEnabled(false);
                context_.commentEdit->setVisible(false);
            }
        }
        if (context_.optionsTable != nullptr) {
            context_.optionsTable->setVisible(false);
        }
        if (context_.optionsLabel != nullptr) {
            context_.optionsLabel->setVisible(false);
        }
        if (context_.addOptionButton != nullptr) {
            context_.addOptionButton->setVisible(false);
        }
        if (context_.removeOptionButton != nullptr) {
            context_.removeOptionButton->setVisible(false);
        }
    } else if (dataHeaderMode) {
        const BlockEditorDataHeaderComponents components = parseBlockEditorDataHeaderComponents(parsedLine.tokens);
        const QStringList styleSuggestions = (*context_.commandMetadata).commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("data"), 0));
        const QStringList readingSuggestions = (*context_.commandMetadata).commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("data"), 1));
        if (context_.primaryFieldLabel != nullptr) {
            context_.primaryFieldLabel->setText(tr("Style"));
        }
        if (context_.secondaryFieldLabel != nullptr) {
            context_.secondaryFieldLabel->setText(tr("Readings Order"));
            context_.secondaryFieldLabel->setVisible(true);
        }
        if (context_.additionalPositionalEdit != nullptr) {
            context_.additionalPositionalEdit->setEnabled(true);
            context_.additionalPositionalEdit->setVisible(false);
            context_.additionalPositionalEdit->setText(components.readingsOrder);
        }
        if (context_.secondaryFieldStack != nullptr) {
            context_.secondaryFieldStack->setVisible(true);
            if (context_.readingsTagEditor != nullptr) {
                context_.secondaryFieldStack->setCurrentWidget(context_.readingsTagEditor);
            }
        }
        context_.setReadingsTagEditor(tr("Type token and press Enter/Space"),
                                                 readingSuggestions,
                                                 TherionDocumentParser::tokenizeLine(components.readingsOrder));
        if (context_.idEdit != nullptr) {
            context_.idEdit->setEnabled(true);
            context_.idEdit->setPlaceholderText(tr("required"));
            context_.idEdit->setText(components.style);
            context_.installLineEditCompleter(context_.idEdit, styleSuggestions);
        }
        if (context_.optionsTable != nullptr) {
            context_.optionsTable->setVisible(false);
        }
        if (context_.optionsLabel != nullptr) {
            context_.optionsLabel->setVisible(false);
        }
        if (context_.addOptionButton != nullptr) {
            context_.addOptionButton->setVisible(false);
        }
        if (context_.removeOptionButton != nullptr) {
            context_.removeOptionButton->setVisible(false);
        }
    }

    *context_.detailsPopulating = false;
    if (context_.refreshOptionArgumentEditors) {
        context_.refreshOptionArgumentEditors();
    }
    if (context_.updateHelpForCurrentFocus) {
        context_.updateHelpForCurrentFocus();
    }
    if (context_.refreshApplyState) {
        context_.refreshApplyState();
    }
    return true;
}
}
