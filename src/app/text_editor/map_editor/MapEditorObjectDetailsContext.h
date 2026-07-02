#pragma once

#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVector>

#include <functional>

#include "MapEditorInspectorData.h"
#include "MapEditorLogicalSourceContext.h"
#include "MapEditorObjectDetailsLogic.h"
#include "../TextEditorSourceTransactionController.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QObject;
class QPushButton;
class QWidget;

namespace TherionStudio
{
class MapEditorStylePreviewWidget;
class TextEditorTab;

struct MapEditorObjectDetailsContext
{
    QObject *callbackContext = nullptr;
    TextEditorTab *textEditor = nullptr;
    const InspectorSymbolCatalog *inspectorSymbolCatalog = nullptr;
    const MapEditorOrientationApplicabilityByCommand *orientationApplicabilityByCommand = nullptr;
    bool *updatingUi = nullptr;
    int *selectedObjectLineNumber = nullptr;
    int *selectedObjectVertexIndex = nullptr;
    QString *selectedObjectKind = nullptr;
    QString *objectQuickCommandKind = nullptr;
    QSet<int> *hiddenInspectorObjectLines = nullptr;
    int *lastInspectorClickedObjectLineNumber = nullptr;
    QString *toolbarStatusNote = nullptr;

    QLabel *selectionLabel = nullptr;
    QWidget *emptySelectionSection = nullptr;
    QWidget *selectionSection = nullptr;
    QLabel *selectionTitleLabel = nullptr;
    QWidget *vertexSection = nullptr;
    QLabel *vertexTitleLabel = nullptr;
    QWidget *linePointActionsSection = nullptr;
    QWidget *geometrySection = nullptr;
    QWidget *advancedSection = nullptr;
    QPushButton *deleteButton = nullptr;
    QLabel *areaReferenceLabel = nullptr;
    QWidget *quickFieldsEditor = nullptr;
    QLabel *quickIdentifierLabel = nullptr;
    QLabel *quickNameLabel = nullptr;
    QLabel *quickTextLabel = nullptr;
    QLabel *quickValueLabel = nullptr;
    QLabel *quickProjectionLabel = nullptr;
    QLabel *quickTypeLabel = nullptr;
    QLabel *quickSubtypeLabel = nullptr;
    QLabel *quickRecentLabel = nullptr;
    QLabel *quickTargetScrapLabel = nullptr;
    QLabel *stylePreviewLabel = nullptr;
    QComboBox *quickTypeCombo = nullptr;
    QComboBox *quickSubtypeCombo = nullptr;
    QComboBox *quickProjectionCombo = nullptr;
    QComboBox *quickTargetScrapCombo = nullptr;
    QLineEdit *quickIdentifierEdit = nullptr;
    QWidget *quickNameEditor = nullptr;
    QWidget *quickTextEditor = nullptr;
    QWidget *quickValueEditor = nullptr;
    QWidget *quickRecentEditor = nullptr;
    QVector<QPushButton *> quickRecentButtons;
    QLineEdit *quickNameEdit = nullptr;
    QLineEdit *quickTextEdit = nullptr;
    QLineEdit *quickValueEdit = nullptr;
    MapEditorStylePreviewWidget *stylePreview = nullptr;
    QWidget *vertexActionsEditor = nullptr;
    QPushButton *vertexInsertBeforeButton = nullptr;
    QPushButton *vertexInsertAfterButton = nullptr;
    QPushButton *vertexDeleteButton = nullptr;
    QPushButton *vertexSplitButton = nullptr;
    QLabel *metadataLabel = nullptr;
    QWidget *orientationEditor = nullptr;
    QCheckBox *linePointPreviousControlCheck = nullptr;
    QCheckBox *linePointSmoothCheck = nullptr;
    QCheckBox *linePointNextControlCheck = nullptr;
    QCheckBox *orientationEnabledCheck = nullptr;
    QDoubleSpinBox *orientationSpin = nullptr;
    QCheckBox *linePointLeftSizeEnabledCheck = nullptr;
    QDoubleSpinBox *linePointLeftSizeSpin = nullptr;
    QLabel *linePointSegmentSubtypeLabel = nullptr;
    QComboBox *linePointSegmentSubtypeCombo = nullptr;
    QCheckBox *linePointAltitudeAutoCheck = nullptr;
    QWidget *linePointFlagsEditor = nullptr;
    QPlainTextEdit *linePointFlagsEdit = nullptr;
    QWidget *lineOptionsEditor = nullptr;
    QWidget *scrapOptionsEditor = nullptr;
    QLabel *scrapProjectionLabel = nullptr;
    QWidget *scrapProjectionEditor = nullptr;
    QWidget *quickProjectionEditor = nullptr;
    QCheckBox *lineClosedCheck = nullptr;
    QCheckBox *lineReversedCheck = nullptr;
    QCheckBox *objectClipDisabledCheck = nullptr;
    QWidget *pointAlignEditor = nullptr;
    QLabel *pointAlignLabel = nullptr;
    QComboBox *pointAlignCombo = nullptr;
    QWidget *scrapScaleEditor = nullptr;
    QDoubleSpinBox *scrapScaleSourceX1Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceY1Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceX2Spin = nullptr;
    QDoubleSpinBox *scrapScaleSourceY2Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealX1Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealY1Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealX2Spin = nullptr;
    QDoubleSpinBox *scrapScaleRealY2Spin = nullptr;
    QComboBox *scrapScaleUnitCombo = nullptr;
    QWidget *configureButtonRow = nullptr;
    QPushButton *configureButton = nullptr;

