#include "MainWindowTherionConsoleBuilder.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QLabel *createFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return label;
}

void addLabeledWidget(QVBoxLayout *layout, const QString &labelText, QWidget *widget, QWidget *parent)
{
    layout->addWidget(createFieldLabel(labelText, parent));
    layout->addWidget(widget);
}

void styleHelperValue(QLabel *label)
{
    if (label == nullptr) {
        return;
    }

    label->setStyleSheet(QStringLiteral(
        "QLabel {"
        " color: palette(midlight);"
        " font-size: 11px;"
        "}"));
}
}

namespace TherionStudio
{
MainWindowTherionConsoleBuilder::BuildResult
MainWindowTherionConsoleBuilder::build(const BuildInput &input)
{
    BuildResult result;

    auto *widget = new QWidget(input.consoleHost);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *description = new QLabel(
        QCoreApplication::translate("MainWindow", "Run Therion for the current project or active config."), widget);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *settingsLayout = new QVBoxLayout;
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(5);

    result.therionArgumentsEdit = new QLineEdit(widget);
    result.therionArgumentsEdit->setPlaceholderText(
        QCoreApplication::translate("MainWindow", "Additional Therion command-line options"));
    result.therionArgumentsEdit->setText(input.initialArguments);
    result.therionArgumentsEdit->setCursorPosition(result.therionArgumentsEdit->text().size());
    addLabeledWidget(settingsLayout, QCoreApplication::translate("MainWindow", "Arguments"), result.therionArgumentsEdit, widget);

    result.therionRunTargetCombo = new QComboBox(widget);
    result.therionRunTargetCombo->addItem(QCoreApplication::translate("MainWindow", "Current Config"), QStringLiteral("current"));
    result.therionRunTargetCombo->addItem(QCoreApplication::translate("MainWindow", "Project Config"), QStringLiteral("project"));
    const int selectedTargetIndex = result.therionRunTargetCombo->findData(input.persistedRunTargetMode);
    const int defaultTargetIndex = result.therionRunTargetCombo->findData(QStringLiteral("project"));
    result.therionRunTargetCombo->setCurrentIndex(selectedTargetIndex >= 0 ? selectedTargetIndex : defaultTargetIndex);
    addLabeledWidget(settingsLayout, QCoreApplication::translate("MainWindow", "Run Target"), result.therionRunTargetCombo, widget);

    result.therionTargetConfigEdit = new QLineEdit(widget);
    result.therionTargetConfigEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Path to project Therion config"));
    result.therionTargetConfigEdit->setText(input.persistedTargetConfigPath);
    result.therionTargetConfigEdit->setCursorPosition(result.therionTargetConfigEdit->text().size());
    result.therionBrowseTargetConfigButton = new QPushButton(QCoreApplication::translate("MainWindow", "..."), widget);
    result.therionBrowseTargetConfigButton->setToolTip(QCoreApplication::translate("MainWindow", "Browse for target Therion config"));
    result.therionBrowseTargetConfigButton->setAccessibleName(QCoreApplication::translate("MainWindow", "Browse for target Therion config"));
    result.therionBrowseTargetConfigButton->setFixedWidth(34);

    auto *targetConfigRow = new QWidget(widget);
    auto *targetConfigRowLayout = new QHBoxLayout(targetConfigRow);
    targetConfigRowLayout->setContentsMargins(0, 0, 0, 0);
    targetConfigRowLayout->setSpacing(6);
    targetConfigRowLayout->addWidget(result.therionTargetConfigEdit, 1);
    targetConfigRowLayout->addWidget(result.therionBrowseTargetConfigButton);
    addLabeledWidget(settingsLayout, QCoreApplication::translate("MainWindow", "Target Config"), targetConfigRow, widget);

    result.therionConfigNameValue = new QLabel(QCoreApplication::translate("MainWindow", "Auto-detect"), widget);
    result.therionConfigNameValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionConfigNameValue->setWordWrap(true);
    result.therionConfigNameValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    {
        QFont font = result.therionConfigNameValue->font();
        font.setBold(true);
        result.therionConfigNameValue->setFont(font);
    }
    settingsLayout->addWidget(result.therionConfigNameValue);
    result.therionConfigPathValue =
        new QLabel(QCoreApplication::translate("MainWindow", "No config file resolved from the current context"), widget);
    result.therionConfigPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionConfigPathValue->setWordWrap(true);
    result.therionConfigPathValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    styleHelperValue(result.therionConfigPathValue);
    settingsLayout->addWidget(result.therionConfigPathValue);

    result.therionWorkingDirectoryEdit = new QLineEdit(widget);
    result.therionWorkingDirectoryEdit->setPlaceholderText(
        QCoreApplication::translate("MainWindow", "Auto: selected config folder"));
    result.therionWorkingDirectoryEdit->setText(input.persistedWorkingDirectory);
    result.therionWorkingDirectoryEdit->setCursorPosition(result.therionWorkingDirectoryEdit->text().size());
    result.therionBrowseWorkingDirectoryButton = new QPushButton(QCoreApplication::translate("MainWindow", "..."), widget);
    result.therionBrowseWorkingDirectoryButton->setToolTip(
        QCoreApplication::translate("MainWindow", "Browse for working directory override"));
    result.therionBrowseWorkingDirectoryButton->setAccessibleName(
        QCoreApplication::translate("MainWindow", "Browse for working directory override"));
    result.therionBrowseWorkingDirectoryButton->setFixedWidth(34);

    auto *workingDirectoryRow = new QWidget(widget);
    auto *workingDirectoryRowLayout = new QHBoxLayout(workingDirectoryRow);
    workingDirectoryRowLayout->setContentsMargins(0, 0, 0, 0);
    workingDirectoryRowLayout->setSpacing(6);
    workingDirectoryRowLayout->addWidget(result.therionWorkingDirectoryEdit, 1);
    workingDirectoryRowLayout->addWidget(result.therionBrowseWorkingDirectoryButton);
    addLabeledWidget(settingsLayout, QCoreApplication::translate("MainWindow", "Working Directory Override"), workingDirectoryRow, widget);

    result.therionWorkingDirectoryValue =
        new QLabel(QCoreApplication::translate("MainWindow", "Project root or selected config folder"), widget);
    result.therionWorkingDirectoryValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionWorkingDirectoryValue->setWordWrap(true);
    result.therionWorkingDirectoryValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    styleHelperValue(result.therionWorkingDirectoryValue);
    settingsLayout->addWidget(result.therionWorkingDirectoryValue);

    result.therionResetWorkingDirectoryButton =
        new QPushButton(QCoreApplication::translate("MainWindow", "Reset Override"), widget);
    result.therionResetWorkingDirectoryButton->setToolTip(
        QCoreApplication::translate("MainWindow", "Clear the project working directory override and use the selected config folder"));
    result.therionResetWorkingDirectoryButton->setEnabled(false);
    result.therionResetWorkingDirectoryButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsLayout->addWidget(result.therionResetWorkingDirectoryButton);
    settingsLayout->addSpacing(10);

    layout->addLayout(settingsLayout);

    auto *buttonGrid = new QGridLayout;
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setHorizontalSpacing(6);
    buttonGrid->setVerticalSpacing(6);
    result.therionRunButton = new QPushButton(QCoreApplication::translate("MainWindow", "Run Therion"), widget);
    result.therionStopButton = new QPushButton(QCoreApplication::translate("MainWindow", "Stop"), widget);
    result.therionStopButton->setEnabled(false);
    result.therionClearOutputButton = new QPushButton(QCoreApplication::translate("MainWindow", "Clear Output"), widget);
    result.therionCopyOutputButton = new QPushButton(QCoreApplication::translate("MainWindow", "Copy Output"), widget);

    for (QPushButton *button : {result.therionRunButton,
                                result.therionStopButton,
                                result.therionClearOutputButton,
                                result.therionCopyOutputButton}) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    buttonGrid->addWidget(result.therionRunButton, 0, 0);
    buttonGrid->addWidget(result.therionStopButton, 0, 1);
    buttonGrid->addWidget(result.therionClearOutputButton, 1, 0);
    buttonGrid->addWidget(result.therionCopyOutputButton, 1, 1);
    buttonGrid->setColumnStretch(0, 1);
    buttonGrid->setColumnStretch(1, 1);
    layout->addLayout(buttonGrid);

    result.consoleView = new QPlainTextEdit(widget);
    result.consoleView->setReadOnly(true);
    result.consoleView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    result.consoleView->setPlaceholderText(QCoreApplication::translate("MainWindow", "Therion runner output will appear here."));
    layout->addWidget(result.consoleView, 1);

    result.rootWidget = widget;
    return result;
}
}
