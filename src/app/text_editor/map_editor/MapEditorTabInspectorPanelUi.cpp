#include "MapEditorTab.h"

#include "../DocumentFileInspector.h"
#include "../DocumentInspectorPanel.h"
#include "../InspectorPanel.h"
#include "MapEditorRecentSymbolsLayout.h"
#include "../TextEditorTab.h"
#include "MapEditorStylePreviewWidget.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <initializer_list>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorFormSpacing = 4;
constexpr int kInspectorInlineSpacing = 6;

void configureSelectionEditableCombo(QComboBox *combo, const QString &objectName)
{
    if (combo == nullptr) {
        return;
    }

    combo->setObjectName(objectName);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (QCompleter *completer = combo->completer(); completer != nullptr) {
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
    }
}

void disableAutoDefault(std::initializer_list<QPushButton *> buttons)
{
    for (QPushButton *button : buttons) {
        if (button != nullptr) {
            button->setAutoDefault(false);
        }
    }
}

void setPlainTextEditVisibleLineCount(QPlainTextEdit *edit, int visibleLineCount)
{
    if (edit != nullptr) {
        edit->setFixedHeight(edit->fontMetrics().lineSpacing() * visibleLineCount + 8);
    }
}
}

void MapEditorTab::buildInspectorPanelUi()
{
    auto *inspectorPanel = new DocumentInspectorPanel(mapDetailsSplitter_);
    objectDetailsPanel_ = inspectorPanel;
    objectDetailsPanel_->setObjectName(QStringLiteral("mapObjectDetailsPanel"));
    objectDetailsPanel_->setMinimumWidth(280);
    mapInspectorTabs_ = inspectorPanel->tabs();
    mapInspectorLeftEdge_ = inspectorPanel->leftEdge();

    auto *selectionPanel = inspectorPanel->addScrollTab(tr("Selection"));
    selectionPanel->setMinimumWidth(0);
    auto *selectionLayout = qobject_cast<QVBoxLayout *>(selectionPanel->layout());

    auto createSelectionSection = [selectionPanel](const QString &title,
                                                   QVBoxLayout **contentLayout,
                                                   QLabel **titleLabelOut = nullptr) {
        return InspectorPanel::createSection(selectionPanel, title, contentLayout, titleLabelOut);
    };

    QVBoxLayout *emptySelectionLayout = nullptr;
    QLabel *emptySelectionTitleLabel = nullptr;
    objectDetailsUiState_.objectDetailsEmptySelectionSection_ =
        createSelectionSection(tr("Selection"), &emptySelectionLayout, &emptySelectionTitleLabel);
    if (emptySelectionTitleLabel != nullptr) {
        emptySelectionTitleLabel->setVisible(false);
    }
    objectDetailsUiState_.objectDetailsSelectionLabel_ =
        new QLabel(tr("No map object selected."), objectDetailsUiState_.objectDetailsEmptySelectionSection_);
    objectDetailsUiState_.objectDetailsSelectionLabel_->setWordWrap(true);
    emptySelectionLayout->addWidget(objectDetailsUiState_.objectDetailsSelectionLabel_);
    selectionLayout->addWidget(objectDetailsUiState_.objectDetailsEmptySelectionSection_);

    QVBoxLayout *objectSelectionLayout = nullptr;
    objectDetailsUiState_.objectSelectionSection_ = createSelectionSection(tr("Object"), &objectSelectionLayout, &objectDetailsUiState_.objectSelectionTitleLabel_);

    objectDetailsUiState_.objectDetailsMetadataLabel_ = new QLabel(QStringLiteral("-"), objectDetailsUiState_.objectSelectionSection_);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setTextFormat(Qt::PlainText);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setWordWrap(true);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectDetailsMetadataLabel_);

    objectDetailsUiState_.objectAreaReferenceLabel_ = new QLabel(objectDetailsUiState_.objectSelectionSection_);
    objectDetailsUiState_.objectAreaReferenceLabel_->setTextFormat(Qt::RichText);
    objectDetailsUiState_.objectAreaReferenceLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    objectDetailsUiState_.objectAreaReferenceLabel_->setOpenExternalLinks(false);
    objectDetailsUiState_.objectAreaReferenceLabel_->setWordWrap(true);
    objectDetailsUiState_.objectAreaReferenceLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    objectDetailsUiState_.objectAreaReferenceLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    connect(objectDetailsUiState_.objectAreaReferenceLabel_, &QLabel::linkActivated, this, [this](const QString &link) {
        bool ok = false;
        const int areaLineNumber = link.toInt(&ok);
        if (ok && areaLineNumber > 0) {
            selectMapLine(areaLineNumber);
            syncInspectorObjectSelectionToLine(areaLineNumber);
        }
    });
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectAreaReferenceLabel_);
    objectDetailsUiState_.objectQuickFieldsEditor_ = new QWidget(objectDetailsUiState_.objectSelectionSection_);
    auto *objectQuickForm = new QFormLayout(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectQuickForm->setContentsMargins(0, 0, 0, 0);
    objectQuickForm->setSpacing(kInspectorFormSpacing);
    objectQuickForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    objectQuickForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    objectDetailsUiState_.objectQuickTypeCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickTypeCombo_, QStringLiteral("mapObjectQuickTypeCombo"));
    objectDetailsUiState_.objectQuickSubtypeCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickSubtypeCombo_, QStringLiteral("mapObjectQuickSubtypeCombo"));
    objectDetailsUiState_.objectQuickProjectionCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickProjectionCombo_, QStringLiteral("mapObjectQuickProjectionCombo"));
    objectDetailsUiState_.objectQuickTargetScrapCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickTargetScrapCombo_->setObjectName(QStringLiteral("mapObjectQuickTargetScrapCombo"));
    objectDetailsUiState_.objectQuickTargetScrapCombo_->setEditable(false);
    objectDetailsUiState_.objectQuickTargetScrapCombo_->setInsertPolicy(QComboBox::NoInsert);
    objectDetailsUiState_.objectQuickTargetScrapCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.objectQuickIdentifierEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickNameEditor_ = new QWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickTextEditor_ = new QWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickValueEditor_ = new QWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickNameEditor_->setObjectName(QStringLiteral("mapObjectQuickNameEditor"));
    objectDetailsUiState_.objectQuickTextEditor_->setObjectName(QStringLiteral("mapObjectQuickTextEditor"));
    objectDetailsUiState_.objectQuickValueEditor_->setObjectName(QStringLiteral("mapObjectQuickValueEditor"));
    objectDetailsUiState_.objectQuickNameEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickNameEditor_);
    objectDetailsUiState_.objectQuickTextEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickTextEditor_);
    objectDetailsUiState_.objectQuickValueEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickValueEditor_);
    objectDetailsUiState_.objectQuickIdentifierEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.objectQuickNameEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.objectQuickTextEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.objectQuickValueEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.objectQuickIdentifierLabel_ = new QLabel(tr("ID"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickNameLabel_ = new QLabel(tr("Name (-name)"), objectDetailsUiState_.objectQuickNameEditor_);
    objectDetailsUiState_.objectQuickTextLabel_ = new QLabel(tr("Text (-text)"), objectDetailsUiState_.objectQuickTextEditor_);
    objectDetailsUiState_.objectQuickValueLabel_ = new QLabel(tr("Value (-value)"), objectDetailsUiState_.objectQuickValueEditor_);
    objectDetailsUiState_.objectQuickProjectionLabel_ = new QLabel(tr("Projection"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickTypeLabel_ = new QLabel(tr("Type"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickSubtypeLabel_ = new QLabel(tr("Subtype"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickRecentLabel_ = new QLabel(tr("Recent"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickTargetScrapLabel_ = new QLabel(tr("Insert into"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreviewLabel_ = new QLabel(tr("Preview"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreview_ = new MapEditorStylePreviewWidget(objectStyleCatalog_, objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreview_->setObjectName(QStringLiteral("mapObjectStylePreview"));
    objectDetailsUiState_.objectStylePreview_->clearStyleSelection();
    connect(objectDetailsUiState_.objectQuickIdentifierEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickNameEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickTextEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickValueEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickTypeCombo_, qOverload<int>(&QComboBox::activated), this, [this]() {
        updateObjectQuickSubtypeChoices();
        applyObjectQuickFieldEdits();
    });
    connect(objectDetailsUiState_.objectQuickSubtypeCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickProjectionCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyScrapProjectionEdit);
    connect(objectDetailsUiState_.objectQuickTargetScrapCombo_, &QComboBox::textActivated, this, [this](const QString &identifier) {
        setPendingInsertTargetScrapIdentifier(identifier);
        refreshObjectDetailsPanel();
    });
    if (objectDetailsUiState_.objectQuickTypeCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickTypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
            updateObjectQuickSubtypeChoices();
            applyObjectQuickFieldEdits();
        });
    }
    if (objectDetailsUiState_.objectQuickSubtypeCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickSubtypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    }
    if (objectDetailsUiState_.objectQuickProjectionCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickProjectionCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyScrapProjectionEdit);
    }
    auto configureQuickOptionRow = [](QWidget *row, QLabel *label, QWidget *field) {
        auto *layout = new QFormLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(kInspectorFormSpacing);
        layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        layout->setRowWrapPolicy(QFormLayout::DontWrapRows);
        layout->addRow(label, field);
    };
    configureQuickOptionRow(objectDetailsUiState_.objectQuickNameEditor_, objectDetailsUiState_.objectQuickNameLabel_, objectDetailsUiState_.objectQuickNameEdit_);
    configureQuickOptionRow(objectDetailsUiState_.objectQuickTextEditor_, objectDetailsUiState_.objectQuickTextLabel_, objectDetailsUiState_.objectQuickTextEdit_);
    configureQuickOptionRow(objectDetailsUiState_.objectQuickValueEditor_, objectDetailsUiState_.objectQuickValueLabel_, objectDetailsUiState_.objectQuickValueEdit_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickTargetScrapLabel_, objectDetailsUiState_.objectQuickTargetScrapCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickIdentifierLabel_, objectDetailsUiState_.objectQuickIdentifierEdit_);
    objectDetailsUiState_.objectQuickProjectionEditor_ = new QWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    auto *objectQuickProjectionLayout = new QHBoxLayout(objectDetailsUiState_.objectQuickProjectionEditor_);
    objectQuickProjectionLayout->setContentsMargins(0, 0, 0, 0); objectQuickProjectionLayout->setSpacing(0);
    objectQuickProjectionLayout->addWidget(objectDetailsUiState_.objectQuickProjectionCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickProjectionLabel_, objectDetailsUiState_.objectQuickProjectionEditor_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickTypeLabel_, objectDetailsUiState_.objectQuickTypeCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickSubtypeLabel_, objectDetailsUiState_.objectQuickSubtypeCombo_);
    objectDetailsUiState_.objectQuickRecentEditor_ = new QWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickRecentEditor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *objectQuickRecentLayout = new MapEditorRecentSymbolsLayout(objectDetailsUiState_.objectQuickRecentEditor_);
    objectQuickRecentLayout->setContentsMargins(0, 0, 0, 0); objectQuickRecentLayout->setSpacing(4);
    for (int index = 0; index < 6; ++index) {
        auto *recentButton = new QPushButton(objectDetailsUiState_.objectQuickRecentEditor_);
        recentButton->setObjectName(QStringLiteral("mapObjectQuickRecentSymbolButton%1").arg(index));
        QFont recentFont = recentButton->font();
        if (recentFont.pointSizeF() > 0.0) { recentFont.setPointSizeF(recentFont.pointSizeF() * 0.85); recentButton->setFont(recentFont); }
        recentButton->setMaximumWidth(160); recentButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed); recentButton->setStyleSheet(QStringLiteral("QPushButton { padding: 1px 6px; min-height: 0; }"));
        recentButton->setVisible(false); disableAutoDefault({recentButton});
        connect(recentButton, &QPushButton::clicked, this, [this, index]() { applyRecentPendingInsertQuickFields(index); });
        objectDetailsUiState_.objectQuickRecentButtons_.append(recentButton); objectQuickRecentLayout->addWidget(recentButton);
    }
    objectQuickForm->addRow(objectDetailsUiState_.objectStylePreviewLabel_, objectDetailsUiState_.objectStylePreview_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickRecentLabel_, objectDetailsUiState_.objectQuickRecentEditor_);
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.objectSelectionSection_);
    QVBoxLayout *geometrySelectionLayout = nullptr;
    objectDetailsUiState_.geometrySelectionSection_ = createSelectionSection(tr("Options"), &geometrySelectionLayout);
    objectDetailsUiState_.lineOptionsEditor_ = new QWidget(objectDetailsUiState_.geometrySelectionSection_);
    objectDetailsUiState_.lineOptionsEditor_->setObjectName(QStringLiteral("mapLineOptionsEditor"));
    auto *lineOptionsLayout = new QVBoxLayout(objectDetailsUiState_.lineOptionsEditor_);
    lineOptionsLayout->setContentsMargins(0, 0, 0, 0);
    lineOptionsLayout->setSpacing(kInspectorFormSpacing);
    objectDetailsUiState_.lineClosedCheck_ = new QCheckBox(tr("Closed (-close)"), objectDetailsUiState_.lineOptionsEditor_);
    objectDetailsUiState_.lineReversedCheck_ = new QCheckBox(tr("Reversed (-reverse)"), objectDetailsUiState_.lineOptionsEditor_);
    objectDetailsUiState_.objectClipDisabledCheck_ = new QCheckBox(tr("Disable clipping (-clip off)"), objectDetailsUiState_.lineOptionsEditor_);
    objectDetailsUiState_.objectClipDisabledCheck_->setObjectName(QStringLiteral("mapObjectClipDisabledCheck"));
    connect(objectDetailsUiState_.lineClosedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineClosedToggled);
    connect(objectDetailsUiState_.lineReversedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineReversedToggled);
    connect(objectDetailsUiState_.objectClipDisabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleObjectClipDisabledToggled);
    lineOptionsLayout->addWidget(objectDetailsUiState_.lineClosedCheck_);
    lineOptionsLayout->addWidget(objectDetailsUiState_.lineReversedCheck_);
    lineOptionsLayout->addWidget(objectDetailsUiState_.objectClipDisabledCheck_);
    geometrySelectionLayout->addWidget(objectDetailsUiState_.lineOptionsEditor_);

    objectDetailsUiState_.scrapOptionsEditor_ = new QWidget(objectDetailsUiState_.geometrySelectionSection_);
    objectDetailsUiState_.scrapOptionsEditor_->setObjectName(QStringLiteral("mapScrapOptionsEditor"));
    auto *scrapOptionsLayout = new QVBoxLayout(objectDetailsUiState_.scrapOptionsEditor_);
    scrapOptionsLayout->setContentsMargins(0, 0, 0, 0);
    scrapOptionsLayout->setSpacing(kInspectorFormSpacing);
    objectDetailsUiState_.scrapProjectionEditor_ = new QWidget(objectDetailsUiState_.scrapOptionsEditor_);
    objectDetailsUiState_.scrapProjectionEditor_->setObjectName(QStringLiteral("mapScrapProjectionEditor"));
    auto *scrapProjectionLayout = new QHBoxLayout(objectDetailsUiState_.scrapProjectionEditor_);
    scrapProjectionLayout->setContentsMargins(0, 0, 0, 0);
    scrapProjectionLayout->setSpacing(kInspectorInlineSpacing);
    objectDetailsUiState_.scrapProjectionLabel_ = new QLabel(tr("Projection"), objectDetailsUiState_.scrapProjectionEditor_);
    scrapProjectionLayout->addWidget(objectDetailsUiState_.scrapProjectionLabel_);
    scrapProjectionLayout->addWidget(objectDetailsUiState_.objectQuickProjectionCombo_, 1);
    scrapOptionsLayout->addWidget(objectDetailsUiState_.scrapProjectionEditor_);
    geometrySelectionLayout->addWidget(objectDetailsUiState_.scrapOptionsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.geometrySelectionSection_);

    QVBoxLayout *vertexSelectionLayout = nullptr;
    objectDetailsUiState_.vertexSelectionSection_ = createSelectionSection(tr("Geometry"), &vertexSelectionLayout, &objectDetailsUiState_.vertexSelectionTitleLabel_);
    objectDetailsUiState_.vertexSelectionSection_->setObjectName(QStringLiteral("mapVertexGeometrySection"));

    objectDetailsUiState_.objectOrientationEditor_ = new QWidget(objectDetailsUiState_.vertexSelectionSection_);
    objectDetailsUiState_.objectOrientationEditor_->setObjectName(QStringLiteral("mapPointOptionsEditor"));
    auto *orientationLayout = new QVBoxLayout(objectDetailsUiState_.objectOrientationEditor_);
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(kInspectorFormSpacing);
    objectDetailsUiState_.objectOrientationEnabledCheck_ = new QCheckBox(tr("Orientation override (-orientation)"), objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.objectOrientationSpin_ = new QDoubleSpinBox(objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.objectOrientationSpin_->setDecimals(3);
    objectDetailsUiState_.objectOrientationSpin_->setRange(0.0, 359.999);
    objectDetailsUiState_.objectOrientationSpin_->setSingleStep(1.0);
    objectDetailsUiState_.objectOrientationSpin_->setSuffix(tr(" deg"));
    objectDetailsUiState_.objectOrientationSpin_->setKeyboardTracking(false);
    auto *linePointControlEditor = new QWidget(objectDetailsUiState_.objectOrientationEditor_);
    auto *linePointControlLayout = new QHBoxLayout(linePointControlEditor);
    linePointControlLayout->setContentsMargins(0, 0, 0, 0);
    linePointControlLayout->setSpacing(8);
    objectDetailsUiState_.linePointPreviousControlCheck_ = new QCheckBox(tr("<<"), linePointControlEditor);
    objectDetailsUiState_.linePointSmoothCheck_ = new QCheckBox(tr("Smooth (-smooth)"), linePointControlEditor);
    objectDetailsUiState_.linePointNextControlCheck_ = new QCheckBox(tr(">>"), linePointControlEditor);
    objectDetailsUiState_.linePointLeftSizeEnabledCheck_ = new QCheckBox(tr("Left size (-l-size)"), objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.linePointLeftSizeSpin_ = new QDoubleSpinBox(objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.linePointLeftSizeSpin_->setDecimals(1);
    objectDetailsUiState_.linePointLeftSizeSpin_->setRange(0.1, 100000.0);
    objectDetailsUiState_.linePointLeftSizeSpin_->setSingleStep(1.0);
    objectDetailsUiState_.linePointLeftSizeSpin_->setKeyboardTracking(false);
    auto *linePointSubtypeEditor = new QWidget(objectDetailsUiState_.objectOrientationEditor_);
    auto *linePointSubtypeLayout = new QHBoxLayout(linePointSubtypeEditor);
    linePointSubtypeLayout->setContentsMargins(0, 0, 0, 0);
    linePointSubtypeLayout->setSpacing(kInspectorInlineSpacing);
    objectDetailsUiState_.linePointSegmentSubtypeLabel_ = new QLabel(tr("Subtype"), linePointSubtypeEditor);
    objectDetailsUiState_.linePointSegmentSubtypeCombo_ = new QComboBox(linePointSubtypeEditor);
    configureSelectionEditableCombo(objectDetailsUiState_.linePointSegmentSubtypeCombo_, QStringLiteral("linePointSegmentSubtypeCombo"));
    objectDetailsUiState_.linePointSegmentSubtypeCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.linePointAltitudeAutoCheck_ = new QCheckBox(tr("Altitude (auto)"), objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.pointAlignEditor_ = new QWidget(objectDetailsUiState_.objectOrientationEditor_);
    auto *pointAlignLayout = new QHBoxLayout(objectDetailsUiState_.pointAlignEditor_);
    pointAlignLayout->setContentsMargins(0, 0, 0, 0);
    pointAlignLayout->setSpacing(kInspectorInlineSpacing);
    objectDetailsUiState_.pointAlignLabel_ = new QLabel(tr("Align (-align)"), objectDetailsUiState_.pointAlignEditor_);
    objectDetailsUiState_.pointAlignCombo_ = new QComboBox(objectDetailsUiState_.pointAlignEditor_);
    objectDetailsUiState_.pointAlignCombo_->setObjectName(QStringLiteral("pointAlignCombo"));
    objectDetailsUiState_.pointAlignCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    objectDetailsUiState_.pointAlignCombo_->addItem(tr("Default"), QString());
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("center"), QStringLiteral("center"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("top"), QStringLiteral("top"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("bottom"), QStringLiteral("bottom"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("left"), QStringLiteral("left"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("right"), QStringLiteral("right"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("top-left"), QStringLiteral("top-left"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("top-right"), QStringLiteral("top-right"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("bottom-left"), QStringLiteral("bottom-left"));
    objectDetailsUiState_.pointAlignCombo_->addItem(QStringLiteral("bottom-right"), QStringLiteral("bottom-right"));
    objectDetailsUiState_.linePointFlagsEditor_ = new QWidget(objectDetailsUiState_.objectOrientationEditor_);
    auto *linePointFlagsLayout = new QVBoxLayout(objectDetailsUiState_.linePointFlagsEditor_);
    linePointFlagsLayout->setContentsMargins(0, 0, 0, 0);
    linePointFlagsLayout->setSpacing(4);
    auto *linePointFlagsLabel = new QLabel(tr("Additional line-point options"), objectDetailsUiState_.linePointFlagsEditor_);
    objectDetailsUiState_.linePointFlagsEdit_ = new QPlainTextEdit(objectDetailsUiState_.linePointFlagsEditor_);
    objectDetailsUiState_.linePointFlagsEdit_->setObjectName(QStringLiteral("linePointFlagsEdit"));
    objectDetailsUiState_.linePointFlagsEdit_->setTabChangesFocus(true);
    setPlainTextEditVisibleLineCount(objectDetailsUiState_.linePointFlagsEdit_, 3);
    objectDetailsUiState_.linePointFlagsEdit_->installEventFilter(this);
    connect(objectDetailsUiState_.linePointFlagsEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        if (!objectDetailsUiState_.updatingObjectDetailsUi_) {
            objectDetailsUiState_.linePointFlagsDirty_ = true;
        }
    });
    linePointFlagsLayout->addWidget(linePointFlagsLabel);
    linePointFlagsLayout->addWidget(objectDetailsUiState_.linePointFlagsEdit_);
    connect(objectDetailsUiState_.objectOrientationEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleObjectOrientationEnabledToggled);
    connect(objectDetailsUiState_.objectOrientationSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleObjectOrientationValueChanged);
    connect(objectDetailsUiState_.linePointPreviousControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointPreviousControlToggled);
    connect(objectDetailsUiState_.linePointSmoothCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointSmoothToggled);
    connect(objectDetailsUiState_.linePointNextControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointNextControlToggled);
    connect(objectDetailsUiState_.linePointLeftSizeEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointLeftSizeEnabledToggled);
    connect(objectDetailsUiState_.linePointLeftSizeSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleLinePointLeftSizeValueChanged);
    connect(objectDetailsUiState_.linePointSegmentSubtypeCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::handleLinePointSegmentSubtypeChanged);
    if (objectDetailsUiState_.linePointSegmentSubtypeCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.linePointSegmentSubtypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::handleLinePointSegmentSubtypeChanged);
    }
    connect(objectDetailsUiState_.linePointAltitudeAutoCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointAltitudeAutoToggled);
    connect(objectDetailsUiState_.pointAlignCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::handlePointAlignChanged);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointPreviousControlCheck_);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointSmoothCheck_);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointNextControlCheck_);
    linePointControlLayout->addStretch(1);
    linePointSubtypeLayout->addWidget(objectDetailsUiState_.linePointSegmentSubtypeLabel_);
    linePointSubtypeLayout->addWidget(objectDetailsUiState_.linePointSegmentSubtypeCombo_, 1);
    pointAlignLayout->addWidget(objectDetailsUiState_.pointAlignLabel_);
    pointAlignLayout->addWidget(objectDetailsUiState_.pointAlignCombo_, 1);
    orientationLayout->addWidget(linePointControlEditor);
    orientationLayout->addWidget(objectDetailsUiState_.objectOrientationEnabledCheck_);
    orientationLayout->addWidget(objectDetailsUiState_.objectOrientationSpin_);
    orientationLayout->addWidget(objectDetailsUiState_.linePointLeftSizeEnabledCheck_);
    orientationLayout->addWidget(objectDetailsUiState_.linePointLeftSizeSpin_);
    orientationLayout->addWidget(linePointSubtypeEditor);
    orientationLayout->addWidget(objectDetailsUiState_.linePointAltitudeAutoCheck_);
    orientationLayout->addWidget(objectDetailsUiState_.pointAlignEditor_);
    orientationLayout->addWidget(objectDetailsUiState_.linePointFlagsEditor_);
    vertexSelectionLayout->addWidget(objectDetailsUiState_.objectOrientationEditor_);

    QVBoxLayout *linePointActionsLayout = nullptr;
    objectDetailsUiState_.linePointActionsSection_ = createSelectionSection(tr("Line Point Actions"), &linePointActionsLayout);
    objectDetailsUiState_.vertexActionsEditor_ = new QWidget(objectDetailsUiState_.linePointActionsSection_);
    auto *vertexActionsLayout = new QGridLayout(objectDetailsUiState_.vertexActionsEditor_);
    vertexActionsLayout->setContentsMargins(0, 0, 0, 0);
    vertexActionsLayout->setSpacing(kInspectorFormSpacing);
    objectDetailsUiState_.vertexInsertBeforeButton_ = new QPushButton(tr("Insert Before"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexInsertAfterButton_ = new QPushButton(tr("Insert After"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexDeleteButton_ = new QPushButton(tr("Delete Point"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexSplitButton_ = new QPushButton(tr("Split Here"), objectDetailsUiState_.vertexActionsEditor_);
    disableAutoDefault({objectDetailsUiState_.vertexInsertBeforeButton_, objectDetailsUiState_.vertexInsertAfterButton_, objectDetailsUiState_.vertexDeleteButton_, objectDetailsUiState_.vertexSplitButton_});
    connect(objectDetailsUiState_.vertexInsertBeforeButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexBeforeFromSelectionPanel);
    connect(objectDetailsUiState_.vertexInsertAfterButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexAfterFromSelectionPanel);
    connect(objectDetailsUiState_.vertexSplitButton_, &QPushButton::clicked, this, &MapEditorTab::splitLineFromSelectionPanel);
    connect(objectDetailsUiState_.vertexDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteVertexFromSelectionPanel);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexInsertBeforeButton_, 0, 0);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexInsertAfterButton_, 0, 1);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexDeleteButton_, 1, 0);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexSplitButton_, 1, 1);
    linePointActionsLayout->addWidget(objectDetailsUiState_.vertexActionsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.vertexSelectionSection_);
    selectionLayout->addWidget(objectDetailsUiState_.linePointActionsSection_);

    QVBoxLayout *scrapScaleLayout = nullptr;
    objectDetailsUiState_.scrapScaleEditor_ = createSelectionSection(tr("Scrap Scale"), &scrapScaleLayout);

    auto createScrapScaleSpin = [](QWidget *parent, int decimals) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setDecimals(decimals);
        spin->setRange(-10000000.0, 10000000.0);
        spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setKeyboardTracking(false);
        spin->setMinimumWidth(72);
        return spin;
    };
    auto createScrapScalePointBlock = [](QWidget *parent,
                                         const QString &title,
                                         QDoubleSpinBox *xSpin,
                                         QDoubleSpinBox *ySpin) {
        auto *block = new QWidget(parent);
        auto *blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0, 0, 0, 0);
        blockLayout->setSpacing(3);
        auto *titleLabel = new QLabel(title, block);
        titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        blockLayout->addWidget(titleLabel);
        auto *row = new QWidget(block);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(kInspectorFormSpacing);
        rowLayout->addWidget(new QLabel(QStringLiteral("X"), row));
        rowLayout->addWidget(xSpin, 1);
        rowLayout->addWidget(new QLabel(QStringLiteral("Y"), row));
        rowLayout->addWidget(ySpin, 1);
        blockLayout->addWidget(row);
        return block;
    };

    objectDetailsUiState_.scrapScaleSourceX1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceY1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceX2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceY2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleRealX1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealY1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealX2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealY2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Picture point 1 (px)"), objectDetailsUiState_.scrapScaleSourceX1Spin_, objectDetailsUiState_.scrapScaleSourceY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Picture point 2 (px)"), objectDetailsUiState_.scrapScaleSourceX2Spin_, objectDetailsUiState_.scrapScaleSourceY2Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Real point 1"), objectDetailsUiState_.scrapScaleRealX1Spin_, objectDetailsUiState_.scrapScaleRealY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Real point 2"), objectDetailsUiState_.scrapScaleRealX2Spin_, objectDetailsUiState_.scrapScaleRealY2Spin_));

    auto *scrapScaleUnitRow = new QWidget(objectDetailsUiState_.scrapScaleEditor_);
    auto *scrapScaleUnitLayout = new QHBoxLayout(scrapScaleUnitRow);
    scrapScaleUnitLayout->setContentsMargins(0, 0, 0, 0);
    scrapScaleUnitLayout->setSpacing(kInspectorInlineSpacing);
    scrapScaleUnitLayout->addWidget(new QLabel(tr("Unit"), scrapScaleUnitRow));
    objectDetailsUiState_.scrapScaleUnitCombo_ = new QComboBox(objectDetailsUiState_.scrapScaleEditor_);
    objectDetailsUiState_.scrapScaleUnitCombo_->addItems({QStringLiteral("m"), QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("ft"), QStringLiteral("in")});
    scrapScaleUnitLayout->addWidget(objectDetailsUiState_.scrapScaleUnitCombo_, 1);
    scrapScaleLayout->addWidget(scrapScaleUnitRow);

    auto *scrapScaleButtons = new QHBoxLayout;
    scrapScaleButtons->setContentsMargins(0, 0, 0, 0);
    objectDetailsUiState_.scrapScaleUseBoundsButton_ = new QPushButton(tr("Use Bounds"), objectDetailsUiState_.scrapScaleEditor_);
    objectDetailsUiState_.scrapScaleUseBoundsButton_->setToolTip(tr("Use current source bounds as the XTherion default scrap scale."));
    objectDetailsUiState_.scrapScaleApplyButton_ = new QPushButton(tr("Apply Scale"), objectDetailsUiState_.scrapScaleEditor_);
    disableAutoDefault({objectDetailsUiState_.scrapScaleUseBoundsButton_, objectDetailsUiState_.scrapScaleApplyButton_});
    connect(objectDetailsUiState_.scrapScaleUseBoundsButton_, &QPushButton::clicked, this, &MapEditorTab::populateScrapScaleFromSourceBounds);
    connect(objectDetailsUiState_.scrapScaleApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyScrapScaleEdits);
    scrapScaleButtons->addWidget(objectDetailsUiState_.scrapScaleUseBoundsButton_);
    scrapScaleButtons->addWidget(objectDetailsUiState_.scrapScaleApplyButton_);
    scrapScaleLayout->addLayout(scrapScaleButtons);
    selectionLayout->addWidget(objectDetailsUiState_.scrapScaleEditor_);

    QVBoxLayout *advancedSelectionLayout = nullptr;
    objectDetailsUiState_.advancedSelectionSection_ = createSelectionSection(tr("Object Actions"), &advancedSelectionLayout);
    objectDetailsUiState_.objectConfigureButtonRow_ = new QWidget(objectDetailsUiState_.advancedSelectionSection_);
    objectDetailsUiState_.objectConfigureButtonRow_->setObjectName(QStringLiteral("mapObjectConfigureButtonRow"));
    auto *objectConfigureButtonRowLayout = new QVBoxLayout(objectDetailsUiState_.objectConfigureButtonRow_);
    objectConfigureButtonRowLayout->setContentsMargins(0, 8, 0, 0);
    objectConfigureButtonRowLayout->setSpacing(0);
    objectDetailsUiState_.objectConfigureButton_ = new QPushButton(tr("Edit All Object Settings..."), objectDetailsUiState_.objectConfigureButtonRow_);
    objectDetailsUiState_.objectConfigureButton_->setObjectName(QStringLiteral("mapObjectConfigureButton"));
    connect(objectDetailsUiState_.objectConfigureButton_, &QPushButton::clicked, this, &MapEditorTab::handleConfigureObjectSettingsTriggered);
    objectConfigureButtonRowLayout->addWidget(objectDetailsUiState_.objectConfigureButton_);
    objectDetailsUiState_.objectDeleteButton_ = new QPushButton(tr("Delete Object"), objectDetailsUiState_.advancedSelectionSection_);
    disableAutoDefault({objectDetailsUiState_.objectConfigureButton_, objectDetailsUiState_.objectDeleteButton_});
    connect(objectDetailsUiState_.objectDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteSelectedObjectFromSelection);
    advancedSelectionLayout->addWidget(objectDetailsUiState_.objectDeleteButton_);
    objectDetailsUiState_.advancedSelectionSection_->setObjectName(QStringLiteral("mapObjectActionsSection"));
    selectionLayout->addWidget(objectDetailsUiState_.advancedSelectionSection_);
    selectionLayout->addStretch(1);

    auto *objectsTab = inspectorPanel->addPlainTab(tr("Objects"));
    auto *objectsLayout = qobject_cast<QVBoxLayout *>(objectsTab->layout());

    mapObjectsAutoCollapseExpandScrapsCheck_ = new QCheckBox(tr("Auto-collapse/expand Scraps"), objectsTab);
    mapObjectsAutoCollapseExpandScrapsCheck_->setObjectName(QStringLiteral("mapObjectsAutoCollapseExpandScrapsCheck"));
    mapObjectsAutoCollapseExpandScrapsCheck_->setToolTip(
        tr("Keep only the selected object's scrap expanded in the Objects inspector."));
    mapObjectsAutoCollapseExpandScrapsCheck_->setChecked(objectsInspectorAutoCollapseExpandScrapsEnabled_);
    connect(mapObjectsAutoCollapseExpandScrapsCheck_,
            &QCheckBox::toggled,
            this,
            &MapEditorTab::setObjectsInspectorAutoCollapseExpandScrapsEnabled);
    objectsLayout->addWidget(mapObjectsAutoCollapseExpandScrapsCheck_);

    mapObjectsTree_ = new QTreeView(objectsTab);
    mapObjectsTree_->setObjectName(QStringLiteral("mapObjectsTree"));
    mapObjectsTree_->setRootIsDecorated(true);
    mapObjectsTree_->setAnimated(true);
    mapObjectsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapObjectsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapObjectsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapObjectsTree_->setAlternatingRowColors(true);
    mapObjectsTree_->setHeaderHidden(true);
    mapObjectsTree_->setIconSize(QSize(16, 16));
    mapObjectsModel_ = new QStandardItemModel(mapObjectsTree_);
    mapObjectsTree_->setModel(mapObjectsModel_);
    configureInspectorObjectTreeColumns();
    if (mapObjectsTree_->viewport() != nullptr) {
        mapObjectsTree_->viewport()->installEventFilter(this);
    }
    connect(mapObjectsTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorObjectClicked);
    objectsLayout->addWidget(mapObjectsTree_, 1);
    if (mapObjectsTree_->selectionModel() != nullptr) {
        connect(mapObjectsTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorObjectSelectionChanged(current);
        });
    }

    buildInspectorBackgroundTab(inspectorPanel);
    if (mapInspectorTabs_ != nullptr) {
        connect(mapInspectorTabs_, &QTabWidget::currentChanged, this, [this](int index) {
            if (index == 0) {
                refreshObjectDetailsPanel();
            }
            refreshBackgroundPivotMarkerVisibility();
        });
    }

    DocumentFileInspectorContext fileContext;
    fileContext.filePath = [this]() {
        return textEditor_ != nullptr ? textEditor_->filePath() : QString();
    };
    fileContext.encodingName = [this]() {
        return textEditor_ != nullptr ? textEditor_->fileEncodingName() : QStringLiteral("UTF-8");
    };
    fileContext.encodingLabel = [this]() {
        return textEditor_ != nullptr ? textEditor_->fileEncodingLabel() : QStringLiteral("UTF-8");
    };
    fileContext.convertToUtf8 = [this]() {
        if (textEditor_ != nullptr) {
            textEditor_->triggerConvertToUtf8();
        }
    };
    mapFileInspector_ = inspectorPanel->addFileTab(std::move(fileContext));

    updateMapInspectorLeftEdgeGeometry();

}

} // namespace TherionStudio
