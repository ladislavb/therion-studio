#include "BlockEditorOptionArgsController.h"

#include <QCoreApplication>

#include "../TextEditorCommandMetadata.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <utility>

namespace TherionStudio
{
BlockEditorOptionArgsController::BlockEditorOptionArgsController(BlockEditorOptionArgsContext context)
    : context_(std::move(context))
{
}

QString BlockEditorOptionArgsController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorOptionArgsController", text);
}

bool BlockEditorOptionArgsController::hasRequiredContext() const
{
    return context_.signalContext != nullptr
        && context_.optionArgsLabel != nullptr
        && context_.optionArgsPanel != nullptr
        && context_.optionArgsFormLayout != nullptr
        && context_.optionsTable != nullptr
        && context_.optionArgEditors != nullptr
        && context_.optionArgsSyncing != nullptr
        && context_.commandMetadata != nullptr
        && context_.isStructuredOptionsMode
        && context_.selectedKind
        && context_.normalizeDirectiveToken
        && context_.refreshApplyState
        && context_.updateHelpForCurrentFocus;
}

void BlockEditorOptionArgsController::clearEditors()
{
    *context_.optionArgsSyncing = true;
    context_.optionArgEditors->clear();
    while (context_.optionArgsFormLayout->rowCount() > 0) {
        QFormLayout::TakeRowResult row = context_.optionArgsFormLayout->takeRow(0);
        if (row.labelItem != nullptr) {
            if (QWidget *widget = row.labelItem->widget(); widget != nullptr) {
                widget->deleteLater();
            }
            delete row.labelItem;
        }
        if (row.fieldItem != nullptr) {
            if (QWidget *widget = row.fieldItem->widget(); widget != nullptr) {
                widget->deleteLater();
            }
            delete row.fieldItem;
        }
    }
    *context_.optionArgsSyncing = false;
}

void BlockEditorOptionArgsController::setArgumentEditorsVisible(bool visible)
{
    context_.optionArgsLabel->setVisible(visible);
    context_.optionArgsPanel->setVisible(visible);
}

