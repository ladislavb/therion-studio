#include "CommandOptionsDialog.h"

#include "CommandOptionTableDelegate.h"
#include "ContextHelpController.h"
#include "TextEditorOptionValidation.h"
#include "../../core/TherionCommandLineModel.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "../../core/TherionCommandSyntax.h"
#include "../../core/TherionSourceDocument.h"

#include <algorithm>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kRowKindRole = Qt::UserRole;
constexpr int kPositionalIndexRole = Qt::UserRole + 1;

enum class OptionsDialogRowKind
{
    Option = 0,
    PositionalAttribute = 1,
};

QString normalizedArgumentSignature(QString signature)
{
    signature = signature.section(QLatin1Char('='), 0, 0).trimmed();
    signature.remove(QLatin1Char('|'));
    return signature.simplified();
}

bool commandFirstPositionalArgumentIsId(const TextEditorCommandMetadata *metadata, const QString &commandName)
{
    if (metadata == nullptr) {
        return false;
    }

    const QStringList signatures = metadata->commandArgumentSignaturesByToken.value(commandName);
    if (signatures.isEmpty()) {
        return false;
    }

    const QString normalized = normalizedArgumentSignature(signatures.first()).toLower();
    const QString firstToken = normalized.section(QLatin1Char(' '), 0, 0);
    return firstToken == QStringLiteral("<id>");
}

QString mapObjectArgumentKey(const QString &commandName, int positionalIndex)
{
    if (commandName == QStringLiteral("point")) {
        switch (positionalIndex) {
        case 0:
            return QStringLiteral("x");
        case 1:
            return QStringLiteral("y");
        case 2:
            return QStringLiteral("type");
        default:
            break;
        }
    }
    if ((commandName == QStringLiteral("line") || commandName == QStringLiteral("area"))
        && positionalIndex == 0) {
        return QStringLiteral("type");
    }
    if (commandName == QStringLiteral("scrap") && positionalIndex == 0) {
        return QStringLiteral("id");
    }
    return QString();
}

QString attributeKeyForPosition(const TextEditorCommandMetadata *metadata,
                                const QString &commandName,
                                int positionalIndex)
{
    const QString mapKey = mapObjectArgumentKey(commandName, positionalIndex);
    if (!mapKey.isEmpty()) {
        return mapKey;
    }

    if (metadata != nullptr) {
        const QStringList signatures = metadata->commandArgumentSignaturesByToken.value(commandName);
        if (positionalIndex >= 0 && positionalIndex < signatures.size()) {
            const QString normalized = normalizedArgumentSignature(signatures.at(positionalIndex));
            if (!normalized.isEmpty()) {
                QString key = normalized.section(QLatin1Char(' '), 0, 0).trimmed().toLower();
                if (key.startsWith(QLatin1Char('<')) && key.endsWith(QLatin1Char('>'))) {
                    key = key.mid(1, key.size() - 2);
                }
                if (!key.isEmpty()) {
                    return key;
                }
            }
        }
    }

    return QStringLiteral("arg%1").arg(positionalIndex + 1);
}

QString argumentHelpHtml(const QString &key, const QString &signature, const QString &commandHelpHtml)
{
    QStringList html;
    html << QStringLiteral("<p><b>%1:</b> %2</p>")
                .arg(QCoreApplication::translate("TherionStudio::CommandOptionsDialog", "Attribute"),
                     key.toHtmlEscaped());
    const QString normalizedSignature = normalizedArgumentSignature(signature);
    if (!normalizedSignature.isEmpty() && normalizedSignature != key) {
        html << QStringLiteral("<p><b>%1:</b> %2</p>")
                    .arg(QCoreApplication::translate("TherionStudio::CommandOptionsDialog", "Catalog signature"),
                         normalizedSignature.toHtmlEscaped());
    }
    html << commandHelpHtml;
    return html.join(QString());
}

bool rowIsPositionalAttribute(const QTableWidget *table, int row)
{
    if (table == nullptr || row < 0 || row >= table->rowCount() || table->item(row, 0) == nullptr) {
        return false;
    }
    return table->item(row, 0)->data(kRowKindRole).toInt() == static_cast<int>(OptionsDialogRowKind::PositionalAttribute);
}

void appendPositionalAttributeRow(QTableWidget *table, const QString &key, const QString &value, int positionalIndex)
{
    if (table == nullptr) {
        return;
    }

    const int row = table->rowCount();
    table->insertRow(row);

    auto *keyItem = new QTableWidgetItem(key);
    keyItem->setData(kRowKindRole, static_cast<int>(OptionsDialogRowKind::PositionalAttribute));
    keyItem->setData(kPositionalIndexRole, positionalIndex);
    keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, keyItem);

    auto *valueItem = new QTableWidgetItem(value);
    valueItem->setData(kRowKindRole, static_cast<int>(OptionsDialogRowKind::PositionalAttribute));
    valueItem->setData(kPositionalIndexRole, positionalIndex);
    table->setItem(row, 1, valueItem);
}

