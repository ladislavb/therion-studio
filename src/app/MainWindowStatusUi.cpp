#include "MainWindow.h"

#include "ui/ApplicationStyleSheets.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QColor>
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QPalette>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTime>
#include <QToolButton>

void MainWindow::initializeDocumentStatusWidgets()
{
    if (statusBar() == nullptr
        || statusHintLabel_ != nullptr
        || statusMapPointerLabel_ != nullptr
        || statusMapZoomLabel_ != nullptr
        || statusMapModeLabel_ != nullptr
        || statusCompilerButton_ != nullptr
        || statusDocumentEncodingLabel_ != nullptr) {
        return;
    }

    statusHintLabel_ = new QLabel(statusBar());
    statusHintLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusHintLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusHintLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusHintLabel_->setMinimumWidth(0);
    statusHintLabel_->setContentsMargins(12, 0, 0, 0);
    statusHintLabel_->setToolTip(tr("Map editor instructions"));

    statusDocumentEncodingLabel_ = new QLabel(statusBar());
    statusDocumentEncodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusDocumentEncodingLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusDocumentEncodingLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    statusMapPointerLabel_ = new QLabel(statusBar());
    statusMapPointerLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapPointerLabel_->setAlignment(Qt::AlignCenter);
    statusMapPointerLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapPointerLabel_->setToolTip(tr("Map pointer position"));
    statusMapPointerLabel_->setVisible(false);

    statusMapZoomLabel_ = new QLabel(statusBar());
    statusMapZoomLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapZoomLabel_->setAlignment(Qt::AlignCenter);
    statusMapZoomLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapZoomLabel_->setVisible(false);

    statusMapModeLabel_ = new QLabel(statusBar());
    statusMapModeLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusMapModeLabel_->setAlignment(Qt::AlignCenter);
    statusMapModeLabel_->setFont(statusBar()->font());
    statusMapModeLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapModeLabel_->setVisible(false);

    statusCompilerButton_ = new QToolButton(statusBar());
    statusCompilerButton_->setAutoRaise(true);
    statusCompilerButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusCompilerButton_->setCursor(Qt::PointingHandCursor);
    statusCompilerButton_->setFont(statusMapModeLabel_->font());
    statusCompilerButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusCompilerButton_->setToolTip(tr("Open Compiler output"));
    connect(statusCompilerButton_, &QToolButton::clicked, this, [this]() {
        const bool compilerSidebarVisible = sidebarContainer_ != nullptr
            && sidebarContainer_->isVisible()
            && sidebarContentContainer_ != nullptr
            && sidebarContentContainer_->isVisible()
            && !sidebarCollapsed_
            && activeSidebarPane_ == SidebarPane::Console;
        if (compilerSidebarVisible) {
            setSidebarCollapsed(true);
            return;
        }

        showSidebarPane(SidebarPane::Console);
    });

    statusBar()->addWidget(statusHintLabel_, 1);
    statusBar()->addPermanentWidget(statusMapPointerLabel_, 0);
    statusBar()->addPermanentWidget(statusMapZoomLabel_, 0);
    statusBar()->addPermanentWidget(statusMapModeLabel_, 0);
    statusBar()->addPermanentWidget(statusCompilerButton_, 0);
    statusBar()->addPermanentWidget(statusDocumentEncodingLabel_, 0);
    setCompilerStatusIdle();
    refreshDocumentStatusWidgets();
}

void MainWindow::refreshDocumentStatusWidgets()
{
    if (statusMapZoomLabel_ == nullptr
        || statusMapModeLabel_ == nullptr
        || statusDocumentEncodingLabel_ == nullptr) {
        updateDocumentMenuActionState();
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    QString encodingText;
    QString statusHintText;
    QString mapModeText;
    QString mapZoomText;
    bool mapModeVisible = false;
    bool mapModeInsertActive = false;

    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        encodingText = textTab->statusEncodingText();
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        encodingText = mapTab->statusEncodingText();
        if (!mapTab->isMapPaneDetached()) {
            statusHintText = mapTab->statusHintText();
            mapModeText = mapTab->statusModeText();
            mapZoomText = tr("%1%").arg(mapTab->zoomPercent());
            mapModeVisible = true;
            mapModeInsertActive = mapTab->isInsertModeActive();
        }
    }

    if (encodingText.trimmed().isEmpty()) {
        statusDocumentEncodingLabel_->clear();
        statusDocumentEncodingLabel_->setToolTip(QString());
    } else {
        statusDocumentEncodingLabel_->setText(encodingText);
        statusDocumentEncodingLabel_->setToolTip(tr("Document encoding"));
    }

    updateStatusHintLabel(statusHintText);

    if (!mapModeVisible) {
        statusMapZoomLabel_->clear();
        statusMapZoomLabel_->setToolTip(QString());
        statusMapZoomLabel_->setVisible(false);
        statusMapModeLabel_->clear();
        statusMapModeLabel_->setToolTip(QString());
        statusMapModeLabel_->setStyleSheet(QString());
        statusMapModeLabel_->setVisible(false);
    } else {
        statusMapZoomLabel_->setText(mapZoomText);
        statusMapZoomLabel_->setToolTip(tr("Map zoom"));
        statusMapZoomLabel_->setVisible(true);

        const QString badgeText = mapModeInsertActive ? tr("M: Insert") : tr("M: Select");
        statusMapModeLabel_->setText(badgeText);
        statusMapModeLabel_->setToolTip(mapModeText);
        statusMapModeLabel_->setStyleSheet(
            TherionStudio::statusBadgeStyleSheet(TherionStudio::mapModeStatusAccentColor(mapModeInsertActive)));
        statusMapModeLabel_->setVisible(true);
    }

    refreshMapPointerStatusLabel();
    updateDocumentMenuActionState();
}

