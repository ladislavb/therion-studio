#include "../src/app/ProjectVersioningService.h"
#include "../src/app/ProjectVersioningSettings.h"
#include "../src/app/ProjectVersioningStore.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace TherionStudio;

class ProjectVersioningServiceTest final : public QObject
{
    Q_OBJECT
private slots:
    void timerUsesConfiguredIntervalAndOnlyChangesCreateHistory()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString data = temporary.filePath(QStringLiteral("data"));
        QVERIFY(QDir().mkpath(root));
        QFile file(QDir(root).filePath(QStringLiteral("survey.th")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("survey cave\n");
        file.close();
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ProjectVersioningSettings preferences(settings, data);
        ProjectVersioningService service(settings, data);
        QSignalSpy completed(&service, &ProjectVersioningService::checkpointFinished);
        service.configure(root);
        auto *timer = service.findChild<QTimer *>(QStringLiteral("automaticCheckpointTimer"));
        QVERIFY(timer);
        QVERIFY(!timer->isActive());
        service.checkNow();
        QCOMPARE(completed.size(), 0);
        QVERIFY(!QFileInfo::exists(data));
        QVERIFY(preferences.setEnabled(root, true).succeeded);
        QVERIFY(preferences.setAutomatic(root, true, 5).succeeded);
        service.configure(root);
        QVERIFY(timer->isActive());
        QCOMPARE(timer->interval(), 5 * 60 * 1000);
        QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection);
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 10000);
        const auto stored = preferences.projectSettings(root).settings;
        ProjectVersioningStore store(root, stored.historyDirectory);
        QCOMPARE(store.history().checkpoints.size(), 1);
        service.checkNow();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 2, 10000);
        QCOMPARE(store.history().checkpoints.size(), 1);
        QFile ignored(QDir(root).filePath(QStringLiteral("export.lox")));
        QVERIFY(ignored.open(QIODevice::WriteOnly));
        ignored.write("ignored output\n");
        ignored.close();
        service.checkNow();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 3, 10000);
        QCOMPARE(store.history().checkpoints.size(), 1);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
        file.write("# changed\n");
        file.close();
        service.checkNow();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 4, 10000);
        QCOMPARE(store.history().checkpoints.size(), 2);
        QVERIFY(preferences.setEnabled(root, false).succeeded);
        service.checkNow();
        QCOMPARE(completed.size(), 4);
        QVERIFY(!timer->isActive());
        QVERIFY(preferences.setEnabled(root, true).succeeded);
        service.configure(root);
        QVERIFY(timer->isActive());
        service.configure({});
        QVERIFY(!timer->isActive());
    }
};

QTEST_GUILESS_MAIN(ProjectVersioningServiceTest)
#include "ProjectVersioningServiceTest.moc"
