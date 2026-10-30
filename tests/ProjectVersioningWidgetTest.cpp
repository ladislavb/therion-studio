#include "../src/app/ProjectVersioningWidget.h"
#include "../src/app/ProjectVersioningSettings.h"
#include "../src/app/GitHubRepositoryWidget.h"
#include "../src/app/ProjectGitHubSettings.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

using namespace TherionStudio;

class ProjectVersioningWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void savedGitHubSettingsAutomaticallyVerifyAccountAndRepository()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake GitHub CLI fixture");
#endif
        QTemporaryDir temporary;
        QFile config(temporary.filePath(QStringLiteral(".therion-studio-github.json")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("{\"version\":1,\"owner\":\"owner\",\"repository\":\"backup\",\"uploadEnabled\":true}");
        config.close();
        QFile gh(temporary.filePath(QStringLiteral("fake-gh")));
        QVERIFY(gh.open(QIODevice::WriteOnly));
        gh.write("#!/bin/sh\n"
                 "if [ \"$1 $2\" = 'auth status' ]; then echo 'github.com (keyring)'; exit 0; fi\n"
                 "if [ \"$1 $2\" = 'api repos/owner/backup' ]; then "
                 "echo '{\"private\":true,\"visibility\":\"private\",\"full_name\":\"owner/backup\"}'; exit 0; fi\n"
                 "exit 99\n");
        gh.close();
        QVERIFY(gh.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        GitHubRepositoryWidget widget(temporary.path(), nullptr, gh.fileName());
        widget.show();
        auto *status = widget.findChild<QLabel *>(QStringLiteral("githubBackupStatus"));
        QTRY_VERIFY(status->text().startsWith(QStringLiteral("Private repository verified:")));
        QVERIFY(widget.findChild<QPushButton *>(QStringLiteral("githubCreateRepository"))->isEnabled());
        QVERIFY(ProjectGitHubSettings::read(temporary.path()).uploadEnabled());
    }

    void githubUploadAndExitPromptAreExplicitAndPersisted()
    {
        QTemporaryDir temporary;
        GitHubRepositoryWidget widget(temporary.path());
        auto *enabled = widget.findChild<QCheckBox *>(QStringLiteral("githubUploadEnabled"));
        auto *prompt = widget.findChild<QCheckBox *>(QStringLiteral("githubPromptOnClose"));
        QVERIFY(enabled && prompt);
        QVERIFY(!enabled->isChecked());
        QVERIFY(!prompt->isEnabled());
        widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->setText(QStringLiteral("owner"));
        widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"))->setText(QStringLiteral("backup"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                box->button(QMessageBox::Cancel)->click();
            }
        });
        enabled->click();
        QVERIFY(!enabled->isChecked());
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                box->button(QMessageBox::Yes)->click();
            }
        });
        enabled->click();
        QVERIFY(enabled->isChecked());
        QVERIFY(prompt->isEnabled());
        prompt->click();
        GitHubRepositoryWidget reopened(temporary.path());
        QVERIFY(reopened.findChild<QCheckBox *>(QStringLiteral("githubUploadEnabled"))->isChecked());
        QVERIFY(reopened.findChild<QCheckBox *>(QStringLiteral("githubPromptOnClose"))->isChecked());
        const auto preferences = ProjectGitHubSettings::read(temporary.path());
        QVERIFY(preferences.succeeded);
        QVERIFY(preferences.uploadEnabled());
        QVERIFY(preferences.promptOnClose());
        QCOMPARE(preferences.repository(), QStringLiteral("owner/backup"));
    }

    void githubProjectSettingsRejectUnexpectedKeysAndInvalidFlags()
    {
        QTemporaryDir temporary;
        QFile config(temporary.filePath(QStringLiteral(".therion-studio-github.json")));
        for (const QByteArray &invalid : {
                 QByteArray("{\"version\":1,\"owner\":\"a\",\"repository\":\"b\",\"token\":\"forbidden\"}"),
                 QByteArray("{\"version\":1,\"owner\":\"a\",\"repository\":\"b\",\"uploadEnabled\":\"true\"}"),
                 QByteArray("{\"version\":2,\"owner\":\"a\",\"repository\":\"b\"}")}) {
            QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(config.write(invalid), invalid.size());
            config.close();
            const auto settings = ProjectGitHubSettings::read(temporary.path());
            QVERIFY(!settings.succeeded);
            QVERIFY(!settings.error.isEmpty());
            QVERIFY(!settings.uploadEnabled());
            QVERIFY(!settings.promptOnClose());
        }
        QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
        config.write("{\"version\":1,\"owner\":\"a\",\"repository\":\"../unsafe\","
                     "\"uploadEnabled\":true,\"promptOnClose\":true}");
        config.close();
        const auto invalidTarget = ProjectGitHubSettings::read(temporary.path());
        QVERIFY(invalidTarget.succeeded);
        QVERIFY(!invalidTarget.uploadEnabled());
        QVERIFY(!invalidTarget.promptOnClose());
    }

    void githubSettingsSurviveClosingSettingsAndAreProjectSpecific()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString otherRoot = temporary.filePath(QStringLiteral("other"));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(QDir().mkpath(otherRoot));
        const QString configuration = QDir(root).filePath(QStringLiteral(".therion-studio-github.json"));
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        {
            ProjectVersioningWidget widget(root, settings, temporary.filePath(QStringLiteral("data")));
            auto *owner = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"));
            auto *name = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"));
            QVERIFY(owner && name);
            QVERIFY(!QFileInfo::exists(configuration));
            owner->setText(QStringLiteral("dankez"));
            name->setText(QStringLiteral("private-survey-backup"));
            QVERIFY(QMetaObject::invokeMethod(name, "editingFinished", Qt::DirectConnection));
        }
        ProjectVersioningWidget reopened(root, settings, temporary.filePath(QStringLiteral("data")));
        QCOMPARE(reopened.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->text(),
                 QStringLiteral("dankez"));
        QCOMPARE(reopened.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"))->text(),
                 QStringLiteral("private-survey-backup"));
        ProjectVersioningWidget other(otherRoot, settings, temporary.filePath(QStringLiteral("data")));
        QVERIFY(other.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->text().isEmpty());
        QVERIFY(!QFileInfo::exists(QDir(root).filePath(QStringLiteral(".git"))));
        QVERIFY(settings.allKeys().isEmpty());
        QFile saved(configuration);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QJsonObject object = QJsonDocument::fromJson(saved.readAll()).object();
        QCOMPARE(object.size(), 3);
        QCOMPARE(object.value(QStringLiteral("version")).toInt(), 1);
        QCOMPARE(object.value(QStringLiteral("owner")).toString(), QStringLiteral("dankez"));
        QCOMPARE(object.value(QStringLiteral("repository")).toString(), QStringLiteral("private-survey-backup"));
        QVERIFY(!reopened.findChild<QPushButton *>(QStringLiteral("githubCreateRepository"))->isEnabled());
    }

    void githubSettingsRejectInvalidFilesLinksAndConcurrentChanges()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral(".therion-studio-github.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("invalid json");
        file.close();
        QTest::ignoreMessage(QtWarningMsg, "GitHub project settings failed: Invalid or unsupported GitHub project settings. The file was not changed.");
        {
            GitHubRepositoryWidget invalid(temporary.path());
            QVERIFY(!invalid.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->isEnabled());
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("invalid json"));
        file.close();
        QVERIFY(file.remove());
        {
            GitHubRepositoryWidget widget(temporary.path());
            auto *owner = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"));
            QVERIFY(owner->isEnabled());
            owner->setText(QStringLiteral("local-owner"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("{\"version\":1,\"owner\":\"external\",\"repository\":\"backup\"}");
            file.close();
            QTest::ignoreMessage(QtWarningMsg, "GitHub project settings failed: GitHub project settings changed on disk. Close and reopen Settings before editing.");
            QVERIFY(QMetaObject::invokeMethod(owner, "editingFinished", Qt::DirectConnection));
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{\"version\":1,\"owner\":\"external\",\"repository\":\"backup\"}"));
        file.close();
#ifndef Q_OS_WIN
        QVERIFY(file.remove());
        QFile target(temporary.filePath(QStringLiteral("target.json")));
        QVERIFY(target.open(QIODevice::WriteOnly));
        target.write("untouched");
        target.close();
        QVERIFY(QFile::link(target.fileName(), path));
        QTest::ignoreMessage(QtWarningMsg, "GitHub project settings failed: GitHub project settings must be a regular file, not a symbolic link.");
        GitHubRepositoryWidget linked(temporary.path());
        QVERIFY(!linked.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->isEnabled());
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.readAll(), QByteArray("untouched"));
#endif
    }

    void githubDraftIsSavedOnHideAndAfterTypingStops()
    {
        QTemporaryDir temporary;
        const QString path = temporary.filePath(QStringLiteral(".therion-studio-github.json"));
        {
            GitHubRepositoryWidget widget(temporary.path());
            widget.show();
            auto *owner = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"));
            auto *name = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"));
            owner->setText(QStringLiteral("owner"));
            name->setText(QStringLiteral("first-repository"));
            widget.hide();
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("repository")).toString(),
                     QStringLiteral("first-repository"));
            file.close();
            name->setText(QStringLiteral("second-repository"));
            bool saved = false;
            QTRY_VERIFY([&] {
                QFile current(path);
                if (current.open(QIODevice::ReadOnly)) {
                    saved = QJsonDocument::fromJson(current.readAll()).object()
                        .value(QStringLiteral("repository")).toString() == QStringLiteral("second-repository");
                }
                return saved;
            }());
        }
        GitHubRepositoryWidget reopened(temporary.path());
        QCOMPARE(reopened.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"))->text(),
                 QStringLiteral("second-repository"));
    }

    void githubCreatesOnlyPrivateRepositoryAndVerifiesResult()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake GitHub CLI fixture");
