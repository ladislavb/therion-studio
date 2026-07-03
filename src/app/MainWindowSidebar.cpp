#include "MainWindow.h"

#include "ui/ApplicationStyleSheets.h"
#include "LucideIconFactory.h"
#include "ui/ApplicationControlMetrics.h"
#include "ui/ApplicationSegmentedControlStyle.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <functional>

namespace
{
enum StructurePanelPage
{
    StructurePanelFilesPage = 0,
    StructurePanelSurveyPage = 1,
    StructurePanelMapPage = 2
};

class PaletteEventFilter final : public QObject
{
public:
    explicit PaletteEventFilter(std::function<void()> callback, QObject *parent = nullptr)
        : QObject(parent)
        , callback_(std::move(callback))
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == qApp
            && event != nullptr
            && (event->type() == QEvent::ApplicationPaletteChange
                || event->type() == QEvent::PaletteChange
                || event->type() == QEvent::StyleChange)) {
            if (callback_) {
                callback_();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> callback_;
};

int sidebarAutoSnapThreshold(int railWidth)
{
    // Keep a small-but-usable content width below which the sidebar snaps to rail.
    return qMax(240, railWidth + 180);
}

void prepareSidebarContentPane(QWidget *contentWidget)
{
    if (contentWidget == nullptr) {
        return;
    }
    contentWidget->setVisible(true);
    contentWidget->setMinimumWidth(0);
    contentWidget->setMaximumWidth(QWIDGETSIZE_MAX);
}

int splitterTotalWidth(QSplitter *splitter)
{
    if (splitter == nullptr) {
        return 0;
    }

    int totalWidth = 0;
    const QList<int> sizes = splitter->sizes();
    for (const int size : sizes) {
        totalWidth += size;
    }
    if (totalWidth <= 0) {
        totalWidth = splitter->width();
    }
    return totalWidth;
}

}

void MainWindow::buildStructureSidebar()
{
    if (mainContentLayout_ == nullptr || mainContentSplitter_ == nullptr) {
        return;
    }

    sidebarCollapseButton_ = nullptr;

    connect(mainContentSplitter_, &QSplitter::splitterMoved, this, [this](int position, int) {
        if (updatingSidebarSplitter_) {
            return;
        }
        refreshWorkspaceModeSwitcherGeometry();
        if (sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
            return;
        }

        const int width = qMax(0, position);
        const int snapThreshold = sidebarAutoSnapThreshold(sidebarRailWidth_);
        if (sidebarCollapsed_) {
            if (width > snapThreshold) {
                sidebarCollapsed_ = false;
                sidebarExpandedWidth_ = qMax(width, sidebarRailWidth_ + 240);
                prepareSidebarContentPane(sidebarContentContainer_);
                updateSidebarCollapseButton();
                refreshViewMenuActions();
            }
            return;
        }

        if (width <= snapThreshold) {
            setSidebarCollapsed(true);
        } else {
            sidebarExpandedWidth_ = width;
        }
    });

    auto *activityBar = new QFrame;
    sidebarContainer_ = activityBar;
    activityBar->setObjectName(QStringLiteral("SidebarActivityRail"));
    activityBar->setFrameShape(QFrame::NoFrame);
    activityBar->setFixedWidth(sidebarRailWidth_);
    activityBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto *activityLayout = new QVBoxLayout(activityBar);
    activityLayout->setContentsMargins(6, 10, 6, 10);
    activityLayout->setSpacing(7);

    const QString structureIconName = QStringLiteral("network");
    const QString outputsIconName = QStringLiteral("eye");
    const QString searchIconName = QStringLiteral("search");
    const QString validationIconName = QStringLiteral("spell-check");
    const QString consoleIconName = QStringLiteral("cog");
    const QSize activityIconSize = TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::sidebarActivityIconSize());
    const QSize activityButtonSize = TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::sidebarActivityButtonSize());
    const auto configureActivityButton = [&](QToolButton *button, const QString &iconName, const QString &toolTip) {
        if (button == nullptr) {
            return;
        }
        button->setToolTip(toolTip);
        button->setIconSize(activityIconSize);
        button->setFixedSize(activityButtonSize);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
    };
    const auto handleActivityButtonClick = [this](SidebarPane pane) {
        if (isSidebarEffectivelyCollapsed()) {
            if (sidebarContainer_ != nullptr && !sidebarContainer_->isVisible()) {
                sidebarContainer_->setVisible(true);
            }

            if (sidebarCollapsed_) {
                setSidebarCollapsed(false);
            } else {
                if (sidebarExpandedWidth_ <= (sidebarRailWidth_ + 8)) {
                    sidebarExpandedWidth_ = qMax(320, sidebarRailWidth_ + 240);
                }
                sidebarCollapsed_ = false;
                prepareSidebarContentPane(sidebarContentContainer_);
                restoreSidebarWidth();
                updateSidebarCollapseButton();
                refreshViewMenuActions();
            }
            setSidebarPane(pane);
            return;
        }
        const bool projectNavigationPaneActive = pane == SidebarPane::StructureBrowser
            && (activeSidebarPane_ == SidebarPane::FileBrowser
                || activeSidebarPane_ == SidebarPane::StructureBrowser);
        if (projectNavigationPaneActive) {
            setSidebarCollapsed(true);
            return;
        }
        if (activeSidebarPane_ == pane) {
            setSidebarCollapsed(true);
            return;
        }
        setSidebarPane(pane);
    };

    sidebarStructureButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarStructureButton_, structureIconName, tr("Structure"));
    connect(sidebarStructureButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::StructureBrowser);
    });
    activityLayout->addWidget(sidebarStructureButton_);

    sidebarOutputsButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarOutputsButton_, outputsIconName, tr("Outputs"));
    connect(sidebarOutputsButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Outputs);
    });
    activityLayout->addWidget(sidebarOutputsButton_);

    sidebarSearchButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarSearchButton_, searchIconName, tr("Search"));
    connect(sidebarSearchButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Search);
    });
    activityLayout->addWidget(sidebarSearchButton_);

    sidebarValidationButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarValidationButton_, validationIconName, tr("Validation"));
    connect(sidebarValidationButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Validation);
    });
    activityLayout->addWidget(sidebarValidationButton_);

    sidebarConsoleButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarConsoleButton_, consoleIconName, tr("Compiler"));
    connect(sidebarConsoleButton_, &QToolButton::clicked, this, [handleActivityButtonClick]() {
        handleActivityButtonClick(SidebarPane::Console);
    });
    activityLayout->addWidget(sidebarConsoleButton_);

    auto *compileActionSeparator = new QFrame(activityBar);
    compileActionSeparator->setObjectName(QStringLiteral("SidebarActivitySeparator"));
    compileActionSeparator->setFrameShape(QFrame::NoFrame);
    compileActionSeparator->setFixedSize(22, 1);
    activityLayout->addWidget(compileActionSeparator, 0, Qt::AlignHCenter);

    sidebarCompileButton_ = new QToolButton(activityBar);
    configureActivityButton(sidebarCompileButton_, QStringLiteral("play"), tr("Compile Project Config"));
    sidebarCompileButton_->setCheckable(false);
    connect(sidebarCompileButton_, &QToolButton::clicked, this, [this]() {
        runTherionProjectConfig();
    });
    activityLayout->addWidget(sidebarCompileButton_);

    const auto applyActivityRailTheme = [activityBar,
                                         structureButton = sidebarStructureButton_,
                                         outputsButton = sidebarOutputsButton_,
                                         searchButton = sidebarSearchButton_,
                                         validationButton = sidebarValidationButton_,
                                         compilerButton = sidebarConsoleButton_,
                                         compileButton = sidebarCompileButton_,
                                         structureIconName,
                                         outputsIconName,
                                         searchIconName,
                                         validationIconName,
                                         consoleIconName,
                                         activityIconSize]() {
        if (activityBar == nullptr) {
            return;
        }

        const QPalette palette = QApplication::palette(activityBar);
        activityBar->setPalette(palette);
        activityBar->setAutoFillBackground(true);
        activityBar->setStyleSheet(TherionStudio::sidebarActivityRailStyleSheet(palette));

        const int extent = activityIconSize.width();
        const qreal devicePixelRatio = activityBar->devicePixelRatioF();
        if (structureButton != nullptr) {
            structureButton->setIcon(TherionStudio::themedLucideIcon(structureIconName, palette, extent, devicePixelRatio));
        }
        if (outputsButton != nullptr) {
            outputsButton->setIcon(TherionStudio::themedLucideIcon(outputsIconName, palette, extent, devicePixelRatio));
        }
        if (searchButton != nullptr) {
            searchButton->setIcon(TherionStudio::themedLucideIcon(searchIconName, palette, extent, devicePixelRatio));
        }
        if (validationButton != nullptr) {
            validationButton->setIcon(TherionStudio::themedLucideIcon(validationIconName, palette, extent, devicePixelRatio));
        }
        if (compilerButton != nullptr) {
            compilerButton->setIcon(TherionStudio::themedLucideIcon(consoleIconName, palette, extent, devicePixelRatio));
        }
        if (compileButton != nullptr) {
            compileButton->setIcon(TherionStudio::themedLucideIcon(QStringLiteral("play"), palette, extent, devicePixelRatio));
        }
    };
    applyActivityRailTheme();
    if (qApp != nullptr) {
        auto *paletteEventFilter = new PaletteEventFilter(applyActivityRailTheme, activityBar);
        qApp->installEventFilter(paletteEventFilter);
    }

    activityLayout->addStretch(1);

    // Keep the activity rail width driven by icon/button metrics to avoid extra blank gutter.
    sidebarRailWidth_ = qMax(40, activityBar->sizeHint().width());
    activityBar->setFixedWidth(sidebarRailWidth_);
    sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
    sidebarContainer_->setMaximumWidth(sidebarRailWidth_);
    mainContentLayout_->addWidget(sidebarContainer_, 0);

    sidebarContentContainer_ = new QWidget(mainContentSplitter_);
    sidebarContentContainer_->setObjectName(QStringLiteral("mainSidebarSplitterPane"));
    sidebarContentContainer_->setMinimumWidth(0);
    sidebarContentContainer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    sidebarContentContainer_->setAttribute(Qt::WA_StyledBackground, true);
    sidebarContentContainer_->setStyleSheet(TherionStudio::sidebarContentPaneStyleSheet());
    auto *sidebarContentLayout = new QVBoxLayout(sidebarContentContainer_);
    sidebarContentLayout->setContentsMargins(0, 0, 0, 0);
    sidebarContentLayout->setSpacing(0);

    sidebarPages_ = new QStackedWidget(sidebarContentContainer_);
    sidebarPages_->setMinimumWidth(0);
    sidebarPages_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    sidebarContentLayout->addWidget(sidebarPages_, 1);

    mainContentSplitter_->addWidget(sidebarContentContainer_);

    buildProjectBrowser();

    auto *structurePage = new QWidget(sidebarPages_);
    auto *structureLayout = new QVBoxLayout(structurePage);
    structureLayout->setContentsMargins(12, 12, 12, 12);
    structureLayout->setSpacing(8);

    auto *structureViewSelector = new QWidget(structurePage);
    structureViewSelector->setObjectName(QStringLiteral("structureViewSegmentedControl"));
    auto *structureViewSelectorLayout = new QHBoxLayout(structureViewSelector);
    structureViewSelectorLayout->setContentsMargins(0, 0, 0, 0);
    structureViewSelectorLayout->setSpacing(0);
    structureViewModeButtons_ = new QButtonGroup(structureViewSelector);
    structureViewModeButtons_->setExclusive(true);

    auto *filesViewButton = new QPushButton(tr("Files"), structureViewSelector);
    auto *surveyViewButton = new QPushButton(tr("Survey"), structureViewSelector);
    auto *mapViewButton = new QPushButton(QStringLiteral("Map"), structureViewSelector);
    const QList<QPushButton *> structureViewButtons = {filesViewButton, surveyViewButton, mapViewButton};
    for (QPushButton *button : structureViewButtons) {
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("structureSegmentButton"));
        button->setMinimumHeight(TherionStudio::UiMetrics::segmentedControlMinimumHeight());
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        structureViewSelectorLayout->addWidget(button);
    }
    filesViewButton->setProperty("firstSegment", true);
    mapViewButton->setProperty("lastSegment", true);
    structureViewSelector->setStyleSheet(
        TherionStudio::segmentedControlButtonStyleSheet(QStringLiteral("QPushButton#structureSegmentButton")));
    structureViewModeButtons_->addButton(filesViewButton, StructurePanelFilesPage);
    structureViewModeButtons_->addButton(surveyViewButton, StructurePanelSurveyPage);
    structureViewModeButtons_->addButton(mapViewButton, StructurePanelMapPage);
    structureLayout->addWidget(structureViewSelector);

    structureViewStack_ = new QStackedWidget(structurePage);
    structureViewStack_->setObjectName(QStringLiteral("structureViewStack"));
    structureViewStack_->setMinimumWidth(0);
    structureViewStack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    if (projectFilesPage_ == nullptr) {
        projectFilesPage_ = new QWidget(structureViewStack_);
    }
    auto *surveyStructurePage = new QWidget(structureViewStack_);
    structureSurveyLayout_ = new QVBoxLayout(surveyStructurePage);
    structureSurveyLayout_->setContentsMargins(0, 0, 0, 0);
    structureSurveyLayout_->setSpacing(0);
    auto *mapStructurePage = new QWidget(structureViewStack_);
    structureMapLayout_ = new QVBoxLayout(mapStructurePage);
    structureMapLayout_->setContentsMargins(0, 0, 0, 0);
    structureMapLayout_->setSpacing(0);
    structureViewStack_->addWidget(projectFilesPage_);
    structureViewStack_->addWidget(surveyStructurePage);
    structureViewStack_->addWidget(mapStructurePage);
    connect(structureViewModeButtons_, &QButtonGroup::idClicked, this, [this](int pageIndex) {
        setStructurePanelPage(pageIndex);
    });

    structureTree_ = new QTreeView(surveyStructurePage);
    structureTree_->setMinimumWidth(0);
    structureTree_->setModel(structureModel_);
    structureTree_->setRootIsDecorated(true);
    structureTree_->setAnimated(true);
    structureTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    structureTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    structureTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    structureTree_->setAlternatingRowColors(true);
    structureTree_->header()->hide();
    connect(structureTree_, &QTreeView::activated, this, [this](const QModelIndex &index) {
        handleStructureItemActivated(index, structureTree_);
    });

    structureSurveyLayout_->addWidget(structureTree_, 1);
    structureLayout->addWidget(structureViewStack_, 1);
    sidebarPages_->addWidget(structurePage);
    structureViewStack_->setCurrentIndex(StructurePanelSurveyPage);
    surveyViewButton->setChecked(true);

    buildOutputsSidebar();
    buildSearchSidebar();
    buildValidationSidebar();

    consoleSidebarPage_ = new QWidget(sidebarPages_);
    consoleSidebarPageLayout_ = new QVBoxLayout(consoleSidebarPage_);
    consoleSidebarPageLayout_->setContentsMargins(12, 12, 12, 12);
    consoleSidebarPageLayout_->setSpacing(8);
    sidebarPages_->addWidget(consoleSidebarPage_);

    updateSidebarCollapseButton();
    setSidebarPane(activeSidebarPane_);
}

