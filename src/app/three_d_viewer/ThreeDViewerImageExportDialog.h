#pragma once

#include <QDialog>
#include <QSize>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace TherionStudio
{

class ThreeDViewerImageExportDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ThreeDViewerImageExportDialog(const QSize &currentViewportSize, QWidget *parent = nullptr);

    QSize exportSize() const;

private:
    void applyPreset(int index);
    void syncHeightFromWidth();
    void syncWidthFromHeight();

    QSize currentViewportSize_;
    double aspectRatio_ = 1.0;
    QComboBox *presetCombo_ = nullptr;
    QSpinBox *widthSpin_ = nullptr;
    QSpinBox *heightSpin_ = nullptr;
    QCheckBox *lockAspectRatioCheck_ = nullptr;
    bool syncing_ = false;
};

} // namespace TherionStudio
