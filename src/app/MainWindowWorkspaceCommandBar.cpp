#include "MainWindow.h"

#include "LucideIconFactory.h"
#include "MainWindowDocumentHelpers.h"
#include "WorkspaceCommandBarStyle.h"
#include "reports/TherionSqlReportTab.h"
#include "three_d_viewer/ThreeDViewerTab.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"
#include "ui/ApplicationControlMetrics.h"

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QMenu>
#include <QPalette>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

#include <algorithm>

namespace
{
class WorkspaceCommandFlowLayout final : public QLayout
{
public:
    explicit WorkspaceCommandFlowLayout(QWidget *parent)
        : QLayout(parent)
    {
        setContentsMargins(4, 4, 8, 4);
        setSpacing(4);
    }

    ~WorkspaceCommandFlowLayout() override
    {
        while (QLayoutItem *item = takeAt(0)) {
            delete item;
        }
    }

    void addItem(QLayoutItem *item) override
    {
        items_.append(item);
    }

    void addStretch()
    {
        addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    }

    int count() const override
    {
        return items_.size();
    }

    QLayoutItem *itemAt(int index) const override
    {
        return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
    }

    QLayoutItem *takeAt(int index) override
    {
        if (index < 0 || index >= items_.size()) {
            return nullptr;
        }
        return items_.takeAt(index);
    }

    Qt::Orientations expandingDirections() const override
    {
        return Qt::Horizontal;
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override
    {
        QSize size;
        for (const QLayoutItem *item : items_) {
            if (item == nullptr || item->isEmpty()) {
                continue;
            }
            size = size.expandedTo(item->minimumSize());
        }
        const QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override
    {
        const QMargins margins = contentsMargins();
        int width = margins.left() + margins.right();
        int height = 0;
        for (const QLayoutItem *item : items_) {
            if (item == nullptr || item->isEmpty()) {
                continue;
            }
            const QSize itemSize = item->sizeHint();
            width += itemSize.width() + spacing();
            height = std::max(height, itemSize.height());
        }
        if (!items_.isEmpty()) {
            width -= spacing();
        }
        return QSize(width, height + margins.top() + margins.bottom());
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        const QMargins margins = contentsMargins();
        const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;
        const int hSpace = spacing();
        const int vSpace = spacing();

        for (QLayoutItem *item : items_) {
            if (item != nullptr && item->spacerItem() != nullptr) {
                const int trailingWidth = visibleItemsWidth(items_.mid(items_.indexOf(item) + 1));
                if (trailingWidth <= 0) {
                    continue;
                }
                if (trailingWidth > effectiveRect.width()) {
                    x = effectiveRect.x();
                    y += lineHeight + vSpace;
                    lineHeight = 0;
                    continue;
                }
                if (x + trailingWidth > effectiveRect.right() + 1 && lineHeight > 0) {
                    y += lineHeight + vSpace;
                    lineHeight = 0;
                }
                x = effectiveRect.right() - trailingWidth + 1;
                continue;
            }
            if (item == nullptr || item->isEmpty()) {
                continue;
            }
            const QSize itemSize = item->sizeHint();
            const int nextX = x + itemSize.width();
            if (nextX > effectiveRect.right() + 1 && lineHeight > 0) {
                x = effectiveRect.x();
                y += lineHeight + vSpace;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), itemSize));
            }
            x += itemSize.width() + hSpace;
            lineHeight = std::max(lineHeight, itemSize.height());
        }
        return y + lineHeight - rect.y() + margins.bottom();
    }

    int visibleItemsWidth(const QList<QLayoutItem *> &items) const
    {
        int width = 0;
        int visibleCount = 0;
        for (QLayoutItem *item : items) {
            if (item == nullptr || item->isEmpty() || item->spacerItem() != nullptr) {
                continue;
            }
            width += item->sizeHint().width();
            ++visibleCount;
        }
        if (visibleCount > 1) {
            width += (visibleCount - 1) * spacing();
        }
        return width;
    }

    QList<QLayoutItem *> items_;
};

QToolButton *createWorkspaceIconButton(QWidget *parent,
                                       const QString &toolTip,
                                       const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setAutoRaise(false);
    button->setIconSize(TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::workspaceCommandIconSize()));
    button->setIcon(TherionStudio::themedLucideIcon(iconName,
                                                    button->palette(),
                                                    TherionStudio::UiMetrics::workspaceCommandIconSize(),
                                                    button->devicePixelRatioF()));
    button->setProperty("lucideIconName", iconName);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(TherionStudio::UiMetrics::squareSize(TherionStudio::UiMetrics::workspaceCommandButtonSize()));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QFrame *createWorkspaceToolbarSeparator(QWidget *parent)
{
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName(QStringLiteral("workspaceToolbarSeparator"));
    separator->setFixedSize(QSize(1, TherionStudio::UiMetrics::workspaceCommandButtonSize()));
    return separator;
}
}

