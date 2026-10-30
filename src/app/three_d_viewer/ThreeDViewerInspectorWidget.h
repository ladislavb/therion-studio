#pragma once

#include <QQuickWidget>

class QUrl;

namespace TherionStudio
{

class ThreeDViewerInspectorState;
class ThreeDViewerLayerListModel;

class ThreeDViewerInspectorWidget final : public QQuickWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerInspectorWidget(QWidget *parent = nullptr);
    ThreeDViewerInspectorWidget(ThreeDViewerInspectorState *state,
                                ThreeDViewerLayerListModel *layerModel,
                                QWidget *parent = nullptr);

    void setInspectorState(ThreeDViewerInspectorState *state);
    void setLayerModel(ThreeDViewerLayerListModel *layerModel);

private:
    void initialize();
    void updateContextProperties();

    ThreeDViewerInspectorState *inspectorState_ = nullptr;
    ThreeDViewerLayerListModel *layerModel_ = nullptr;
};

} // namespace TherionStudio
