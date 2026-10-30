#include "../src/app/ProjectVersioningNavigationService.h"
#include "../src/app/ProjectVersioningToolbar.h"

#include <QFile>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

using namespace TherionStudio;

class ProjectVersioningToolbarTest final : public QObject
{
    Q_OBJECT
private slots:
    void iconsLabelsUndoRedoAndDisabledState()
    {
        QTemporaryDir temporary;
        const QString root = temporary.path();
        QFile file(temporary.filePath(QStringLiteral("survey.th")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("old\n");
        file.close();
        QTemporaryDir history;
        ProjectVersioningStore store(root, history.path());
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("new\n");
        file.close();
        const auto second = store.createCheckpoint(QStringLiteral("Second"));
        QVERIFY(second.succeeded);

        ProjectVersioningNavigationService service;
        QWidget host;
        host.setObjectName(QStringLiteral("fileInspector"));
        ProjectVersioningToolbar toolbar(service, &host);
        toolbar.resize(toolbar.sizeHint());
        host.resize(toolbar.size());
        host.show();
        toolbar.show();
        auto *older = toolbar.findChild<QToolButton *>(QStringLiteral("olderProjectBackup"));
        auto *newer = toolbar.findChild<QToolButton *>(QStringLiteral("newerProjectBackup"));
        auto *choose = toolbar.findChild<QToolButton *>(QStringLiteral("chooseProjectBackup"));
        auto *settings = toolbar.findChild<QToolButton *>(QStringLiteral("projectBackupSettings"));
        auto *status = toolbar.findChild<QLabel *>(QStringLiteral("projectBackupStatus"));
        QVERIFY(older && newer && choose && settings && status);
        auto *github = toolbar.findChild<QToolButton *>(QStringLiteral("githubProjectBackup"));
        QVERIFY(github);
        QCOMPARE(github->iconSize(), QSize(16, 16));
        QVERIFY(!github->icon().isNull());
        QVERIFY(!github->accessibleName().isEmpty());
        QSignalSpy githubRequested(&toolbar, &ProjectVersioningToolbar::githubBackupRequested);
        github->click();
        QCOMPARE(githubRequested.size(), 1);
        QCOMPARE(status->text(), QStringLiteral("Local project history is disabled."));
        QSignalSpy settingsRequested(&toolbar, &ProjectVersioningToolbar::settingsRequested);
        settings->click();
        QCOMPARE(settingsRequested.size(), 1);
        QVERIFY(!older->isEnabled());
        QVERIFY(!newer->isEnabled());
        for (auto *button : {older, newer, choose, settings}) {
            QCOMPARE(button->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
            QVERIFY(!button->text().isEmpty());
            QVERIFY(!button->accessibleName().isEmpty());
            QVERIFY(!button->icon().isNull());
            QVERIFY(button->sizeHint().width() > 40);
            QCOMPARE(button->iconSize(), QSize(16, 16));
            QVERIFY(button->height() >= 28);
            QVERIFY(button->height() < 40);
        }
        QSignalSpy historyReady(&service, &ProjectVersioningNavigationService::historyReady);
        service.configure(root, history.path(), true);
        QTRY_VERIFY_WITH_TIMEOUT(!service.isBusy(), 10000);
        QVERIFY(older->isEnabled());
        QVERIFY(!newer->isEnabled());
        QVERIFY(status->text().contains(second.checkpointId.left(12)));
        QSignalSpy requested(&toolbar, &ProjectVersioningToolbar::restoreRequested);
        older->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toString(), first.checkpointId);

        QSignalSpy restored(&service, &ProjectVersioningNavigationService::restoreFinished);
        connect(&service, &ProjectVersioningNavigationService::restoreFinished, &toolbar,
                [&toolbar](const ProjectVersioningStore::RestoreResult &result) {
            if (result.succeeded) {
                toolbar.restored(result.checkpointId);
            }
        });
        service.restore(first.checkpointId);
        QTRY_COMPARE_WITH_TIMEOUT(restored.size(), 1, 10000);
        const auto restoredResult = qvariant_cast<ProjectVersioningStore::RestoreResult>(restored.first().first());
        QVERIFY2(restoredResult.succeeded, qPrintable(restoredResult.error));
        QTRY_VERIFY_WITH_TIMEOUT(!service.isBusy(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(newer->isEnabled(), 10000);
        QVERIFY(!older->isEnabled());
        QVERIFY(status->text().contains(first.checkpointId.left(12)));
        newer->click();
        QCOMPARE(requested.last().first().toString(), second.checkpointId);
        service.refresh(false);
        QTRY_VERIFY_WITH_TIMEOUT(!service.isBusy(), 10000);
        QVERIFY(newer->isEnabled());
        service.configure({}, {}, false);
        QVERIFY(!older->isEnabled());
        QVERIFY(!newer->isEnabled());
        QVERIFY(choose->isEnabled());
    }
};

QTEST_MAIN(ProjectVersioningToolbarTest)
#include "ProjectVersioningToolbarTest.moc"