void MainWindow::initializeWorkspaceModeSwitcher()
{
    if (editorTabs_ != nullptr) {
        editorTabs_->setObjectName(QStringLiteral("mainEditorTabs"));
        if (!editorTabs_->property("baseStyleSheet").isValid()) {
            editorTabs_->setProperty("baseStyleSheet", editorTabs_->styleSheet());
        }
    }

    QWidget *workspaceHost = editorAreaColumn_ != nullptr ? editorAreaColumn_ : editorAreaHost_;
    if (workspaceHost == nullptr) {
        workspaceHost = centralWidget();
    }
    if (workspaceHost == nullptr) {
        workspaceHost = editorTabs_;
    }
    workspaceModeSwitcher_ = new QWidget(workspaceHost);
    workspaceModeSwitcher_->setObjectName(QStringLiteral("workspaceCommandBar"));
    workspaceModeSwitcher_->setAttribute(Qt::WA_StyledBackground, true);
    workspaceModeSwitcher_->setStyleSheet(TherionStudio::workspaceCommandBarStyleSheet(palette().color(QPalette::Window),
                                                                                        false,
                                                                                        false));
    auto *hostLayout = new WorkspaceCommandFlowLayout(workspaceModeSwitcher_);
    workspaceModeSwitcher_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    workspaceNewDocumentButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("New Document"), QStringLiteral("file-plus"));
    auto *newDocumentMenu = new QMenu(workspaceNewDocumentButton_);
    if (newTherionSourceAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionSourceAction_);
    }
    if (newTherionMapAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionMapAction_);
    }
    if (newTherionConfigAction_ != nullptr) {
        newDocumentMenu->addAction(newTherionConfigAction_);
    }
    connect(workspaceNewDocumentButton_, &QToolButton::clicked, workspaceNewDocumentButton_, [newDocumentMenu, button = workspaceNewDocumentButton_]() {
        if (newDocumentMenu == nullptr || button == nullptr) {
            return;
        }
        newDocumentMenu->popup(button->mapToGlobal(QPoint(0, button->height())));
    });
    workspaceSaveButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Save"), QStringLiteral("save"));
    workspaceExportCsvButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("Export CSV"), QStringLiteral("download"));
    workspaceEditSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    workspaceUndoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Undo"), QStringLiteral("undo-2"));
    workspaceRedoButton_ = createWorkspaceIconButton(workspaceModeSwitcher_, tr("Redo"), QStringLiteral("redo-2"));
    workspaceCompileCurrentConfigButton_ =
        createWorkspaceIconButton(workspaceModeSwitcher_, tr("Compile Current Config"), QStringLiteral("play"));
    hostLayout->addWidget(workspaceNewDocumentButton_);
    hostLayout->addWidget(workspaceSaveButton_);
    hostLayout->addWidget(workspaceExportCsvButton_);
    hostLayout->addWidget(workspaceEditSeparator_);
    hostLayout->addWidget(workspaceUndoButton_);
    hostLayout->addWidget(workspaceRedoButton_);
    workspaceCompileSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceCompileSeparator_);
    hostLayout->addWidget(workspaceCompileCurrentConfigButton_);
    workspaceHistorySeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceHistorySeparator_);
    workspaceZoomGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *zoomLayout = new QHBoxLayout(workspaceZoomGroup_);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->setSpacing(4);
    workspaceZoomInButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom In"), QStringLiteral("zoom-in"));
    workspaceZoomOutButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Zoom Out"), QStringLiteral("zoom-out"));
    workspaceFitButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit"), QStringLiteral("scan"));
    workspaceFitBackgroundButton_ = createWorkspaceIconButton(workspaceZoomGroup_, tr("Fit With Background"), QStringLiteral("image"));
    zoomLayout->addWidget(workspaceZoomInButton_);
    zoomLayout->addWidget(workspaceZoomOutButton_);
    zoomLayout->addWidget(workspaceFitButton_);
    zoomLayout->addWidget(workspaceFitBackgroundButton_);
    hostLayout->addWidget(workspaceZoomGroup_);
    workspaceZoomSeparator_ = createWorkspaceToolbarSeparator(workspaceModeSwitcher_);
    hostLayout->addWidget(workspaceZoomSeparator_);
    workspaceThreeDViewerGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *viewerLayout = new QHBoxLayout(workspaceThreeDViewerGroup_);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(4);
    workspaceThreeDViewerResetButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Reset 3D View"), QStringLiteral("house"));
    workspaceThreeDViewerFitButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Fit 3D View"), QStringLiteral("scan"));
    workspaceThreeDViewerOrthographicButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Orthogonal Projection"), QStringLiteral("square-plus"));
    workspaceThreeDViewerOrthographicButton_->setCheckable(true);
    workspaceThreeDViewerMeasureButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Measure points"), QStringLiteral("ruler"));
    workspaceThreeDViewerMeasureButton_->setCheckable(true);
    workspaceThreeDViewerAutoRotateButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Start Auto Rotation"), QStringLiteral("play"));
    workspaceThreeDViewerAutoRotateButton_->setCheckable(true);
    workspaceThreeDViewerTopViewButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Top View"), QStringLiteral("arrow-big-down"));
    workspaceThreeDViewerSideViewButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Side View"), QStringLiteral("arrow-big-right"));
    workspaceThreeDViewerRollLeftButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Rotate Left"), QStringLiteral("rotate-cw"));
    workspaceThreeDViewerRollRightButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Rotate Right"), QStringLiteral("rotate-ccw"));
    workspaceThreeDViewerExportImageButton_ = createWorkspaceIconButton(workspaceThreeDViewerGroup_, tr("Export 3D Image"), QStringLiteral("download"));
    viewerLayout->addWidget(workspaceThreeDViewerResetButton_);
    viewerLayout->addWidget(workspaceThreeDViewerFitButton_);
    viewerLayout->addWidget(createWorkspaceToolbarSeparator(workspaceThreeDViewerGroup_));
    viewerLayout->addWidget(workspaceThreeDViewerOrthographicButton_);
    viewerLayout->addWidget(workspaceThreeDViewerTopViewButton_);
    viewerLayout->addWidget(workspaceThreeDViewerSideViewButton_);
    viewerLayout->addWidget(createWorkspaceToolbarSeparator(workspaceThreeDViewerGroup_));
    viewerLayout->addWidget(workspaceThreeDViewerRollLeftButton_);
    viewerLayout->addWidget(workspaceThreeDViewerRollRightButton_);
    viewerLayout->addWidget(workspaceThreeDViewerAutoRotateButton_);
    viewerLayout->addWidget(createWorkspaceToolbarSeparator(workspaceThreeDViewerGroup_));
    viewerLayout->addWidget(workspaceThreeDViewerMeasureButton_);
    viewerLayout->addWidget(createWorkspaceToolbarSeparator(workspaceThreeDViewerGroup_));
    viewerLayout->addWidget(workspaceThreeDViewerExportImageButton_);
    hostLayout->addWidget(workspaceThreeDViewerGroup_);
    workspaceThreeDViewerGroup_->setVisible(false);
    workspaceMapToolsGroup_ = new QWidget(workspaceModeSwitcher_);
    auto *mapToolsLayout = new QHBoxLayout(workspaceMapToolsGroup_);
    mapToolsLayout->setContentsMargins(0, 0, 0, 0);
    mapToolsLayout->setSpacing(4);
    workspaceSelectButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Select"), QStringLiteral("mouse-pointer-2"));
    workspaceCompleteDraftButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Complete Draft"), QStringLiteral("check"));
    workspaceInsertScrapButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Insert Scrap"), QStringLiteral("puzzle"));
    workspacePointButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Point"), QStringLiteral("locate-fixed"));
    workspaceLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Line"), QStringLiteral("spline"));
    workspaceFreehandLineButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Freehand"), QStringLiteral("pencil-line"));
    workspaceAreaButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Area"), QStringLiteral("pentagon"));
    workspaceSmartAreaButton_ = createWorkspaceIconButton(workspaceMapToolsGroup_, tr("Smart Area"), QStringLiteral("square-dashed-mouse-pointer"));
    workspaceSelectButton_->setCheckable(true);
    workspacePointButton_->setCheckable(true);
    workspaceLineButton_->setCheckable(true);
    workspaceFreehandLineButton_->setCheckable(true);
    workspaceAreaButton_->setCheckable(true);
    workspaceSmartAreaButton_->setCheckable(true);
    mapToolsLayout->addWidget(workspaceSelectButton_);
    mapToolsLayout->addWidget(workspaceCompleteDraftButton_);
    mapToolsLayout->addWidget(createWorkspaceToolbarSeparator(workspaceMapToolsGroup_));
    mapToolsLayout->addWidget(workspaceInsertScrapButton_);
    mapToolsLayout->addWidget(workspacePointButton_);
    mapToolsLayout->addWidget(workspaceLineButton_);
    mapToolsLayout->addWidget(workspaceFreehandLineButton_);
    mapToolsLayout->addWidget(workspaceAreaButton_);
    mapToolsLayout->addWidget(workspaceSmartAreaButton_);
    hostLayout->addWidget(workspaceMapToolsGroup_);
    hostLayout->addStretch();

    workspaceMapModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *mapLayout = new QHBoxLayout(workspaceMapModeSwitcher_);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(4);
    workspaceVisualModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Visual"), QStringLiteral("pen-tool"));
    workspaceRawModeButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceVisualModeButton_->setCheckable(true);
    workspaceRawModeButton_->setCheckable(true);
    workspaceMapPaneWindowButton_ = createWorkspaceIconButton(workspaceMapModeSwitcher_, tr("Separate Map"), QStringLiteral("screen-share"));
    workspaceMapPaneWindowButton_->setObjectName(QStringLiteral("workspaceMapPaneWindowButton"));
    mapLayout->addWidget(workspaceRawModeButton_);
    mapLayout->addWidget(workspaceVisualModeButton_);
    mapLayout->addWidget(workspaceMapPaneWindowButton_);

    workspaceTextModeSwitcher_ = new QWidget(workspaceModeSwitcher_);
    auto *textLayout = new QHBoxLayout(workspaceTextModeSwitcher_);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    workspaceTextRawModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Raw"), QStringLiteral("code"));
    workspaceBlocksModeButton_ = createWorkspaceIconButton(workspaceTextModeSwitcher_, tr("Blocks"), QStringLiteral("toy-brick"));
    workspaceTextRawModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setCheckable(true);
    workspaceBlocksModeButton_->setToolTip(tr("Structured block canvas for .th and Therion config files."));
    workspaceBlocksModeButton_->setAccessibleName(tr("Blocks"));
    textLayout->addWidget(workspaceTextRawModeButton_);
    textLayout->addWidget(workspaceBlocksModeButton_);

    hostLayout->addWidget(workspaceMapModeSwitcher_);
    hostLayout->addWidget(workspaceTextModeSwitcher_);

    connect(workspaceSaveButton_, &QToolButton::clicked, this, &MainWindow::saveActiveDocument);
    connect(workspaceExportCsvButton_, &QToolButton::clicked, this, [this]() {
        if (auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(currentDocumentWidget());
            reportTab != nullptr) {
            reportTab->exportCurrentTableCsv();
        }
    });
    connect(workspaceUndoButton_, &QToolButton::clicked, this, &MainWindow::triggerUndoForActiveDocument);
    connect(workspaceRedoButton_, &QToolButton::clicked, this, &MainWindow::triggerRedoForActiveDocument);
    connect(workspaceCompileCurrentConfigButton_, &QToolButton::clicked, this, &MainWindow::triggerCompileCurrentConfigForActiveDocument);
    connect(workspaceZoomInButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomInForActiveDocument);
    connect(workspaceZoomOutButton_, &QToolButton::clicked, this, &MainWindow::triggerZoomOutForActiveDocument);
    connect(workspaceFitButton_, &QToolButton::clicked, this, &MainWindow::triggerFitForActiveDocument);
    connect(workspaceFitBackgroundButton_, &QToolButton::clicked, this, &MainWindow::triggerFitWithBackgroundForActiveDocument);
    connect(workspaceThreeDViewerFitButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerFitForActiveDocument);
    connect(workspaceThreeDViewerResetButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerResetForActiveDocument);
    connect(workspaceThreeDViewerMeasureButton_, &QToolButton::toggled, this, [this](bool checked) {
        if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
            viewerTab->setMeasurementMode(checked);
        }
    });
    connect(workspaceThreeDViewerAutoRotateButton_, &QToolButton::toggled, this, [this](bool checked) {
        if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
            viewerTab->setAutoRotationEnabled(checked);
        }
        updateThreeDViewerAutoRotationButton(checked);
    });
    connect(workspaceThreeDViewerOrthographicButton_, &QToolButton::toggled, this, [this](bool checked) {
        if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
            viewerTab->setOrthographicProjection(checked);
        }
    });
    connect(workspaceThreeDViewerTopViewButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerTopViewForActiveDocument);
    connect(workspaceThreeDViewerSideViewButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerSideViewForActiveDocument);
    connect(workspaceThreeDViewerRollLeftButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerRollLeftForActiveDocument);
    connect(workspaceThreeDViewerRollRightButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerRollRightForActiveDocument);
    connect(workspaceThreeDViewerExportImageButton_, &QToolButton::clicked, this, &MainWindow::triggerThreeDViewerExportImageForActiveDocument);
    connect(workspaceSelectButton_, &QToolButton::clicked, this, &MainWindow::triggerSelectForActiveDocument);
    connect(workspaceCompleteDraftButton_, &QToolButton::clicked, this, &MainWindow::triggerCompleteDraftForActiveDocument);
    connect(workspaceInsertScrapButton_, &QToolButton::clicked, this, &MainWindow::triggerInsertScrapForActiveDocument);
    connect(workspacePointButton_, &QToolButton::clicked, this, &MainWindow::triggerPointForActiveDocument);
    connect(workspaceLineButton_, &QToolButton::clicked, this, &MainWindow::triggerLineForActiveDocument);
    connect(workspaceFreehandLineButton_, &QToolButton::clicked, this, &MainWindow::triggerFreehandLineForActiveDocument);
    connect(workspaceAreaButton_, &QToolButton::clicked, this, &MainWindow::triggerAreaForActiveDocument);
    connect(workspaceSmartAreaButton_, &QToolButton::clicked, this, &MainWindow::triggerSmartAreaForActiveDocument);
    connect(workspaceVisualModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        }
    });
    connect(workspaceRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        }
    });
    connect(workspaceMapPaneWindowButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
            mapTab->toggleMapPaneWindow();
        }
    });
    connect(workspaceTextRawModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        }
    });
    connect(workspaceBlocksModeButton_, &QToolButton::clicked, this, [this]() {
        if (workspaceModeSwitcherSyncInProgress_) {
            return;
        }
        if (auto *textTab = currentTextTab(); textTab != nullptr) {
            textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
        }
    });

    workspaceModeSwitcher_->raise();
    workspaceMapModeSwitcher_->setVisible(false);
    workspaceTextModeSwitcher_->setVisible(false);
    workspaceZoomGroup_->setVisible(false);
    workspaceMapToolsGroup_->setVisible(false);
    workspaceEditSeparator_->setVisible(false);
    workspaceHistorySeparator_->setVisible(false);
    workspaceCompileSeparator_->setVisible(false);
    workspaceCompileCurrentConfigButton_->setVisible(false);
    workspaceZoomSeparator_->setVisible(false);
    workspaceThreeDViewerGroup_->setVisible(false);
    workspaceModeSwitcher_->setVisible(true);
    if (editorAreaLayout_ != nullptr) {
        editorAreaLayout_->insertWidget(0, workspaceModeSwitcher_);
    }
}

void MainWindow::refreshWorkspaceModeSwitcher()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceMapModeSwitcher_ == nullptr
        || workspaceTextModeSwitcher_ == nullptr
        || workspaceNewDocumentButton_ == nullptr
        || workspaceEditSeparator_ == nullptr
        || workspaceVisualModeButton_ == nullptr
        || workspaceRawModeButton_ == nullptr
        || workspaceMapPaneWindowButton_ == nullptr
        || workspaceSaveButton_ == nullptr
        || workspaceExportCsvButton_ == nullptr
        || workspaceUndoButton_ == nullptr
        || workspaceRedoButton_ == nullptr
        || workspaceCompileCurrentConfigButton_ == nullptr
        || workspaceZoomGroup_ == nullptr
        || workspaceZoomInButton_ == nullptr
        || workspaceZoomOutButton_ == nullptr
        || workspaceFitButton_ == nullptr
        || workspaceFitBackgroundButton_ == nullptr
        || workspaceThreeDViewerGroup_ == nullptr
        || workspaceThreeDViewerFitButton_ == nullptr
        || workspaceThreeDViewerResetButton_ == nullptr
        || workspaceThreeDViewerMeasureButton_ == nullptr
        || workspaceThreeDViewerAutoRotateButton_ == nullptr
        || workspaceThreeDViewerOrthographicButton_ == nullptr
        || workspaceThreeDViewerTopViewButton_ == nullptr
        || workspaceThreeDViewerSideViewButton_ == nullptr
        || workspaceThreeDViewerRollLeftButton_ == nullptr
        || workspaceThreeDViewerRollRightButton_ == nullptr
        || workspaceThreeDViewerExportImageButton_ == nullptr
        || workspaceMapToolsGroup_ == nullptr
        || workspaceSelectButton_ == nullptr
        || workspaceCompleteDraftButton_ == nullptr
        || workspaceInsertScrapButton_ == nullptr
        || workspacePointButton_ == nullptr
        || workspaceLineButton_ == nullptr
        || workspaceFreehandLineButton_ == nullptr
        || workspaceAreaButton_ == nullptr
        || workspaceSmartAreaButton_ == nullptr
        || workspaceHistorySeparator_ == nullptr
        || workspaceCompileSeparator_ == nullptr
        || workspaceZoomSeparator_ == nullptr
        || workspaceTextRawModeButton_ == nullptr
        || workspaceBlocksModeButton_ == nullptr) {
        return;
    }

    QWidget *tabWidget = currentDocumentWidget();
    refreshWorkspaceIconTheme();
    auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget);
    auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget);
    auto *viewerTab = qobject_cast<TherionStudio::ThreeDViewerTab *>(tabWidget);
    auto *reportTab = qobject_cast<TherionStudio::TherionSqlReportTab *>(tabWidget);
    const bool showMapModes = mapTab != nullptr;
    const bool showTextModes = textTab != nullptr;
    const bool showThreeDViewerModes = viewerTab != nullptr;
    const bool showSqlReportActions = reportTab != nullptr;
    const bool showEditorActions = !showThreeDViewerModes && !showSqlReportActions;
    const bool showCompileCurrentConfig = showTextModes && !currentDocumentTherionConfigPath().isEmpty();
    const bool mapPaneDetached = mapTab != nullptr && mapTab->isMapPaneDetached();
    const bool embeddedMapSurfaceActive = mapTab != nullptr
        && !mapPaneDetached
        && mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Visual;
    const bool showZoomTools = showMapModes && !mapPaneDetached;
    const bool showMapTools = showMapModes && !mapPaneDetached;
    QColor commandBarBackground = palette().color(QPalette::Window);
    if (showTextModes && textTab != nullptr) {
        const QColor sourceSurface = textTab->sourceSurfaceColor();
        if (sourceSurface.isValid()) {
            commandBarBackground = sourceSurface;
        }
    }
    workspaceModeSwitcher_->setStyleSheet(TherionStudio::workspaceCommandBarStyleSheet(commandBarBackground,
                                                                                        false,
                                                                                        false));

    workspaceModeSwitcher_->setVisible(true);
    workspaceMapModeSwitcher_->setVisible(showMapModes);
    workspaceTextModeSwitcher_->setVisible(showTextModes);
    workspaceThreeDViewerGroup_->setVisible(showThreeDViewerModes);
    workspaceNewDocumentButton_->setVisible(showEditorActions);
    workspaceEditSeparator_->setVisible(showEditorActions && tabWidget != nullptr);
    workspaceHistorySeparator_->setVisible(showEditorActions && showZoomTools);
    workspaceZoomGroup_->setVisible(showEditorActions && showZoomTools);
    workspaceZoomSeparator_->setVisible(showEditorActions && showZoomTools);
    workspaceThreeDViewerFitButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerResetButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerMeasureButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerAutoRotateButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerOrthographicButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerTopViewButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerSideViewButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerRollLeftButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerRollRightButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerExportImageButton_->setEnabled(showThreeDViewerModes);
    {
        const QSignalBlocker measureBlocker(workspaceThreeDViewerMeasureButton_);
        workspaceThreeDViewerMeasureButton_->setChecked(showThreeDViewerModes
                                                           && currentThreeDViewerTab() != nullptr
                                                           && currentThreeDViewerTab()->measurementMode());
    }
    {
        const bool autoRotationEnabled = showThreeDViewerModes
            && currentThreeDViewerTab() != nullptr
            && currentThreeDViewerTab()->autoRotationEnabled();
        const QSignalBlocker autoRotationBlocker(workspaceThreeDViewerAutoRotateButton_);
        workspaceThreeDViewerAutoRotateButton_->setChecked(autoRotationEnabled);
        updateThreeDViewerAutoRotationButton(autoRotationEnabled);
    }
    {
        const bool orthographicProjection = showThreeDViewerModes
            && currentThreeDViewerTab() != nullptr
            && currentThreeDViewerTab()->orthographicProjection();
        const QSignalBlocker orthographicBlocker(workspaceThreeDViewerOrthographicButton_);
        workspaceThreeDViewerOrthographicButton_->setChecked(orthographicProjection);
    }
    workspaceMapToolsGroup_->setVisible(showEditorActions && showMapTools);
    workspaceSaveButton_->setVisible(showEditorActions);
    workspaceSaveButton_->setEnabled(showEditorActions && tabWidget != nullptr);
    workspaceExportCsvButton_->setVisible(showSqlReportActions);
    workspaceExportCsvButton_->setEnabled(showSqlReportActions
                                          && reportTab != nullptr
                                          && reportTab->canExportCsv());
    const bool canUndo = documentCanUndoForWidget(tabWidget);
    const bool canRedo = documentCanRedoForWidget(tabWidget);
    workspaceUndoButton_->setVisible(showEditorActions);
    workspaceRedoButton_->setVisible(showEditorActions);
    workspaceUndoButton_->setEnabled(showEditorActions && canUndo);
    workspaceRedoButton_->setEnabled(showEditorActions && canRedo);
    if (undoAction_ != nullptr) {
        undoAction_->setEnabled(canUndo);
    }
    if (redoAction_ != nullptr) {
        redoAction_->setEnabled(canRedo);
    }
    workspaceCompileSeparator_->setVisible(showEditorActions && showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setVisible(showEditorActions && showCompileCurrentConfig);
    workspaceCompileCurrentConfigButton_->setEnabled(showEditorActions && showCompileCurrentConfig);
    workspaceZoomInButton_->setVisible(showEditorActions);
    workspaceZoomOutButton_->setVisible(showEditorActions);
    workspaceFitButton_->setVisible(showEditorActions);
    workspaceFitBackgroundButton_->setVisible(showEditorActions);
    workspaceZoomInButton_->setEnabled(showEditorActions && showZoomTools && embeddedMapSurfaceActive);
    workspaceZoomOutButton_->setEnabled(showEditorActions && showZoomTools && embeddedMapSurfaceActive);
    workspaceFitButton_->setEnabled(showEditorActions && showZoomTools && embeddedMapSurfaceActive);
    workspaceFitBackgroundButton_->setEnabled(showEditorActions && showZoomTools
                                              && embeddedMapSurfaceActive
                                              && mapTab != nullptr
                                              && mapTab->backgroundLayerCount() > 0);
    workspaceSelectButton_->setVisible(showEditorActions);
    workspaceCompleteDraftButton_->setVisible(showEditorActions);
    workspaceInsertScrapButton_->setVisible(showEditorActions);
    workspacePointButton_->setVisible(showEditorActions);
    workspaceLineButton_->setVisible(showEditorActions);
    workspaceFreehandLineButton_->setVisible(showEditorActions);
    workspaceAreaButton_->setVisible(showEditorActions);
    workspaceSmartAreaButton_->setVisible(showEditorActions);
    workspaceSelectButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceCompleteDraftButton_->setEnabled(showEditorActions && showMapTools
                                              && embeddedMapSurfaceActive
                                              && mapTab != nullptr
                                              && mapTab->canCompleteDraftAction());
    workspaceInsertScrapButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspacePointButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceLineButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceFreehandLineButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceAreaButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceSmartAreaButton_->setEnabled(showEditorActions && showMapTools && embeddedMapSurfaceActive);
    workspaceThreeDViewerFitButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerResetButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerMeasureButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerAutoRotateButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerOrthographicButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerTopViewButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerSideViewButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerRollLeftButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerRollRightButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerExportImageButton_->setVisible(showThreeDViewerModes);
    workspaceThreeDViewerFitButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerResetButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerMeasureButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerAutoRotateButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerOrthographicButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerTopViewButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerSideViewButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerRollLeftButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerRollRightButton_->setEnabled(showThreeDViewerModes);
    workspaceThreeDViewerExportImageButton_->setEnabled(showThreeDViewerModes);

    workspaceModeSwitcherSyncInProgress_ = true;
    if (showMapModes) {
        const QSignalBlocker selectBlocker(workspaceSelectButton_);
        const QSignalBlocker pointBlocker(workspacePointButton_);
        const QSignalBlocker lineBlocker(workspaceLineButton_);
        const QSignalBlocker freehandBlocker(workspaceFreehandLineButton_);
        const QSignalBlocker areaBlocker(workspaceAreaButton_);
        const QSignalBlocker smartAreaBlocker(workspaceSmartAreaButton_);
        const QSignalBlocker visualBlocker(workspaceVisualModeButton_);
        const QSignalBlocker rawBlocker(workspaceRawModeButton_);
        const QSignalBlocker mapWindowBlocker(workspaceMapPaneWindowButton_);
        const TherionStudio::MapEditorTab::InteractiveDrawMode drawMode = mapTab->interactiveDrawMode();
        workspaceSelectButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::None);
        workspacePointButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Point);
        workspaceLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Line);
        workspaceFreehandLineButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Freehand);
        workspaceAreaButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::Area);
        workspaceSmartAreaButton_->setChecked(drawMode == TherionStudio::MapEditorTab::InteractiveDrawMode::SmartArea);
        workspaceVisualModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        workspaceRawModeButton_->setChecked(mapTab->workspaceMode() == TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        workspaceVisualModeButton_->setEnabled(!mapPaneDetached);
        workspaceRawModeButton_->setEnabled(!mapPaneDetached);
        workspaceMapPaneWindowButton_->setEnabled(true);
        workspaceMapPaneWindowButton_->setProperty("lucideIconName",
                                                   mapPaneDetached
                                                       ? QStringLiteral("monitor-x")
                                                       : QStringLiteral("screen-share"));
        refreshWorkspaceIconTheme();
        workspaceMapPaneWindowButton_->setToolTip(mapTab->mapPaneWindowActionToolTip());
        workspaceMapPaneWindowButton_->setAccessibleName(mapTab->mapPaneWindowActionText());
        workspaceModeSwitcher_->setToolTip(mapPaneDetached
                                               ? tr("Map pane is detached: raw editor remains in this tab while visual map stays in the detached window.")
                                               : QString());
    } else if (showTextModes) {
        const QSignalBlocker rawTextBlocker(workspaceTextRawModeButton_);
        const QSignalBlocker blocksBlocker(workspaceBlocksModeButton_);
        const bool blocksActive = textTab->editorMode() == TherionStudio::TextEditorTab::EditorMode::Blocks;
        workspaceTextRawModeButton_->setChecked(!blocksActive);
        workspaceBlocksModeButton_->setChecked(blocksActive);
        workspaceTextRawModeButton_->setEnabled(true);
        workspaceBlocksModeButton_->setEnabled(textTab->isBlocksModeAvailable());
        workspaceModeSwitcher_->setToolTip(QString());
    } else {
        workspaceModeSwitcher_->setToolTip(QString());
    }
    workspaceModeSwitcherSyncInProgress_ = false;
    refreshWorkspaceModeSwitcherGeometry();
}

