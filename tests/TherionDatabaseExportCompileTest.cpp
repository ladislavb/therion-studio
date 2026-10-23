#include "../src/app/TherionDatabaseExportCompile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionDatabaseExportCompileTest : public QObject
{
    Q_OBJECT

private slots:
    void appendsExportAfterConfigText();
    void endsUnterminatedLastLine();
    void writesTemporaryConfigNextToTheConfig();
    void removesTemporaryConfig();
    void reportsUnreadableConfig();
};

void TherionDatabaseExportCompileTest::appendsExportAfterConfigText()
{
    const QByteArray config("source cave.th\nselect MP@cave\n");

    QCOMPARE(therionConfigWithDatabaseExport(config, QStringLiteral("/tmp/run 1/database.sql")),
             QByteArray("source cave.th\nselect MP@cave\nexport database -fmt sql -o \"/tmp/run 1/database.sql\"\n"));
}

void TherionDatabaseExportCompileTest::endsUnterminatedLastLine()
{
    QCOMPARE(therionConfigWithDatabaseExport("source cave.th", QStringLiteral("db.sql")),
             QByteArray("source cave.th\nexport database -fmt sql -o \"db.sql\"\n"));
}

void TherionDatabaseExportCompileTest::writesTemporaryConfigNextToTheConfig()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString configPath = dir.filePath(QStringLiteral("thconfig"));
    QFile config(configPath);
    QVERIFY(config.open(QIODevice::WriteOnly));
    config.write("source cave.th\n");
    config.close();

    TherionDatabaseExportCompile compile;
    QString errorMessage;
    const QString temporaryConfigPath = compile.prepare(configPath, &errorMessage);

    QVERIFY2(!temporaryConfigPath.isEmpty(), qPrintable(errorMessage));
    QCOMPARE(QFileInfo(temporaryConfigPath).absolutePath(), QFileInfo(configPath).absolutePath());
    QVERIFY(QFileInfo(temporaryConfigPath).fileName().startsWith(QLatin1Char('.')));
    QFile temporaryConfig(temporaryConfigPath);
    QVERIFY(temporaryConfig.open(QIODevice::ReadOnly));
    const QByteArray text = temporaryConfig.readAll();
    QVERIFY(text.startsWith("source cave.th\nexport database -fmt sql -o \""));
    QVERIFY(!text.contains(QDir::fromNativeSeparators(dir.path()).toUtf8()));
}

void TherionDatabaseExportCompileTest::removesTemporaryConfig()
{
    QTemporaryDir dir;
    const QString configPath = dir.filePath(QStringLiteral("thconfig"));
    QFile config(configPath);
    QVERIFY(config.open(QIODevice::WriteOnly));
    config.close();

    TherionDatabaseExportCompile compile;
    QString errorMessage;
    const QString temporaryConfigPath = compile.prepare(configPath, &errorMessage);
    QVERIFY(QFileInfo::exists(temporaryConfigPath));
    compile.removeTemporaryConfig();

    QVERIFY(!QFileInfo::exists(temporaryConfigPath));
    QVERIFY(QFileInfo::exists(configPath));
}

void TherionDatabaseExportCompileTest::reportsUnreadableConfig()
{
    QTemporaryDir dir;
    TherionDatabaseExportCompile compile;
    QString errorMessage;

    QVERIFY(compile.prepare(dir.filePath(QStringLiteral("missing.thconfig")), &errorMessage).isEmpty());
    QVERIFY(!errorMessage.isEmpty());
}
}

int runTherionDatabaseExportCompileTest(int argc, char **argv)
{
    TherionDatabaseExportCompileTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionDatabaseExportCompileTest.moc"
