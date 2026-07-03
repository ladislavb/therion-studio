#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowDocumentOpenController.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "reports/TherionSqlReportTab.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <utility>

namespace
{
QString canonicalDocumentPath(const QString &filePath)
{
    return TherionStudio::MainWindowDocumentOpenController::canonicalDocumentPath(filePath);
}
}

void MainWindow::registerDocumentFileWatcher(const QString &filePath)
{
    if (documentFileWatcher_ == nullptr) {
        return;
    }

    const QString canonicalPath = canonicalDocumentPath(filePath);
    if (canonicalPath.isEmpty() || !QFileInfo::exists(canonicalPath)) {
        return;
    }

    watchedDocumentFingerprints_.insert(canonicalPath, documentFileFingerprint(canonicalPath));
    if (!documentFileWatcher_->files().contains(canonicalPath)) {
        documentFileWatcher_->addPath(canonicalPath);
    }

    const QString directoryPath = QFileInfo(canonicalPath).absolutePath();
    if (!directoryPath.isEmpty() && !documentFileWatcher_->directories().contains(directoryPath)) {
        documentFileWatcher_->addPath(directoryPath);
    }
}

void MainWindow::unregisterDocumentFileWatcherIfUnused(const QString &filePath)
{
    if (documentFileWatcher_ == nullptr) {
        return;
    }

    const QString canonicalPath = canonicalDocumentPath(filePath);
    if (canonicalPath.isEmpty() || documentWidgetForFilePath(canonicalPath) != nullptr) {
        return;
    }

    watchedDocumentFingerprints_.remove(canonicalPath);
    pendingWatchedDocumentChanges_.remove(canonicalPath);
    if (documentFileWatcher_->files().contains(canonicalPath)) {
        documentFileWatcher_->removePath(canonicalPath);
    }

    const QString directoryPath = QFileInfo(canonicalPath).absolutePath();
    if (directoryPath.isEmpty()) {
        return;
    }

    bool directoryStillUsed = false;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        const QString documentPath = documentPathForWidget(tabWidget);
        if (!documentPath.isEmpty()
            && QFileInfo(canonicalDocumentPath(documentPath)).absolutePath() == directoryPath) {
            directoryStillUsed = true;
            break;
        }
    }
    if (!directoryStillUsed) {
        for (auto it = detachedMapPathsByTab_.cbegin(); it != detachedMapPathsByTab_.cend(); ++it) {
            if (QFileInfo(canonicalDocumentPath(it.value())).absolutePath() == directoryPath) {
                directoryStillUsed = true;
                break;
            }
        }
    }
    if (!directoryStillUsed && documentFileWatcher_->directories().contains(directoryPath)) {
        documentFileWatcher_->removePath(directoryPath);
    }
}

bool MainWindow::reloadDocumentWidgetFromDisk(QWidget *documentWidget, QString *errorMessage)
{
    if (documentWidget == nullptr) {
        return false;
    }

    const QString documentPath = documentPathForWidget(documentWidget);
    if (documentPath.isEmpty()) {
        return false;
    }

    bool loaded = false;
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(documentWidget)) {
        loaded = textTab->loadFile(documentPath, errorMessage);
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(documentWidget)) {
        loaded = mapTab->loadFile(documentPath, errorMessage);
    } else if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(documentWidget)) {
        loaded = viewerTab->reloadFile(errorMessage);
    } else if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(documentWidget)) {
        loaded = reportTab->reloadFile(errorMessage);
    }

    if (loaded) {
        updateTabTitle(documentWidget);
        if (currentDocumentWidget() == documentWidget) {
            refreshDocumentStatusWidgets();
            refreshWorkspaceModeSwitcher();
            rebuildMapObjectsTree();
        }
        registerDocumentFileWatcher(documentPath);
    }

    return loaded;
}

void MainWindow::handleWatchedDocumentFileChanged(const QString &filePath)
{
    const QString canonicalPath = canonicalDocumentPath(filePath);
    if (canonicalPath.isEmpty() || pendingWatchedDocumentChanges_.contains(canonicalPath)) {
        return;
    }

    pendingWatchedDocumentChanges_.insert(canonicalPath);
    QTimer::singleShot(150, this, [this, canonicalPath]() {
        pendingWatchedDocumentChanges_.remove(canonicalPath);
        processWatchedDocumentFileChange(canonicalPath);
    });
}

void MainWindow::handleWatchedDocumentDirectoryChanged(const QString &directoryPath)
{
    const QString canonicalDirectoryPath = canonicalDocumentPath(directoryPath);
    if (canonicalDirectoryPath.isEmpty()
        || pendingWatchedDocumentDirectoryChanges_.contains(canonicalDirectoryPath)) {
        return;
    }

    pendingWatchedDocumentDirectoryChanges_.insert(canonicalDirectoryPath);
    QTimer::singleShot(200, this, [this, canonicalDirectoryPath]() {
        pendingWatchedDocumentDirectoryChanges_.remove(canonicalDirectoryPath);
        if (documentFileWatcher_ != nullptr
            && QFileInfo(canonicalDirectoryPath).isDir()
            && !documentFileWatcher_->directories().contains(canonicalDirectoryPath)) {
            documentFileWatcher_->addPath(canonicalDirectoryPath);
        }

        QSet<QString> candidatePaths;
        for (int index = 0; index < editorTabs_->count(); ++index) {
            QWidget *tabWidget = editorTabs_->widget(index);
            const QString documentPath = documentPathForWidget(tabWidget);
            if (documentPath.isEmpty()) {
                continue;
            }
            const QString canonicalPath = canonicalDocumentPath(documentPath);
            if (QFileInfo(canonicalPath).absolutePath() == canonicalDirectoryPath) {
                candidatePaths.insert(canonicalPath);
            }
        }
        for (auto it = detachedMapPathsByTab_.cbegin(); it != detachedMapPathsByTab_.cend(); ++it) {
            const QString canonicalPath = canonicalDocumentPath(it.value());
            if (QFileInfo(canonicalPath).absolutePath() == canonicalDirectoryPath) {
                candidatePaths.insert(canonicalPath);
            }
        }
        for (const QString &candidatePath : std::as_const(candidatePaths)) {
            handleWatchedDocumentFileChanged(candidatePath);
        }
    });
}
