#include "MainWindow.h"

#include "MainWindowTherionConsoleBootstrapper.h"
#include "MainWindowDocumentHelpers.h"
#include "MainWindowTherionConsoleBuilder.h"
#include "MainWindowTherionConsoleWiring.h"
#include "MainWindowTherionRunnerController.h"
#include "TherionRunnerConfigDisplayController.h"
#include "TherionRunnerLifecyclePresenter.h"
#include "TherionRunnerOutputLinker.h"
#include "TherionRunnerService.h"
#include "TherionRunnerStartResultPresenter.h"
#include "TherionRunnerStartSuccessPresenter.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QComboBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTextBrowser>
#include <QUrl>

namespace
{
constexpr auto kTherionRunTargetCurrent = "current";
constexpr auto kTherionRunTargetProject = "project";
}

void MainWindow::buildConsole()
{
    QWidget *consoleHost = consoleSidebarPage_ != nullptr ? consoleSidebarPage_ : this;
    TherionStudio::MainWindowTherionConsoleBuilder::BuildInput buildInput;
    buildInput.consoleHost = consoleHost;
    buildInput.persistedWorkingDirectory = sessionStore_->therionWorkingDirectory().trimmed();
    buildInput.initialArguments = QString();
    buildInput.persistedRunTargetMode = sessionStore_->therionRunTargetMode().trimmed();
    buildInput.persistedTargetConfigPath = sessionStore_->therionTargetConfigPath().trimmed();
    const TherionStudio::MainWindowTherionConsoleBuilder::BuildResult buildResult =
        TherionStudio::MainWindowTherionConsoleBuilder::build(buildInput);

    therionWorkingDirectoryEdit_ = buildResult.therionWorkingDirectoryEdit;
    therionBrowseWorkingDirectoryButton_ = buildResult.therionBrowseWorkingDirectoryButton;
    therionArgumentsEdit_ = buildResult.therionArgumentsEdit;
    therionRunTargetCombo_ = buildResult.therionRunTargetCombo;
    therionTargetConfigEdit_ = buildResult.therionTargetConfigEdit;
    therionBrowseTargetConfigButton_ = buildResult.therionBrowseTargetConfigButton;
    therionConfigNameValue_ = buildResult.therionConfigNameValue;
    therionConfigPathValue_ = buildResult.therionConfigPathValue;
    therionWorkingDirectoryValue_ = buildResult.therionWorkingDirectoryValue;
    therionRunButton_ = buildResult.therionRunButton;
    therionStopButton_ = buildResult.therionStopButton;
    therionResetWorkingDirectoryButton_ = buildResult.therionResetWorkingDirectoryButton;
    therionClearOutputButton_ = buildResult.therionClearOutputButton;
    therionCopyOutputButton_ = buildResult.therionCopyOutputButton;
    consoleView_ = buildResult.consoleView;
    if (consoleView_ != nullptr) {
        connect(consoleView_,
                &QTextBrowser::anchorClicked,
                this,
                &MainWindow::handleTherionConsoleLinkActivated);
    }

    therionRunnerService_ = new TherionStudio::TherionRunnerService(this);
    TherionStudio::MainWindowTherionConsoleWiring::WiringInput wiringInput;
    wiringInput.context = this;
    wiringInput.therionRunButton = therionRunButton_;
    wiringInput.therionStopButton = therionStopButton_;
    wiringInput.therionBrowseTargetConfigButton = therionBrowseTargetConfigButton_;
    wiringInput.therionBrowseWorkingDirectoryButton = therionBrowseWorkingDirectoryButton_;
    wiringInput.therionResetWorkingDirectoryButton = therionResetWorkingDirectoryButton_;
    wiringInput.therionClearOutputButton = therionClearOutputButton_;
    wiringInput.therionCopyOutputButton = therionCopyOutputButton_;
    wiringInput.therionWorkingDirectoryEdit = therionWorkingDirectoryEdit_;
    wiringInput.therionArgumentsEdit = therionArgumentsEdit_;
    wiringInput.therionRunTargetCombo = therionRunTargetCombo_;
    wiringInput.therionTargetConfigEdit = therionTargetConfigEdit_;
    wiringInput.therionRunnerService = therionRunnerService_;
    wiringInput.onRunRequested = [this]() { runTherion(); };
    wiringInput.onStopRequested = [this]() { stopTherion(); };
    wiringInput.onBrowseTargetConfigRequested = [this]() { browseTherionTargetConfig(); };
    wiringInput.onBrowseWorkingDirectoryRequested = [this]() { browseTherionWorkingDirectoryOverride(); };
    wiringInput.onResetWorkingDirectoryRequested = [this]() {
        therionWorkingDirectoryEdit_->clear();
    };
    wiringInput.onClearOutputRequested = [this]() { clearTherionConsoleOutput(); };
    wiringInput.onCopyOutputRequested = [this]() { copyTherionConsoleOutput(); };
    wiringInput.onWorkingDirectoryChanged = [this](const QString &) {
        refreshTherionConfigDisplay();
        requestStructureSidebarRebuild();
    };
    wiringInput.onArgumentsChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
    wiringInput.onRunTargetChanged = [this]() { refreshTherionConfigDisplay(); };
    wiringInput.onTargetConfigChanged = [this](const QString &) {
        refreshTherionConfigDisplay();
        requestStructureSidebarRebuild();
    };
    wiringInput.onRunnerStandardOutput = [this](const QString &output) {
        handleTherionRunnerStandardOutput(output);
    };
    wiringInput.onRunnerStandardError = [this](const QString &output) {
        handleTherionRunnerStandardError(output);
    };
    wiringInput.onRunnerFinished = [this](int exitCode, QProcess::ExitStatus exitStatus) {
        handleTherionRunnerFinished(exitCode, exitStatus);
    };
    wiringInput.onRunnerError = [this](const QString &errorText) { handleTherionRunnerError(errorText); };
    wiringInput.onRunnerStateChanged = [this](bool running) { handleTherionRunnerStateChanged(running); };
    TherionStudio::MainWindowTherionConsoleWiring::wire(wiringInput);

    TherionStudio::MainWindowTherionConsoleBootstrapper::BootstrapInput bootstrapInput;
    bootstrapInput.consoleController = &therionConsoleController_;
    bootstrapInput.consoleView = consoleView_;
    bootstrapInput.therionRunButton = therionRunButton_;
    bootstrapInput.therionStopButton = therionStopButton_;
    bootstrapInput.therionWorkingDirectoryEdit = therionWorkingDirectoryEdit_;
    bootstrapInput.therionArgumentsEdit = therionArgumentsEdit_;
    bootstrapInput.consoleSidebarPageLayout = consoleSidebarPageLayout_;
    bootstrapInput.rootWidget = buildResult.rootWidget;
    bootstrapInput.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    bootstrapInput.updateTherionRunnerState = [this]() { updateTherionRunnerState(); };
    TherionStudio::MainWindowTherionConsoleBootstrapper::bootstrap(bootstrapInput);
}