void MainWindow::showSidebarPane(SidebarPane pane)
{
    if (sidebarContainer_ == nullptr) {
        setSidebarPane(pane);
        return;
    }

    sidebarContainer_->setVisible(true);
    if (sidebarContentContainer_ != nullptr) {
        sidebarContentContainer_->setVisible(true);
    }

    if (sidebarCollapsed_) {
        setSidebarCollapsed(false);
    } else {
        restoreSidebarWidth();
    }

    setSidebarPane(pane);
}

void MainWindow::setStructurePanelPage(int pageIndex)
{
    if (structureViewStack_ == nullptr) {
        return;
    }

    const int boundedPageIndex = qBound(static_cast<int>(StructurePanelFilesPage),
                                        pageIndex,
                                        static_cast<int>(StructurePanelMapPage));
    structureViewStack_->setCurrentIndex(boundedPageIndex);
    if (QAbstractButton *button = structureViewModeButtons_ != nullptr
            ? structureViewModeButtons_->button(boundedPageIndex)
            : nullptr) {
        button->setChecked(true);
    }

    if (boundedPageIndex == StructurePanelFilesPage) {
        activeSidebarPane_ = SidebarPane::FileBrowser;
        if (sidebarStructureButton_ != nullptr) {
            sidebarStructureButton_->setChecked(true);
        }
        return;
    }

    activeSidebarPane_ = SidebarPane::StructureBrowser;
    if (sidebarStructureButton_ != nullptr) {
        sidebarStructureButton_->setChecked(true);
    }

    const StructureViewMode nextMode = boundedPageIndex == StructurePanelMapPage
        ? StructureViewMode::Map
        : StructureViewMode::Survey;
    if (structureTree_ != nullptr) {
        if (nextMode == StructureViewMode::Map && structureMapLayout_ != nullptr) {
            structureMapLayout_->addWidget(structureTree_, 1);
        } else if (nextMode == StructureViewMode::Survey && structureSurveyLayout_ != nullptr) {
            structureSurveyLayout_->addWidget(structureTree_, 1);
        }
    }
    if (structureViewMode_ == nextMode) {
        return;
    }

    storeCurrentStructureExpansionState();
    structureViewMode_ = nextMode;
    hasAppliedStructureSidebarIndex_ = false;
    lastAppliedStructureSidebarSignature_.clear();
    if (!lastStructureSidebarProjectIndex_.projectRootPath.isEmpty()) {
        applyStructureSidebarIndex(lastStructureSidebarProjectIndex_);
    }
}

