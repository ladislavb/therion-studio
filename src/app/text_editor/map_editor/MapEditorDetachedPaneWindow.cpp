#include "MapEditorDetachedPaneWindow.h"

#include "MapEditorTab.h"
#include "../../LucideIconFactory.h"
#include "../../ui/ApplicationStyleSheets.h"

#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
constexpr int kDetachedCommandButtonSize = 22;
constexpr int kDetachedCommandIconSize = 14;

QToolButton *createDetachedIconButton(QWidget *parent, const QString &toolTip, const QString &iconName)
{
    auto *button = new QToolButton(parent);
    button->setAutoRaise(false);
    button->setIconSize(QSize(kDetachedCommandIconSize, kDetachedCommandIconSize));
    button->setIcon(TherionStudio::themedLucideIcon(iconName,
                                                    button->palette(),
                                                    kDetachedCommandIconSize,
                                                    button->devicePixelRatioF()));
    button->setProperty("lucideIconName", iconName);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setFixedSize(QSize(kDetachedCommandButtonSize, kDetachedCommandButtonSize));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
}

QFrame *createDetachedToolbarSeparator(QWidget *parent)
{
    auto *separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setObjectName(QStringLiteral("workspaceToolbarSeparator"));
    return separator;
}
}

MapEditorDetachedPaneWindow::MapEditorDetachedPaneWindow(MapEditorTab *mapTab, QWidget *parent)
    : QMainWindow(parent, Qt::Window)
    , mapTab_(mapTab)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    auto *centralHost = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralHost);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    commandBar_ = new QWidget(centralHost);
    commandBar_->setObjectName(QStringLiteral("workspaceCommandBar"));
    commandBar_->setAttribute(Qt::WA_StyledBackground, true);
    commandBar_->setStyleSheet(TherionStudio::workspaceCommandBarStyleSheet(palette().color(QPalette::Base),
                                                                             true,
                                                                             true));
    auto *commandLayout = new QHBoxLayout(commandBar_);
    commandLayout->setContentsMargins(4, 4, 8, 4);
    commandLayout->setSpacing(4);

    saveButton_ = createDetachedIconButton(commandBar_, QObject::tr("Save"), QStringLiteral("save"));
    undoButton_ = createDetachedIconButton(commandBar_, QObject::tr("Undo"), QStringLiteral("undo-2"));
    redoButton_ = createDetachedIconButton(commandBar_, QObject::tr("Redo"), QStringLiteral("redo-2"));
    commandLayout->addWidget(saveButton_);
    commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));
    commandLayout->addWidget(undoButton_);
    commandLayout->addWidget(redoButton_);
    commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));

    zoomInButton_ = createDetachedIconButton(commandBar_, QObject::tr("Zoom In"), QStringLiteral("zoom-in"));
    zoomOutButton_ = createDetachedIconButton(commandBar_, QObject::tr("Zoom Out"), QStringLiteral("zoom-out"));
    fitButton_ = createDetachedIconButton(commandBar_, QObject::tr("Fit"), QStringLiteral("scan"));
    fitBackgroundButton_ = createDetachedIconButton(commandBar_, QObject::tr("Fit With Background"), QStringLiteral("image"));
    commandLayout->addWidget(zoomInButton_);
    commandLayout->addWidget(zoomOutButton_);
    commandLayout->addWidget(fitButton_);
    commandLayout->addWidget(fitBackgroundButton_);
    commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));

    selectButton_ = createDetachedIconButton(commandBar_, QObject::tr("Select"), QStringLiteral("mouse-pointer-2"));
    completeDraftButton_ = createDetachedIconButton(commandBar_, QObject::tr("Complete Draft"), QStringLiteral("check"));
    insertScrapButton_ = createDetachedIconButton(commandBar_, QObject::tr("Insert Scrap"), QStringLiteral("puzzle"));
    pointButton_ = createDetachedIconButton(commandBar_, QObject::tr("Point"), QStringLiteral("locate-fixed"));
    lineButton_ = createDetachedIconButton(commandBar_, QObject::tr("Line"), QStringLiteral("spline"));
    freehandLineButton_ = createDetachedIconButton(commandBar_, QObject::tr("Freehand"), QStringLiteral("pencil-line"));
    areaButton_ = createDetachedIconButton(commandBar_, QObject::tr("Area"), QStringLiteral("pentagon"));
    smartAreaButton_ = createDetachedIconButton(commandBar_, QObject::tr("Smart Area"), QStringLiteral("square-dashed-mouse-pointer"));
    selectButton_->setCheckable(true);
    pointButton_->setCheckable(true);
    lineButton_->setCheckable(true);
    freehandLineButton_->setCheckable(true);
    areaButton_->setCheckable(true);
    smartAreaButton_->setCheckable(true);
    commandLayout->addWidget(selectButton_);
    commandLayout->addWidget(completeDraftButton_);
    commandLayout->addWidget(createDetachedToolbarSeparator(commandBar_));
    commandLayout->addWidget(insertScrapButton_);
    commandLayout->addWidget(pointButton_);
    commandLayout->addWidget(lineButton_);
    commandLayout->addWidget(freehandLineButton_);
    commandLayout->addWidget(areaButton_);
    commandLayout->addWidget(smartAreaButton_);
    commandLayout->addStretch(1);

    mapWindowButton_ = createDetachedIconButton(commandBar_, QObject::tr("Return Map"), QStringLiteral("monitor-x"));
    commandLayout->addWidget(mapWindowButton_);

    centralLayout->addWidget(commandBar_);
    setCentralWidget(centralHost);

    if (mapTab_ != nullptr) {
        connect(saveButton_, &QToolButton::clicked, mapTab_, [this]() {
            if (mapTab_ != nullptr) {
                mapTab_->save();
            }
        });
        connect(undoButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerUndo);
        connect(redoButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerRedo);
        connect(zoomInButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerZoomIn);
        connect(zoomOutButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerZoomOut);
        connect(fitButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerFit);
        connect(fitBackgroundButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerFitWithBackground);
        connect(selectButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerSelectMode);
        connect(completeDraftButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerCompleteDraft);
        connect(insertScrapButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerInsertScrap);
        connect(pointButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddPoint);
        connect(lineButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddLine);
        connect(freehandLineButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddFreehandLine);
        connect(areaButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerAddArea);
        connect(smartAreaButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::triggerSmartArea);
        connect(mapWindowButton_, &QToolButton::clicked, mapTab_, &MapEditorTab::toggleMapPaneWindow);
        connect(mapTab_, &MapEditorTab::mapPaneDetachStateChanged, this, [this](bool) {
            refreshCommandBarState();
        });
        connect(mapTab_, &MapEditorTab::zoomStatusChanged, this, [this](int) {
            refreshCommandBarState();
        });
        connect(mapTab_, &MapEditorTab::modeStatusChanged, this, [this]() {
            refreshCommandBarState();
        });
        connect(mapTab_, &MapEditorTab::statusHintChanged, this, [this]() {
            refreshStatusHint();
        });
        connect(mapTab_, &MapEditorTab::pointerStatusChanged, this, [this]() {
            refreshPointerStatus();
        });
        connect(mapTab_, &MapEditorTab::backgroundLayersChanged, this, [this]() {
            refreshCommandBarState();
        });
        connect(mapTab_, &MapEditorTab::commandSurfaceStateChanged, this, [this]() {
            refreshCommandBarState();
        });
        refreshCommandBarState();
    }

    statusHintLabel_ = new QLabel(statusBar());
    statusHintLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusHintLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    statusHintLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusHintLabel_->setMinimumWidth(0);
    statusHintLabel_->setContentsMargins(12, 0, 0, 0);
    pointerLabel_ = new QLabel(statusBar());
    pointerLabel_->setAlignment(Qt::AlignCenter);
    pointerLabel_->setToolTip(QObject::tr("Map pointer position"));
    zoomLabel_ = new QLabel(statusBar());
    zoomLabel_->setAlignment(Qt::AlignCenter);
    zoomLabel_->setMinimumWidth(56);
    modeLabel_ = new QLabel(statusBar());
    modeLabel_->setAlignment(Qt::AlignCenter);
    modeLabel_->setMinimumWidth(78);
    statusBar()->addWidget(statusHintLabel_, 1);
    statusBar()->addPermanentWidget(pointerLabel_, 0);
    statusBar()->addPermanentWidget(zoomLabel_, 0);
    statusBar()->addPermanentWidget(modeLabel_, 0);
    refreshStatusHint();
    refreshPointerStatus();
}

