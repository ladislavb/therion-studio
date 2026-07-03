#pragma once

#include <QString>

class QLabel;
class QComboBox;
class QLineEdit;
class QTextBrowser;
class QPushButton;
class QWidget;

namespace TherionStudio
{
class MainWindowTherionConsoleBuilder final
{
public:
    struct BuildInput
    {
        QWidget *consoleHost = nullptr;
        QString persistedWorkingDirectory;
        QString initialArguments;
        QString persistedRunTargetMode;
        QString persistedTargetConfigPath;
    };

    struct BuildResult
    {
        QWidget *rootWidget = nullptr;
        QLineEdit *therionWorkingDirectoryEdit = nullptr;
        QPushButton *therionBrowseWorkingDirectoryButton = nullptr;
        QLineEdit *therionArgumentsEdit = nullptr;
        QComboBox *therionRunTargetCombo = nullptr;
        QLineEdit *therionTargetConfigEdit = nullptr;
        QPushButton *therionBrowseTargetConfigButton = nullptr;
        QLabel *therionConfigNameValue = nullptr;
        QLabel *therionConfigPathValue = nullptr;
        QLabel *therionWorkingDirectoryValue = nullptr;
        QPushButton *therionRunButton = nullptr;
        QPushButton *therionStopButton = nullptr;
        QPushButton *therionResetWorkingDirectoryButton = nullptr;
        QPushButton *therionClearOutputButton = nullptr;
        QPushButton *therionCopyOutputButton = nullptr;
        QTextBrowser *consoleView = nullptr;
    };

    static BuildResult build(const BuildInput &input);
};
}