void MainWindow::setSidebarPane(SidebarPane pane)
{
    if (sidebarPages_ == nullptr) {
        return;
    }

    activeSidebarPane_ = pane;
    const int sidebarPageIndex = pane == SidebarPane::FileBrowser
        ? static_cast<int>(SidebarPane::StructureBrowser)
        : static_cast<int>(pane);
    sidebarPages_->setCurrentIndex(sidebarPageIndex);
    if (structureViewStack_ != nullptr) {
        if (pane == SidebarPane::FileBrowser) {
            setStructurePanelPage(StructurePanelFilesPage);
        }
    }
    if (sidebarStructureButton_ != nullptr) {
        sidebarStructureButton_->setChecked(pane == SidebarPane::FileBrowser || pane == SidebarPane::StructureBrowser);
    }
    if (sidebarSearchButton_ != nullptr) {
        sidebarSearchButton_->setChecked(pane == SidebarPane::Search);
    }
    if (sidebarOutputsButton_ != nullptr) {
        sidebarOutputsButton_->setChecked(pane == SidebarPane::Outputs);
    }
    if (sidebarValidationButton_ != nullptr) {
        sidebarValidationButton_->setChecked(pane == SidebarPane::Validation);
    }
    if (sidebarConsoleButton_ != nullptr) {
        sidebarConsoleButton_->setChecked(pane == SidebarPane::Console);
    }
    if (sidebarCompileButton_ != nullptr) {
        sidebarCompileButton_->setChecked(false);
    }
}

