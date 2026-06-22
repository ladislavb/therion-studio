#include "MapEditorTab.h"

#include "../DocumentInspectorPanel.h"
#include "../InspectorPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace TherionStudio
{

void MapEditorTab::buildInspectorBackgroundTab(DocumentInspectorPanel *inspectorPanel)
{
    auto *backgroundTab = inspectorPanel->addPlainTab(tr("Backgrounds"));
    mapInspectorBackgroundTabIndex_ = inspectorPanel->tabs() != nullptr
        ? inspectorPanel->tabs()->indexOf(backgroundTab)
        : -1;
    auto *backgroundLayout = qobject_cast<QVBoxLayout *>(backgroundTab->layout());
    auto createBackgroundSection = [backgroundTab](const QString &title, QVBoxLayout **contentLayout) {
        return InspectorPanel::createSection(backgroundTab, title, contentLayout);
    };

    auto *layersFrame = new QFrame(backgroundTab);
    layersFrame->setFrameShape(QFrame::StyledPanel);
    auto *layersLayout = new QVBoxLayout(layersFrame);
    layersLayout->setContentsMargins(8, 8, 8, 8);
    layersLayout->setSpacing(6);

    auto *layersRow = new QHBoxLayout;
    auto *layersLabel = new QLabel(tr("Layers"), layersFrame);
    QFont sectionFont = layersLabel->font();
    sectionFont.setBold(true);
    layersLabel->setFont(sectionFont);
    layersRow->addWidget(layersLabel);
    layersRow->addStretch(1);
    mapBackgroundAddButton_ = new QToolButton(layersFrame);
    mapBackgroundAddButton_->setText(QStringLiteral("+"));
    mapBackgroundAddButton_->setToolTip(tr("Add background images"));
    layersRow->addWidget(mapBackgroundAddButton_);
    layersLayout->addLayout(layersRow);

    mapBackgroundLayersTree_ = new QTreeView(layersFrame);
    mapBackgroundLayersTree_->setRootIsDecorated(false);
    mapBackgroundLayersTree_->setAnimated(false);
    mapBackgroundLayersTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapBackgroundLayersTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapBackgroundLayersTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapBackgroundLayersTree_->setAlternatingRowColors(true);
    mapBackgroundLayersTree_->setHeaderHidden(true);
    mapBackgroundLayersTree_->setIconSize(QSize(16, 16));
    mapBackgroundLayersTree_->setMinimumHeight(88);
    mapBackgroundLayersModel_ = new QStandardItemModel(mapBackgroundLayersTree_);
    mapBackgroundLayersTree_->setModel(mapBackgroundLayersModel_);
    configureInspectorBackgroundLayerTreeColumns();
    connect(mapBackgroundLayersTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorBackgroundLayerClicked);
    layersLayout->addWidget(mapBackgroundLayersTree_);

    auto *layerActionsRow = new QHBoxLayout;
    mapBackgroundMoveUpButton_ = new QPushButton(tr("Up"), layersFrame);
    mapBackgroundMoveDownButton_ = new QPushButton(tr("Down"), layersFrame);
    mapBackgroundMoveUpButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundMoveDownButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layerActionsRow->addWidget(mapBackgroundMoveUpButton_);
    layerActionsRow->addWidget(mapBackgroundMoveDownButton_);
    layersLayout->addLayout(layerActionsRow);
    backgroundLayout->addWidget(layersFrame);

    QVBoxLayout *positionLayout = nullptr;
    auto *positionFrame = createBackgroundSection(tr("Position"), &positionLayout);
    auto *xRow = new QHBoxLayout;
    xRow->addWidget(new QLabel(tr("X"), positionFrame));
    mapBackgroundPosXSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosXSpin_->setObjectName(QStringLiteral("mapBackgroundPosXSpin"));
    mapBackgroundPosXSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosXSpin_->setDecimals(1);
    mapBackgroundPosXSpin_->setSingleStep(1.0);
    mapBackgroundPosXSpin_->setKeyboardTracking(false);
    xRow->addWidget(mapBackgroundPosXSpin_, 1);
    positionLayout->addLayout(xRow);

    auto *yRow = new QHBoxLayout;
    yRow->addWidget(new QLabel(tr("Y"), positionFrame));
    mapBackgroundPosYSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosYSpin_->setObjectName(QStringLiteral("mapBackgroundPosYSpin"));
    mapBackgroundPosYSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosYSpin_->setDecimals(1);
    mapBackgroundPosYSpin_->setSingleStep(1.0);
    mapBackgroundPosYSpin_->setKeyboardTracking(false);
    yRow->addWidget(mapBackgroundPosYSpin_, 1);
    positionLayout->addLayout(yRow);
    backgroundLayout->addWidget(positionFrame);

    QVBoxLayout *transformLayout = nullptr;
    auto *transformFrame = createBackgroundSection(tr("Transform"), &transformLayout);
    auto *scaleXRow = new QHBoxLayout;
    scaleXRow->addWidget(new QLabel(tr("Scale X"), transformFrame));
    mapBackgroundScaleXSpin_ = new QDoubleSpinBox(transformFrame);
    mapBackgroundScaleXSpin_->setObjectName(QStringLiteral("mapBackgroundScaleXSpin"));
    mapBackgroundScaleXSpin_->setRange(0.01, 100.0);
    mapBackgroundScaleXSpin_->setDecimals(3);
    mapBackgroundScaleXSpin_->setSingleStep(0.1);
    mapBackgroundScaleXSpin_->setKeyboardTracking(false);
    scaleXRow->addWidget(mapBackgroundScaleXSpin_, 1);
    transformLayout->addLayout(scaleXRow);

    auto *scaleYRow = new QHBoxLayout;
    scaleYRow->addWidget(new QLabel(tr("Scale Y"), transformFrame));
    mapBackgroundScaleYSpin_ = new QDoubleSpinBox(transformFrame);
    mapBackgroundScaleYSpin_->setObjectName(QStringLiteral("mapBackgroundScaleYSpin"));
    mapBackgroundScaleYSpin_->setRange(0.01, 100.0);
    mapBackgroundScaleYSpin_->setDecimals(3);
    mapBackgroundScaleYSpin_->setSingleStep(0.1);
    mapBackgroundScaleYSpin_->setKeyboardTracking(false);
    scaleYRow->addWidget(mapBackgroundScaleYSpin_, 1);
    transformLayout->addLayout(scaleYRow);

    mapBackgroundLockScaleCheck_ = new QCheckBox(tr("Lock proportions"), transformFrame);
    mapBackgroundLockScaleCheck_->setChecked(true);
    transformLayout->addWidget(mapBackgroundLockScaleCheck_);

    auto *rotationRow = new QHBoxLayout;
    rotationRow->addWidget(new QLabel(tr("Rotation"), transformFrame));
    mapBackgroundRotationSpin_ = new QDoubleSpinBox(transformFrame);
    mapBackgroundRotationSpin_->setObjectName(QStringLiteral("mapBackgroundRotationSpin"));
    mapBackgroundRotationSpin_->setRange(-360.0, 360.0);
    mapBackgroundRotationSpin_->setDecimals(1);
    mapBackgroundRotationSpin_->setSingleStep(1.0);
    mapBackgroundRotationSpin_->setSuffix(QStringLiteral("°"));
    mapBackgroundRotationSpin_->setKeyboardTracking(false);
    rotationRow->addWidget(mapBackgroundRotationSpin_, 1);
    transformLayout->addLayout(rotationRow);

    auto *pivotRow = new QHBoxLayout;
    mapBackgroundSetPivotButton_ = new QPushButton(tr("Set Pivot"), transformFrame);
    mapBackgroundResetPivotButton_ = new QPushButton(tr("Reset Pivot"), transformFrame);
    pivotRow->addWidget(mapBackgroundSetPivotButton_);
    pivotRow->addWidget(mapBackgroundResetPivotButton_);
    transformLayout->addLayout(pivotRow);

    backgroundLayout->addWidget(transformFrame);

    QVBoxLayout *adjustmentsLayout = nullptr;
    auto *adjustmentsFrame = createBackgroundSection(tr("Display"), &adjustmentsLayout);
    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacity"), adjustmentsFrame));
    opacityRow->addStretch(1);
    mapBackgroundOpacityResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    opacityRow->addWidget(mapBackgroundOpacityResetButton_);
    adjustmentsLayout->addLayout(opacityRow);
    mapBackgroundOpacitySlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundOpacitySlider_->setRange(5, 100);
    adjustmentsLayout->addWidget(mapBackgroundOpacitySlider_);

    auto *gammaRow = new QHBoxLayout;
    gammaRow->addWidget(new QLabel(tr("Gamma"), adjustmentsFrame));
    gammaRow->addStretch(1);
    mapBackgroundGammaResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    gammaRow->addWidget(mapBackgroundGammaResetButton_);
    adjustmentsLayout->addLayout(gammaRow);
    mapBackgroundGammaSlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundGammaSlider_->setRange(20, 250);
    adjustmentsLayout->addWidget(mapBackgroundGammaSlider_);
    backgroundLayout->addWidget(adjustmentsFrame);
    backgroundLayout->addStretch(1);

    connect(mapBackgroundAddButton_, &QToolButton::clicked, this, [this]() { browseAndAddBackgroundImages(); });
    connect(mapBackgroundMoveUpButton_, &QPushButton::clicked, this, [this]() { moveSelectedBackgroundLayerUp(); });
    connect(mapBackgroundMoveDownButton_, &QPushButton::clicked, this, [this]() { moveSelectedBackgroundLayerDown(); });
    if (mapBackgroundLayersTree_->selectionModel() != nullptr) {
        connect(mapBackgroundLayersTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorBackgroundLayerSelectionChanged(current);
        });
    }
    connect(mapBackgroundPosXSpin_, &QDoubleSpinBox::valueChanged, this, [this](double x) {
        if (!updatingMapInspectorBackgroundUi_ && mapBackgroundPosYSpin_ != nullptr) {
            setSelectedBackgroundLayerPosition(QPointF(x, mapBackgroundPosYSpin_->value()));
        }
    });
    connect(mapBackgroundPosYSpin_, &QDoubleSpinBox::valueChanged, this, [this](double y) {
        if (!updatingMapInspectorBackgroundUi_ && mapBackgroundPosXSpin_ != nullptr) {
            setSelectedBackgroundLayerPosition(QPointF(mapBackgroundPosXSpin_->value(), y));
        }
    });
    connect(mapBackgroundScaleXSpin_, &QDoubleSpinBox::valueChanged, this, [this](double scale) {
        if (!updatingMapInspectorBackgroundUi_) {
            setSelectedBackgroundLayerXScale(scale);
            if (mapBackgroundLockScaleCheck_ != nullptr
                && mapBackgroundLockScaleCheck_->isChecked()
                && mapBackgroundScaleYSpin_ != nullptr
                && !qFuzzyCompare(mapBackgroundScaleYSpin_->value(), scale)) {
                mapBackgroundScaleYSpin_->setValue(scale);
            }
        }
    });
    connect(mapBackgroundScaleYSpin_, &QDoubleSpinBox::valueChanged, this, [this](double scale) {
        if (!updatingMapInspectorBackgroundUi_) {
            setSelectedBackgroundLayerYScale(scale);
            if (mapBackgroundLockScaleCheck_ != nullptr
                && mapBackgroundLockScaleCheck_->isChecked()
                && mapBackgroundScaleXSpin_ != nullptr
                && !qFuzzyCompare(mapBackgroundScaleXSpin_->value(), scale)) {
                mapBackgroundScaleXSpin_->setValue(scale);
            }
        }
    });
    connect(mapBackgroundRotationSpin_, &QDoubleSpinBox::valueChanged, this, [this](double rotationDeg) {
        if (!updatingMapInspectorBackgroundUi_) {
            setSelectedBackgroundLayerRotationDeg(rotationDeg);
        }
    });
    connect(mapBackgroundOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (!updatingMapInspectorBackgroundUi_) {
            setSelectedBackgroundLayerOpacity(static_cast<qreal>(value) / 100.0);
        }
    });
    connect(mapBackgroundGammaSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (!updatingMapInspectorBackgroundUi_) {
            setSelectedBackgroundLayerGamma(static_cast<qreal>(value) / 100.0);
        }
    });
    connect(mapBackgroundOpacityResetButton_, &QPushButton::clicked, this, [this]() { resetSelectedBackgroundLayerOpacity(); });
    connect(mapBackgroundGammaResetButton_, &QPushButton::clicked, this, [this]() { resetSelectedBackgroundLayerGamma(); });
    connect(mapBackgroundSetPivotButton_, &QPushButton::clicked, this, [this]() { beginSetSelectedBackgroundLayerPivot(); });
    connect(mapBackgroundResetPivotButton_, &QPushButton::clicked, this, [this]() { resetSelectedBackgroundLayerPivot(); });
}

} // namespace TherionStudio