void appendOptionRow(QTableWidget *table, const QString &key, const QString &value)
{
    if (table == nullptr) {
        return;
    }

    const int row = table->rowCount();
    table->insertRow(row);

    auto *keyItem = new QTableWidgetItem(key);
    keyItem->setData(kRowKindRole, static_cast<int>(OptionsDialogRowKind::Option));
    table->setItem(row, 0, keyItem);

    auto *valueItem = new QTableWidgetItem(value);
    valueItem->setData(kRowKindRole, static_cast<int>(OptionsDialogRowKind::Option));
    table->setItem(row, 1, valueItem);
}

void appendUniqueSuggestion(QStringList *target, const QString &value)
{
    if (target == nullptr) {
        return;
    }

    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return;
    }
    if (target->contains(normalized, Qt::CaseInsensitive)) {
        return;
    }
    target->append(normalized);
}

QStringList suggestionsForCell(const TextEditorCommandMetadata *metadata,
                               QTableWidget *optionsTable,
                               const QString &commandName,
                               const QModelIndex &index)
{
    if (metadata == nullptr || optionsTable == nullptr || !index.isValid()) {
        return {};
    }

    QStringList suggestions;
    if (index.column() == 0) {
        for (const QString &option : metadata->commandOptionTokens.value(commandName)) {
            appendUniqueSuggestion(&suggestions, option);
        }
    } else if (index.column() == 1 && !rowIsPositionalAttribute(optionsTable, index.row())) {
        const QString optionToken = (optionsTable->item(index.row(), 0) != nullptr
                                         ? optionsTable->item(index.row(), 0)->text()
                                         : QString())
                                        .trimmed()
                                        .toLower();
        if (!optionToken.isEmpty()) {
            const QString key = commandOptionValueKey(commandName, optionToken);
            for (const QString &value : metadata->commandOptionValueTokens.value(key)) {
                appendUniqueSuggestion(&suggestions, value);
            }
        }
    }

    std::sort(suggestions.begin(), suggestions.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    return suggestions;
}
}

CommandOptionsDialog::CommandOptionsDialog(CommandOptionsDialogContext context)
    : context_(std::move(context))
{
}

QString CommandOptionsDialog::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::CommandOptionsDialog", text);
}