void MainWindow::updateValidationRailIndicator()
{
    if (sidebarValidationButton_ == nullptr) {
        return;
    }

    const bool hasProblems = validationProblemCount_ > 0;
    sidebarValidationButton_->setProperty("validationProblems", hasProblems);
    const bool hasErrors = hasProblems &&
        validationHighestSeverity_ == TherionStudio::TherionSourceDiagnosticSeverity::Error;
    sidebarValidationButton_->setProperty(
        "validationSeverity",
        hasErrors ? QStringLiteral("error") : (hasProblems ? QStringLiteral("warning") : QStringLiteral("none")));
    if (hasErrors) {
        sidebarValidationButton_->setToolTip(tr("Validation: %1 problem(s), errors present")
                                                 .arg(validationProblemCount_));
    } else if (hasProblems) {
        sidebarValidationButton_->setToolTip(tr("Validation: %1 warning(s)").arg(validationProblemCount_));
    } else {
        sidebarValidationButton_->setToolTip(tr("Validation"));
    }
    sidebarValidationButton_->style()->unpolish(sidebarValidationButton_);
    sidebarValidationButton_->style()->polish(sidebarValidationButton_);
    sidebarValidationButton_->update();
}

void MainWindow::clearValidationRailIndicator()
{
    validationProblemCount_ = 0;
    validationHighestSeverity_ = TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    updateValidationRailIndicator();
}

