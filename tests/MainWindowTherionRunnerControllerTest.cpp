#include "../src/app/MainWindowTherionRunnerController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runCurrentDocumentConfigModeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temp directory setup failed for runner controller test.")) {
        return 1;
    }

    const QString configPath = QDir(tempDir.path()).absoluteFilePath(QStringLiteral("thconfig.work"));
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::WriteOnly)) {
        std::cerr << "Failed to create thconfig.work test file.\n";
        return 1;
    }
    configFile.write("# test\n");
    configFile.close();

    MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = tempDir.path();
    input.executableText = QString();
    input.workingDirectoryOverrideText = QString();
    input.argumentsText = QString();
    input.runTargetMode = QStringLiteral("project");
    input.targetConfigPathText = QString();
    input.currentDocumentPath = configPath;

    const auto state = MainWindowTherionRunnerController::computeRuntimeState(input);

    if (!expect(state.currentConfigAvailable,
                "Current config should be detected from active thconfig document.")) {
        return 1;
    }
    if (!expect(state.effectiveRunTargetMode == QStringLiteral("current"),
                "Run target mode should switch to current when a thconfig document is active.")) {
        return 1;
    }
    if (!expect(!state.projectConfigActive,
                "Project-config controls should be inactive in current-config mode.")) {
        return 1;
    }
    if (!expect(state.resolvedConfigPath == QFileInfo(configPath).canonicalFilePath(),
                "Resolved config should match active current-document config file.")) {
        return 1;
    }
    if (!expect(state.canRun,
                "Runner should be allowed when current config is resolved.")) {
        return 1;
    }
    if (!expect(state.runArguments == QStringList{state.resolvedConfigPath},
                "Run arguments should append resolved config when no explicit config argument is present.")) {
        return 1;
    }

    return 0;
}

int runProjectTargetConfigResolutionTest()
{
    QTemporaryDir projectDir;
    if (!expect(projectDir.isValid(), "Project temp directory setup failed.")) {
        return 1;
    }

    const QString workDirPath = QDir(projectDir.path()).absoluteFilePath(QStringLiteral("build"));
    QDir().mkpath(workDirPath);
    const QString targetPath = QDir(workDirPath).absoluteFilePath(QStringLiteral("survey.thconfig"));
    QFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        std::cerr << "Failed to create target thconfig file.\n";
        return 1;
    }
    targetFile.write("# target\n");
    targetFile.close();

    MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectDir.path();
    input.executableText = QStringLiteral("/opt/therion");
    input.workingDirectoryOverrideText = workDirPath;
    input.argumentsText = QStringLiteral("--foo bar");
    input.runTargetMode = QStringLiteral("project");
    input.targetConfigPathText = QStringLiteral("survey.thconfig");
    input.currentDocumentPath = QString();

    const auto state = MainWindowTherionRunnerController::computeRuntimeState(input);
    const QString canonicalTarget = QFileInfo(targetPath).canonicalFilePath();
    const QStringList expectedArguments = {
        QStringLiteral("--foo"),
        QStringLiteral("bar"),
        canonicalTarget};

    if (!expect(!state.currentConfigAvailable,
                "Current config should be unavailable when no active thconfig is set.")) {
        return 1;
    }
    if (!expect(state.effectiveRunTargetMode == QStringLiteral("project"),
                "Run target mode should stay project when no current config is available.")) {
        return 1;
    }
    if (!expect(state.projectConfigActive,
                "Project-config controls should remain active in project mode.")) {
        return 1;
    }
    if (!expect(state.hasWorkingDirectoryOverride,
                "Working-directory override should be tracked when provided.")) {
        return 1;
    }
    if (!expect(state.resolvedTargetConfigPath == canonicalTarget
                    && state.resolvedConfigPath == canonicalTarget,
                "Project target config should resolve via working-directory override.")) {
        return 1;
    }
    if (!expect(state.runArguments == expectedArguments,
                "Resolved project config should be appended to run arguments.")) {
        return 1;
    }
    if (!expect(state.canRun,
                "Runner should be allowed when project target config resolves.")) {
        return 1;
    }

    return 0;
}

