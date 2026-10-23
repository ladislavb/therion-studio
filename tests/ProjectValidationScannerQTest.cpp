#include "../src/app/ProjectValidationScanner.h"

#include "../src/core/TherionCommandSyntax.h"

#include <QDir>

#include <algorithm>
#include <QFile>
#include <QFileInfo>
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
    // A drawing no config reads is noted once and its station names are not checked (validation plan V4).
    void notesUnreadDrawingWithStationNamesOnce()
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

        const auto unreadNotes = [&result]() {
            return std::count_if(result.findings.cbegin(), result.findings.cend(), [](const auto &finding) {
                return finding.diagnostic.code == QStringLiteral("drawing-not-read")
                    && finding.diagnostic.severity == TherionSourceDiagnosticSeverity::Information;
            });
        };
        QCOMPARE(unreadNotes(), 1);

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

        const QSet<QString> expectedStationReferenceFindings;
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
        QCOMPARE(unreadNotes(), 1);

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
        QCOMPARE(unreadNotes(), 1);
    }

    // A .th file a config inputs is config text to Therion, so its layout is not in the wrong document type (V4b).
    void checksFilesAsTheTypeTherionReadsThemAs()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QDir projectDir(tempDir.path());
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")),
                              QStringLiteral("source cave.th\ninput common.th\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("common.th")),
                              QStringLiteral("layout common\n  scale 1 500\nendlayout\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("cave.th")),
                              QStringLiteral("survey cave\nendsurvey\nlayout wrong\nendlayout\n")));

        TherionSourceValidationCatalog catalog = validationCatalog();
        for (const QString &command : {QStringLiteral("input"), QStringLiteral("layout"), QStringLiteral("endlayout"),
                                       QStringLiteral("scale")}) {
            catalog.commandNames.insert(command);
        }
        catalog.commandContexts.insert(QStringLiteral("input"), {QStringLiteral("all")});
        catalog.commandContexts.insert(QStringLiteral("layout"), {QStringLiteral("none")});
        catalog.commandContexts.insert(QStringLiteral("endlayout"), {QStringLiteral("layout")});
        catalog.commandContexts.insert(QStringLiteral("scale"), {QStringLiteral("layout")});
        for (const QString &command : {QStringLiteral("layout"), QStringLiteral("endlayout"), QStringLiteral("scale")}) {
            catalog.commandDocumentTypes.insert(command, {QStringLiteral("thconfig")});
        }

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
        scanner.requestScan(projectDir.path(), catalog, {});
        QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
        QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

        QStringList documentTypeFindings;
        for (const ProjectValidationScanner::Finding &finding : result.findings) {
            if (finding.diagnostic.code == QStringLiteral("invalid-document-type")) {
                documentTypeFindings.append(QFileInfo(finding.filePath).fileName());
            }
        }
        // common.th is read as config; cave.th is survey source, where layout does not belong.
        QVERIFY2(!documentTypeFindings.contains(QStringLiteral("common.th")), qPrintable(documentTypeFindings.join(u' ')));
        QVERIFY(documentTypeFindings.contains(QStringLiteral("cave.th")));
        QCOMPARE(result.readAsTypes.size(), 1);
        QCOMPARE(QFileInfo(result.readAsTypes.keys().constFirst()).fileName(), QStringLiteral("common.th"));
    }

    // A drawing the config does not read gets one note instead of an error per station (validation plan V4).
    void notesUnreadDrawingOnceAndChecksReadDrawings()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QDir projectDir(tempDir.path());
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")), QStringLiteral("source root.th\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                              QStringLiteral("survey cave\n"
                                             "  centerline\n"
                                             "    data normal from to length compass clino\n"
                                             "    0 1 1 0 0\n"
                                             "  endcenterline\n"
                                             "  input read.th2\n"
                                             "endsurvey\n")));
        QVERIFY(writeTextFile(projectDir.filePath(QStringLiteral("read.th2")),
                              QStringLiteral("scrap read\n"
                                             "point 0 0 station -name 0\n"
                                             "point 1 1 station -name missing\n"
                                             "endscrap\n")));
        const QString unreadFile = projectDir.filePath(QStringLiteral("unread.th2"));
        QVERIFY(writeTextFile(unreadFile,
                              QStringLiteral("scrap unread\n"
                                             "point 0 0 station -name 0\n"
                                             "point 1 1 station -name 7\n"
                                             "point 2 2 station -name 8\n"
                                             "endscrap\n")));

        TherionSourceValidationCatalog catalog = validationCatalog();
        catalog.commandNames.insert(QStringLiteral("input"));
        catalog.commandContexts.insert(QStringLiteral("input"), {QStringLiteral("all")});
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
        scanner.requestScan(projectDir.path(), catalog, {});
        QTRY_VERIFY_WITH_TIMEOUT(received, 5000);
        QVERIFY2(result.errorMessage.isEmpty(), qPrintable(result.errorMessage));

        int unreadNotes = 0;
        QStringList stationFindings;
        for (const ProjectValidationScanner::Finding &finding : result.findings) {
            const bool unread = QFileInfo(finding.filePath).fileName() == QStringLiteral("unread.th2");
            if (finding.diagnostic.code == QStringLiteral("drawing-not-read")) {
                QVERIFY(unread);
                QCOMPARE(finding.diagnostic.severity, TherionSourceDiagnosticSeverity::Information);
                ++unreadNotes;
            } else if (finding.diagnostic.code == QStringLiteral("unknown-station-reference")) {
                QVERIFY2(!unread, qPrintable(finding.diagnostic.message));
                stationFindings.append(finding.diagnostic.currentText.trimmed());
            }
        }
        QCOMPARE(unreadNotes, 1);
        // The drawing the config reads keeps its station checks.
        QCOMPARE(stationFindings, QStringList({QStringLiteral("point 1 1 station -name missing")}));
    }
};

int runProjectValidationScannerQTest(int argc, char **argv)
{
    ProjectValidationScannerQTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectValidationScannerQTest.moc"