void MainWindow::appendConsoleLine(const QString &line)
{
    Q_UNUSED(line);
}

void MainWindow::copyTherionConsoleOutput()
{
    const TherionStudio::MainWindowTherionConsoleController::CopyOutputResult result =
        therionConsoleController_.copyConsoleOutputToClipboard();
    switch (result) {
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::Empty:
        statusBar()->showMessage(tr("Console output is empty"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::ClipboardUnavailable:
        statusBar()->showMessage(tr("Clipboard is unavailable"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::ConsoleUnavailable:
        statusBar()->showMessage(tr("Console output is unavailable"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::Copied:
        break;
    }
    statusBar()->showMessage(tr("Copied console output"), 2000);
}

void MainWindow::clearTherionConsoleOutput()
{
    if (!therionConsoleController_.clearConsoleOutput()) {
        statusBar()->showMessage(tr("Console output is unavailable"), 2000);
        return;
    }

    statusBar()->showMessage(tr("Cleared console output"), 2000);
}

void MainWindow::browseTherionTargetConfig()
{
    QString initialPath = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString();
    if (initialPath.isEmpty()) {
        initialPath = resolvedTherionConfigPath();
    }
    if (initialPath.isEmpty()) {
        initialPath = therionConfigResolutionDirectory();
    }
    if (initialPath.isEmpty()) {
        initialPath = projectRootPath_;
    }

    const QString selectedConfigPath =
        QFileDialog::getOpenFileName(this,
                                     tr("Select Therion Config"),
                                     initialPath,
                                     tr("Therion config files (thconfig thconfig.* *.thconfig);;All files (*)"));
    if (selectedConfigPath.isEmpty() || therionTargetConfigEdit_ == nullptr) {
        return;
    }

    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (projectIndex >= 0) {
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }
    therionTargetConfigEdit_->setText(selectedConfigPath);
    therionTargetConfigEdit_->setCursorPosition(therionTargetConfigEdit_->text().size());
}

void MainWindow::browseTherionWorkingDirectoryOverride()
{
    QString initialPath = therionWorkingDirectoryEdit_ != nullptr
        ? therionWorkingDirectoryEdit_->text().trimmed()
        : QString();
    if (initialPath.isEmpty()) {
        initialPath = resolvedTherionWorkingDirectory();
    }
    if (initialPath.isEmpty()) {
        initialPath = projectRootPath_;
    }

    const QString selectedDirectory =
        QFileDialog::getExistingDirectory(this,
                                          tr("Select Working Directory Override"),
                                          initialPath);
    if (selectedDirectory.isEmpty() || therionWorkingDirectoryEdit_ == nullptr) {
        return;
    }

    therionWorkingDirectoryEdit_->setText(selectedDirectory);
    therionWorkingDirectoryEdit_->setCursorPosition(therionWorkingDirectoryEdit_->text().size());
}

QString MainWindow::therionWorkingDirectoryOverride() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    const auto state = TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input);
    return state.projectConfigActive ? input.workingDirectoryOverrideText.trimmed() : QString();
}

QString MainWindow::therionConfigResolutionDirectory() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).configResolutionDirectory;
}

QString MainWindow::resolvedTherionWorkingDirectory() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).resolvedWorkingDirectory;
}