    std::function<QString(const char *)> translate;
    std::function<QRectF()> mapSourceBoundsForCurrentDocument;
    std::function<QVector<TherionParsedLine>()> parsedLinesForCurrentDocument;
    MapEditorLogicalSourceContext logicalSource;
    std::function<std::optional<InspectorScrapContext>()> pendingInsertTargetScrapContext;
    std::function<void()> refreshToolbarSummary;
    std::function<void()> refreshObjectDetailsPanel;
    std::function<std::optional<InspectorObjectQuickFields>()> pendingInsertQuickFields;
    std::function<void(const InspectorObjectQuickFields &)> setPendingInsertQuickFields;
    std::function<QVector<InspectorObjectQuickFields>(const QString &)> recentPendingInsertQuickFields;
    std::function<bool()> pendingInsertLinePointAvailable;
    std::function<bool()> pendingInsertLinePointSmooth;
    std::function<void(bool)> setPendingInsertLinePointSmooth;
    std::function<bool()> pendingInsertLinePointIncomingControl;
    std::function<bool()> pendingInsertLinePointOutgoingControl;
    std::function<void(bool, bool)> setPendingInsertLinePointControl;
    std::function<QString()> pendingInsertLinePointSegmentSubtype;
    std::function<void(const QString &)> setPendingInsertLinePointSegmentSubtype;
    std::function<bool()> pendingInsertLinePointOrientationEnabled;
    std::function<qreal()> pendingInsertLinePointOrientationDegrees;
    std::function<void(bool, qreal)> setPendingInsertLinePointOrientation;
    std::function<bool()> pendingInsertLinePointLeftSizeEnabled;
    std::function<qreal()> pendingInsertLinePointLeftSize;
    std::function<void(bool, qreal)> setPendingInsertLinePointLeftSize;
    std::function<void()> clearInspectorObjectSelection;
    std::function<void(int, bool)> selectMapLine;
    std::function<void(int)> restorePointSelectionLater;
    std::function<void(int, int)> restoreLineAnchorSelectionLater;
    std::function<TextEditorSourceTransactionResult(const QString &, const QString &, const QString &, int, std::function<void()>)>
        applySourceTextChangeWithSnapshot;
    std::function<bool(bool)> insertLineVertexFromSelection;
    std::function<bool()> splitLineAtSelection;
    std::function<bool()> removeLineVertexFromSelection;
    std::function<bool()> toggleLineVertexSmoothFromSelection;
    std::function<bool(bool)> setLineVertexSmoothForSelection;
    std::function<bool(bool, bool)> setLineVertexControlHandleForSelection;
};
}
