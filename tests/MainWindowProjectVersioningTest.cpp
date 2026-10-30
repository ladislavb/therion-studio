#include "../src/app/MainWindow.h"
#include "../src/app/ProjectVersioningSettings.h"
#include "../src/app/ProjectVersioningStore.h"
#include "../src/app/ProjectVersioningToolbar.h"
#include "../src/app/ProjectVersioningNavigationService.h"
#include "../src/app/MainWindowSettingsDialog.h"
#include "../src/app/ProjectVersioningWidget.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/app/text_editor/DocumentFileInspector.h"
#include "../src/app/text_editor/DocumentInspectorPanel.h"
#include "FakeSessionStore.h"

#include <QFile>
#include <QAbstractButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTabWidget>
#include <QTranslator>
#include <QtTest>

using namespace TherionStudio;

class MainWindowProjectVersioningTest final : public QObject
{
    Q_OBJECT
private slots:
    void configuredUploadShowsStatusWithoutFurtherDialogs()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake GitHub CLI fixture");
#endif
        QTemporaryDir temporary;
        const QString realGit = QStandardPaths::findExecutable(QStringLiteral("git"));
        const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        QVERIFY(!realGit.isEmpty() && !python.isEmpty());
        const auto write = [](const QString &path, const QByteArray &bytes, bool executable = false) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()
                && (!executable || file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        };
        const QString root = temporary.filePath(QStringLiteral("project"));
        const QString remote = temporary.filePath(QStringLiteral("remote.git"));
        QVERIFY(QDir().mkpath(root));
        QVERIFY(write(root + QStringLiteral("/survey.th"), "survey original\nendsurvey\n"));
        QVERIFY(write(root + QStringLiteral("/.gitignore"), "*.secret\n"));
        QVERIFY(write(root + QStringLiteral("/private.secret"), "must stay local"));
        QVERIFY(write(root + QStringLiteral("/.therion-studio-github.json"),
            "{\"version\":1,\"owner\":\"owner\",\"repository\":\"backup\",\"uploadEnabled\":true}"));
        QCOMPARE(QProcess::execute(realGit, {QStringLiteral("init"), QStringLiteral("--quiet"),
            QStringLiteral("--bare"), remote}), 0);
        const auto json = [](const QString &value) {
            return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
        };
        const QByteArray preamble = "#!" + python.toUtf8() + '\n';
        QVERIFY(write(temporary.filePath(QStringLiteral("gh")), preamble +
            "import json,sys\n"
            "a=sys.argv[1:]\n"
            "if a[:2]==['auth','status']: print('github.com (keyring)')\n"
            "elif a[:2]==['auth','token']: print('fixture-token')\n"
            "elif a==['api','--hostname','github.com','repos/owner/backup']:\n"
            " print(json.dumps(dict(id=123,private=True,visibility='private',full_name='owner/backup',permissions=dict(push=True))))\n"
            "else: sys.exit(99)\n", true));
        QVERIFY(write(temporary.filePath(QStringLiteral("git")), preamble +
            "import os,sys\n"
            "args=sys.argv[1:]\n"
            "url='https://github.com/owner/backup.git'\n"
            "assert all('https://' not in a or a==url or a=='http.https://github.com/.extraheader' for a in args)\n"
            "args=[" + json(remote) + " if a==url else a for a in args]\n"
            "os.environ['GIT_ALLOW_PROTOCOL']='file'\n"
            "os.execv(" + json(realGit) + ",[" + json(realGit) + "]+args)\n", true));
        const auto oldPath = qgetenv("PATH");
        const auto oldData = qgetenv("XDG_DATA_HOME");
        const bool dataWasSet = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        const auto restoreEnvironment = qScopeGuard([&] {
            qputenv("PATH", oldPath);
            if (dataWasSet) qputenv("XDG_DATA_HOME", oldData);
            else qunsetenv("XDG_DATA_HOME");
        });
        qputenv("PATH", temporary.path().toUtf8() + ':' + oldPath);
        qputenv("XDG_DATA_HOME", temporary.filePath(QStringLiteral("data")).toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        FakeSessionStore session;
        session.setLastProjectPath(root);
        session.setOpenDocumentPaths({root + QStringLiteral("/survey.th")});
        MainWindow window(session, CommandCatalogStore{});
        window.show();
        auto *editor = window.findChild<TextEditorTab *>();
        QVERIFY(editor);
        auto *raw = editor->findChild<QPlainTextEdit *>();
        QVERIFY(raw);
        raw->setPlainText(QStringLiteral("survey saved_by_upload\nendsurvey\n"));
        bool unexpectedDialog = false;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            auto *modal = QApplication::activeModalWidget();
            if (auto *dialog = qobject_cast<QDialog *>(modal)) {
                if (dialog->objectName() != QStringLiteral("githubBackupProgress")) {
                    unexpectedDialog = true;
                    dialog->reject();
                }
            }
        });
        dialogs.start(10);
        auto *button = window.findChild<QToolButton *>(QStringLiteral("workspaceGitHubBackupButton"));
        QVERIFY(button);
        button->click();
        QVERIFY(!unexpectedDialog);
        QVERIFY2(window.statusBar()->currentMessage().startsWith(QStringLiteral("GitHub OK - project uploaded")),
                 qPrintable(window.statusBar()->currentMessage()));
        QVERIFY(!editor->isDirty());
        QProcess inspect;
        inspect.start(realGit, {QStringLiteral("--git-dir=") + remote, QStringLiteral("show"),
                               QStringLiteral("main:survey.th")});
        QVERIFY(inspect.waitForFinished());
        QCOMPARE(inspect.readAllStandardOutput(), QByteArray("survey saved_by_upload\nendsurvey\n"));
        inspect.start(realGit, {QStringLiteral("--git-dir=") + remote, QStringLiteral("ls-tree"),
                               QStringLiteral("-r"), QStringLiteral("main")});
        QVERIFY(inspect.waitForFinished());
        QVERIFY(!inspect.readAllStandardOutput().contains("private.secret"));
        button->click();
        QVERIFY(!unexpectedDialog);
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("GitHub OK - project is up to date"));
        window.close();
    }

    void configuredUploadAutomaticallyChecksAccountWithoutCommitDialog()
    {
#ifdef Q_OS_WIN
        QSKIP("POSIX fake GitHub CLI fixture");
#endif
        QTemporaryDir temporary;
        const auto oldPath = qgetenv("PATH");
        const auto oldData = qgetenv("XDG_DATA_HOME");
        const bool dataWasSet = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        const auto restoreEnvironment = qScopeGuard([&] {
            qputenv("PATH", oldPath);
            if (dataWasSet) qputenv("XDG_DATA_HOME", oldData);
            else qunsetenv("XDG_DATA_HOME");
        });
        qputenv("XDG_DATA_HOME", temporary.filePath(QStringLiteral("data")).toUtf8());
        qputenv("PATH", temporary.path().toUtf8() + ':' + oldPath);
        QFile gh(temporary.filePath(QStringLiteral("gh")));
        QVERIFY(gh.open(QIODevice::WriteOnly));
        gh.write("#!/bin/sh\nprintf checked > \"$0.checked\"\nexit 1\n");
        gh.close();
        QVERIFY(gh.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QString root = temporary.filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(root));
        QFile config(QDir(root).filePath(QStringLiteral(".therion-studio-github.json")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("{\"version\":1,\"owner\":\"owner\",\"repository\":\"backup\",\"uploadEnabled\":true}");
        config.close();
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        FakeSessionStore session;
        session.setLastProjectPath(root);
        MainWindow window(session, CommandCatalogStore{});
        bool commitDialogShown = false;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            if (auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                commitDialogShown = true;
                input->reject();
            } else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                box->accept();
            } else if (auto *settings = qobject_cast<MainWindowSettingsDialog *>(QApplication::activeModalWidget())) {
                settings->reject();
            }
        });
        dialogs.start(10);
        QTest::ignoreMessage(QtWarningMsg,
            "github_backup stage=authentication status=failed GitHub CLI is missing or not signed in to github.com. Sign in using gh auth login.");
        window.findChild<QToolButton *>(QStringLiteral("workspaceGitHubBackupButton"))->click();
        QVERIFY(!commitDialogShown);
        QVERIFY(QFileInfo::exists(gh.fileName() + QStringLiteral(".checked")));
    }

    void topBarGitHubActionExplainsSetupAndOpensGitHubSettings()
    {
        QTemporaryDir temporary;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        FakeSessionStore session;
        session.setLastProjectPath(temporary.path());
        MainWindow window(session, CommandCatalogStore{});
        window.show();
        auto *bar = window.findChild<QWidget *>(QStringLiteral("workspaceCommandBar"));
        QVERIFY(bar);
        auto *github = bar->findChild<QToolButton *>(QStringLiteral("workspaceGitHubBackupButton"));
        QVERIFY(github);
        QVERIFY(github->isVisible());
        QVERIFY(github->isEnabled());
        QVERIFY(!github->icon().isNull());
        QVERIFY(!github->accessibleName().isEmpty());
        auto *save = bar->findChild<QToolButton *>(QStringLiteral("workspaceSaveButton"));
        QVERIFY(save);
        QCOMPARE(github->size(), save->size());
        QCOMPARE(github->iconSize(), save->iconSize());
        bool instructionsShown = false;
        bool githubSettingsShown = false;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            if (auto *info = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                instructionsShown = info->objectName() == QStringLiteral("githubBackupSetupRequired");
                info->accept();
            } else if (auto *dialog = qobject_cast<MainWindowSettingsDialog *>(QApplication::activeModalWidget())) {
                auto *tabs = dialog->findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
                auto *backups = qobject_cast<ProjectVersioningWidget *>(tabs->currentWidget());
                if (backups) {
                    auto *pages = backups->findChild<QTabWidget *>(QStringLiteral("backupSettingsPages"));
                    githubSettingsShown = pages && pages->currentWidget()->objectName()
                        == QStringLiteral("githubSettingsPage");
                }
                dialog->reject();
            }
        });
        dialogs.start(10);
        github->click();
        QVERIFY(instructionsShown);
        QVERIFY(githubSettingsShown);
        window.close();
    }

    void closeOffersBackupOnlyWhenEnabledAndCanBeCanceled()
    {
        QTemporaryDir temporary;
        const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
        const bool dataHomeWasSet = qEnvironmentVariableIsSet("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", temporary.filePath(QStringLiteral("app-data")).toUtf8());
        const auto restoreDataHome = qScopeGuard([&] {
            if (dataHomeWasSet) {
                qputenv("XDG_DATA_HOME", previousDataHome);
            } else {
                qunsetenv("XDG_DATA_HOME");
            }
        });
        QCoreApplication::setOrganizationName(QStringLiteral("TherionStudioRestoreTest"));
        QCoreApplication::setApplicationName(QStringLiteral("ProjectRestore"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        const QString root = temporary.filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(root));
        QFile source(QDir(root).filePath(QStringLiteral("survey.th")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("# project\n");
        source.close();
        QFile config(QDir(root).filePath(QStringLiteral(".therion-studio-github.json")));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("{\"version\":1,\"owner\":\"test-owner\",\"repository\":\"backup\","
                     "\"uploadEnabled\":true,\"promptOnClose\":true}");
        config.close();
        FakeSessionStore session;
        session.setLastProjectPath(root);
        session.setOpenDocumentPaths({source.fileName()});
        MainWindow window(session, CommandCatalogStore{});
        window.show();
        bool promptShown = false;
        bool skip = false;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box && box->objectName() == QStringLiteral("githubBackupBeforeClose")) {
                promptShown = true;
                if (skip) {
                    box->findChild<QPushButton *>(QStringLiteral("exitWithoutGitHubBackup"))->click();
                } else {
                    box->button(QMessageBox::Cancel)->click();
                }
            } else if (box) {
                box->reject();
            }
        });
        dialogs.start(10);
        QVERIFY(!window.close());
        QVERIFY(promptShown);
        QVERIFY(window.isVisible());
        skip = true;
        promptShown = false;
        QVERIFY(window.close());
        QVERIFY(promptShown);
    }

    void restoreSavesDirtyEditorReloadsItAndShowsComparison()
    {
        QTemporaryDir temporary;
        QCoreApplication::setOrganizationName(QStringLiteral("TherionStudioRestoreTest"));
        QCoreApplication::setApplicationName(QStringLiteral("ProjectRestore"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        const QString root = temporary.filePath(QStringLiteral("project"));
        QVERIFY(QDir().mkpath(root));
        const QString source = QDir(root).filePath(QStringLiteral("survey.th"));
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# old project\n");
        file.close();
        QSettings settings;
        ProjectVersioningSettings preferences(settings,
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        const auto enabled = preferences.setEnabled(root, true);
        QVERIFY(enabled.succeeded);
        ProjectVersioningStore store(root, enabled.settings.historyDirectory);
        const auto cleanup = qScopeGuard([&store] { store.removeHistory(); });
        const auto first = store.createCheckpoint(QStringLiteral("First"));
        QVERIFY(first.succeeded);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# second project\n");
        file.close();
        QVERIFY(store.createCheckpoint(QStringLiteral("Second")).succeeded);

        FakeSessionStore session;
        session.setLastProjectPath(root);
        session.setOpenDocumentPaths({source});
        MainWindow window(session, CommandCatalogStore{});
        window.resize(1280, 800);
        window.show();
        auto *panel = window.findChild<DocumentInspectorPanel *>(QStringLiteral("textContextHelpPanel"));
        QVERIFY(panel);
        auto *toolbar = panel->findChild<ProjectVersioningToolbar *>();
        auto *navigation = window.findChild<ProjectVersioningNavigationService *>();
        QVERIFY(toolbar && navigation);
        auto *topBar = window.findChild<QWidget *>(QStringLiteral("workspaceCommandBar"));
        QVERIFY(topBar);
        QVERIFY(topBar->findChild<ProjectVersioningToolbar *>() == nullptr);
        auto *inspector = panel->fileInspector();
        QVERIFY(inspector);
        QVERIFY(inspector->isAncestorOf(toolbar));
        auto *tabs = panel->tabs();
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->widget(i)->isAncestorOf(inspector)) {
                tabs->setCurrentIndex(i);
                break;
            }
        }
        QTRY_VERIFY(toolbar->isVisible());
        bool backupPageOpened = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<MainWindowSettingsDialog *>(QApplication::activeModalWidget());
            if (dialog) {
                auto *settingsTabs = dialog->findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
                backupPageOpened = settingsTabs && qobject_cast<ProjectVersioningWidget *>(
                    settingsTabs->currentWidget()) != nullptr;
                dialog->reject();
            }
        });
        toolbar->findChild<QToolButton *>(QStringLiteral("projectBackupSettings"))->click();
        QVERIFY2(backupPageOpened, "Backup settings shortcut must open Backups and versioning, not General");
        QTRY_VERIFY_WITH_TIMEOUT(!navigation->isBusy(), 10000);
        auto *editor = window.findChild<TextEditorTab *>();
        QVERIFY(editor);
        auto *raw = editor->findChild<QPlainTextEdit *>();
        QVERIFY(raw);
        QTextCursor cursor(raw->document());
        cursor.select(QTextCursor::Document);
        cursor.insertText(QStringLiteral("# unsaved project\n"));
        QVERIFY(editor->isDirty());
        auto *older = toolbar->findChild<QToolButton *>(QStringLiteral("olderProjectBackup"));
        QTRY_VERIFY_WITH_TIMEOUT(older->isEnabled(), 10000);
        bool successShown = false;
        bool allowRestore = false;
        QString summary;
        QString details;
        QString unexpectedError;
        QTimer dialogs;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            auto *message = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!message) {
                return;
            }
            if (message->objectName() == QStringLiteral("projectRestoreResult")) {
                successShown = true;
                summary = message->text();
                details = message->detailedText();
                message->button(QMessageBox::Ok)->click();
            } else if (message->standardButtons().testFlag(QMessageBox::Yes)) {
                message->button(allowRestore ? QMessageBox::Yes : QMessageBox::Cancel)->click();
            } else if (message->standardButtons().testFlag(QMessageBox::Ok)) {
                unexpectedError = message->text();
                message->button(QMessageBox::Ok)->click();
            }
        });
        dialogs.start(10);
        older->click();
        QVERIFY(editor->isDirty());
        QCOMPARE(store.history().checkpoints.size(), 2);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("# second project\n"));
        file.close();
        allowRestore = true;
        older->click();
        QTRY_VERIFY_WITH_TIMEOUT(successShown, 15000);
        QVERIFY2(unexpectedError.isEmpty(), qPrintable(unexpectedError));
        QCOMPARE(editor->text(), QStringLiteral("# old project\n"));
        QVERIFY(!editor->isDirty());
        QVERIFY(summary.contains(QStringLiteral("Changed: 1 file(s)")));
        QVERIFY(details.contains(QStringLiteral("survey.th")));
        QString safety;
        for (const auto &checkpoint : store.history().checkpoints) {
            if (checkpoint.label == QStringLiteral("Safety checkpoint before restore")) {
                safety = checkpoint.id;
                break;
            }
        }
        QVERIFY(!safety.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!navigation->isBusy(), 10000);
        successShown = false;
        toolbar->restoreRequested(safety);
        QTRY_VERIFY_WITH_TIMEOUT(successShown, 15000);
        QCOMPARE(editor->text(), QStringLiteral("# unsaved project\n"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("# unsaved project\n"));
        file.close();
        dialogs.stop();
        window.close();
        QVERIFY(store.removeHistory().succeeded);
    }

    void backupPagesFitWithoutPageScrolling_data()
    {
        QTest::addColumn<bool>("slovak");
        QTest::addColumn<QSize>("size");
        QTest::newRow("english-default") << false << QSize(760, 700);
        QTest::newRow("slovak-default") << true << QSize(760, 700);
        QTest::newRow("slovak-small-screen") << true << QSize(900, 650);
    }

    void backupPagesFitWithoutPageScrolling()
    {
        QFETCH(bool, slovak);
        QFETCH(QSize, size);
        QTranslator translator;
        if (slovak) {
            if (!translator.load(QCoreApplication::applicationDirPath()
                                 + QStringLiteral("/therion_studio_sk.qm"))) {
                QSKIP("Slovak translation requires Qt LinguistTools");
            }
            QApplication::installTranslator(&translator);
        }
        const auto removeTranslator = qScopeGuard([&] { QApplication::removeTranslator(&translator); });
        QTemporaryDir project;
        const QString root = project.filePath(QString(120, QLatin1Char('a')));
        QVERIFY(QDir().mkpath(root));
        MainWindowSettingsDialog::Settings settings;
        settings.projectRootPath = root;
        MainWindowSettingsDialog dialog(settings);
        auto *mainTabs = dialog.findChild<QTabWidget *>(QStringLiteral("settingsTabs"));
        QCOMPARE(mainTabs->currentIndex(), 0);
        dialog.showBackupPage();
        dialog.resize(size);
        dialog.show();
        QTest::qWait(20);
        QVERIFY(dialog.width() <= size.width());
        QVERIFY(dialog.height() <= size.height());
        auto *backups = qobject_cast<ProjectVersioningWidget *>(mainTabs->currentWidget());
        QVERIFY(backups);
        QVERIFY(backups->findChildren<QScrollArea *>().isEmpty());
        auto *pages = backups->findChild<QTabWidget *>(QStringLiteral("backupSettingsPages"));
        QVERIFY(pages);
        QCOMPARE(pages->count(), 3);
        for (int i = 0; i < pages->count(); ++i) {
            pages->setCurrentIndex(i);
            if (i == 1) {
                auto *instructions = pages->currentWidget()->findChild<QPlainTextEdit *>(
                    QStringLiteral("githubLoginOutput"));
                QVERIFY(instructions);
                instructions->setPlainText(QStringLiteral("Enter the device code in your browser."));
                instructions->show();
            }
            QTest::qWait(20);
            auto *page = pages->currentWidget();
            for (auto *button : page->findChildren<QAbstractButton *>()) {
                if (!qobject_cast<QPushButton *>(button) && !qobject_cast<QCheckBox *>(button)) {
                    continue;
                }
                QVERIFY2(button->isVisible(), qPrintable(button->objectName()));
                const QRect bounds(button->mapTo(&dialog, QPoint()), button->size());
                QVERIFY2(dialog.rect().contains(bounds), qPrintable(button->objectName()));
                QVERIFY2(page->rect().contains(QRect(button->mapTo(page, QPoint()), button->size())),
                         qPrintable(button->objectName()));
            }
        }
    }
};

QTEST_MAIN(MainWindowProjectVersioningTest)
#include "MainWindowProjectVersioningTest.moc"
