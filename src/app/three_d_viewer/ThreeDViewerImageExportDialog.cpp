#include "ThreeDViewerImageExportDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr int kMinimumImageDimension = 64;
constexpr int kMaximumImageDimension = 8192;

QSize validViewportSize(const QSize &size)
{
    return QSize(std::clamp(size.width(), kMinimumImageDimension, kMaximumImageDimension),
                 std::clamp(size.height(), kMinimumImageDimension, kMaximumImageDimension));
}

int exportHeightForWidth(int width, double aspectRatio)
{
    return std::clamp(int(std::lround(double(width) / aspectRatio)),
                      kMinimumImageDimension,
                      kMaximumImageDimension);
}

int exportWidthForHeight(int height, double aspectRatio)
{
    return std::clamp(int(std::lround(double(height) * aspectRatio)),
                      kMinimumImageDimension,
                      kMaximumImageDimension);
}
} // namespace

ThreeDViewerImageExportDialog::ThreeDViewerImageExportDialog(const QSize &currentViewportSize, QWidget *parent)
    : QDialog(parent)
    , currentViewportSize_(validViewportSize(currentViewportSize))
{
    setWindowTitle(tr("Export 3D Image"));
    aspectRatio_ = double(currentViewportSize_.width()) / double(std::max(1, currentViewportSize_.height()));

    auto *layout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout;
    layout->addLayout(formLayout);

    presetCombo_ = new QComboBox(this);
    presetCombo_->addItem(tr("Current viewport (%1 x %2)")
                              .arg(currentViewportSize_.width())
                              .arg(currentViewportSize_.height()),
                          currentViewportSize_);
    presetCombo_->addItem(tr("1920 px wide"), QSize(1920, 1080));
    presetCombo_->addItem(tr("3840 px wide"), QSize(3840, 2160));
    presetCombo_->addItem(tr("Custom"), QSize());
    formLayout->addRow(tr("Preset"), presetCombo_);

    widthSpin_ = new QSpinBox(this);
    widthSpin_->setRange(kMinimumImageDimension, kMaximumImageDimension);
    widthSpin_->setSuffix(tr(" px"));
    widthSpin_->setValue(currentViewportSize_.width());
    formLayout->addRow(tr("Width"), widthSpin_);

    heightSpin_ = new QSpinBox(this);
    heightSpin_->setRange(kMinimumImageDimension, kMaximumImageDimension);
    heightSpin_->setSuffix(tr(" px"));
    heightSpin_->setValue(currentViewportSize_.height());
    formLayout->addRow(tr("Height"), heightSpin_);

    lockAspectRatioCheck_ = new QCheckBox(tr("Lock aspect ratio"), this);
    lockAspectRatioCheck_->setChecked(true);
    formLayout->addRow(QString(), lockAspectRatioCheck_);

    auto *note = new QLabel(tr("The exported PNG uses the current 3D view, visible layers, overlays, and scene background."), this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(presetCombo_, &QComboBox::currentIndexChanged, this, &ThreeDViewerImageExportDialog::applyPreset);
    connect(widthSpin_, &QSpinBox::valueChanged, this, [this] {
        if (syncing_) {
            return;
        }
        if (presetCombo_ != nullptr && presetCombo_->currentIndex() != presetCombo_->count() - 1) {
            const QSignalBlocker blocker(presetCombo_);
            presetCombo_->setCurrentIndex(presetCombo_->count() - 1);
        }
        syncHeightFromWidth();
    });
    connect(heightSpin_, &QSpinBox::valueChanged, this, [this] {
        if (syncing_) {
            return;
        }
        if (presetCombo_ != nullptr && presetCombo_->currentIndex() != presetCombo_->count() - 1) {
            const QSignalBlocker blocker(presetCombo_);
            presetCombo_->setCurrentIndex(presetCombo_->count() - 1);
        }
        syncWidthFromHeight();
    });
    connect(lockAspectRatioCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            syncHeightFromWidth();
        }
    });
}

QSize ThreeDViewerImageExportDialog::exportSize() const
{
    return QSize(widthSpin_ != nullptr ? widthSpin_->value() : currentViewportSize_.width(),
                 heightSpin_ != nullptr ? heightSpin_->value() : currentViewportSize_.height());
}

void ThreeDViewerImageExportDialog::applyPreset(int index)
{
    if (presetCombo_ == nullptr || widthSpin_ == nullptr || heightSpin_ == nullptr || index < 0) {
        return;
    }
    const QSize presetSize = presetCombo_->itemData(index).toSize();
    if (!presetSize.isValid()) {
        return;
    }

    syncing_ = true;
    if (lockAspectRatioCheck_ != nullptr && lockAspectRatioCheck_->isChecked()) {
        widthSpin_->setValue(presetSize.width());
        heightSpin_->setValue(exportHeightForWidth(presetSize.width(), aspectRatio_));
    } else {
        widthSpin_->setValue(presetSize.width());
        heightSpin_->setValue(presetSize.height());
    }
    syncing_ = false;
}

void ThreeDViewerImageExportDialog::syncHeightFromWidth()
{
    if (syncing_ || lockAspectRatioCheck_ == nullptr || !lockAspectRatioCheck_->isChecked() || heightSpin_ == nullptr
        || widthSpin_ == nullptr) {
        return;
    }
    syncing_ = true;
    heightSpin_->setValue(exportHeightForWidth(widthSpin_->value(), aspectRatio_));
    syncing_ = false;
}

void ThreeDViewerImageExportDialog::syncWidthFromHeight()
{
    if (syncing_ || lockAspectRatioCheck_ == nullptr || !lockAspectRatioCheck_->isChecked() || heightSpin_ == nullptr
        || widthSpin_ == nullptr) {
        return;
    }
    syncing_ = true;
    widthSpin_->setValue(exportWidthForHeight(heightSpin_->value(), aspectRatio_));
    syncing_ = false;
}

} // namespace TherionStudio
