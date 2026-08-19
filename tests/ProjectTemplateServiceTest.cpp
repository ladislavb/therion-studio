#include "../src/app/ProjectTemplateService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeFile(const QString &filePath, const QByteArray &contents)
{
    const QFileInfo fileInfo(filePath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(contents) == contents.size();
}

class ProjectTemplateServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsProjectFromTemplate();
};

void ProjectTemplateServiceTest::createsProjectFromTemplate()
{
    QTemporaryDir sourceDir;
    QTemporaryDir targetParentDir;
    QVERIFY2(sourceDir.isValid() && targetParentDir.isValid(), "Temporary directories should be available.");

    const QString templateRoot = sourceDir.path();
    const QByteArray manifest = R"({
  "schemaVersion": 1,
  "targetConfig": "thconfig",
  "openFiles": ["thconfig", "index.th", "surveys/survey1.th", "scraps/scrap1.th2"],
  "directories": ["output"],
  "files": ["thconfig", "index.th", "surveys/survey1.th", "scraps/scrap1.th2"]
})";
    QVERIFY2(writeFile(QDir(templateRoot).filePath(QStringLiteral("template.json")), manifest),
             "Template manifest should be writable.");
    QVERIFY2(writeFile(QDir(templateRoot).filePath(QStringLiteral("thconfig")), "source index.th\n"),
             "Template thconfig should be writable.");
    QVERIFY2(writeFile(QDir(templateRoot).filePath(QStringLiteral("index.th")), "encoding utf-8\n"),
             "Template index should be writable.");
    QVERIFY2(writeFile(QDir(templateRoot).filePath(QStringLiteral("surveys/survey1.th")), "survey survey1\nendsurvey\n"),
             "Template survey should be writable.");
    QVERIFY2(writeFile(QDir(templateRoot).filePath(QStringLiteral("scraps/scrap1.th2")), "scrap scrap1\nendscrap\n"),
             "Template scrap should be writable.");

    const QString targetRoot = QDir(targetParentDir.path()).filePath(QStringLiteral("New Project"));
    const auto result = ProjectTemplateService::createProjectFromTemplate(templateRoot, targetRoot);
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY2(QFileInfo::exists(QDir(targetRoot).filePath(QStringLiteral("thconfig"))),
             "Created project should contain thconfig.");
    QVERIFY2(QFileInfo::exists(QDir(targetRoot).filePath(QStringLiteral("surveys/survey1.th"))),
             "Created project should contain nested survey source.");
    QVERIFY2(QFileInfo(QDir(targetRoot).filePath(QStringLiteral("output"))).isDir(),
             "Created project should contain declared output directory.");
    QCOMPARE(result.targetConfigPath, QDir(targetRoot).filePath(QStringLiteral("thconfig")));
    QCOMPARE(result.openFilePaths.size(), 4);
    QCOMPARE(result.openFilePaths,
             QStringList({QDir(targetRoot).filePath(QStringLiteral("thconfig")),
                          QDir(targetRoot).filePath(QStringLiteral("index.th")),
                          QDir(targetRoot).filePath(QStringLiteral("surveys/survey1.th")),
                          QDir(targetRoot).filePath(QStringLiteral("scraps/scrap1.th2"))}));

    const auto secondResult = ProjectTemplateService::createProjectFromTemplate(templateRoot, targetRoot);
    QVERIFY(!secondResult.success);
}
}

int runProjectTemplateServiceTest(int argc, char **argv)
{
    ProjectTemplateServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectTemplateServiceTest.moc"