bool MainWindow::hasExplicitTherionConfigArgument() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).hasExplicitConfigArgument;
}

QString MainWindow::therionExecutableInput() const
{
    return sessionStore_ != nullptr
        ? sessionStore_->therionExecutablePath().trimmed()
        : QString();
}

QString MainWindow::therionRunTargetMode() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).effectiveRunTargetMode;
}

QString MainWindow::currentDocumentTherionConfigPath() const
{
    QWidget *activeWidget = currentDocumentWidget();
    const QString activeDocumentPath =
        activeWidget != nullptr ? documentPathForWidget(activeWidget) : QString();
    return TherionStudio::MainWindowTherionRunnerController::currentDocumentConfigPath(activeDocumentPath);
}

QString MainWindow::resolvedTherionTargetConfigPath() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).resolvedTargetConfigPath;
}

void MainWindow::resetProjectTherionRunContext()
{
    if (therionWorkingDirectoryEdit_ != nullptr) {
        const QSignalBlocker blocker(therionWorkingDirectoryEdit_);
        therionWorkingDirectoryEdit_->clear();
    }
    if (therionTargetConfigEdit_ != nullptr) {
        const QSignalBlocker blocker(therionTargetConfigEdit_);
        therionTargetConfigEdit_->clear();
    }
    if (sessionStore_ != nullptr) {
        sessionStore_->setTherionWorkingDirectory(QString());
        sessionStore_->setTherionTargetConfigPath(QString());
    }

    activeTherionRunConfigPath_.clear();
}

QString MainWindow::resolvedTherionConfigPath() const
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    return TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input).resolvedConfigPath;
}

void MainWindow::refreshTherionConfigDisplay()
{
    if (therionConfigNameValue_ == nullptr || therionConfigPathValue_ == nullptr) {
        return;
    }

    refreshTherionRunTargetControls();
    const QString resolvedPath = resolvedTherionConfigPath();
    const TherionStudio::TherionRunnerConfigDisplayController::RefreshComputation computation =
        TherionStudio::TherionRunnerConfigDisplayController::compute(
            resolvedPath);
    const QString workingDirectory = resolvedTherionWorkingDirectory();
    if (therionWorkingDirectoryValue_ != nullptr) {
        therionWorkingDirectoryValue_->setText(workingDirectory.isEmpty()
                                                   ? tr("No working directory resolved")
                                                   : workingDirectory);
        therionWorkingDirectoryValue_->setToolTip(workingDirectory);
    }

    if (!computation.hasResolvedConfigPath) {
        therionConfigNameValue_->setText(tr("Auto-detect"));
        therionConfigPathValue_->setText(tr("No config file resolved from the current context"));
        therionConfigPathValue_->setToolTip(QString());
        return;
    }

    therionConfigNameValue_->setText(computation.configName.isEmpty() ? tr("thconfig") : computation.configName);
    therionConfigPathValue_->setText(computation.configPath);
    therionConfigPathValue_->setToolTip(computation.configPath);

}

