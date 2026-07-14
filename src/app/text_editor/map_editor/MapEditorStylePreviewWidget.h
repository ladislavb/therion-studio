#pragma once

#include <QString>
#include <QWidget>

#include "MapEditorObjectStyleCatalog.h"

namespace TherionStudio
{
class MapEditorStylePreviewWidget final : public QWidget
{
public:
    explicit MapEditorStylePreviewWidget(const MapEditorObjectStyleCatalog &styleCatalog,
                                         QWidget *parent = nullptr);

    void setStyleSelection(const QString &commandKind,
                           const QString &rawType,
                           const QString &subtype);
    void clearStyleSelection();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    const MapEditorObjectStyleCatalog &styleCatalog_;
    QString commandKind_;
    QString rawType_;
    QString subtype_;
};
}
