#include "../src/app/TherionRunnerConfigResolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionRunnerConfigResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesWorkingDirectory();
    void detectsExplicitConfigArguments();
    void resolvesConfigPath();
    void resolvesThconfigWork();
};

bool writeTextFile(const QString &filePath, const QString &contents = QString())
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

void TherionRunnerConfigResolverTest::resolvesWorkingDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const QDir root(tempDir.path());
    QVERIFY2(root.mkpath(QStringLiteral("project")) && root.mkpath(QStringLiteral("typed")),
             "Temporary working directories could not be created.");

    const QString projectPath = root.filePath(QStringLiteral("project"));
    const QString typedPath = root.filePath(QStringLiteral("typed"));
    QCOMPARE(TherionRunnerConfigResolver::resolveWorkingDirectory(QString(), projectPath),
             QDir(projectPath).absolutePath());
    QCOMPARE(TherionRunnerConfigResolver::resolveWorkingDirectory(typedPath, projectPath),
             QDir(typedPath).absolutePath());
}

void TherionRunnerConfigResolverTest::detectsExplicitConfigArguments()
{
    QVERIFY(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-c"),
                                                                    QStringLiteral("custom.thconfig")}));
    QVERIFY(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("--config=custom.thconfig")}));
    QVERIFY(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-c=custom.thconfig")}));
    QVERIFY(!TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-q"),
                                                                     QStringLiteral("survey.th")}));
}

void TherionRunnerConfigResolverTest::resolvesConfigPath()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    QDir root(tempDir.path());
    QVERIFY2(root.mkpath(QStringLiteral("project")) && root.mkpath(QStringLiteral("work")),
             "Temporary config directories could not be created.");

    const QString projectPath = root.filePath(QStringLiteral("project"));
    const QString workingDirectory = root.filePath(QStringLiteral("work"));
    const QString explicitConfigPath = QDir(workingDirectory).filePath(QStringLiteral("custom.thconfig"));
    const QString activeConfigPath = QDir(projectPath).filePath(QStringLiteral("active.thconfig"));
    const QString defaultConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig"));

    QVERIFY2(writeTextFile(explicitConfigPath, QStringLiteral("source custom.th\n")),
             "Explicit config fixture could not be written.");
    QVERIFY2(writeTextFile(activeConfigPath, QStringLiteral("source active.th\n")),
             "Active config fixture could not be written.");
    QVERIFY2(writeTextFile(defaultConfigPath, QStringLiteral("source default.th\n")),
             "Default config fixture could not be written.");

    const QString resolvedExplicitConfig =
        TherionRunnerConfigResolver::resolveConfigPath({QStringLiteral("--config=custom.thconfig")},
                                                       workingDirectory,
                                                       projectPath,
                                                       activeConfigPath);
    QCOMPARE(canonicalOrAbsolutePath(resolvedExplicitConfig), canonicalOrAbsolutePath(explicitConfigPath));

    const QString resolvedActiveConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       projectPath,
                                                       activeConfigPath);
    QCOMPARE(canonicalOrAbsolutePath(resolvedActiveConfig), canonicalOrAbsolutePath(activeConfigPath));

    const QString resolvedDefaultConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       projectPath,
                                                       QDir(projectPath).filePath(QStringLiteral("survey.th")));
    QCOMPARE(canonicalOrAbsolutePath(resolvedDefaultConfig), canonicalOrAbsolutePath(defaultConfigPath));
}

void TherionRunnerConfigResolverTest::resolvesThconfigWork()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    QDir root(tempDir.path());
    QVERIFY2(root.mkpath(QStringLiteral("work")), "Temporary working directory could not be created.");

    const QString workingDirectory = root.filePath(QStringLiteral("work"));
    const QString namedConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig.work"));
    QVERIFY2(writeTextFile(namedConfigPath, QStringLiteral("source work.th\n")),
             "thconfig.work fixture could not be written.");

    const QString resolvedNamedConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       QString(),
                                                       QString());
    QCOMPARE(canonicalOrAbsolutePath(resolvedNamedConfig), canonicalOrAbsolutePath(namedConfigPath));

    const QString secondConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig.debug"));
    QVERIFY2(writeTextFile(secondConfigPath, QStringLiteral("source debug.th\n")),
             "Second thconfig.* fixture could not be written.");

    const QString ambiguousConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       QString(),
                                                       QString());
    QVERIFY(ambiguousConfig.isEmpty());

    const QString lowPriorityConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig.thconfig"));
    QVERIFY2(writeTextFile(lowPriorityConfigPath, QStringLiteral("source low.th\n")),
             "thconfig.thconfig fixture could not be written.");

    const QString indexConfigPath = QDir(workingDirectory).filePath(QStringLiteral("index.thconfig"));
    QVERIFY2(writeTextFile(indexConfigPath, QStringLiteral("source index.th\n")),
             "index.thconfig fixture could not be written.");

    const QString mainConfigPath = QDir(workingDirectory).filePath(QStringLiteral("main.thconfig"));
    QVERIFY2(writeTextFile(mainConfigPath, QStringLiteral("source main.th\n")),
             "main.thconfig fixture could not be written.");

    const QString preferredConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       QString(),
                                                       QString());
    QCOMPARE(canonicalOrAbsolutePath(preferredConfig), canonicalOrAbsolutePath(lowPriorityConfigPath));

    QDir projectNamedRoot(tempDir.path());
    QVERIFY2(projectNamedRoot.mkpath(QStringLiteral("named")), "Project-named config directory could not be created.");
    const QString projectNamedDirectory = projectNamedRoot.filePath(QStringLiteral("named"));
    const QString projectNamedConfigPath = QDir(projectNamedDirectory).filePath(QStringLiteral("named.thconfig"));
    const QString otherConfigPath = QDir(projectNamedDirectory).filePath(QStringLiteral("other.thconfig"));
    QVERIFY2(writeTextFile(projectNamedConfigPath, QStringLiteral("source named.th\n")),
             "Project-named config fixture could not be written.");
    QVERIFY2(writeTextFile(otherConfigPath, QStringLiteral("source other.th\n")),
             "Other config fixture could not be written.");

    const QString resolvedProjectNamedConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       projectNamedDirectory,
                                                       QString(),
                                                       QString());
    QCOMPARE(canonicalOrAbsolutePath(resolvedProjectNamedConfig), canonicalOrAbsolutePath(projectNamedConfigPath));
}
}

int runTherionRunnerConfigResolverTest(int argc, char **argv)
{
    TherionRunnerConfigResolverTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionRunnerConfigResolverTest.moc"
