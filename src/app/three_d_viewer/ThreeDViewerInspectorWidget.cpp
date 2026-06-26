#include "ThreeDViewerInspectorWidget.h"

#include "ThreeDViewerInspectorState.h"
#include "ThreeDViewerLayerListModel.h"

#include <QQmlContext>
#include <QQmlError>
#include <QDebug>
#include <QUrl>

namespace TherionStudio
{

ThreeDViewerInspectorWidget::ThreeDViewerInspectorWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setClearColor(palette().window().color());
    updateContextProperties();
    connect(this, &QQuickWidget::statusChanged, this, [this](QQuickWidget::Status status) {
        if (status != QQuickWidget::Error) {
            return;
        }
        for (const QQmlError &error : errors()) {
            qWarning().noquote() << "3D viewer inspector QML error:" << error.toString();
        }
    });
    setSource(QUrl(QStringLiteral("qrc:/resources/qml/three_d_viewer/ThreeDViewerInspector.qml")));
}

void ThreeDViewerInspectorWidget::setInspectorState(ThreeDViewerInspectorState *state)
{
    if (inspectorState_ == state) {
        return;
    }

    inspectorState_ = state;
    updateContextProperties();
}

void ThreeDViewerInspectorWidget::setLayerModel(ThreeDViewerLayerListModel *layerModel)
{
    if (layerModel_ == layerModel) {
        return;
    }

    layerModel_ = layerModel;
    updateContextProperties();
}

void ThreeDViewerInspectorWidget::updateContextProperties()
{
    rootContext()->setContextProperty(QStringLiteral("inspectorState"), inspectorState_);
    rootContext()->setContextProperty(QStringLiteral("layerModel"), layerModel_);
}

} // namespace TherionStudio