int runExplicitConfigArgumentBehaviorTest()
{
    MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = QStringLiteral("/tmp/project");
    input.executableText = QString();
    input.workingDirectoryOverrideText = QString();
    input.argumentsText = QStringLiteral("--config missing.thconfig");
    input.runTargetMode = QStringLiteral("project");
    input.targetConfigPathText = QString();
    input.currentDocumentPath = QString();

    const auto state = MainWindowTherionRunnerController::computeRuntimeState(input);
    const QStringList expectedArguments = {
        QStringLiteral("--config"),
        QStringLiteral("missing.thconfig")};

    if (!expect(state.hasExplicitConfigArgument,
                "Explicit --config argument should be detected.")) {
        return 1;
    }
    if (!expect(state.runArguments == expectedArguments,
                "Run arguments should remain unchanged when explicit config argument is provided.")) {
        return 1;
    }
    if (!expect(state.canRun,
                "Runner should be allowed when explicit config argument is present.")) {
        return 1;
    }

    return 0;
}

int runMissingConfigValidationTest()
{
    QTemporaryDir projectDir;
    if (!expect(projectDir.isValid(), "Project temp directory setup failed.")) {
        return 1;
    }

    MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectDir.path();
    input.executableText = QString();
    input.workingDirectoryOverrideText = QString();
    input.argumentsText = QString();
    input.runTargetMode = QStringLiteral("project");
    input.targetConfigPathText = QString();
    input.currentDocumentPath = QString();

    const auto state = MainWindowTherionRunnerController::computeRuntimeState(input);
    if (!expect(!state.canRun,
                "Runner should require a resolved config path when no explicit config argument exists.")) {
        return 1;
    }
    if (!expect(state.resolvedConfigPath.isEmpty(),
                "Resolved config should be empty when no config source is available.")) {
        return 1;
    }

    return 0;
}

int runMissingProjectTargetFallsBackToDefaultConfigTest()
{
    QTemporaryDir projectDir;
    if (!expect(projectDir.isValid(), "Project temp directory setup failed.")) {
        return 1;
    }

    const QString defaultConfigPath = QDir(projectDir.path()).absoluteFilePath(QStringLiteral("thconfig"));
    QFile defaultConfigFile(defaultConfigPath);
    if (!defaultConfigFile.open(QIODevice::WriteOnly)) {
        std::cerr << "Failed to create default thconfig test file.\n";
        return 1;
    }
    defaultConfigFile.write("# default\n");
    defaultConfigFile.close();

    MainWindowTherionRunnerController::RuntimeInput input;
    input.projectRootPath = projectDir.path();
    input.executableText = QString();
    input.workingDirectoryOverrideText = QString();
    input.argumentsText = QString();
    input.runTargetMode = QStringLiteral("project");
    input.targetConfigPathText = QStringLiteral("removed.thconfig");
    input.currentDocumentPath = QString();

    const auto state = MainWindowTherionRunnerController::computeRuntimeState(input);
    const QString canonicalDefaultConfigPath = QFileInfo(defaultConfigPath).canonicalFilePath();

    if (!expect(state.resolvedTargetConfigPath.isEmpty(),
                "Missing project target config should not resolve as an explicit target.")) {
        return 1;
    }
    if (!expect(state.resolvedConfigPath == canonicalDefaultConfigPath,
                "Missing project target config should fall back to the default project config.")) {
        return 1;
    }
    if (!expect(state.runArguments == QStringList{canonicalDefaultConfigPath},
                "Run arguments should use the fallback default project config.")) {
        return 1;
    }
    if (!expect(state.canRun,
                "Runner should remain available when a stale target falls back to a default config.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runCurrentDocumentConfigModeTest() != 0) {
        return 1;
    }
    if (runProjectTargetConfigResolutionTest() != 0) {
        return 1;
    }
    if (runExplicitConfigArgumentBehaviorTest() != 0) {
        return 1;
    }
    if (runMissingConfigValidationTest() != 0) {
        return 1;
    }
    if (runMissingProjectTargetFallsBackToDefaultConfigTest() != 0) {
        return 1;
    }

    return 0;
}