void MainWindow::refreshWorkspaceIconTheme()
{
    if (workspaceModeSwitcher_ == nullptr) {
        return;
    }

    const QPalette palette = workspaceModeSwitcher_->palette();
    const qreal devicePixelRatio = workspaceModeSwitcher_->devicePixelRatioF();
    const QList<QToolButton *> buttons = workspaceModeSwitcher_->findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button == nullptr) {
            continue;
        }

        const QString iconName = button->property("lucideIconName").toString();
        if (iconName.isEmpty()) {
            continue;
        }

        button->setIcon(TherionStudio::themedLucideIcon(iconName, palette, button->iconSize().width(), devicePixelRatio));
    }
}

void MainWindow::updateThreeDViewerAutoRotationButton(bool autoRotationEnabled)
{
    if (workspaceThreeDViewerAutoRotateButton_ == nullptr) {
        return;
    }

    const QString iconName = autoRotationEnabled ? QStringLiteral("square") : QStringLiteral("play");
    const QString label = autoRotationEnabled ? tr("Stop Auto Rotation") : tr("Start Auto Rotation");
    workspaceThreeDViewerAutoRotateButton_->setProperty("lucideIconName", iconName);
    workspaceThreeDViewerAutoRotateButton_->setToolTip(label);
    workspaceThreeDViewerAutoRotateButton_->setAccessibleName(label);
    workspaceThreeDViewerAutoRotateButton_->setIcon(TherionStudio::themedLucideIcon(iconName,
                                                                                     workspaceThreeDViewerAutoRotateButton_->palette(),
                                                                                     workspaceThreeDViewerAutoRotateButton_->iconSize().width(),
                                                                                     workspaceThreeDViewerAutoRotateButton_->devicePixelRatioF()));
}