void MainWindow::refreshTherionRunTargetControls()
{
    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    const auto state = TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input);
    const bool currentConfigAvailable = state.currentConfigAvailable;
    if (therionRunTargetCombo_ != nullptr) {
        const int currentIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetCurrent));
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (currentConfigAvailable && currentIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(currentIndex);
        } else if (!currentConfigAvailable && projectIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
        therionRunTargetCombo_->setEnabled(currentConfigAvailable);
        therionRunTargetCombo_->setToolTip(
            currentConfigAvailable
                ? QString()
                : tr("Open a Therion config tab to use Current Config."));
    }

    const bool projectConfigActive = state.projectConfigActive;
    if (therionTargetConfigEdit_ != nullptr) {
        therionTargetConfigEdit_->setEnabled(projectConfigActive);
    }
    if (therionBrowseTargetConfigButton_ != nullptr) {
        therionBrowseTargetConfigButton_->setEnabled(projectConfigActive);
    }
    if (therionWorkingDirectoryEdit_ != nullptr) {
        therionWorkingDirectoryEdit_->setEnabled(projectConfigActive);
        therionWorkingDirectoryEdit_->setToolTip(
            projectConfigActive
                ? QString()
                : tr("Current Config always runs from the active config file folder."));
    }
    if (therionBrowseWorkingDirectoryButton_ != nullptr) {
        therionBrowseWorkingDirectoryButton_->setEnabled(projectConfigActive);
    }
    if (therionResetWorkingDirectoryButton_ != nullptr) {
        const bool hasOverride = therionWorkingDirectoryEdit_ != nullptr
            && !therionWorkingDirectoryEdit_->text().trimmed().isEmpty();
        therionResetWorkingDirectoryButton_->setEnabled(projectConfigActive && hasOverride);
    }
}

void MainWindow::updateTherionRunnerState()
{
    const bool isRunning = therionRunnerService_ != nullptr && therionRunnerService_->isRunning();
    therionConsoleController_.applyRunnerState(isRunning);
}

void MainWindow::runTherion()
{
    if (therionRunnerService_ == nullptr) {
        statusBar()->showMessage(tr("Therion runner is unavailable."), 3000);
        setCompilerStatusResult(false, tr("Therion runner is unavailable."));
        return;
    }

    if (!saveAllOpenDocuments()) {
        const QString message = tr("Compile canceled because open documents could not be saved.");
        statusBar()->showMessage(message, 3000);
        setCompilerStatusResult(false, message);
        return;
    }

    const QString currentDocumentPath =
        currentDocumentWidget() != nullptr ? documentPathForWidget(currentDocumentWidget()) : QString();
    TherionStudio::MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectRootPath_;
    input.executableText = therionExecutableInput();
    input.workingDirectoryOverrideText = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString();
    input.argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text() : QString();
    input.runTargetMode = therionRunTargetCombo_ != nullptr ? therionRunTargetCombo_->currentData().toString() : QString();
    input.targetConfigPathText = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text() : QString();
    input.currentDocumentPath = currentDocumentPath;
    const auto state = TherionStudio::MainWindowTherionRunnerController::computeRuntimeState(input);
    if (!state.canRun) {
        const QString message = tr("Select a current or project Therion config before running Therion.");
        QMessageBox::warning(this, tr("Run Therion"), message);
        setCompilerStatusResult(false, message);
        return;
    }

    if (!therionRunnerService_->isRunning()) {
        therionConsoleController_.clearConsoleOutput();
        activeTherionRunStandardError_.clear();
    }

    const TherionStudio::TherionRunnerService::StartResult startResult =
        therionRunnerService_->start(state.executableInput, state.resolvedWorkingDirectory, state.runArguments);
    const TherionStudio::TherionRunnerStartResultPresenter::Presentation startPresentation =
        TherionStudio::TherionRunnerStartResultPresenter::present(startResult.code, state.executableInput);

    if (startPresentation.isHandled) {
        if (startPresentation.showWarningDialog) {
            QMessageBox::warning(this,
                                 startPresentation.warningDialogTitle,
                                 startPresentation.warningDialogMessage);
        }
        if (startPresentation.showStatusBarMessage) {
            statusBar()->showMessage(startPresentation.statusBarMessage,
                                     startPresentation.statusBarTimeoutMs);
        }
        if (startResult.code != TherionStudio::TherionRunnerService::StartCode::AlreadyRunning) {
            setCompilerStatusResult(false,
                                    startPresentation.statusLabelMessage.isEmpty()
                                        ? startPresentation.warningDialogMessage
                                        : startPresentation.statusLabelMessage);
        }
        return;
    }

    activeTherionRunConfigPath_ = state.resolvedConfigPath;
    activeTherionRunWorkingDirectory_ = state.resolvedWorkingDirectory;
    const TherionStudio::TherionRunnerStartSuccessPresenter::Presentation successPresentation =
        TherionStudio::TherionRunnerStartSuccessPresenter::present(startResult,
                                                                   state.runArguments.join(QLatin1Char(' ')),
                                                                   state.resolvedWorkingDirectory);
    therionConsoleController_.appendConsoleLine(successPresentation.consoleMessage);
    setCompilerStatusRunning(activeTherionRunConfigPath_);

    updateTherionRunnerState();
}

