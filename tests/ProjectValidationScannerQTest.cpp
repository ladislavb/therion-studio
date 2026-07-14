#include "../src/app/ProjectValidationScanner.h"

#include "../src/core/TherionCommandSyntax.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QByteArray encodedContents = contents.toUtf8();
    return file.write(encodedContents) == encodedContents.size();
}

TherionSourceValidationCatalog validationCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("source"),
        QStringLiteral("survey"),
        QStringLiteral("endsurvey"),
        QStringLiteral("centerline"),
        QStringLiteral("endcenterline"),
        QStringLiteral("data"),
        QStringLiteral("scrap"),
        QStringLiteral("endscrap"),
        QStringLiteral("point")};
    catalog.commandContexts.insert(QStringLiteral("source"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("survey"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("endsurvey"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("centerline"), {QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("endcenterline"), {QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("data"), {QStringLiteral("centerline")});
    catalog.commandContexts.insert(QStringLiteral("scrap"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("endscrap"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("point"), {QStringLiteral("scrap")});
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("survey"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("endsurvey"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("centerline"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("endcenterline"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("data"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("scrap"), {QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("endscrap"), {QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("point"), {QStringLiteral("th2")});
    catalog.commandOptionNames.insert(QStringLiteral("scrap"), {QStringLiteral("-station-names")});
    catalog.commandOptionNames.insert(QStringLiteral("point"), {QStringLiteral("-name")});
    catalog.commandOptionValueArityTokens.insert(
        commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-station-names")),
        QStringLiteral("N"));
    catalog.commandOptionValueArityTokens.insert(
        commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-name")),
        QStringLiteral("1"));
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("survey"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("scrap"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("point"), 3);
    return catalog;
}
}

class ProjectValidationScannerQTest final : public QObject
{
    Q_OBJECT

private slots:
    void suppressesUnindexedQualifiedStationNamesAfterScrapTransform()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir projectDir(tempDir.path());
        const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
        const QString configFile = projectDir.filePath(QStringLiteral("thconfig"));
        const QString looseMapFile = projectDir.filePath(QStringLiteral("loose.th2"));
        QVERIFY(writeTextFile(configFile, QStringLiteral("source root.th\n")));
        QVERIFY(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey 1302\n"
                                  "  survey hp\n"
                                  "    centerline\n"
                                  "      data normal from to length compass clino\n"
                                  "      0 1 1 0 0\n"
                                  "    endcenterline\n"
                                  "  endsurvey hp\n"
                                  "endsurvey 1302\n")));
        const QString mapContents = QStringLiteral(
            "scrap loose -station-names \"\" @hp\n"
            "point 0 0 station -name 0\n"
            "point 1 1 station -name 1@hp\n"
            "point 2 2 station -name 1@hpc\n"
            "endscrap\n"
            "scrap plain\n"
            "point 3 3 station -name missing\n"
            "endscrap\n");
        QVERIFY(writeTextFile(looseMapFile, mapContents));

        ProjectValidationScanner scanner;
        scanner.setDebounceIntervalMs(0);
        ProjectValidationScanner::Result result;
        bool received = false;
        QObject::connect(&scanner,
                         &ProjectValidationScanner::validationFinished,
                         &scanner,
                         [&](const ProjectValidationScanner::Result &validationResult) {
                             result = validationResult;
                             received = true;
                         });
        scanner.requestScan(projectDir.path(), validationCatalog(), {});

        QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
        QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

        QSet<QString> stationReferenceFindings;
        for (const ProjectValidationScanner::Finding &finding : result.findings) {
            if (finding.diagnostic.code != QStringLiteral("unknown-station-reference")
                && finding.diagnostic.code != QStringLiteral("ambiguous-station-reference")) {
                continue;
            }
            QVERIFY(finding.diagnostic.lineNumber > 0);
            QVERIFY(finding.diagnostic.columnNumber > 0);
            QVERIFY(finding.diagnostic.columnLength > 0);
            QVERIFY(finding.diagnostic.columnNumber - 1 + finding.diagnostic.columnLength
                    <= finding.diagnostic.currentText.size());
            stationReferenceFindings.insert(finding.diagnostic.currentText);
        }

        const QSet<QString> expectedStationReferenceFindings = {
            QStringLiteral("point 3 3 station -name missing")};
        QCOMPARE(stationReferenceFindings, expectedStationReferenceFindings);

        const QString hpcMapContents =
            QStringLiteral(
                "scrap loose -station-names \"\" @hpc\n"
                "point 0 0 station -name 0\n"
                "point 1 1 station -name 1@hp\n"
                "point 2 2 station -name 1@hpc\n"
                "endscrap\n"
                "scrap plain\n"
                "point 3 3 station -name missing\n"
                "endscrap\n");
        QHash<QString, QString> inMemoryContents;
        inMemoryContents.insert(looseMapFile, hpcMapContents);
        received = false;
        scanner.requestScan(projectDir.path(), validationCatalog(), inMemoryContents);
        QTRY_VERIFY_WITH_TIMEOUT(received, 5000);

        QSet<QString> hpcStationReferenceFindings;
        for (const ProjectValidationScanner::Finding &finding : result.findings) {
            if (finding.diagnostic.code == QStringLiteral("unknown-station-reference")
                || finding.diagnostic.code == QStringLiteral("ambiguous-station-reference")) {
                hpcStationReferenceFindings.insert(finding.diagnostic.currentText);
            }
        }
        QCOMPARE(hpcStationReferenceFindings, expectedStationReferenceFindings);

        inMemoryContents.insert(looseMapFile, mapContents);
        received = false;
        scanner.requestScan(projectDir.path(), validationCatalog(), inMemoryContents);
        QTRY_VERIFY_WITH_TIMEOUT(received, 5000);

        QSet<QString> restoredStationReferenceFindings;
        for (const ProjectValidationScanner::Finding &finding : result.findings) {
            if (finding.diagnostic.code == QStringLiteral("unknown-station-reference")
                || finding.diagnostic.code == QStringLiteral("ambiguous-station-reference")) {
                restoredStationReferenceFindings.insert(finding.diagnostic.currentText);
            }
        }
        QCOMPARE(restoredStationReferenceFindings, expectedStationReferenceFindings);
    }
};

int runProjectValidationScannerQTest(int argc, char **argv)
{
    ProjectValidationScannerQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectValidationScannerQTest.moc"