void BlockEditorOptionArgsController::refreshOptionArgumentEditors()
{
    if (!hasRequiredContext()) {
        return;
    }

    const bool supportedMode = context_.isStructuredOptionsMode()
        && context_.optionsTable->isVisible()
        && context_.optionsTable->isEnabled();
    if (!supportedMode) {
        clearEditors();
        setArgumentEditorsVisible(false);
        return;
    }

    auto updateOptionValueCellEditability = [this]() {
        QSignalBlocker tableSignalBlocker(context_.optionsTable);
        const QString commandToken = context_.normalizeDirectiveToken(context_.selectedKind());
        const QString multiValueToolTip
            = tr("Use Selected Option Parameters below to edit this multi-value option.");
        for (int optionRow = 0; optionRow < context_.optionsTable->rowCount(); ++optionRow) {
            QTableWidgetItem *valueItem = context_.optionsTable->item(optionRow, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem(QString());
                context_.optionsTable->setItem(optionRow, 1, valueItem);
            }
            const QString optionToken = (context_.optionsTable->item(optionRow, 0) != nullptr
                                             ? context_.optionsTable->item(optionRow, 0)->text()
                                             : QString())
                                            .trimmed()
                                            .toLower();
            const int fixedArity = context_.commandMetadata->commandOptionFixedArityByKey.value(
                commandOptionValueKey(commandToken, optionToken), -1);
            Qt::ItemFlags flags = valueItem->flags();
            if (fixedArity > 1) {
                flags &= ~Qt::ItemIsEditable;
                if (valueItem->toolTip() != multiValueToolTip) {
                    valueItem->setToolTip(multiValueToolTip);
                }
            } else {
                flags |= Qt::ItemIsEditable;
                if (!valueItem->toolTip().isEmpty()) {
                    valueItem->setToolTip(QString());
                }
            }
            if (valueItem->flags() != flags) {
                valueItem->setFlags(flags);
            }
        }
    };

    updateOptionValueCellEditability();

    const int row = context_.optionsTable->currentRow();
    if (row < 0 || row >= context_.optionsTable->rowCount()) {
        clearEditors();
        setArgumentEditorsVisible(false);
        return;
    }

    const QString commandToken = context_.normalizeDirectiveToken(context_.selectedKind());
    const QString optionToken = (context_.optionsTable->item(row, 0) != nullptr
                                     ? context_.optionsTable->item(row, 0)->text()
                                     : QString())
                                    .trimmed()
                                    .toLower();
    if (optionToken.isEmpty()) {
        clearEditors();
        setArgumentEditorsVisible(false);
        return;
    }

    const QString optionKey = commandOptionValueKey(commandToken, optionToken);
    const int fixedArity = context_.commandMetadata->commandOptionFixedArityByKey.value(optionKey, -1);
    if (fixedArity <= 1) {
        clearEditors();
        setArgumentEditorsVisible(false);
        return;
    }

    QStringList argumentLabels = context_.commandMetadata->commandOptionArgumentLabelsByKey.value(optionKey);
    while (argumentLabels.size() < fixedArity) {
        argumentLabels.append(tr("Value %1").arg(argumentLabels.size() + 1));
    }
    if (argumentLabels.size() > fixedArity) {
        argumentLabels = argumentLabels.mid(0, fixedArity);
    }

    const QString valueText = (context_.optionsTable->item(row, 1) != nullptr
                                   ? context_.optionsTable->item(row, 1)->text()
                                   : QString())
                                  .trimmed();
    const QString rawArityToken = context_.commandMetadata->commandOptionValueArityTokens.value(optionKey);
    const QStringList valueTokens = parseOptionValuesFromEditor(valueText, rawArityToken, fixedArity);

    clearEditors();
    *context_.optionArgsSyncing = true;
    for (int index = 0; index < fixedArity; ++index) {
        auto *edit = new QLineEdit(context_.optionArgsPanel);
        edit->setPlaceholderText(tr("required"));
        edit->setText(index < valueTokens.size() ? valueTokens.at(index) : QString());
        context_.optionArgsFormLayout->addRow(argumentLabels.at(index), edit);
        context_.optionArgEditors->append(edit);

        QObject::connect(edit, &QLineEdit::textChanged, context_.signalContext, [this, optionKey, row](const QString &) {
            if (*context_.optionArgsSyncing
                || row < 0
                || row >= context_.optionsTable->rowCount()) {
                return;
            }

            const QString selectedOptionToken = (context_.optionsTable->item(row, 0) != nullptr
                                                     ? context_.optionsTable->item(row, 0)->text()
                                                     : QString())
                                                    .trimmed()
                                                    .toLower();
            if (commandOptionValueKey(context_.normalizeDirectiveToken(context_.selectedKind()), selectedOptionToken)
                != optionKey) {
                return;
            }

            QStringList rawValues;
            rawValues.reserve(context_.optionArgEditors->size());
            for (QLineEdit *valueEdit : *context_.optionArgEditors) {
                const QString rawValue = valueEdit != nullptr ? valueEdit->text().trimmed() : QString();
                rawValues.append(rawValue);
            }

            QSignalBlocker blocker(context_.optionsTable);
            *context_.optionArgsSyncing = true;
            QTableWidgetItem *valueItem = context_.optionsTable->item(row, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem;
                context_.optionsTable->setItem(row, 1, valueItem);
            }
            valueItem->setText(serializeCommandArgumentValues(rawValues));
            *context_.optionArgsSyncing = false;

            context_.refreshApplyState();
            context_.updateHelpForCurrentFocus();
        });
        QObject::connect(edit, &QLineEdit::selectionChanged, context_.signalContext, [this]() {
            context_.updateHelpForCurrentFocus();
        });
        QObject::connect(edit, &QLineEdit::editingFinished, context_.signalContext, [this]() {
            if (!*context_.optionArgsSyncing && context_.applyChanges) {
                context_.applyChanges();
            }
        });
    }
    *context_.optionArgsSyncing = false;

    setArgumentEditorsVisible(true);
}
}
