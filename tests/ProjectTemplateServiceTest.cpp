#include "../src/app/ProjectTemplateService.h"

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

int runCreateProjectFromTemplateTest()
{
    QTemporaryDir sourceDir;
    QTemporaryDir targetParentDir;
    if (!expect(sourceDir.isValid() && targetParentDir.isValid(), "Temporary directories should be available.")) {
        return 1;
    }

    const QString templateRoot = sourceDir.path();
    const QByteArray manifest = R"({
  "schemaVersion": 1,
  "targetConfig": "thconfig",
  "openFiles": ["thconfig", "index.th", "surveys/survey1.th", "scraps/scrap1.th2"],
  "directories": ["output"],
  "files": ["thconfig", "index.th", "surveys/survey1.th", "scraps/scrap1.th2"]
})";
    if (!expect(writeFile(QDir(templateRoot).filePath(QStringLiteral("template.json")), manifest),
                "Template manifest should be writable.")) {
        return 1;
    }
    if (!expect(writeFile(QDir(templateRoot).filePath(QStringLiteral("thconfig")), "source index.th\n"),
                "Template thconfig should be writable.")) {
        return 1;
    }
    if (!expect(writeFile(QDir(templateRoot).filePath(QStringLiteral("index.th")), "encoding utf-8\n"),
                "Template index should be writable.")) {
        return 1;
    }
    if (!expect(writeFile(QDir(templateRoot).filePath(QStringLiteral("surveys/survey1.th")), "survey survey1\nendsurvey\n"),
                "Template survey should be writable.")) {
        return 1;
    }
    if (!expect(writeFile(QDir(templateRoot).filePath(QStringLiteral("scraps/scrap1.th2")), "scrap scrap1\nendscrap\n"),
                "Template scrap should be writable.")) {
        return 1;
    }

    const QString targetRoot = QDir(targetParentDir.path()).filePath(QStringLiteral("New Project"));
    const auto result = ProjectTemplateService::createProjectFromTemplate(templateRoot, targetRoot);
    if (!expect(result.success, "Template project creation should succeed.")) {
        std::cerr << result.errorMessage.toStdString() << '\n';
        return 1;
    }
    if (!expect(QFileInfo::exists(QDir(targetRoot).filePath(QStringLiteral("thconfig"))),
                "Created project should contain thconfig.")) {
        return 1;
    }
    if (!expect(QFileInfo::exists(QDir(targetRoot).filePath(QStringLiteral("surveys/survey1.th"))),
                "Created project should contain nested survey source.")) {
        return 1;
    }
    if (!expect(QFileInfo(QDir(targetRoot).filePath(QStringLiteral("output"))).isDir(),
                "Created project should contain declared output directory.")) {
        return 1;
    }
    if (!expect(result.targetConfigPath == QDir(targetRoot).filePath(QStringLiteral("thconfig")),
                "Created project should report target config path.")) {
        return 1;
    }
    if (!expect(result.openFilePaths.size() == 4,
                "Created project should report manifest open files.")) {
        return 1;
    }
    if (!expect(result.openFilePaths.at(0) == QDir(targetRoot).filePath(QStringLiteral("thconfig"))
                    && result.openFilePaths.at(1) == QDir(targetRoot).filePath(QStringLiteral("index.th"))
                    && result.openFilePaths.at(2) == QDir(targetRoot).filePath(QStringLiteral("surveys/survey1.th"))
                    && result.openFilePaths.at(3) == QDir(targetRoot).filePath(QStringLiteral("scraps/scrap1.th2")),
                "Created project should preserve manifest open-file order when the project folder contains spaces.")) {
        return 1;
    }

    const auto secondResult = ProjectTemplateService::createProjectFromTemplate(templateRoot, targetRoot);
    if (!expect(!secondResult.success,
                "Template project creation should reject non-empty target folders.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    return runCreateProjectFromTemplateTest();
}