void MapEditorDetachedPaneWindow::setMapPaneWidget(QWidget *mapPaneWidget)
{
    if (mapPaneWidget == nullptr || centralWidget() == nullptr) {
        return;
    }

    auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
    if (layout == nullptr) {
        return;
    }

    if (layout->indexOf(mapPaneWidget) < 0) {
        layout->addWidget(mapPaneWidget, 1);
    }
}

void MapEditorDetachedPaneWindow::setMapStatus(int zoomPercent, bool insertMode, const QString &modeText)
{
    if (zoomLabel_ != nullptr) {
        zoomLabel_->setText(QObject::tr("%1%").arg(zoomPercent));
        zoomLabel_->setToolTip(QObject::tr("Map zoom"));
    }
    if (modeLabel_ != nullptr) {
        const QString badgeText = insertMode ? QObject::tr("M: Insert") : QObject::tr("M: Select");
        modeLabel_->setText(badgeText);
        modeLabel_->setToolTip(modeText);
        modeLabel_->setStyleSheet(
            TherionStudio::statusBadgeStyleSheet(TherionStudio::mapModeStatusAccentColor(insertMode)));
    }
    refreshCommandBarIconTheme();
}

void MapEditorDetachedPaneWindow::setCloseCallback(std::function<void()> callback)
{
    closeCallback_ = std::move(callback);
}