void MainWindow::rememberSidebarWidth()
{
    if (sidebarContentContainer_ == nullptr || !sidebarContentContainer_->isVisible()) {
        return;
    }

    const int width = sidebarContentContainer_->width();
    if (width <= sidebarAutoSnapThreshold(sidebarRailWidth_)) {
        return;
    }

    sidebarExpandedWidth_ = qMax(sidebarRailWidth_ + 120, width);
}

void MainWindow::restoreSidebarWidth()
{
    if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || sidebarExpandedWidth_ <= 0) {
        return;
    }

    const auto applyRestore = [this]() {
        if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
            return;
        }
        sidebarContentContainer_->setVisible(true);

        const int totalWidth = splitterTotalWidth(mainContentSplitter_);
        if (totalWidth <= 0) {
            return;
        }

        const int targetContentWidth = qBound(0, sidebarExpandedWidth_, qMax(0, totalWidth - 240));
        const int editorWidth = qMax(240, totalWidth - targetContentWidth);
        updatingSidebarSplitter_ = true;
        mainContentSplitter_->setSizes({targetContentWidth, editorWidth});
        updatingSidebarSplitter_ = false;
        refreshWorkspaceModeSwitcherGeometry();
    };

    if (mainContentSplitter_->width() <= 0) {
        QTimer::singleShot(0, this, applyRestore);
        return;
    }
    applyRestore();
}

