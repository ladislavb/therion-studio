#include "MainWindow.h"

#include "MainWindowDocumentOpenController.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "reports/TherionSqlReportTab.h"
#include "reports/TherionSqlReportWorkerSession.h"
#include "../core/TherionFileTypes.h"

#include <QApplication>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QModelIndex>
#include <QUrl>

bool openFileExternally(QWidget *parent, const QString &filePath)
{
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

    QMessageBox::warning(parent,
                         QApplication::translate("MainWindow", "Open in External App"),
                         QApplication::translate("MainWindow",
                                                 "Failed to open %1 with the system default application.")
                             .arg(QDir::toNativeSeparators(filePath)));
    return false;
}

namespace
{

bool isSupportedTextEditorFilePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    const QString fileName = info.fileName();
    if (TherionStudio::isTherionConfigFileName(fileName)) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("th")
        || suffix == QStringLiteral("txt")
        || suffix == QStringLiteral("log")) {
        return true;
    }

    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(filePath, QMimeDatabase::MatchDefault);
    return mimeType.isValid() && mimeType.inherits(QStringLiteral("text/plain"));
}

void showUnsupportedFilePrompt(QWidget *parent, const QString &filePath)
{
    QMessageBox messageBox(parent);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(QApplication::translate("MainWindow", "Unsupported File"));
    messageBox.setText(QApplication::translate("MainWindow", "Unsupported file."));
    messageBox.setInformativeText(
        QApplication::translate("MainWindow",
                                "Therion Studio cannot open this file type in the internal editor:\n%1")
            .arg(QDir::toNativeSeparators(filePath)));
    auto *openExternalButton = messageBox.addButton(QApplication::translate("MainWindow", "Open in External App"),
                                                    QMessageBox::ActionRole);
    messageBox.addButton(QMessageBox::Cancel);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    messageBox.exec();

    if (messageBox.clickedButton() == openExternalButton) {
        openFileExternally(parent, filePath);
    }
}

} // namespace

void MainWindow::openProjectFilePath(const QString &filePath)
{
    if (TherionStudio::isThreeDViewerArtifactFilePath(filePath)) {
        openThreeDViewerTab(filePath);
    } else if (TherionStudio::isTherionSqlExportFilePath(filePath)) {
        openTherionSqlReportTab(filePath);
    } else if (QFileInfo(filePath).suffix().toLower() == QStringLiteral("th2")) {
        openMapEditorTab(filePath);
    } else if (isSupportedTextEditorFilePath(filePath)) {
        openTextTab(filePath);
    } else {
        showUnsupportedFilePrompt(this, filePath);
    }
}

void MainWindow::handleProjectTreeActivated(const QModelIndex &index)
{
    if (projectModel_ == nullptr || !index.isValid() || projectModel_->isDir(index)) {
        return;
    }

    const QString filePath = projectModel_->filePath(index);
    if (filePath.isEmpty()) {
        return;
    }

    if (TherionStudio::isThreeDViewerArtifactFilePath(filePath)) {
        openThreeDViewerTab(filePath);
    } else if (TherionStudio::isTherionSqlExportFilePath(filePath)) {
        openTherionSqlReportTab(filePath);
    } else if (QFileInfo(filePath).suffix().toLower() == QStringLiteral("th2")) {
        openMapEditorTab(filePath);
    } else if (isSupportedTextEditorFilePath(filePath)) {
        openTextTab(filePath);
    } else {
        showUnsupportedFilePrompt(this, filePath);
    }
}

TherionStudio::ThreeDViewerTab *MainWindow::openThreeDViewerTab(const QString &filePath,
                                                                bool recordRecentFile)
{
    const QString canonicalPath = TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath);
    if (auto *existingTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(documentWidgetForFilePath(canonicalPath))) {
        const int existingIndex = editorTabs_->indexOf(existingTab);
        if (existingIndex >= 0) {
            editorTabs_->setCurrentIndex(existingIndex);
        }
        if (recordRecentFile) {
            recordRecentFilePath(existingTab->filePath());
        }
        return existingTab;
    }

    auto *tab = new TherionStudio::ThreeDViewerTab();
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open File"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::ThreeDViewerTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
    });
    connect(tab, &TherionStudio::ThreeDViewerTab::measurementModeChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::ThreeDViewerTab::autoRotationEnabledChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
        }
    });
    connect(tab, &TherionStudio::ThreeDViewerTab::orthographicProjectionChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshWorkspaceModeSwitcher();
        }
    });

    const int tabIndex = editorTabs_->addTab(tab, tab->displayName());
    editorTabs_->setCurrentIndex(tabIndex);
    registerDocumentFileWatcher(tab->filePath());
    if (recordRecentFile) {
        recordRecentFilePath(tab->filePath());
    }
    updateTabTitle(tab);
    return tab;
}

TherionStudio::TherionSqlReportTab *MainWindow::openTherionSqlReportTab(const QString &filePath,
                                                                        bool recordRecentFile)
{
    const QString canonicalPath = TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath);
    if (auto *existingTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(documentWidgetForFilePath(canonicalPath))) {
        const int existingIndex = editorTabs_->indexOf(existingTab);
        if (existingIndex >= 0) {
            editorTabs_->setCurrentIndex(existingIndex);
        }
        if (recordRecentFile) {
            recordRecentFilePath(existingTab->filePath());
        }
        return existingTab;
    }

    auto *reportSession = new TherionStudio::TherionSqlReportWorkerSession();
    auto *tab = new TherionStudio::TherionSqlReportTab(reportSession);
    reportSession->setParent(tab);
    tab->setProjectRootPath(projectRootPath_);
    QString errorMessage;
    if (!tab->loadFile(canonicalPath, &errorMessage)) {
        QMessageBox::warning(this, tr("Open SQL Export"), errorMessage);
        tab->deleteLater();
        return nullptr;
    }

    connect(tab, &TherionStudio::TherionSqlReportTab::titleChanged, this, [this, tab]() {
        updateTabTitle(tab);
    });
    connect(tab, &TherionStudio::TherionSqlReportTab::statusChanged, this, [this, tab]() {
        if (currentDocumentWidget() == tab) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
        }
    });

    const int tabIndex = editorTabs_->addTab(tab, tab->displayName());
    editorTabs_->setCurrentIndex(tabIndex);
    registerDocumentFileWatcher(tab->filePath());
    if (recordRecentFile) {
        recordRecentFilePath(tab->filePath());
    }
    updateTabTitle(tab);
    persistOpenDocuments();
    return tab;
}
