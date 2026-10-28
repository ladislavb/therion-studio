#pragma once

#include <QMainWindow>
#include <QPointer>

#include <functional>

class QLabel;
class QToolButton;
class QWidget;

namespace TherionStudio
{
class MapEditorTab;

class MapEditorDetachedPaneWindow final : public QMainWindow
{
public:
    explicit MapEditorDetachedPaneWindow(MapEditorTab *mapTab, QWidget *parent = nullptr);

    void setMapPaneWidget(QWidget *mapPaneWidget);
    void setMapStatus(int zoomPercent, bool insertMode, const QString &modeText);
    void setCloseCallback(std::function<void()> callback);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void refreshCommandBarState();
    void refreshCommandBarIconTheme();
    void refreshStatusHint();
    void refreshPointerStatus();

    QPointer<MapEditorTab> mapTab_;
    QWidget *commandBar_ = nullptr;
    QToolButton *saveButton_ = nullptr;
    QToolButton *undoButton_ = nullptr;
    QToolButton *redoButton_ = nullptr;
    QToolButton *zoomInButton_ = nullptr;
    QToolButton *zoomOutButton_ = nullptr;
    QToolButton *fitButton_ = nullptr;
    QToolButton *fitBackgroundButton_ = nullptr;
    QToolButton *selectButton_ = nullptr;
    QToolButton *completeDraftButton_ = nullptr;
    QToolButton *insertScrapButton_ = nullptr;
    QToolButton *pointButton_ = nullptr;
    QToolButton *lineButton_ = nullptr;
    QToolButton *freehandLineButton_ = nullptr;
    QToolButton *areaButton_ = nullptr;
    QToolButton *smartAreaButton_ = nullptr;
    QToolButton *mapWindowButton_ = nullptr;
    QLabel *statusHintLabel_ = nullptr;
    QLabel *pointerLabel_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QLabel *modeLabel_ = nullptr;
    std::function<void()> closeCallback_;
};
}
