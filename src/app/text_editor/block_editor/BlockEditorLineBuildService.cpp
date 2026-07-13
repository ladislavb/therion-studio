#include "BlockEditorLineBuildService.h"

#include <QCoreApplication>

#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"
#include "../TextEditorCommandMetadata.h"
#include "../TextEditorOptionValidation.h"

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceDocument.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <utility>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorLineBuildService::BlockEditorLineBuildService(BlockEditorLineBuildContext context)
    : context_(std::move(context))
{
}

QString BlockEditorLineBuildService::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorLineBuildService", text);
}

bool BlockEditorLineBuildService::hasRequiredContext() const
{
    return context_.selectedLineNumber != nullptr
        && context_.selectedKind != nullptr
        && context_.commentMarker != nullptr
        && context_.commandMetadata != nullptr
        && context_.sourceContext
        && context_.readingsOrderTextForBuild
        && context_.normalizeDirectiveToken
        && context_.commandHasRequiredIdArgument
        && context_.isSimpleValueMode
        && context_.isDataHeaderMode
        && context_.isStructuredOptionsMode;
}

bool BlockEditorLineBuildService::buildUpdatedLine(QString *updatedLine, QString *validationError) const
{
    if (!hasRequiredContext() || updatedLine == nullptr) {
        return false;
    }
    updatedLine->clear();
    if (validationError != nullptr) {
        validationError->clear();
    }

    const BlockEditorSourceController source(context_.sourceContext());
    if (*context_.selectedLineNumber <= 0
        || context_.selectedKind->isEmpty()
        || !source.hasEditor()) {
        if (validationError != nullptr) {
            *validationError = tr("No block is selected.");
        }
        return false;
    }

    QStringList lines = source.normalizedLines();
    if (*context_.selectedLineNumber > lines.size()) {
        if (validationError != nullptr) {
            *validationError = tr("Selected line is out of range.");
        }
        return false;
    }

    const QString normalizedKind = context_.normalizeDirectiveToken(*context_.selectedKind);
    if (isUnrecognizedKind(normalizedKind)) {
        const QString updatedRawLine = context_.idEdit != nullptr ? context_.idEdit->text() : QString();
        if (updatedRawLine.trimmed().isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Line cannot be empty.");
            }
            return false;
        }
        *updatedLine = updatedRawLine;
        return true;
    }

    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, *context_.selectedLineNumber, &logicalLine)) {
        if (validationError != nullptr) {
            *validationError = tr("Selected line is out of range.");
        }
        return false;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(source.text());
    const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(logicalLine.startLine);
    const TherionParsedLine parsedLine = sourceLine != nullptr
        ? sourceLine->sourceLine.parsed
        : TherionParsedLine{};
    const bool commentOnlyLine = normalizedKind == QStringLiteral("comment")
        && isFullLineComment(parsedLine);
    if (parsedLine.tokens.isEmpty()) {
        if (commentOnlyLine) {
            // Full-line comments intentionally have no directive tokens.
            // They are editable in Block details via the dedicated comment branch below.
        } else if (validationError != nullptr) {
            *validationError = tr("Selected line is empty.");
        }
        if (!commentOnlyLine) {
            return false;
        }
    }

    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(lines.at(logicalLine.startLine - 1));
    const QString indent = match.hasMatch() ? match.captured(0) : QString();
    const QString commandToken = parsedLine.tokens.value(0).trimmed().isEmpty()
        ? normalizedKind
        : parsedLine.tokens.value(0).trimmed();

    if (commentOnlyLine) {
        const QString updatedComment = context_.idEdit != nullptr
            ? context_.idEdit->text().trimmed()
            : QString();
        if (updatedComment.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Comment cannot be empty.");
            }
            return false;
        }
        QChar marker = *context_.commentMarker;
        if (marker != QLatin1Char('#') && marker != QLatin1Char('%')) {
            marker = QLatin1Char('#');
        }
        *updatedLine = QStringLiteral("%1%2 %3").arg(indent, QString(marker), updatedComment);
        return true;
    }

    QString result = QStringLiteral("%1%2").arg(indent, commandToken);

    if (context_.isSimpleValueMode()) {
        const bool noValueCommand = !isMapObjectReferenceKind(normalizedKind)
            && context_.commandMetadata->commandArgumentSignaturesByToken.value(normalizedKind).isEmpty();
        const QString updatedValue = context_.idEdit != nullptr
            ? context_.idEdit->text().trimmed()
            : QString();
        if (!noValueCommand && updatedValue.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Value cannot be empty.");
            }
            return false;
        }
        if (noValueCommand) {
            result = QStringLiteral("%1%2").arg(indent, commandToken);
        } else if (isMapObjectReferenceKind(normalizedKind)) {
            result = QStringLiteral("%1%2").arg(indent, updatedValue);
        } else if (context_.commandMetadata->commandPrimaryValueIsPerson.value(normalizedKind, false)) {
            result += QStringLiteral(" ") + serializeTherionArgumentToken(updatedValue);
        } else {
            result += QStringLiteral(" ") + updatedValue;
        }
        if (!noValueCommand
            && context_.additionalPositionalEdit != nullptr
            && context_.additionalPositionalEdit->isVisible()) {
            const QString secondaryValue = context_.additionalPositionalEdit->text().trimmed();
            if (!secondaryValue.isEmpty()) {
                result += QStringLiteral(" ") + secondaryValue;
            }
        }
    } else if (context_.isDataHeaderMode()) {
        const QString updatedStyle = context_.idEdit != nullptr
            ? context_.idEdit->text().trimmed()
            : QString();
        if (updatedStyle.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Style cannot be empty.");
            }
            return false;
        }
        const QString updatedReadingsOrder = context_.readingsOrderTextForBuild().trimmed();
        if (updatedReadingsOrder.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Readings order cannot be empty.");
            }
            return false;
        }
        result += QStringLiteral(" ") + updatedStyle;
        result += QStringLiteral(" ") + updatedReadingsOrder;
    } else if (context_.isStructuredOptionsMode()) {
        const bool requiresId = context_.commandHasRequiredIdArgument(normalizedKind);
        const QString updatedId = context_.idEdit != nullptr
            ? context_.idEdit->text().trimmed()
            : QString();
        if (requiresId && updatedId.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("ID cannot be empty.");
            }
            return false;
        }

        QStringList serializedOptions;
        if (context_.optionsTable != nullptr) {
            QVector<TextEditorOptionRow> optionRows;
            optionRows.reserve(context_.optionsTable->rowCount());
            for (int row = 0; row < context_.optionsTable->rowCount(); ++row) {
                const QString key = (context_.optionsTable->item(row, 0) != nullptr
                                         ? context_.optionsTable->item(row, 0)->text()
                                         : QString())
                                        .trimmed();
                const QString value = (context_.optionsTable->item(row, 1) != nullptr
                                           ? context_.optionsTable->item(row, 1)->text()
                                           : QString())
                                          .trimmed();
                optionRows.append(TextEditorOptionRow{key, value, 0});
            }

            const TextEditorOptionValidationResult validation = validateAndSerializeCommandOptions(
                commandToken,
                optionRows,
                context_.commandMetadata->commandOptionValueArityTokens,
                context_.commandMetadata->commandOptionFixedArityByKey,
                context_.commandMetadata->commandOptionArgumentLabelsByKey,
                context_.commandMetadata->commandOptionValueTokens,
                true);
            if (!validation.ok) {
                if (validation.failingRow >= 0
                    && validation.failingRow < context_.optionsTable->rowCount()) {
                    context_.optionsTable->setCurrentCell(validation.failingRow, 0);
                }
                if (validationError != nullptr) {
                    *validationError = validation.errorMessage;
                }
                return false;
            }
            serializedOptions = validation.serializedOptions;
        }

        if (!updatedId.isEmpty()) {
            result += QStringLiteral(" ") + updatedId;
        }
        const QString updatedAdditionalPositionalTokens = context_.additionalPositionalEdit != nullptr
            ? context_.additionalPositionalEdit->text().trimmed()
            : QString();
        if (!updatedAdditionalPositionalTokens.isEmpty()) {
            result += QStringLiteral(" ") + updatedAdditionalPositionalTokens;
        }
        if (!serializedOptions.isEmpty()) {
            result += QStringLiteral(" ") + serializedOptions.join(QLatin1Char(' '));
        }
    } else {
        if (validationError != nullptr) {
            *validationError = tr("This block cannot be edited in details pane.");
        }
        return false;
    }

    const QString updatedComment = context_.commentEdit != nullptr
        ? context_.commentEdit->text().trimmed()
        : QString();
    if (!updatedComment.isEmpty()) {
        QChar marker = *context_.commentMarker;
        if (marker != QLatin1Char('#') && marker != QLatin1Char('%')) {
            marker = QLatin1Char('#');
        }
        result += QStringLiteral(" %1 %2").arg(QString(marker), updatedComment);
    }

    *updatedLine = result;
    return true;
}
}