#endif
        QTemporaryDir temporary;
        const QString program = temporary.filePath(QStringLiteral("fake-gh"));
        QFile executable(program);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write(
            "#!/bin/sh\n"
            "if [ \"$1 $2\" = 'auth status' ]; then echo 'github.com (keyring)'; exit 0; fi\n"
            "if [ \"$1 $2\" = 'repo create' ]; then\n"
            "  [ \"$3\" = 'test-owner/private-backup' ] && [ \"$4\" = '--private' ] && [ \"$#\" = 4 ]; exit $?; fi\n"
            "if [ \"$1 $2\" = 'api repos/test-owner/private-backup' ]; then\n"
            " echo '{\"private\":true,\"visibility\":\"private\",\"full_name\":\"test-owner/private-backup\"}'; exit 0; fi\n"
            "exit 1\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        GitHubRepositoryWidget widget(temporary.path(), nullptr, program);
        auto *check = widget.findChild<QPushButton *>(QStringLiteral("githubCheckAccount"));
        auto *create = widget.findChild<QPushButton *>(QStringLiteral("githubCreateRepository"));
        auto *owner = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"));
        auto *name = widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"));
        auto *status = widget.findChild<QLabel *>(QStringLiteral("githubBackupStatus"));
        QVERIFY(check && create && owner && name && status);
        QVERIFY(!create->isEnabled());
        check->click();
        QTRY_VERIFY(create->isEnabled());
        owner->setText(QStringLiteral("test-owner"));
        name->setText(QStringLiteral("private-backup"));
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box) {
                box->button(QMessageBox::Yes)->click();
            }
        });
        create->click();
        QTRY_VERIFY(status->text().startsWith(QStringLiteral("Private repository verified:")));
        QVERIFY(status->text().contains(QStringLiteral("No project data was uploaded.")));
    }

    void githubRejectsFileCredentialsAndPublicRepository()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake GitHub CLI fixture");