void MainWindow::refreshWorkspaceModeSwitcherGeometry()
{
    if (workspaceModeSwitcher_ == nullptr
        || workspaceModeSwitcher_->parentWidget() == nullptr
        || editorTabs_ == nullptr
        || editorTabs_->tabBar() == nullptr) {
        return;
    }

    workspaceModeSwitcher_->updateGeometry();
    if (sidebarCollapsed_) {
        scheduleSidebarCollapseLayoutSync();
    }
}

void MainWindow::triggerUndoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentUndoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerRedoForActiveDocument()
{
    QWidget *tabWidget = currentDocumentWidget();
    if (tabWidget == nullptr) {
        return;
    }

    if (documentRedoForWidget(tabWidget)) {
        refreshWorkspaceModeSwitcher();
    }
}

void MainWindow::triggerRawModeForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        if (!mapTab->isMapPaneDetached()) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        }
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
    }
}

void MainWindow::triggerSecondaryEditorModeForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        if (!mapTab->isMapPaneDetached()) {
            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Visual);
        }
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr && textTab->isBlocksModeAvailable()) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Blocks);
    }
}

void MainWindow::triggerCompileCurrentConfigForActiveDocument()
{
    runTherionCurrentConfig();
}

void MainWindow::triggerZoomInForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomIn();
    }
}