void MainWindow::runTherionProjectConfig()
{
    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (projectIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }

    runTherion();
    refreshTherionConfigDisplay();
}

void MainWindow::runTherionCurrentConfig()
{
    if (currentDocumentTherionConfigPath().isEmpty()) {
        const QString message = tr("Open a Therion config tab before compiling the current config.");
        QMessageBox::warning(this, tr("Run Therion"), message);
        setCompilerStatusResult(false, message);
        return;
    }

    if (therionRunTargetCombo_ != nullptr) {
        const int currentIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetCurrent));
        if (currentIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(currentIndex);
        }
    }

    runTherion();
    refreshTherionConfigDisplay();
}

void MainWindow::stopTherion()
{
    const TherionStudio::TherionRunnerLifecyclePresenter::StopPresentation stopPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentStopRequest(
            therionRunnerService_ != nullptr && therionRunnerService_->isRunning());

    if (!stopPresentation.shouldStopProcess || therionRunnerService_ == nullptr) {
        if (stopPresentation.shouldUpdateStatusLabel) {
            statusBar()->showMessage(stopPresentation.statusLabelMessage, 2000);
        }
        return;
    }

    therionRunnerService_->stop();
    if (stopPresentation.shouldUpdateStatusLabel) {
        statusBar()->showMessage(stopPresentation.statusLabelMessage, 2000);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerStandardOutput(const QString &output)
{
    therionConsoleController_.appendProcessStandardOutput(output, activeTherionRunWorkingDirectory_);
}

void MainWindow::handleTherionRunnerStandardError(const QString &output)
{
    activeTherionRunStandardError_ += output;
    therionConsoleController_.appendProcessStandardError(output,
                                                        tr("[stderr] %1"),
                                                        activeTherionRunWorkingDirectory_);
}

void MainWindow::handleTherionRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const TherionStudio::TherionRunnerLifecyclePresenter::EventPresentation eventPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentFinished(exitCode,
                                                                        exitStatus,
                                                                        activeTherionRunStandardError_);
    setCompilerStatusResult(eventPresentation.succeeded, eventPresentation.statusText);
    activeTherionRunConfigPath_.clear();
    activeTherionRunWorkingDirectory_.clear();
    activeTherionRunStandardError_.clear();
    requestProjectOutputsRefresh();
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerError(const QString &errorText)
{
    const TherionStudio::TherionRunnerLifecyclePresenter::EventPresentation eventPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentError(errorText);
    setCompilerStatusResult(false, eventPresentation.statusText);
    activeTherionRunConfigPath_.clear();
    activeTherionRunWorkingDirectory_.clear();
    activeTherionRunStandardError_.clear();
    updateTherionRunnerState();
}

void MainWindow::handleTherionConsoleLinkActivated(const QUrl &url)
{
    const TherionStudio::TherionRunnerOutputLinker::SourceLocation location =
        TherionStudio::TherionRunnerOutputLinker::sourceLocationFromUrl(url);
    if (!location.isValid()) {
        return;
    }

    const QString filePath = location.path;
    if (QFileInfo(filePath).suffix().toLower() == QStringLiteral("th2")) {
        if (auto *mapTab = openMapEditorTab(filePath)) {
            mapTab->goToLine(location.lineNumber);
            showSidebarPane(SidebarPane::Console);
        }
    } else {
        if (auto *textTab = openTextTab(filePath)) {
            textTab->goToLine(location.lineNumber);
            showSidebarPane(SidebarPane::Console);
        }
    }
}

void MainWindow::handleTherionRunnerStateChanged(bool running)
{
    if (running && statusCompilerButton_ != nullptr) {
        setCompilerStatusRunning(activeTherionRunConfigPath_);
    }
    updateTherionRunnerState();
}