#endif
        QTemporaryDir temporary;
        const QString program = temporary.filePath(QStringLiteral("fake-gh"));
        QFile executable(program);
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("#!/bin/sh\nif [ \"$1\" = auth ]; then echo 'github.com (file)'; else "
                         "echo '{\"private\":false,\"visibility\":\"public\",\"full_name\":\"a/b\"}'; fi\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        GitHubRepositoryWidget widget(temporary.path(), nullptr, program);
        auto *check = widget.findChild<QPushButton *>(QStringLiteral("githubCheckAccount"));
        auto *create = widget.findChild<QPushButton *>(QStringLiteral("githubCreateRepository"));
        auto *status = widget.findChild<QLabel *>(QStringLiteral("githubBackupStatus"));
        QTest::ignoreMessage(QtWarningMsg, "GitHub repository operation failed: GitHub CLI must use the system keyring. File-based or environment tokens are not accepted. Configure a system credential store and sign in again.");
        check->click();
        QTRY_VERIFY(status->text().startsWith(QStringLiteral("GitHub CLI must use")));
        QVERIFY(!create->isEnabled());
        QVERIFY(executable.open(QIODevice::WriteOnly | QIODevice::Truncate));
        executable.write("#!/bin/sh\nif [ \"$1\" = auth ]; then echo 'github.com (keyring)'; else "
                         "echo '{\"private\":false,\"visibility\":\"public\",\"full_name\":\"a/b\"}'; fi\n");
        executable.close();
        check->click();
        QTRY_VERIFY(create->isEnabled());
        widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryOwner"))->setText(QStringLiteral("a"));
        widget.findChild<QLineEdit *>(QStringLiteral("githubRepositoryName"))->setText(QStringLiteral("b"));
        QTest::ignoreMessage(QtWarningMsg, "GitHub repository operation failed: GitHub did not confirm a private repository with the requested owner and name. Nothing is linked or uploaded.");
        widget.findChild<QPushButton *>(QStringLiteral("githubVerifyRepository"))->click();
        QTRY_VERIFY(status->text().startsWith(QStringLiteral("GitHub did not confirm")));
        GitHubRepositoryWidget noProject({}, nullptr, program);
        QVERIFY(!noProject.findChild<QPushButton *>(QStringLiteral("githubCheckAccount"))->isEnabled());
    }

    void ignoreRulesCanBeLoadedSavedAndRejectConcurrentEdits()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(root));
        const QString path = QDir(root).filePath(QStringLiteral(".gitignore"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# original\nprivate/\n");
        file.close();
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ProjectVersioningWidget widget(root, settings, temporary.filePath(QStringLiteral("data")));
        auto *editor = widget.findChild<QPlainTextEdit *>(QStringLiteral("projectGitignore"));
        auto *save = widget.findChild<QPushButton *>(QStringLiteral("saveProjectGitignore"));
        auto *reload = widget.findChild<QPushButton *>(QStringLiteral("reloadProjectGitignore"));
        QVERIFY(editor && save && reload);
        QCOMPARE(editor->toPlainText(), QStringLiteral("# original\nprivate/\n"));
        editor->setPlainText(QStringLiteral("private/\n*.pdf\n"));
        save->click();
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("private/\n*.pdf\n"));
        file.close();
        editor->setPlainText(QStringLiteral("mine/\n"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("external/\n");
        file.close();
        QTest::ignoreMessage(QtWarningMsg, "Local project history operation failed: The project .gitignore changed on disk. Reload it before saving.");
        save->click();
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("external/\n"));
        file.close();
        reload->click();
        QCOMPARE(editor->toPlainText(), QStringLiteral("external/\n"));
        QVERIFY(settings.allKeys().isEmpty());
    }

    void noProjectExplainsHowToStart()
    {
        QTemporaryDir temporary;
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ProjectVersioningWidget widget({}, settings, temporary.filePath(QStringLiteral("data")));
        auto *enabled = widget.findChild<QCheckBox *>(QStringLiteral("localHistoryEnabled"));
        QVERIFY(enabled);
        QVERIFY(!enabled->isEnabled());
        auto *status = widget.findChild<QLabel *>(QStringLiteral("localHistoryStatus"));
        QVERIFY(status);
        QVERIFY(!status->text().isEmpty());
        QVERIFY(settings.allKeys().isEmpty());
    }

    void intervalAndConfirmedDeletionAreUsable()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString data = temporary.filePath(QStringLiteral("data"));
        QVERIFY(QDir().mkpath(root));
        QFile source(QDir(root).filePath(QStringLiteral("survey.th")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("survey cave\n");
        source.close();
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ProjectVersioningSettings preferences(settings, data);
        const auto initial = preferences.setEnabled(root, true);
        QVERIFY(initial.succeeded);
        ProjectVersioningStore store(root, initial.settings.historyDirectory);
        QVERIFY(store.createCheckpoint(QStringLiteral("Manual checkpoint")).succeeded);
        ProjectVersioningWidget widget(root, settings, data);
        widget.show();
        auto *automatic = widget.findChild<QCheckBox *>(QStringLiteral("automaticHistoryEnabled"));
        auto *interval = widget.findChild<QSpinBox *>(QStringLiteral("automaticHistoryMinutes"));
        auto *remove = widget.findChild<QPushButton *>(QStringLiteral("removeLocalHistory"));
        auto *history = widget.findChild<QTableWidget *>(QStringLiteral("localCheckpointHistory"));
        QVERIFY(automatic && interval && remove && history);
        QTRY_VERIFY_WITH_TIMEOUT(automatic->isEnabled(), 10000);
        QCOMPARE(interval->value(), 5);
        automatic->click();
        interval->setValue(1);
        QVERIFY(preferences.projectSettings(root).settings.automaticEnabled);
        QCOMPARE(preferences.projectSettings(root).settings.intervalMinutes, 1);
        QTimer::singleShot(0, [] {
            auto *confirmation = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (confirmation) {
                confirmation->button(QMessageBox::Cancel)->click();
            }
        });
        remove->click();
        QCOMPARE(history->rowCount(), 1);
        QVERIFY(preferences.projectSettings(root).settings.enabled);
        QTimer::singleShot(0, [] {
            auto *confirmation = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (confirmation) {
                confirmation->button(QMessageBox::Yes)->click();
            }
        });
        remove->click();
        QTRY_COMPARE_WITH_TIMEOUT(history->rowCount(), 0, 10000);
        QVERIFY(!preferences.projectSettings(root).settings.enabled);
        QVERIFY(!QFileInfo::exists(initial.settings.historyDirectory));
        QVERIFY(QFileInfo::exists(source.fileName()));
    }

    void enablingCreatesCheckpointAndHistorySurvivesReopen()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString data = temporary.filePath(QStringLiteral("data"));
        QVERIFY(QDir().mkpath(root));
        QFile source(QDir(root).filePath(QStringLiteral("survey.th")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("survey cave\nendsurvey\n");
        source.close();
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        {
            ProjectVersioningWidget widget(root, settings, data);
            widget.show();
            auto *enabled = widget.findChild<QCheckBox *>(QStringLiteral("localHistoryEnabled"));
            auto *checkpoint = widget.findChild<QPushButton *>(QStringLiteral("createLocalCheckpoint"));
            auto *history = widget.findChild<QTableWidget *>(QStringLiteral("localCheckpointHistory"));
            QVERIFY(enabled && checkpoint && history);
            QVERIFY(!enabled->isChecked());
            QVERIFY(!checkpoint->isEnabled());
            QVERIFY(!QFileInfo::exists(data));
            QTimer::singleShot(0, [] {
                auto *confirmation = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (confirmation) {
                    confirmation->button(QMessageBox::Yes)->click();
                }
            });
            enabled->click();
            QTRY_VERIFY_WITH_TIMEOUT(checkpoint->isEnabled(), 10000);
            QTRY_COMPARE_WITH_TIMEOUT(history->rowCount(), 1, 10000);
            ProjectVersioningSettings preferences(settings, data);
            QVERIFY(preferences.projectSettings(root).settings.enabled);
            QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Append));
            source.write("# saved edit\n");
            source.close();
            checkpoint->click();
            QTRY_COMPARE_WITH_TIMEOUT(history->rowCount(), 2, 10000);
            QTRY_VERIFY_WITH_TIMEOUT(enabled->isEnabled(), 10000);
            enabled->click();
            QVERIFY(!preferences.projectSettings(root).settings.enabled);
            QCOMPARE(history->rowCount(), 2);
        }
        ProjectVersioningWidget reopened(root, settings, data);
        auto *history = reopened.findChild<QTableWidget *>(QStringLiteral("localCheckpointHistory"));
        auto *enabled = reopened.findChild<QCheckBox *>(QStringLiteral("localHistoryEnabled"));
        QVERIFY(history && enabled);
        QTRY_COMPARE_WITH_TIMEOUT(history->rowCount(), 2, 10000);
        QVERIFY(!enabled->isChecked());
    }

    void cancelingOptInLeavesProjectDisabled()
    {
        QTemporaryDir temporary;
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString data = temporary.filePath(QStringLiteral("data"));
        QVERIFY(QDir().mkpath(root));
        QSettings settings(temporary.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        ProjectVersioningWidget widget(root, settings, data);
        widget.show();
        auto *enabled = widget.findChild<QCheckBox *>(QStringLiteral("localHistoryEnabled"));
        QVERIFY(enabled);
        QTimer::singleShot(0, [] {
            auto *confirmation = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (confirmation) {
                confirmation->button(QMessageBox::Cancel)->click();
            }
        });
        enabled->click();
        QVERIFY(!enabled->isChecked());
        QVERIFY(settings.allKeys().isEmpty());
        QVERIFY(!QFileInfo::exists(data));
    }
};

QTEST_MAIN(ProjectVersioningWidgetTest)
#include "ProjectVersioningWidgetTest.moc"