void MainWindow::triggerZoomOutForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerZoomOut();
    }
}

void MainWindow::triggerFitForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFit();
    }
}

void MainWindow::triggerFitWithBackgroundForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerFitWithBackground();
    }
}

void MainWindow::triggerThreeDViewerFitForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->fitToScene();
    }
}

void MainWindow::triggerThreeDViewerResetForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->resetView();
    }
}

void MainWindow::triggerThreeDViewerTopViewForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->setTopView();
    }
}

void MainWindow::triggerThreeDViewerSideViewForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->setSideView();
    }
}

void MainWindow::triggerThreeDViewerRollLeftForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->rollViewLeft();
    }
}

void MainWindow::triggerThreeDViewerRollRightForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->rollViewRight();
    }
}

void MainWindow::triggerThreeDViewerExportImageForActiveDocument()
{
    if (auto *viewerTab = currentThreeDViewerTab(); viewerTab != nullptr) {
        viewerTab->exportImage();
    }
}

void MainWindow::triggerSelectForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerSelectMode();
    }
}

void MainWindow::triggerCompleteDraftForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerCompleteDraft();
    }
}

void MainWindow::triggerInsertScrapForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerInsertScrap();
    }
}

void MainWindow::triggerPointForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddPoint();
    }
}

void MainWindow::triggerLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddLine();
    }
}

void MainWindow::triggerFreehandLineForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddFreehandLine();
    }
}

void MainWindow::triggerAreaForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerAddArea();
    }
}

void MainWindow::triggerSmartAreaForActiveDocument()
{
    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->triggerSmartArea();
    }
}
