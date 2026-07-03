#include "MainWindowDocumentHelpers.h"

#include <QTabBar>
#include <QStyle>
#include <QWidget>

#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "reports/TherionSqlReportTab.h"

QString documentPathForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->filePath();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->filePath();
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        return viewerTab->filePath();
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        return reportTab->filePath();
    }

    return QString();
}

QString documentDisplayNameForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->displayName();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->displayName();
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        return viewerTab->displayName();
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        return reportTab->displayName();
    }

    return QString();
}

void documentGoToLineForWidget(QWidget *widget, int lineNumber)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->goToLine(lineNumber);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->goToLine(lineNumber);
        return;
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        viewerTab->goToLine(lineNumber);
        return;
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        reportTab->goToLine(lineNumber);
    }
}

int documentCurrentLineNumberForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->currentLineNumber();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->currentLineNumber();
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        return viewerTab->currentLineNumber();
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        return reportTab->currentLineNumber();
    }

    return 0;
}

bool documentIsDirtyForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->isDirty();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->isDirty();
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        return viewerTab->isDirty();
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        return reportTab->isDirty();
    }

    return false;
}

bool documentSaveForWidget(QWidget *widget, QString *errorMessage)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->save(errorMessage);
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->save(errorMessage);
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        return viewerTab->save(errorMessage);
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        return reportTab->save(errorMessage);
    }

    if (errorMessage != nullptr) {
        *errorMessage = QObject::tr("The current document cannot be saved.");
    }

    return false;
}

void documentSetProjectRootPathForWidget(QWidget *widget, const QString &projectRootPath)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->setProjectRootPath(projectRootPath);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->setProjectRootPath(projectRootPath);
        return;
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        viewerTab->setProjectRootPath(projectRootPath);
        return;
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        reportTab->setProjectRootPath(projectRootPath);
    }
}

void documentShowFindBarForWidget(QWidget *widget, bool replaceMode)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->showFindBar(replaceMode);
        return;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->showFindBar(replaceMode);
        return;
    }

    if (auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(widget)) {
        viewerTab->showFindBar(replaceMode);
        return;
    }

    if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(widget)) {
        reportTab->showFindBar(replaceMode);
    }
}

bool documentCanUndoForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->canUndo();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->canUndo();
    }

    return false;
}

bool documentCanRedoForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        return textTab->canRedo();
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        return mapTab->canRedo();
    }

    return false;
}

bool documentUndoForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->triggerUndo();
        return true;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->triggerUndo();
        return true;
    }

    return false;
}

bool documentRedoForWidget(QWidget *widget)
{
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
        textTab->triggerRedo();
        return true;
    }

    if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
        mapTab->triggerRedo();
        return true;
    }

    return false;
}

void updateSidebarModeTabIcons(QTabBar *tabBar, int)
{
    if (tabBar == nullptr) {
        return;
    }

    QStyle *style = tabBar->style();
    tabBar->setTabIcon(0, style->standardIcon(QStyle::SP_DirIcon));
    tabBar->setTabIcon(1, style->standardIcon(QStyle::SP_FileDialogDetailedView));
    tabBar->setTabIcon(2, style->standardIcon(QStyle::SP_FileIcon));
}
