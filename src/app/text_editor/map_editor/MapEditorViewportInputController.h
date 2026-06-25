#pragma once

#include "MapEditorViewportInputContext.h"

#include <QPoint>

#include <optional>

class QEvent;
class QObject;

namespace TherionStudio
{
class MapEditorViewportInputController final
{
public:
    explicit MapEditorViewportInputController(MapEditorViewportInputContext context);

    std::optional<bool> handleEvent(QObject *watched, QEvent *event);
    void showContextMenuAtViewportPosition(const QPoint &viewportPosition, const QPoint &globalPosition);

private:
    MapEditorInteractiveDrawMode drawMode() const;
    QString tr(const char *text) const;

    bool forwardingTabletEventAsMouse_ = false;
    MapEditorViewportInputContext context_;
};
}