std::optional<QString> CommandOptionsDialog::configureLine(const QString &commandName,
                                                           const QString &sourceLine,
                                                           int lineNumber,
                                                           CommandOptionsIdFieldMode idFieldMode,
                                                           CommandOptionsHelpMode helpMode)
{
    if (context_.commandMetadata == nullptr || lineNumber <= 0) {
        return std::nullopt;
    }

    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(sourceLine);
    const TherionSourceDocumentLine *sourceDocumentLine = sourceDocument.lineAtLineNumber(1);
    if (sourceDocumentLine == nullptr) {
        return std::nullopt;
    }
    TherionParsedLine commandParsedLine = sourceDocumentLine->sourceLine.parsed;
    commandParsedLine.lineNumber = lineNumber;
    if (commandParsedLine.tokens.isEmpty()) {
        return std::nullopt;
    }

    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(sourceLine);
    const QString indent = match.hasMatch() ? match.captured(0) : QString();
    const bool firstArgumentIsId = commandFirstPositionalArgumentIsId(context_.commandMetadata, commandName);
    const bool hasIdField = idFieldMode != CommandOptionsIdFieldMode::None && firstArgumentIsId;
    const bool requiresId = idFieldMode == CommandOptionsIdFieldMode::Required && hasIdField;

    const ParsedCommandOptions parsedOptions = parseCommandOptions(commandName,
                                                                   commandParsedLine.tokens,
                                                                   context_.commandMetadata->commandOptionFixedArityByKey,
                                                                   hasIdField);
    const int positionalArgumentOffset = hasIdField ? 1 : 0;

    QDialog dialog(context_.dialogParent);
    dialog.setWindowTitle(tr("Configure %1").arg(commandName));
    dialog.setModal(true);
    dialog.setMinimumWidth(620);
    dialog.resize(680, 720);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *optionsLabel = new QLabel(tr("Options"), &dialog);
    QFont optionsLabelFont = optionsLabel->font();
    optionsLabelFont.setBold(true);
    optionsLabel->setFont(optionsLabelFont);
    layout->addWidget(optionsLabel);

    auto *optionsActionsLayout = new QHBoxLayout;
    optionsActionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsActionsLayout->setSpacing(6);
    auto *addOptionButton = new QPushButton(tr("Add Option"), &dialog);
    auto *removeOptionButton = new QPushButton(tr("Remove Option"), &dialog);
    addOptionButton->setAutoDefault(false);
    removeOptionButton->setAutoDefault(false);
    optionsActionsLayout->addWidget(addOptionButton);
    optionsActionsLayout->addWidget(removeOptionButton);
    optionsActionsLayout->addStretch(1);
    layout->addLayout(optionsActionsLayout);

    auto *optionsTable = new QTableWidget(&dialog);
    optionsTable->setColumnCount(2);
    optionsTable->setHorizontalHeaderLabels({tr("Option"), tr("Value")});
    optionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    optionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    optionsTable->setColumnWidth(0, 180);
    optionsTable->verticalHeader()->setVisible(false);
    optionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    optionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    optionsTable->setAlternatingRowColors(true);
    optionsTable->setMinimumHeight(180);
    optionsTable->setItemDelegate(new CommandOptionTableDelegate(
        [this, optionsTable, commandName](const QModelIndex &index) {
            return suggestionsForCell(context_.commandMetadata, optionsTable, commandName, index);
        },
        optionsTable));

    if (hasIdField) {
        appendPositionalAttributeRow(optionsTable, QStringLiteral("id"), parsedOptions.leadingValue, 0);
    }
    for (int index = 0; index < parsedOptions.extraPositionalTokens.size(); ++index) {
        const int positionalIndex = positionalArgumentOffset + index;
        appendPositionalAttributeRow(optionsTable,
                                     attributeKeyForPosition(context_.commandMetadata, commandName, positionalIndex),
                                     parsedOptions.extraPositionalTokens.at(index),
                                     positionalIndex);
    }
    for (const CommandOptionEntry &entry : parsedOptions.optionEntries) {
        appendOptionRow(optionsTable, entry.key, entry.value);
    }
    layout->addWidget(optionsTable, 1);

    auto *helpLabel = new QLabel(tr("Help"), &dialog);
    QFont helpLabelFont = helpLabel->font();
    helpLabelFont.setBold(true);
    helpLabel->setFont(helpLabelFont);
    layout->addWidget(helpLabel);

    auto *helpBrowser = new QTextBrowser(&dialog);
    helpBrowser->setOpenLinks(false);
    helpBrowser->setOpenExternalLinks(false);
    helpBrowser->setMinimumHeight(160);
    layout->addWidget(helpBrowser, 1);

    const TherionHelpEntry commandHelpEntry = context_.commandMetadata->helpEntries.value(commandName);
    const QString commandHelpHtml = ContextHelpController::renderHelpHtml(commandName,
                                                                          commandHelpEntry.summary,
                                                                          commandHelpEntry.syntax,
                                                                          commandHelpEntry.arguments,
                                                                          commandHelpEntry.acceptedValues,
                                                                          commandHelpEntry.options,
                                                                          true);

    const auto updateHelpForCurrentOption = [this, helpBrowser, optionsTable, commandName, commandHelpHtml, helpMode]() {
        if (helpBrowser == nullptr || optionsTable == nullptr) {
            return;
        }
        if (helpMode == CommandOptionsHelpMode::CommandOnly) {
            helpBrowser->setHtml(commandHelpHtml);
            return;
        }

        const int row = optionsTable->currentRow();
        if (rowIsPositionalAttribute(optionsTable, row)) {
            const int positionalIndex = optionsTable->item(row, 0)->data(kPositionalIndexRole).toInt();
            const QString key = optionsTable->item(row, 0)->text().trimmed();
            const QStringList signatures = context_.commandMetadata->commandArgumentSignaturesByToken.value(commandName);
            const QString signature = positionalIndex >= 0 && positionalIndex < signatures.size()
                ? signatures.at(positionalIndex)
                : QString();
            helpBrowser->setHtml(argumentHelpHtml(key, signature, commandHelpHtml));
            return;
        }

        const QString optionToken = row >= 0 && optionsTable->item(row, 0) != nullptr
            ? optionsTable->item(row, 0)->text().trimmed().toLower()
            : QString();
        const QString optionHelp = context_.commandMetadata->commandOptionHelpHtmlByKey.value(commandOptionHelpKey(commandName, optionToken));
        if (!optionHelp.trimmed().isEmpty()) {
            helpBrowser->setHtml(optionHelp);
            return;
        }

        helpBrowser->setHtml(commandHelpHtml);
    };
    const auto updateRemoveOptionButtonState = [optionsTable, removeOptionButton]() {
        if (removeOptionButton == nullptr || optionsTable == nullptr) {
            return;
        }
        const int row = optionsTable->currentRow();
        removeOptionButton->setEnabled(row >= 0 && !rowIsPositionalAttribute(optionsTable, row));
    };
    updateHelpForCurrentOption();

    QObject::connect(optionsTable, &QTableWidget::currentCellChanged, &dialog, [updateHelpForCurrentOption, updateRemoveOptionButtonState](int, int, int, int) {
        updateHelpForCurrentOption();
        updateRemoveOptionButtonState();
    });
    QObject::connect(optionsTable, &QTableWidget::itemChanged, &dialog, [updateHelpForCurrentOption](QTableWidgetItem *) {
        updateHelpForCurrentOption();
    });

    QObject::connect(addOptionButton, &QPushButton::clicked, &dialog, [this, optionsTable, commandName, updateHelpForCurrentOption]() {
        if (optionsTable == nullptr) {
            return;
        }
        const int row = optionsTable->rowCount();
        const QString defaultOption = !context_.commandMetadata->commandOptionTokens.value(commandName).isEmpty()
            ? context_.commandMetadata->commandOptionTokens.value(commandName).first()
            : QStringLiteral("-option");
        appendOptionRow(optionsTable, defaultOption, QString());
        optionsTable->setCurrentCell(row, 0);
        optionsTable->editItem(optionsTable->item(row, 0));
        updateHelpForCurrentOption();
    });

    QObject::connect(removeOptionButton, &QPushButton::clicked, &dialog, [optionsTable, updateHelpForCurrentOption]() {
        if (optionsTable == nullptr || optionsTable->rowCount() == 0) {
            return;
        }
        const int row = optionsTable->currentRow() >= 0 ? optionsTable->currentRow() : optionsTable->rowCount() - 1;
        if (rowIsPositionalAttribute(optionsTable, row)) {
            return;
        }
        optionsTable->removeRow(row);
        updateHelpForCurrentOption();
    });
    if (optionsTable->rowCount() > 0) {
        optionsTable->setCurrentCell(0, 0);
    }
    updateRemoveOptionButtonState();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    while (true) {
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }

        QVector<QPair<int, QString>> positionalRows;
        QVector<TextEditorOptionRow> optionRows;
        QVector<int> optionTableRows;
        optionRows.reserve(optionsTable->rowCount());
        bool rejected = false;
        for (int row = 0; row < optionsTable->rowCount(); ++row) {
            const QString key = (optionsTable->item(row, 0) != nullptr
                                     ? optionsTable->item(row, 0)->text()
                                     : QString())
                                    .trimmed();
            const QString value = (optionsTable->item(row, 1) != nullptr
                                       ? optionsTable->item(row, 1)->text()
                                       : QString())
                                      .trimmed();
            if (rowIsPositionalAttribute(optionsTable, row)) {
                const int positionalIndex = optionsTable->item(row, 0)->data(kPositionalIndexRole).toInt();
                const bool optionalId = hasIdField && positionalIndex == 0 && !requiresId;
                if (value.isEmpty() && !optionalId) {
                    optionsTable->setCurrentCell(row, 1);
                    QMessageBox::warning(context_.dialogParent,
                                         tr("Configure Command"),
                                         tr("Attribute `%1` cannot be empty.").arg(key));
                    rejected = true;
                    break;
                }
                if (!value.isEmpty()) {
                    positionalRows.append(qMakePair(positionalIndex, value));
                }
                continue;
            }

            optionRows.append(TextEditorOptionRow{key, value, row + 1});
            optionTableRows.append(row);
        }
        if (rejected) {
            continue;
        }

        std::sort(positionalRows.begin(), positionalRows.end(), [](const QPair<int, QString> &left, const QPair<int, QString> &right) {
            return left.first < right.first;
        });

        const TextEditorOptionValidationResult validation = validateAndSerializeCommandOptions(
            commandName,
            optionRows,
            context_.commandMetadata->commandOptionValueArityTokens,
            context_.commandMetadata->commandOptionFixedArityByKey,
            context_.commandMetadata->commandOptionArgumentLabelsByKey,
            context_.commandMetadata->commandOptionValueTokens,
            false);
        if (!validation.ok) {
            if (validation.failingRow >= 0 && validation.failingRow < optionTableRows.size()) {
                optionsTable->setCurrentCell(optionTableRows.at(validation.failingRow), 0);
            }
            QMessageBox::warning(context_.dialogParent, tr("Configure Command"), validation.errorMessage);
            continue;
        }

        QString updatedLine = QStringLiteral("%1%2").arg(indent, commandName);
        QStringList updatedPositionalArguments;
        updatedPositionalArguments.reserve(positionalRows.size());
        for (const QPair<int, QString> &row : positionalRows) {
            updatedPositionalArguments.append(row.second.trimmed());
        }
        if (!updatedPositionalArguments.isEmpty()) {
            updatedLine += QStringLiteral(" ") + serializeCommandArgumentValues(updatedPositionalArguments);
        }
        if (!validation.serializedOptions.isEmpty()) {
            updatedLine += QStringLiteral(" ") + validation.serializedOptions.join(QLatin1Char(' '));
        }

        return updatedLine;
    }
}
}