void MainWindow::connectMapEditorTabStatusSignals(TherionStudio::MapEditorTab *tab)
{
    connect(tab, &TherionStudio::MapEditorTab::statusHintChanged, this,
            [this, tab]() { if (currentDocumentWidget() == tab) { refreshDocumentStatusWidgets(); } });
    connect(tab, &TherionStudio::MapEditorTab::pointerStatusChanged, this,
            [this, tab]() { if (currentDocumentWidget() == tab) { refreshMapPointerStatusLabel(); } });
}

void MainWindow::refreshMapPointerStatusLabel()
{
    if (statusMapPointerLabel_ == nullptr) {
        return;
    }

    auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(currentDocumentWidget());
    const bool visible = mapTab != nullptr && !mapTab->isMapPaneDetached() && mapTab->isPointerStatusEnabled();
    statusMapPointerLabel_->setText(visible ? mapTab->pointerStatusText() : QString());
    if (visible) {
        statusMapPointerLabel_->setMinimumWidth(mapTab->pointerStatusMinimumWidth(statusMapPointerLabel_->fontMetrics()));
    }
    statusMapPointerLabel_->setVisible(visible);
}

void MainWindow::updateStatusHintLabel(const QString &text)
{
    if (statusHintLabel_ == nullptr) {
        return;
    }

    const QString trimmedText = text.trimmed();
    statusHintLabel_->setText(trimmedText);
    statusHintLabel_->setToolTip(trimmedText.isEmpty() ? QString() : tr("Map editor instructions"));
    statusHintLabel_->setVisible(!trimmedText.isEmpty());
}

void MainWindow::updateDocumentMenuActionState()
{
    const bool hasCurrentAttachedDocument =
        qobject_cast<TherionStudio::TextEditorTab *>(currentDocumentWidget()) != nullptr
        || qobject_cast<TherionStudio::MapEditorTab *>(currentDocumentWidget()) != nullptr;

    bool hasAnyAttachedDocument = false;
    for (int index = 0; index < editorTabs_->count(); ++index) {
        QWidget *tabWidget = editorTabs_->widget(index);
        if (qobject_cast<TherionStudio::TextEditorTab *>(tabWidget) != nullptr
            || qobject_cast<TherionStudio::MapEditorTab *>(tabWidget) != nullptr) {
            hasAnyAttachedDocument = true;
            break;
        }
    }

    const bool hasAnyDetachedDocument = !detachedMapEditorTabs().isEmpty();
    const bool hasAnyDocument = hasAnyAttachedDocument || hasAnyDetachedDocument;

    if (saveAction_ != nullptr) {
        saveAction_->setEnabled(hasCurrentAttachedDocument);
    }
    if (saveAllAction_ != nullptr) {
        saveAllAction_->setEnabled(hasAnyDocument);
    }
    if (closeTabAction_ != nullptr) {
        closeTabAction_->setEnabled(hasCurrentAttachedDocument);
    }
    if (closeAllTabsAction_ != nullptr) {
        closeAllTabsAction_->setEnabled(hasAnyDocument);
    }
}

void MainWindow::setCompilerStatusIdle()
{
    updateCompilerStatusButton(tr("C: Idle"),
                               tr("Therion compiler is idle. Click to open Compiler output."),
                               QStringLiteral("#6c757d"));
}

void MainWindow::setCompilerStatusRunning(const QString &configPath)
{
    const QString toolTip = configPath.isEmpty()
        ? tr("Therion is running. Click to open Compiler output.")
        : tr("Therion is running %1. Click to open Compiler output.").arg(QDir::toNativeSeparators(configPath));
    updateCompilerStatusButton(tr("C: Running..."), toolTip, QStringLiteral("#2f80ed"));
}

void MainWindow::setCompilerStatusResult(bool success, const QString &details)
{
    const QString finishedAt = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    const QString statusText = success
        ? tr("C: OK %1").arg(finishedAt)
        : tr("C: Failed %1").arg(finishedAt);
    const QString toolTip = details.trimmed().isEmpty()
        ? tr("Last Therion run finished at %1. Click to open Compiler output.").arg(finishedAt)
        : tr("%1 Click to open Compiler output.").arg(details);
    updateCompilerStatusButton(statusText,
                               toolTip,
                               success ? QStringLiteral("#2e9f5c") : QStringLiteral("#c0392b"));
}

void MainWindow::updateCompilerStatusButton(const QString &text,
                                            const QString &toolTip,
                                            const QString &accentColor)
{
    if (statusCompilerButton_ == nullptr) {
        return;
    }

    statusCompilerButton_->setText(text);
    statusCompilerButton_->setToolTip(toolTip);
    statusCompilerButton_->setStyleSheet(
        TherionStudio::compilerStatusBadgeStyleSheet(QColor(accentColor)));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    refreshDocumentStatusWidgets();
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::WindowStateChange:
        refreshFullScreenAction();
        break;
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        refreshWorkspaceModeSwitcher();
        refreshWorkspaceIconTheme();
        rebuildStructureSidebar();
        rebuildMapObjectsTree();
        refreshMapBackgroundPanel();
        break;
    default:
        break;
    }
}