void MapEditorDetachedPaneWindow::closeEvent(QCloseEvent *event)
{
    if (closeCallback_) {
        closeCallback_();
    }
    QMainWindow::closeEvent(event);
}

void MapEditorDetachedPaneWindow::refreshCommandBarState()
{
    if (mapTab_ == nullptr) {
        return;
    }

    const QSignalBlocker selectBlocker(selectButton_);
    const QSignalBlocker pointBlocker(pointButton_);
    const QSignalBlocker lineBlocker(lineButton_);
    const QSignalBlocker freehandBlocker(freehandLineButton_);
    const QSignalBlocker areaBlocker(areaButton_);
    const QSignalBlocker smartAreaBlocker(smartAreaButton_);
    undoButton_->setEnabled(mapTab_->canUndo());
    redoButton_->setEnabled(mapTab_->canRedo());
    fitBackgroundButton_->setEnabled(mapTab_->backgroundLayerCount() > 0);
    completeDraftButton_->setEnabled(mapTab_->canCompleteDraftAction());
    mapWindowButton_->setToolTip(QObject::tr("Return Map"));
    mapWindowButton_->setAccessibleName(QObject::tr("Return Map"));

    const MapEditorTab::InteractiveDrawMode drawMode = mapTab_->interactiveDrawMode();
    selectButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::None);
    pointButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Point);
    lineButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Line);
    freehandLineButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Freehand);
    areaButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::Area);
    smartAreaButton_->setChecked(drawMode == MapEditorTab::InteractiveDrawMode::SmartArea);
    refreshCommandBarIconTheme();
}

void MapEditorDetachedPaneWindow::refreshCommandBarIconTheme()
{
    if (commandBar_ == nullptr) {
        return;
    }

    const QPalette palette = commandBar_->palette();
    const qreal devicePixelRatio = commandBar_->devicePixelRatioF();
    const QList<QToolButton *> buttons = commandBar_->findChildren<QToolButton *>();
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

void MapEditorDetachedPaneWindow::refreshStatusHint()
{
    if (statusHintLabel_ == nullptr || mapTab_ == nullptr) {
        return;
    }

    const QString text = mapTab_->statusHintText().trimmed();
    statusHintLabel_->setText(text);
    statusHintLabel_->setToolTip(text.isEmpty() ? QString() : QObject::tr("Map editor instructions"));
    statusHintLabel_->setVisible(!text.isEmpty());
}

void MapEditorDetachedPaneWindow::refreshPointerStatus()
{
    if (pointerLabel_ == nullptr || mapTab_ == nullptr) {
        return;
    }

    const bool visible = mapTab_->isPointerStatusEnabled();
    pointerLabel_->setText(visible ? mapTab_->pointerStatusText() : QString());
    if (visible) {
        pointerLabel_->setMinimumWidth(mapTab_->pointerStatusMinimumWidth(pointerLabel_->fontMetrics()));
    }
    pointerLabel_->setVisible(visible);
}
}
