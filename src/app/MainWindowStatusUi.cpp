#include "MainWindow.h"
#include "ui/ApplicationStyleTokens.h"
#include "ProjectJoinsPanel.h"
#include "LucideIconFactory.h"
#include "text_editor/map_editor/MapEditorJoinOutline.h"
#include "text_editor/map_editor/MapEditorJoinPairs.h"

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
#include <QPointer>
#include <QMenu>

void MainWindow::initializeDocumentStatusWidgets()
{
    if (statusBar() == nullptr
        || statusHintLabel_ != nullptr
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

    statusMapJoinButton_ = new QToolButton(statusBar());
    statusMapJoinButton_->setAutoRaise(true);
    statusMapJoinButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    statusMapJoinButton_->setPopupMode(QToolButton::InstantPopup);
    statusMapJoinButton_->setFont(statusMapModeLabel_->font());
    statusMapJoinButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    statusMapJoinButton_->setToolTip(tr("Drawings connected to this one"));
    statusMapJoinButton_->setMenu(new QMenu(statusMapJoinButton_));
    statusMapJoinButton_->setVisible(false);
    connect(statusMapJoinButton_->menu(), &QMenu::aboutToShow, this, [this]() {
        populateMapJoinMenu(statusMapJoinButton_->menu());
    });

    statusBar()->addWidget(statusHintLabel_, 1);
    statusBar()->addPermanentWidget(statusMapJoinButton_, 0);
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

    refreshMapJoinStatusButton(qobject_cast<TherionStudio::MapEditorTab *>(tabWidget));

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

    updateDocumentMenuActionState();
}

void MainWindow::populateMapJoinMenu(QMenu *menu)
{
    menu->clear();
    auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(currentDocumentWidget());
    const QVector<TherionStudio::MapEditorJoinPair> pairs = TherionStudio::mapEditorJoinPairsFor(mapTab);
    if (pairs.isEmpty()) {
        menu->addAction(tr("No linked drawings"))->setEnabled(false);
        return;
    }
    for (const TherionStudio::MapEditorJoinPair &pair : pairs) {
        if (!menu->isEmpty()) {
            menu->addSeparator();
        }
        // The drawing's name brings it to the front: its separate map window, or its tab.
        QAction *showDrawing = menu->addAction(pair.partner->displayName());
        QFont nameFont = showDrawing->font();
        nameFont.setBold(true);
        showDrawing->setFont(nameFont);
        const QPointer<TherionStudio::MapEditorTab> shown(pair.partner);
        connect(showDrawing, &QAction::triggered, this, [this, shown]() {
            if (shown == nullptr) {
                return;
            }
            if (shown->isMapPaneDetached()) {
                shown->focusDetachedMapPaneWindow();
            } else if (editorTabs_->indexOf(shown) >= 0) {
                editorTabs_->setCurrentWidget(shown);
            } else {
                shown->window()->raise();
                shown->window()->activateWindow();
            }
        });
        QAction *showOutline = menu->addAction(tr("Show Outline"));
        showOutline->setCheckable(true);
        showOutline->setChecked(pair.outline->isOutlineVisible());
        connect(showOutline, &QAction::toggled, pair.outline, &TherionStudio::MapEditorJoinOutline::setOutlineVisible);
        QAction *removeJoin = menu->addAction(tr("Remove Connection"));
        const QPointer<TherionStudio::MapEditorTab> host(mapTab);
        const QPointer<TherionStudio::MapEditorTab> partner(pair.partner);
        connect(removeJoin, &QAction::triggered, this, [this, host, partner]() {
            TherionStudio::closeMapEditorJoinPair(host, partner);
            refreshDocumentStatusWidgets();
        });
    }
}

void MainWindow::applyJoinConnectionColors()
{
    if (projectJoinsPanel_ == nullptr) {
        return;
    }
    using TherionStudio::UiStyleTokens::JoinConnectionColorRole;
    const bool darkBackground = palette().color(QPalette::Base).lightness() < 128;
    const auto color = [darkBackground](JoinConnectionColorRole role) {
        return TherionStudio::UiStyleTokens::joinConnectionColor(role, darkBackground);
    };
    projectJoinsPanel_->setConnectionColors({color(JoinConnectionColorRole::AutomaticScrapJoin),
                                             color(JoinConnectionColorRole::CountedScrapJoin),
                                             color(JoinConnectionColorRole::PointLineJoin)});
}

void MainWindow::refreshMapJoinLinkIcons()
{
    // Linked drawings show the link icon on their tab and on their separate map window.
    const QIcon linkIcon = TherionStudio::themedLucideIcon(QStringLiteral("link"), palette(), 16, devicePixelRatioF());
    QList<TherionStudio::MapEditorTab *> mapTabs = detachedMapEditorTabs();
    for (int index = 0; index < editorTabs_->count(); ++index) {
        auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(editorTabs_->widget(index));
        if (mapTab == nullptr) {
            continue;
        }
        mapTabs.append(mapTab);
        editorTabs_->setTabIcon(index, TherionStudio::mapEditorHasJoinLink(mapTab) ? linkIcon : QIcon());
    }
    for (TherionStudio::MapEditorTab *mapTab : std::as_const(mapTabs)) {
        mapTab->setMapPaneWindowIcon(TherionStudio::mapEditorHasJoinLink(mapTab) ? linkIcon : QIcon());
    }
}

void MainWindow::refreshMapJoinStatusButton(TherionStudio::MapEditorTab *mapTab)
{
    refreshMapJoinLinkIcons();
    if (statusMapJoinButton_ == nullptr) {
        return;
    }
    const QVector<TherionStudio::MapEditorJoinPair> pairs = TherionStudio::mapEditorJoinPairsFor(mapTab);
    statusMapJoinButton_->setVisible(!pairs.isEmpty());
    if (pairs.size() == 1) {
        statusMapJoinButton_->setText(tr("Connected: %1").arg(pairs.first().partner->displayName()));
    } else if (pairs.size() > 1) {
        statusMapJoinButton_->setText(tr("Connected: %1 drawings").arg(pairs.size()));
    }
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
        refreshMapJoinLinkIcons();
        applyJoinConnectionColors();
        break;
    default:
        break;
    }
}