bool MainWindow::isSidebarEffectivelyCollapsed() const
{
    if (sidebarCollapsed_) {
        return true;
    }
    if (sidebarContentContainer_ == nullptr || !sidebarContentContainer_->isVisible()) {
        return true;
    }
    return sidebarContentContainer_->width() <= sidebarAutoSnapThreshold(sidebarRailWidth_);
}

void MainWindow::scheduleSidebarCollapseLayoutSync()
{
    if (sidebarCollapseSyncPending_) {
        return;
    }

    sidebarCollapseSyncPending_ = true;
    QTimer::singleShot(0, this, [this]() {
        sidebarCollapseSyncPending_ = false;
        if (sidebarCollapsed_) {
            setSidebarCollapsed(true);
            return;
        }

        updateSidebarCollapseButton();
        refreshViewMenuActions();
    });
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    if (collapsed == sidebarCollapsed_ && collapsed == isSidebarEffectivelyCollapsed()) {
        updateSidebarCollapseButton();
        refreshViewMenuActions();
        return;
    }

    sidebarCollapsed_ = collapsed;
    QWidget *contentWidget = sidebarContentContainer_;
    if (collapsed) {
        rememberSidebarWidth();
        sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
        prepareSidebarContentPane(contentWidget);

        const auto applyCollapse = [this]() {
            if (mainContentSplitter_ == nullptr || sidebarContainer_ == nullptr || sidebarContentContainer_ == nullptr || !sidebarContainer_->isVisible()) {
                return;
            }

            const int totalWidth = splitterTotalWidth(mainContentSplitter_);
            if (totalWidth <= 0) {
                return;
            }
            updatingSidebarSplitter_ = true;
            mainContentSplitter_->setSizes({0, qMax(240, totalWidth)});
            updatingSidebarSplitter_ = false;
            refreshWorkspaceModeSwitcherGeometry();
        };

        if (mainContentSplitter_->width() <= 0) {
            QTimer::singleShot(0, this, applyCollapse);
        } else {
            applyCollapse();
        }
    } else {
        sidebarContainer_->setMinimumWidth(sidebarRailWidth_);
        prepareSidebarContentPane(contentWidget);
        restoreSidebarWidth();
    }

    updateSidebarCollapseButton();
    refreshViewMenuActions();
}

void MainWindow::updateSidebarCollapseButton()
{
    if (sidebarCollapseButton_ == nullptr || sidebarContainer_ == nullptr) {
        return;
    }

    const bool collapsed = isSidebarEffectivelyCollapsed();
    const Qt::ArrowType arrowType = collapsed ? Qt::RightArrow : Qt::LeftArrow;
    sidebarCollapseButton_->setArrowType(arrowType);
    sidebarCollapseButton_->setToolTip(collapsed ? tr("Expand sidebar") : tr("Collapse sidebar"));
}
