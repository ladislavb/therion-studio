#pragma once

#include "MapEditorInteractiveDrawContext.h"

#include <QPointF>
#include <QString>

namespace TherionStudio
{
class MapEditorInteractiveDrawController final
{
public:
    explicit MapEditorInteractiveDrawController(MapEditorInteractiveDrawContext context);

    void setInteractiveDrawMode(MapEditorInteractiveDrawMode mode);
    bool handleInteractiveDrawClick(const QPointF &scenePosition);
    bool commitInteractiveDrawSession(bool closeLineDraft = false);
    void clearInteractiveDrawSession(bool clearMode);
    void updateInteractiveDrawPreview();
    bool cancelInteractiveDrawingToSelectMode();

private:
    QString tr(const char *text) const;
    MapEditorInteractiveDrawMode mode() const;
    void setMode(MapEditorInteractiveDrawMode mode);
    bool canPlanDraftInsertionForMode(MapEditorInteractiveDrawMode mode, QString *errorMessage) const;

    MapEditorInteractiveDrawContext context_;
};
}
