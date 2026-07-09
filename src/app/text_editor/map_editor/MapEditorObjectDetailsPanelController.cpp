#include "MapEditorObjectDetailsPanelController.h"

#include "MapEditorInspectorData.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "MapEditorStylePreviewWidget.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceText.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QWidget>

#include <utility>

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &vertex = lineFeature.lineVertices.at(index);
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex
            || vertex.incomingSourceVertexIndex == sourceVertexIndex
            || vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

QStringList linePointStandaloneOptionRowsForSelection(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    const int lineVertexIndex = lineVertexIndexForSourceVertex(lineFeature, sourceVertexIndex);
    if (lineVertexIndex < 0 || lineVertexIndex >= lineFeature.lineVertices.size()) {
        return {};
    }
    return lineFeature.lineVertices.at(lineVertexIndex).standaloneOptionRows;
}

std::optional<TherionSourceLogicalCommand> logicalCommandForLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                                 int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    for (const TherionSourceLogicalCommand &command : commands) {
        if (command.startLineNumber <= lineNumber && lineNumber <= command.endLineNumber) {
            return command;
        }
    }
    return std::nullopt;
}

std::optional<MapGeometryFeature> lineFeatureForLine(const MapEditorLogicalSourceContext &logicalSource,
                                                     const QVector<TherionSourceLogicalCommand> &commands,
                                                     int lineNumber)
{
    if (lineNumber <= 0) {
        return std::nullopt;
    }

    if (logicalSource.geometryProjectionForCurrentDocument) {
        const Th2GeometryProjection projection = logicalSource.geometryProjectionForCurrentDocument();
        if (const std::optional<MapGeometryFeature> feature = lineFeatureForLineNumber(projection, commands, lineNumber);
            feature.has_value()) {
            return feature;
        }
    }

    return lineFeatureForLineNumber(commands, lineNumber);
}

QVector<MapEditorAreaReference> areaReferencesForBorderLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                            int lineNumber)
{
    if (lineNumber <= 0) {
        return {};
    }

    return mapEditorAreaReferencesForBorderLine(commands, lineNumber);
}

std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsForLine(
    const QVector<TherionSourceLogicalCommand> &commands,
    int lineNumber)
{
    const std::optional<TherionSourceLogicalCommand> command = logicalCommandForLine(commands, lineNumber);
    if (!command.has_value()) {
        return std::nullopt;
    }
    return inspectorObjectQuickFieldsFromLogicalCommand(*command);
}

QString scrapContextMetadataSuffix(const std::optional<InspectorScrapContext> &scrapContext)
{
    if (!scrapContext.has_value() || scrapContext->identifier.trimmed().isEmpty()) {
        return QString();
    }

    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                       " - Scrap %1")
        .arg(scrapContext->identifier.trimmed());
}

QString targetScrapMetadataSuffix(const std::optional<InspectorScrapContext> &scrapContext)
{
    if (!scrapContext.has_value() || scrapContext->identifier.trimmed().isEmpty()) {
        return QString();
    }

    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                       " - Target scrap %1")
        .arg(scrapContext->identifier.trimmed());
}

QString metadataForPendingInsert(const std::optional<InspectorScrapContext> &scrapContext)
{
    if (scrapContext.has_value() && scrapContext->willBeCreated) {
        return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                           "First object will create scrap \"%1\".")
            .arg(scrapContext->identifier.trimmed());
    }

    const QString suffix = targetScrapMetadataSuffix(scrapContext);
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                       "Pending insert%1")
        .arg(suffix);
}

QString metadataLabelStyleSheet(bool notice)
{
    if (notice) {
        return QStringLiteral(
            "QLabel {"
            " color: palette(text);"
            " background-color: palette(alternate-base);"
            " border: 1px solid palette(highlight);"
            " border-radius: 3px;"
            " padding: 6px;"
            "}");
    }

    return QStringLiteral("QLabel { color: palette(midlight); }");
}

QString recentPendingSymbolButtonText(const InspectorObjectQuickFields &fields)
{
    const QString type = fields.type.trimmed();
    const QString subtype = fields.subtype.trimmed();
    return subtype.isEmpty() ? type : QStringLiteral("%1:%2").arg(type, subtype);
}

void updateRecentPendingSymbolButtons(const MapEditorObjectDetailsContext &context,
                                      const QString &commandKind,
                                      bool visible)
{
    const QVector<InspectorObjectQuickFields> recentFields =
        visible && context.recentPendingInsertQuickFields
            ? context.recentPendingInsertQuickFields(commandKind)
            : QVector<InspectorObjectQuickFields>{};
    const bool recentVisible = visible && !recentFields.isEmpty();
    if (context.quickRecentLabel != nullptr) {
        context.quickRecentLabel->setVisible(recentVisible);
    }
    if (context.quickRecentEditor != nullptr) {
        context.quickRecentEditor->setVisible(recentVisible);
    }

    for (int index = 0; index < context.quickRecentButtons.size(); ++index) {
        QPushButton *button = context.quickRecentButtons.at(index);
        if (button == nullptr) {
            continue;
        }

        const bool buttonVisible = index < recentFields.size();
        button->setVisible(buttonVisible);
        button->setEnabled(buttonVisible);
        if (buttonVisible) {
            const QString label = recentPendingSymbolButtonText(recentFields.at(index));
            button->setText(label);
            button->setToolTip(QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                                           "Use recent type/subtype %1")
                                   .arg(label));
        } else {
            button->setText(QString());
            button->setToolTip(QString());
        }
    }
}

QString metadataForSourceLine(const QVector<TherionParsedLine> &parsedLines, int lineNumber)
{
    const std::optional<InspectorScrapContext> scrapContext = inspectorScrapContextForSourceLine(parsedLines,
                                                                                                  lineNumber);
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController",
                                       "Source line %1%2")
        .arg(lineNumber)
        .arg(scrapContextMetadataSuffix(scrapContext));
}

void moveSelectionControlToContainer(QWidget *control, QWidget *container, bool forceReappend = false)
{
    if (control == nullptr || container == nullptr) {
        return;
    }

    if (!forceReappend && control->parentWidget() == container) {
        return;
    }

    if (QWidget *currentParent = control->parentWidget(); currentParent != nullptr) {
        if (QLayout *currentLayout = currentParent->layout(); currentLayout != nullptr) {
            currentLayout->removeWidget(control);
        }
    }

    control->setParent(container);
    if (auto *targetLayout = qobject_cast<QBoxLayout *>(container->layout()); targetLayout != nullptr) {
        targetLayout->addWidget(control);
    }
}

void reorderSelectionControls(QWidget *container, std::initializer_list<QWidget *> orderedControls)
{
    auto *layout = container != nullptr ? qobject_cast<QBoxLayout *>(container->layout()) : nullptr;
    if (layout == nullptr) {
        return;
    }

    int insertIndex = 0;
    for (QWidget *control : orderedControls) {
        if (control == nullptr || control->parentWidget() != container) {
            continue;
        }
        layout->removeWidget(control);
        layout->insertWidget(insertIndex++, control);
    }
}

void setTargetScrapComboValues(QComboBox *combo,
                               const QVector<InspectorScrapContext> &scrapContexts,
                               const std::optional<InspectorScrapContext> &currentContext)
{
    if (combo == nullptr) {
        return;
    }

    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const InspectorScrapContext &scrapContext : scrapContexts) {
        if (!scrapContext.identifier.trimmed().isEmpty()) {
            combo->addItem(scrapContext.identifier.trimmed());
        }
    }

    const QString currentIdentifier = currentContext.has_value()
        ? currentContext->identifier.trimmed()
        : QString();
    int currentIndex = -1;
    for (int index = 0; index < combo->count(); ++index) {
        if (combo->itemText(index).compare(currentIdentifier, Qt::CaseInsensitive) == 0) {
            currentIndex = index;
            break;
        }
    }
    combo->setCurrentIndex(currentIndex);
}

QString singleValueOptionValue(const TherionSourceLogicalCommand &command, const QString &optionName)
{
    const QString normalizedOption = normalizedCommandOptionName(optionName);
    for (const TherionSourceLogicalOptionEntryRange &entry : command.optionEntryRanges) {
        if (normalizedCommandOptionName(entry.key) != normalizedOption) {
            continue;
        }
        if (!entry.valueRanges.isEmpty()) {
            return entry.valueRanges.constFirst().text.trimmed();
        }
        return QString();
    }

    return QString();
}

bool pointTypeSupportsClip(const QString &pointType)
{
    const QString normalizedType = pointType.section(QLatin1Char(':'), 0, 0).trimmed().toLower();
    static const QSet<QString> disallowedTypes = {
        QStringLiteral("station"),
        QStringLiteral("station-name"),
        QStringLiteral("label"),
        QStringLiteral("remark"),
        QStringLiteral("date"),
        QStringLiteral("altitude"),
        QStringLiteral("height"),
        QStringLiteral("passage-height"),
    };
    return !normalizedType.isEmpty() && !disallowedTypes.contains(normalizedType);
}

int indexForComboUserData(QComboBox *combo, const QString &value)
{
    if (combo == nullptr) {
        return -1;
    }
    const QString normalized = value.trimmed().toLower();
    for (int index = 0; index < combo->count(); ++index) {
        if (combo->itemData(index).toString().trimmed().toLower() == normalized) {
            return index;
        }
    }
    return normalized.isEmpty() ? 0 : -1;
}
}

MapEditorObjectDetailsPanelController::MapEditorObjectDetailsPanelController(MapEditorObjectDetailsContext context)
    : context_(std::move(context))
{
}

QString MapEditorObjectDetailsPanelController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController", text);
}

const InspectorSymbolCatalog &MapEditorObjectDetailsPanelController::inspectorSymbolCatalog() const
{
    Q_ASSERT(context_.inspectorSymbolCatalog != nullptr);
    static const InspectorSymbolCatalog emptyCatalog;
    return context_.inspectorSymbolCatalog != nullptr ? *context_.inspectorSymbolCatalog : emptyCatalog;
}

const MapEditorOrientationApplicabilityByCommand &MapEditorObjectDetailsPanelController::orientationApplicabilityByCommand() const
{
    Q_ASSERT(context_.orientationApplicabilityByCommand != nullptr);
    static const MapEditorOrientationApplicabilityByCommand emptyApplicability;
    return context_.orientationApplicabilityByCommand != nullptr
        ? *context_.orientationApplicabilityByCommand
        : emptyApplicability;
}

void MapEditorObjectDetailsPanelController::refreshObjectDetailsPanel()
{
    if (context_.selectionLabel == nullptr
        || context_.emptySelectionSection == nullptr
        || context_.selectionSection == nullptr
        || context_.selectionTitleLabel == nullptr
        || context_.vertexSection == nullptr
        || context_.vertexTitleLabel == nullptr
        || context_.geometrySection == nullptr
        || context_.advancedSection == nullptr
        || context_.deleteButton == nullptr
        || context_.areaReferenceLabel == nullptr
        || context_.quickFieldsEditor == nullptr
        || context_.quickIdentifierLabel == nullptr
        || context_.quickNameLabel == nullptr
        || context_.quickTextLabel == nullptr
        || context_.quickValueLabel == nullptr
        || context_.quickProjectionLabel == nullptr
        || context_.quickTypeLabel == nullptr
        || context_.quickSubtypeLabel == nullptr
        || context_.quickRecentLabel == nullptr
        || context_.quickTargetScrapLabel == nullptr
        || context_.quickNameEditor == nullptr
        || context_.quickTextEditor == nullptr
        || context_.quickValueEditor == nullptr
        || context_.quickRecentEditor == nullptr
        || context_.stylePreviewLabel == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickProjectionCombo == nullptr
        || context_.quickTargetScrapCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr
        || context_.quickTextEdit == nullptr
        || context_.quickValueEdit == nullptr
        || context_.stylePreview == nullptr
        || context_.vertexActionsEditor == nullptr
        || context_.vertexInsertBeforeButton == nullptr
        || context_.vertexInsertAfterButton == nullptr
        || context_.vertexDeleteButton == nullptr
        || context_.vertexSplitButton == nullptr
        || context_.metadataLabel == nullptr
        || context_.orientationEditor == nullptr
        || context_.linePointPreviousControlCheck == nullptr
        || context_.linePointSmoothCheck == nullptr
        || context_.linePointNextControlCheck == nullptr
        || context_.orientationEnabledCheck == nullptr
        || context_.orientationSpin == nullptr
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || context_.linePointLeftSizeSpin == nullptr
        || context_.linePointSegmentSubtypeLabel == nullptr
        || context_.linePointSegmentSubtypeCombo == nullptr
        || context_.linePointAltitudeAutoCheck == nullptr
        || context_.linePointFlagsEditor == nullptr
        || context_.linePointFlagsEdit == nullptr
        || context_.lineOptionsEditor == nullptr
        || context_.scrapOptionsEditor == nullptr
        || context_.scrapProjectionLabel == nullptr
        || context_.scrapProjectionEditor == nullptr
        || context_.quickProjectionEditor == nullptr
        || context_.lineClosedCheck == nullptr
        || context_.lineReversedCheck == nullptr
        || context_.objectClipDisabledCheck == nullptr
        || context_.pointAlignEditor == nullptr
        || context_.pointAlignLabel == nullptr
        || context_.pointAlignCombo == nullptr
        || context_.scrapScaleEditor == nullptr
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr
        || context_.configureButtonRow == nullptr
        || context_.configureButton == nullptr) {
        return;
    }

    const QScopedValueRollback<bool> uiGuard(*context_.updatingUi, true);

    auto clearStylePreview = [this]() {
        context_.stylePreviewLabel->setVisible(false);
        context_.stylePreview->clearStyleSelection();
    };
    auto showStylePreview = [this](const QString &commandKind, const QString &type, const QString &subtype) {
        const QString normalizedCommand = commandKind.trimmed().toLower();
        const bool visible = normalizedCommand == QStringLiteral("point")
            || normalizedCommand == QStringLiteral("line")
            || normalizedCommand == QStringLiteral("area");
        context_.stylePreviewLabel->setVisible(visible);
        if (visible) {
            context_.stylePreview->setStyleSelection(normalizedCommand, type, subtype);
        } else {
            context_.stylePreview->clearStyleSelection();
        }
    };
    auto moveProjectionComboForSelection = [this](const QString &commandKind) {
        const bool scrapCommand = commandKind.trimmed().compare(QStringLiteral("scrap"), Qt::CaseInsensitive) == 0;
        moveSelectionControlToContainer(context_.quickProjectionCombo,
                                        scrapCommand ? context_.scrapProjectionEditor : context_.quickProjectionEditor);
        context_.quickProjectionLabel->setVisible(false);
        context_.scrapProjectionLabel->setVisible(false);
    };
    auto moveQuickOptionEditorsForSelection = [this](const QString &commandKind,
                                                     bool nameVisible,
                                                     bool textVisible,
                                                     bool valueVisible) {
        QWidget *targetContainer = context_.lineOptionsEditor;
        const QString normalizedCommand = commandKind.trimmed().toLower();
        if (normalizedCommand == QStringLiteral("point")) {
            targetContainer = context_.orientationEditor;
        } else if (normalizedCommand == QStringLiteral("scrap")) {
            targetContainer = context_.scrapOptionsEditor;
        }

        moveSelectionControlToContainer(context_.quickNameEditor, targetContainer);
        moveSelectionControlToContainer(context_.quickTextEditor, targetContainer);
        moveSelectionControlToContainer(context_.quickValueEditor, targetContainer);
        context_.quickNameEditor->setVisible(nameVisible);
        context_.quickNameLabel->setVisible(nameVisible);
        context_.quickNameEdit->setVisible(nameVisible);
        context_.quickTextEditor->setVisible(textVisible);
        context_.quickTextLabel->setVisible(textVisible);
        context_.quickTextEdit->setVisible(textVisible);
        context_.quickValueEditor->setVisible(valueVisible);
        context_.quickValueLabel->setVisible(valueVisible);
        context_.quickValueEdit->setVisible(valueVisible);
        reorderSelectionControls(targetContainer,
                                 {context_.quickNameEditor,
                                  context_.quickTextEditor,
                                  context_.quickValueEditor});
    };
    auto moveConfigureButtonForSelection = [this](const QString &commandKind, bool allowConfigure) {
        QWidget *targetContainer = context_.advancedSection;
        const QString normalizedCommand = commandKind.trimmed().toLower();
        if (allowConfigure) {
            if (normalizedCommand == QStringLiteral("point")) {
                targetContainer = context_.orientationEditor;
            } else if (normalizedCommand == QStringLiteral("line")
                       || normalizedCommand == QStringLiteral("area")) {
                targetContainer = context_.lineOptionsEditor;
            } else if (normalizedCommand == QStringLiteral("scrap")) {
                targetContainer = context_.scrapOptionsEditor;
            }
        }
        moveSelectionControlToContainer(context_.configureButtonRow, targetContainer, true);
        context_.configureButtonRow->setVisible(allowConfigure);
        context_.configureButton->setEnabled(allowConfigure);
    };
    bool scrapOptionsVisible = false;
    bool objectQuickOptionsVisible = false;

    if (context_.pendingInsertQuickFields) {
        const std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && !pendingFields->commandKind.trimmed().isEmpty()) {
            const QString commandKind = pendingFields->commandKind.trimmed().toLower();
            QString objectSectionTitle = commandKind;
            if (commandKind == QStringLiteral("line")) {
                objectSectionTitle = tr("New Line");
            } else if (commandKind == QStringLiteral("area")) {
                objectSectionTitle = tr("New Area");
            } else if (commandKind == QStringLiteral("point")) {
                objectSectionTitle = tr("New Point");
            } else if (commandKind == QStringLiteral("scrap")) {
                objectSectionTitle = tr("New Scrap");
            }

            context_.selectionLabel->setVisible(false);
            context_.emptySelectionSection->setVisible(false);
            context_.selectionSection->setVisible(true);
            context_.selectionTitleLabel->setText(objectSectionTitle);
            context_.vertexTitleLabel->setText(commandKind == QStringLiteral("point")
                                                   ? tr("Options")
                                                   : tr("Line Point"));
            const std::optional<InspectorScrapContext> targetScrapContext = commandKind != QStringLiteral("scrap")
                    && context_.pendingInsertTargetScrapContext
                ? context_.pendingInsertTargetScrapContext()
                : std::nullopt;
            context_.deleteButton->setEnabled(false);
            context_.deleteButton->setToolTip(QString());
            context_.areaReferenceLabel->setVisible(false);
            context_.areaReferenceLabel->clear();
            context_.quickFieldsEditor->setVisible(true);
            context_.objectQuickCommandKind->clear();
            *context_.objectQuickCommandKind = commandKind;

            const bool typeFieldsVisible = commandKind != QStringLiteral("scrap");
            const bool projectionFieldVisible = commandKind == QStringLiteral("scrap");
            const bool pendingLineLike = commandKind == QStringLiteral("line") || commandKind == QStringLiteral("area");
            const bool pendingLinePointAvailable = pendingLineLike
                && context_.pendingInsertLinePointAvailable
                && context_.pendingInsertLinePointAvailable();
            const bool pendingLinePointSmooth = pendingLinePointAvailable
                && context_.pendingInsertLinePointSmooth
                && context_.pendingInsertLinePointSmooth();
            const bool pendingLinePointIncomingControl = pendingLinePointAvailable
                && context_.pendingInsertLinePointIncomingControl
                && context_.pendingInsertLinePointIncomingControl();
            const bool pendingLinePointOutgoingControl = pendingLinePointAvailable
                && context_.pendingInsertLinePointOutgoingControl
                && context_.pendingInsertLinePointOutgoingControl();
            const QString pendingLinePointSegmentSubtype =
                pendingLinePointAvailable && context_.pendingInsertLinePointSegmentSubtype
                    ? context_.pendingInsertLinePointSegmentSubtype()
                    : QString();
            const TherionParsedLine pendingPointParsedLine =
                TherionDocumentParser::parseLine(QStringLiteral("point 0 0 %1").arg(pendingFields->type.trimmed()));
            const TherionParsedLine pendingLineParsedLine =
                TherionDocumentParser::parseLine(QStringLiteral("line %1").arg(pendingFields->type.trimmed()));
            const bool pendingPointOrientationApplicable = commandKind == QStringLiteral("point")
                && isOrientationSupportedForParsedLine(pendingPointParsedLine, orientationApplicabilityByCommand());
            const bool pendingLinePointOrientationApplicable = pendingLinePointAvailable
                && commandKind == QStringLiteral("line")
                && isOrientationSupportedForParsedLine(pendingLineParsedLine, orientationApplicabilityByCommand());
            const bool pendingLinePointLeftSizeApplicable = pendingLinePointAvailable
                && commandKind == QStringLiteral("line")
                && isLinePointLeftSizeSupportedForParsedLine(pendingLineParsedLine);
            const bool pendingOrientationApplicable = pendingPointOrientationApplicable
                || pendingLinePointOrientationApplicable;
            const bool pendingOrientationEnabled = pendingLinePointOrientationApplicable
                && context_.pendingInsertLinePointOrientationEnabled
                && context_.pendingInsertLinePointOrientationEnabled();
            const qreal pendingOrientationDegrees =
                pendingLinePointOrientationApplicable && context_.pendingInsertLinePointOrientationDegrees
                    ? context_.pendingInsertLinePointOrientationDegrees()
                    : 0.0;
            const bool pendingLeftSizeEnabled = pendingLinePointLeftSizeApplicable
                && context_.pendingInsertLinePointLeftSizeEnabled
                && context_.pendingInsertLinePointLeftSizeEnabled();
            const qreal pendingLeftSize =
                pendingLinePointLeftSizeApplicable && context_.pendingInsertLinePointLeftSize
                    ? context_.pendingInsertLinePointLeftSize()
                    : 40.0;
            const bool textFieldVisible = pendingFields->textVisible;
            const bool valueFieldVisible = pendingFields->valueVisible;
            moveProjectionComboForSelection(commandKind);
            moveQuickOptionEditorsForSelection(commandKind,
                                               pendingFields->nameVisible,
                                               textFieldVisible,
                                               valueFieldVisible);
            objectQuickOptionsVisible = pendingFields->nameVisible || textFieldVisible || valueFieldVisible;
            context_.quickProjectionLabel->setVisible(false);
            context_.scrapProjectionLabel->setVisible(projectionFieldVisible);
            context_.quickTypeLabel->setVisible(typeFieldsVisible);
            context_.quickSubtypeLabel->setVisible(typeFieldsVisible);
            const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument
                ? context_.parsedLinesForCurrentDocument()
                : QVector<TherionParsedLine>();
            const QVector<InspectorScrapContext> scrapContexts = inspectorScrapContexts(parsedLines);
            const bool targetScrapVisible = commandKind != QStringLiteral("scrap") && !scrapContexts.isEmpty();
            context_.metadataLabel->setVisible(commandKind != QStringLiteral("scrap") && !targetScrapVisible);
            context_.metadataLabel->setText(metadataForPendingInsert(targetScrapContext));
            context_.metadataLabel->setStyleSheet(metadataLabelStyleSheet(targetScrapContext.has_value()
                                                                          && targetScrapContext->willBeCreated));
            context_.quickTargetScrapLabel->setVisible(targetScrapVisible);
            context_.quickProjectionEditor->setVisible(false);
            context_.scrapOptionsEditor->setVisible(projectionFieldVisible);
            scrapOptionsVisible = projectionFieldVisible;
            context_.quickProjectionCombo->setVisible(projectionFieldVisible);
            context_.quickTypeCombo->setVisible(typeFieldsVisible);
            context_.quickSubtypeCombo->setVisible(typeFieldsVisible);
            context_.quickTargetScrapCombo->setVisible(targetScrapVisible);
            context_.quickProjectionCombo->setEnabled(projectionFieldVisible);
            context_.quickTypeCombo->setEnabled(typeFieldsVisible && pendingFields->typeEditable);
            context_.quickSubtypeCombo->setEnabled(typeFieldsVisible && pendingFields->typeEditable);
            context_.quickTargetScrapCombo->setEnabled(targetScrapVisible);

            const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
            setEditableComboValues(context_.quickTypeCombo,
                                   inspectorTypeValuesForCommand(catalog, commandKind),
                                   pendingFields->type);
            setEditableComboValues(context_.quickProjectionCombo,
                                   inspectorProjectionValues(catalog),
                                   pendingFields->projection);
            setEditableComboValues(context_.quickSubtypeCombo,
                                   inspectorSubtypeValuesForCommandTypeWithEmptyChoice(catalog, commandKind, pendingFields->type),
                                   pendingFields->subtype);
            setTargetScrapComboValues(context_.quickTargetScrapCombo, scrapContexts, targetScrapContext);
            updateRecentPendingSymbolButtons(context_, commandKind, typeFieldsVisible);
            context_.quickIdentifierLabel->setText(tr("ID"));
            context_.quickIdentifierEdit->setText(pendingFields->identifier);
            context_.quickNameEdit->setText(pendingFields->name);
            context_.quickTextEdit->setText(pendingFields->text);
            context_.quickValueEdit->setText(pendingFields->value);
            showStylePreview(commandKind, pendingFields->type, pendingFields->subtype);

            context_.vertexSection->setVisible((commandKind == QStringLiteral("point") && objectQuickOptionsVisible)
                                               || pendingLinePointAvailable);
            context_.geometrySection->setVisible(projectionFieldVisible
                                                 || pendingLineLike
                                                 || (commandKind != QStringLiteral("point") && objectQuickOptionsVisible));
            context_.advancedSection->setVisible(false);
            context_.vertexActionsEditor->setVisible(false);
            context_.linePointActionsSection->setVisible(false);
            context_.orientationEditor->setVisible((commandKind == QStringLiteral("point") && objectQuickOptionsVisible)
                                                   || pendingLinePointAvailable
                                                   || pendingOrientationApplicable
                                                   || pendingLinePointLeftSizeApplicable);
            context_.orientationEnabledCheck->setText(commandKind == QStringLiteral("point")
                                                          ? tr("Orientation override (-orientation)")
                                                          : tr("Orientation (-orientation)"));
            context_.orientationEnabledCheck->setVisible(pendingOrientationApplicable);
            context_.orientationEnabledCheck->setEnabled(pendingLinePointOrientationApplicable);
            context_.orientationEnabledCheck->setChecked(pendingOrientationEnabled);
            context_.orientationSpin->setVisible(pendingOrientationApplicable);
            context_.orientationSpin->setEnabled(pendingLinePointOrientationApplicable && pendingOrientationEnabled);
            context_.orientationSpin->setValue(pendingOrientationDegrees);
            context_.lineOptionsEditor->setVisible(pendingLineLike);
            if (QWidget *controlRow = context_.linePointSmoothCheck->parentWidget(); controlRow != nullptr) {
                controlRow->setVisible(pendingLinePointAvailable);
            }
            context_.linePointPreviousControlCheck->setVisible(pendingLinePointAvailable);
            context_.linePointPreviousControlCheck->setEnabled(pendingLinePointAvailable);
            context_.linePointPreviousControlCheck->setChecked(pendingLinePointIncomingControl);
            context_.linePointSmoothCheck->setVisible(pendingLinePointAvailable);
            context_.linePointSmoothCheck->setEnabled(pendingLinePointAvailable);
            context_.linePointSmoothCheck->setChecked(pendingLinePointAvailable && pendingLinePointSmooth);
            context_.linePointNextControlCheck->setVisible(pendingLinePointAvailable);
            context_.linePointNextControlCheck->setEnabled(pendingLinePointAvailable);
            context_.linePointNextControlCheck->setChecked(pendingLinePointOutgoingControl);
            const QStringList pendingLinePointSubtypeValues =
                pendingLinePointAvailable
                    ? inspectorSubtypeValuesForCommandTypeWithEmptyChoice(catalog,
                                                                          QStringLiteral("line"),
                                                                          pendingFields->type)
                    : QStringList();
            const bool pendingLinePointSegmentSubtypeAvailable = !pendingLinePointSubtypeValues.isEmpty();
            context_.linePointSegmentSubtypeLabel->setVisible(pendingLinePointSegmentSubtypeAvailable);
            context_.linePointSegmentSubtypeCombo->setVisible(pendingLinePointSegmentSubtypeAvailable);
            context_.linePointSegmentSubtypeCombo->setEnabled(pendingLinePointSegmentSubtypeAvailable);
            if (pendingLinePointSegmentSubtypeAvailable) {
                setEditableComboValues(context_.linePointSegmentSubtypeCombo,
                                       pendingLinePointSubtypeValues,
                                       pendingLinePointSegmentSubtype);
            } else {
                context_.linePointSegmentSubtypeCombo->clear();
            }
            context_.linePointAltitudeAutoCheck->setVisible(false);
            context_.linePointAltitudeAutoCheck->setChecked(false);
            context_.linePointFlagsEditor->setVisible(false);
            context_.linePointFlagsEdit->setEnabled(false);
            context_.linePointLeftSizeEnabledCheck->setChecked(pendingLeftSizeEnabled);
            context_.linePointLeftSizeEnabledCheck->setVisible(pendingLinePointLeftSizeApplicable);
            context_.linePointLeftSizeEnabledCheck->setEnabled(pendingLinePointLeftSizeApplicable);
            context_.linePointLeftSizeSpin->setEnabled(pendingLinePointLeftSizeApplicable && pendingLeftSizeEnabled);
            context_.linePointLeftSizeSpin->setVisible(pendingLinePointLeftSizeApplicable);
            context_.linePointLeftSizeSpin->setValue(pendingLeftSize);
            context_.objectClipDisabledCheck->setChecked(false);
            context_.objectClipDisabledCheck->setVisible(false);
            context_.pointAlignEditor->setVisible(false);
            context_.pointAlignCombo->setCurrentIndex(0);
            context_.scrapScaleEditor->setVisible(false);
            moveConfigureButtonForSelection(commandKind, false);
            context_.lineClosedCheck->setChecked(false);
            context_.lineClosedCheck->setVisible(false);
            context_.lineReversedCheck->setChecked(false);
            context_.lineReversedCheck->setVisible(false);
            return;
        }
    }

    const int effectiveLineNumber = *context_.selectedObjectLineNumber;
    const QString effectiveKind = context_.selectedObjectKind->trimmed().toLower();

    if (effectiveLineNumber <= 0 || effectiveKind.isEmpty()) {
        context_.selectionLabel->setText(tr("No map object selected."));
        context_.selectionLabel->setVisible(true);
        context_.emptySelectionSection->setVisible(true);
        context_.selectionTitleLabel->setText(tr("Object"));
        context_.selectionSection->setVisible(false);
        clearStylePreview();
        context_.vertexSection->setVisible(false);
        context_.geometrySection->setVisible(false);
        context_.advancedSection->setVisible(false);
        context_.deleteButton->setEnabled(false);
        context_.deleteButton->setToolTip(QString());
        context_.areaReferenceLabel->setVisible(false);
        context_.areaReferenceLabel->clear();
        context_.quickFieldsEditor->setVisible(false);
        context_.objectQuickCommandKind->clear();
        context_.quickTargetScrapLabel->setVisible(false);
        context_.quickTargetScrapCombo->setVisible(false);
        context_.quickValueLabel->setVisible(false);
        context_.quickValueEdit->setVisible(false);
        context_.quickValueEdit->clear();
        context_.metadataLabel->setVisible(true);
        context_.linePointActionsSection->setVisible(false);
        context_.vertexActionsEditor->setVisible(false);
        context_.metadataLabel->setText(QStringLiteral("-"));
        context_.metadataLabel->setStyleSheet(metadataLabelStyleSheet(false));
        context_.orientationEditor->setVisible(false);
        context_.linePointPreviousControlCheck->setChecked(false);
        context_.linePointPreviousControlCheck->setVisible(false);
        context_.linePointSmoothCheck->setChecked(false);
        context_.linePointSmoothCheck->setVisible(false);
        context_.linePointNextControlCheck->setChecked(false);
        context_.linePointNextControlCheck->setVisible(false);
        context_.linePointLeftSizeEnabledCheck->setChecked(false);
        context_.linePointLeftSizeSpin->setEnabled(false);
        context_.linePointLeftSizeSpin->setValue(40.0);
        context_.linePointSegmentSubtypeLabel->setVisible(false);
        context_.linePointSegmentSubtypeCombo->setVisible(false);
        context_.linePointSegmentSubtypeCombo->clear();
        context_.linePointAltitudeAutoCheck->setVisible(false);
        context_.linePointAltitudeAutoCheck->setChecked(false);
        {
            const QSignalBlocker blocker(context_.linePointFlagsEdit);
            context_.linePointFlagsEdit->clear();
        }
        context_.linePointFlagsEditor->setVisible(false);
        context_.linePointFlagsEdit->setEnabled(false);
        context_.lineOptionsEditor->setVisible(false);
        context_.scrapOptionsEditor->setVisible(false);
        moveProjectionComboForSelection(QString());
        context_.quickProjectionEditor->setVisible(false);
        context_.objectClipDisabledCheck->setChecked(false);
        context_.objectClipDisabledCheck->setVisible(false);
        context_.pointAlignEditor->setVisible(false);
        context_.pointAlignCombo->setCurrentIndex(0);
        context_.scrapScaleEditor->setVisible(false);
        moveConfigureButtonForSelection(QString(), false);
        context_.lineClosedCheck->setChecked(false);
        context_.lineReversedCheck->setChecked(false);
        return;
    }

    context_.selectionLabel->setVisible(false);
    context_.emptySelectionSection->setVisible(false);
    context_.selectionSection->setVisible(true);
    context_.advancedSection->setVisible(true);
    QString objectSectionTitle = effectiveKind;
    if (effectiveKind == QStringLiteral("line")) {
        objectSectionTitle = tr("Line");
    } else if (effectiveKind == QStringLiteral("area")) {
        objectSectionTitle = tr("Area");
    } else if (effectiveKind == QStringLiteral("point")) {
        objectSectionTitle = tr("Point");
    } else if (effectiveKind == QStringLiteral("scrap")) {
        objectSectionTitle = tr("Scrap");
    }
    context_.selectionTitleLabel->setText(objectSectionTitle);
    context_.vertexTitleLabel->setText(effectiveKind == QStringLiteral("line")
                                           ? tr("Line Point")
                                           : tr("Options"));
    const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument
        ? context_.parsedLinesForCurrentDocument()
        : QVector<TherionParsedLine>();
    const QVector<TherionSourceLogicalCommand> logicalCommands = context_.logicalSource.logicalCommandsForCurrentDocument
        ? context_.logicalSource.logicalCommandsForCurrentDocument()
        : QVector<TherionSourceLogicalCommand>();
    context_.metadataLabel->setText(metadataForSourceLine(parsedLines, effectiveLineNumber));
    context_.metadataLabel->setStyleSheet(metadataLabelStyleSheet(false));
    QVector<MapEditorAreaReference> areaReferences;
    if (context_.textEditor != nullptr
        && effectiveLineNumber > 0
        && effectiveKind == QStringLiteral("line")) {
        areaReferences = areaReferencesForBorderLine(logicalCommands, effectiveLineNumber);
    }
    const bool deleteBlockedByAreaReference = !areaReferences.isEmpty();
    context_.deleteButton->setEnabled(effectiveLineNumber > 0 && !deleteBlockedByAreaReference);
    context_.deleteButton->setToolTip(deleteBlockedByAreaReference
        ? tr("This line is used as an area border. Select or delete the area instead.")
        : QString());
    context_.areaReferenceLabel->setVisible(deleteBlockedByAreaReference);
    if (deleteBlockedByAreaReference) {
        QStringList areaLinks;
        areaLinks.reserve(areaReferences.size());
        for (const MapEditorAreaReference &reference : areaReferences) {
            const QString label = reference.areaLabel.toHtmlEscaped();
            areaLinks.append(QStringLiteral("<a href=\"%1\">%2</a>").arg(reference.areaLineNumber).arg(label));
        }
        context_.areaReferenceLabel->setText(tr("Used by area: %1").arg(areaLinks.join(QStringLiteral(", "))));
        context_.areaReferenceLabel->setToolTip(tr("Click the area name to select the area that owns this border line."));
    } else {
        context_.areaReferenceLabel->clear();
        context_.areaReferenceLabel->setToolTip(QString());
    }

    context_.quickFieldsEditor->setVisible(false);
    context_.objectQuickCommandKind->clear();
    context_.quickTargetScrapLabel->setVisible(false);
    context_.quickTargetScrapCombo->setVisible(false);
    context_.metadataLabel->setVisible(true);
    if (context_.textEditor != nullptr && effectiveLineNumber > 0) {
        if (const std::optional<InspectorObjectQuickFields> fields =
                inspectorObjectQuickFieldsForLine(logicalCommands, effectiveLineNumber)) {
            const bool typeFieldsVisible = fields->commandKind != QStringLiteral("scrap");
            const bool projectionFieldVisible = fields->commandKind == QStringLiteral("scrap");
            const bool textFieldVisible = fields->textVisible;
            const bool valueFieldVisible =
                inspectorValueOptionSupportedForCommandType(inspectorSymbolCatalog(), fields->commandKind, fields->type)
                || fields->valueVisible;
            moveProjectionComboForSelection(fields->commandKind);
            moveQuickOptionEditorsForSelection(fields->commandKind,
                                               fields->nameVisible,
                                               textFieldVisible,
                                               valueFieldVisible);
            objectQuickOptionsVisible = fields->nameVisible || textFieldVisible || valueFieldVisible;
            context_.quickFieldsEditor->setVisible(true);
            context_.quickProjectionLabel->setVisible(false);
            context_.scrapProjectionLabel->setVisible(projectionFieldVisible);
            context_.quickTypeLabel->setVisible(typeFieldsVisible);
            context_.quickSubtypeLabel->setVisible(typeFieldsVisible);
            context_.quickTargetScrapLabel->setVisible(false);
            context_.quickProjectionEditor->setVisible(false);
            context_.scrapOptionsEditor->setVisible(projectionFieldVisible);
            scrapOptionsVisible = projectionFieldVisible;
            context_.quickProjectionCombo->setVisible(projectionFieldVisible);
            context_.quickTypeCombo->setVisible(typeFieldsVisible);
            context_.quickSubtypeCombo->setVisible(typeFieldsVisible);
            context_.quickTargetScrapCombo->setVisible(false);
            context_.quickProjectionCombo->setEnabled(projectionFieldVisible);
            context_.quickTypeCombo->setEnabled(typeFieldsVisible && fields->typeEditable);
            context_.quickSubtypeCombo->setEnabled(typeFieldsVisible && fields->typeEditable);
            const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
            setEditableComboValues(context_.quickTypeCombo,
                                   inspectorTypeValuesForCommand(catalog, fields->commandKind),
                                   fields->type);
            setEditableComboValues(context_.quickProjectionCombo,
                                   inspectorProjectionValues(catalog),
                                   fields->projection);
            context_.quickIdentifierEdit->setText(fields->identifier);
            context_.quickIdentifierLabel->setText(tr("ID"));
            context_.quickNameEdit->setText(fields->name);
            context_.quickTextEdit->setText(fields->text);
            context_.quickValueEdit->setText(fields->value);
            *context_.objectQuickCommandKind = fields->commandKind;
            setEditableComboValues(context_.quickSubtypeCombo,
                                   inspectorSubtypeValuesForCommandTypeWithEmptyChoice(catalog, fields->commandKind, fields->type),
                                   fields->subtype);
            updateRecentPendingSymbolButtons(context_, fields->commandKind, false);
            showStylePreview(fields->commandKind, fields->type, fields->subtype);
        }
    }
    if (!context_.quickFieldsEditor->isVisible()) {
        clearStylePreview();
        context_.quickNameEditor->setVisible(false);
        context_.quickTextEditor->setVisible(false);
        context_.quickValueEditor->setVisible(false);
        updateRecentPendingSymbolButtons(context_, QString(), false);
        objectQuickOptionsVisible = false;
        context_.scrapOptionsEditor->setVisible(false);
        scrapOptionsVisible = false;
        moveProjectionComboForSelection(effectiveKind);
        context_.quickProjectionEditor->setVisible(false);
    }
    const bool lineVertexActionsAvailable = effectiveKind == QStringLiteral("line")
        && effectiveLineNumber > 0
        && *context_.selectedObjectVertexIndex >= 0
        && context_.textEditor != nullptr;
    context_.linePointActionsSection->setVisible(lineVertexActionsAvailable);
    context_.vertexActionsEditor->setVisible(lineVertexActionsAvailable);
    context_.vertexInsertBeforeButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexInsertAfterButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexDeleteButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexSplitButton->setEnabled(false);
    context_.vertexSplitButton->setToolTip(tr("Split Here is available for interior vertices of open lines."));
    if (lineVertexActionsAvailable) {
        bool firstVertex = false;
        bool lastVertex = false;
        if (const std::optional<MapGeometryFeature> lineFeature =
                lineFeatureForLine(context_.logicalSource, logicalCommands, *context_.selectedObjectLineNumber);
            lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
            const int lineVertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex);
            firstVertex = lineVertexIndex == 0;
            lastVertex = lineVertexIndex == lineFeature->lineVertices.size() - 1;
            const bool interiorVertex = lineVertexIndex > 0 && lineVertexIndex < lineFeature->lineVertices.size() - 1;
            context_.vertexSplitButton->setEnabled(interiorVertex && !lineFeature->closed);
            if (!interiorVertex) {
                context_.vertexSplitButton->setToolTip(tr("Cannot split at the first or last vertex."));
            } else if (lineFeature->closed) {
                context_.vertexSplitButton->setToolTip(tr("Cannot split closed lines yet."));
            } else {
                context_.vertexSplitButton->setToolTip(tr("Split the selected line at this vertex."));
            }
        }
        context_.vertexInsertBeforeButton->setText(firstVertex ? tr("Extend Before") : tr("Insert Before"));
        context_.vertexInsertAfterButton->setText(lastVertex ? tr("Extend After") : tr("Insert After"));
        context_.vertexInsertBeforeButton->setToolTip(firstVertex
                                                          ? tr("Extend the line before the first vertex.")
                                                          : tr("Insert a vertex before the selected vertex."));
        context_.vertexInsertAfterButton->setToolTip(lastVertex
                                                         ? tr("Extend the line after the last vertex.")
                                                         : tr("Insert a vertex after the selected vertex."));
    } else {
        context_.vertexInsertBeforeButton->setText(tr("Insert Before"));
        context_.vertexInsertAfterButton->setText(tr("Insert After"));
        context_.vertexInsertBeforeButton->setToolTip(QString());
        context_.vertexInsertAfterButton->setToolTip(QString());
        context_.vertexSplitButton->setToolTip(QString());
    }

    bool clipDisabledVisible = false;
    bool clipDisabled = false;
    QString pointAlign;
    const bool objectOptionSourceVisible = *context_.selectedObjectLineNumber > 0
        && (*context_.selectedObjectKind == QStringLiteral("line")
            || *context_.selectedObjectKind == QStringLiteral("area")
            || *context_.selectedObjectKind == QStringLiteral("point"))
        && context_.textEditor != nullptr;
    const bool lineOptionsVisible = *context_.selectedObjectLineNumber > 0
        && (*context_.selectedObjectKind == QStringLiteral("line")
            || *context_.selectedObjectKind == QStringLiteral("area"))
        && context_.textEditor != nullptr;
    context_.lineOptionsEditor->setVisible(lineOptionsVisible);
    if (objectOptionSourceVisible) {
        const std::optional<TherionSourceLogicalCommand> command =
            logicalCommandForLine(logicalCommands, *context_.selectedObjectLineNumber);
        if (command.has_value()
            && (command->parsed.directive == QStringLiteral("line")
                || command->parsed.directive == QStringLiteral("area")
                || command->parsed.directive == QStringLiteral("point"))) {
            clipDisabled =
                singleValueOptionValue(*command, QStringLiteral("-clip")).compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0;
            clipDisabledVisible = command->parsed.directive != QStringLiteral("point")
                || pointTypeSupportsClip(command->parsed.tokens.value(3))
                || clipDisabled;
            pointAlign = singleValueOptionValue(*command, QStringLiteral("-align"));
        }
    }
    if (lineOptionsVisible) {
        if (const std::optional<MapGeometryFeature> lineFeature =
                lineFeatureForLine(context_.logicalSource, logicalCommands, *context_.selectedObjectLineNumber);
            *context_.selectedObjectKind == QStringLiteral("line")
            && lineFeature.has_value()
            && lineFeature->kind == MapGeometryFeature::Kind::Line) {
            context_.lineClosedCheck->setVisible(true);
            context_.lineReversedCheck->setVisible(true);
            context_.lineClosedCheck->setChecked(lineFeature->closed);
            context_.lineReversedCheck->setChecked(lineFeature->reversed);
        } else {
            context_.lineClosedCheck->setVisible(false);
            context_.lineReversedCheck->setVisible(false);
            context_.lineClosedCheck->setChecked(false);
            context_.lineReversedCheck->setChecked(false);
        }
    } else {
        context_.lineClosedCheck->setVisible(false);
        context_.lineReversedCheck->setVisible(false);
        context_.lineClosedCheck->setChecked(false);
        context_.lineReversedCheck->setChecked(false);
    }
    const bool pointClipInGeometrySection = *context_.selectedObjectKind == QStringLiteral("point") && clipDisabledVisible;
    moveSelectionControlToContainer(context_.objectClipDisabledCheck,
                                    pointClipInGeometrySection ? context_.orientationEditor : context_.lineOptionsEditor);
    context_.objectClipDisabledCheck->setVisible(clipDisabledVisible);
    context_.objectClipDisabledCheck->setEnabled(clipDisabledVisible);
    context_.objectClipDisabledCheck->setChecked(clipDisabledVisible && clipDisabled);

    context_.geometrySection->setVisible(lineOptionsVisible && (clipDisabledVisible || *context_.selectedObjectKind == QStringLiteral("line")));
    if (effectiveKind == QStringLiteral("scrap")) {
        context_.geometrySection->setVisible(scrapOptionsVisible);
    }

    const bool scrapScaleVisible = effectiveLineNumber > 0
        && effectiveKind == QStringLiteral("scrap")
        && context_.textEditor != nullptr;
    context_.scrapScaleEditor->setVisible(scrapScaleVisible);
    if (scrapScaleVisible) {
        auto configureScaleSpin = [](QDoubleSpinBox *spin, int decimals) {
            if (spin == nullptr) {
                return;
            }
            spin->setDecimals(decimals);
            spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        };
        configureScaleSpin(context_.scrapScaleSourceX1Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceY1Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceX2Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceY2Spin, 0);
        configureScaleSpin(context_.scrapScaleRealX1Spin, 4);
        configureScaleSpin(context_.scrapScaleRealY1Spin, 4);
        configureScaleSpin(context_.scrapScaleRealX2Spin, 4);
        configureScaleSpin(context_.scrapScaleRealY2Spin, 4);

        InspectorScrapScale scale = defaultInspectorScrapScale(context_.mapSourceBoundsForCurrentDocument());
        const std::optional<TherionSourceLogicalCommand> command =
            logicalCommandForLine(logicalCommands, effectiveLineNumber);
        if (command.has_value()) {
            if (const std::optional<InspectorScrapScale> parsedScale =
                    inspectorScrapScaleFromTokens(command->parsed.tokens)) {
                scale = *parsedScale;
            }
        }

        context_.scrapScaleSourceX1Spin->setValue(scale.sourcePoint1.x());
        context_.scrapScaleSourceY1Spin->setValue(scale.sourcePoint1.y());
        context_.scrapScaleSourceX2Spin->setValue(scale.sourcePoint2.x());
        context_.scrapScaleSourceY2Spin->setValue(scale.sourcePoint2.y());
        context_.scrapScaleRealX1Spin->setValue(scale.realPoint1.x());
        context_.scrapScaleRealY1Spin->setValue(scale.realPoint1.y());
        context_.scrapScaleRealX2Spin->setValue(scale.realPoint2.x());
        context_.scrapScaleRealY2Spin->setValue(scale.realPoint2.y());
        const int unitIndex = context_.scrapScaleUnitCombo->findText(scale.unitToken);
        context_.scrapScaleUnitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
    }

    bool orientationApplicable = false;
    bool pointAlignApplicable = false;
    bool linePointLeftSizeApplicable = false;
    bool linePointFlagsApplicable = false;
    bool linePointSegmentSubtypeApplicable = false;
    bool linePointAltitudeAutoApplicable = false;
    bool linePointSmoothApplicable = false;
    bool linePointSmooth = false;
    bool linePointPreviousControl = false;
    bool linePointNextControl = false;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> linePointLeftSize;
    QStringList linePointFlagsRows;
    QString linePointSegmentSubtype;
    QString linePointType;
    if (context_.textEditor != nullptr && *context_.selectedObjectLineNumber > 0) {
        if (*context_.selectedObjectKind == QStringLiteral("point")) {
            const std::optional<TherionSourceLogicalCommand> command =
                logicalCommandForLine(logicalCommands, *context_.selectedObjectLineNumber);
            if (command.has_value()) {
                if (command->parsed.directive == QStringLiteral("point")
                    && isOrientationSupportedForParsedLine(command->parsed, orientationApplicabilityByCommand())) {
                    orientationApplicable = true;
                    orientationDegrees = pointOrientationFromParsedLine(command->parsed);
                }
                if (command->parsed.directive == QStringLiteral("point")) {
                    pointAlignApplicable = true;
                    pointAlign = singleValueOptionValue(*command, QStringLiteral("-align"));
                }
            }
        } else if (*context_.selectedObjectKind == QStringLiteral("line") && *context_.selectedObjectVertexIndex >= 0) {
            if (const std::optional<MapGeometryFeature> lineFeature =
                    lineFeatureForLine(context_.logicalSource, logicalCommands, *context_.selectedObjectLineNumber);
                lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
                const int lineVertexIndex =
                    lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex);
                if (lineVertexIndex >= 0) {
                    linePointSmoothApplicable = true;
                    const MapGeometryFeature::TH2LineVertex &lineVertex = lineFeature->lineVertices.at(lineVertexIndex);
                    linePointPreviousControl = lineVertex.incomingControl.has_value();
                    linePointNextControl = lineVertex.outgoingControl.has_value();
                    linePointSmooth = lineVertex.isSmooth && linePointPreviousControl && linePointNextControl;
                    const std::optional<TherionSourceLogicalCommand> command =
                        logicalCommandForLine(logicalCommands, *context_.selectedObjectLineNumber);
                    if (!command.has_value()) {
                        orientationApplicable = false;
                    } else {
                        linePointType = command->parsed.tokens.value(1).section(QLatin1Char(':'), 0, 0).trimmed();
                        if (isOrientationSupportedForParsedLine(command->parsed, orientationApplicabilityByCommand())) {
                            orientationApplicable = true;
                            orientationDegrees = lineVertex.orientationDegrees;
                        }
                        if (isLinePointLeftSizeSupportedForParsedLine(command->parsed)) {
                            linePointLeftSizeApplicable = true;
                            if (lineVertex.leftSize.has_value() && lineVertex.leftSize.value() > 0.0) {
                                linePointLeftSize = lineVertex.leftSize;
                            }
                        }
                        linePointFlagsApplicable = true;
                        linePointFlagsRows = linePointStandaloneOptionRowsForSelection(lineFeature.value(),
                                                                                       *context_.selectedObjectVertexIndex);
                        const QStringList subtypeValues =
                            inspectorSubtypeValuesForCommandType(inspectorSymbolCatalog(), QStringLiteral("line"), linePointType);
                        linePointSegmentSubtypeApplicable = !subtypeValues.isEmpty();
                        linePointAltitudeAutoApplicable =
                            linePointType.compare(QStringLiteral("wall"), Qt::CaseInsensitive) == 0;
                        linePointSegmentSubtype = linePointSegmentSubtypeFromStandaloneRows(linePointFlagsRows);
                    }
                }
            }
        }
    }

    context_.orientationEditor->setVisible(linePointSmoothApplicable
                                           || objectQuickOptionsVisible
                                           || orientationApplicable
                                           || pointAlignApplicable
                                           || pointClipInGeometrySection
                                           || linePointLeftSizeApplicable
                                           || linePointSegmentSubtypeApplicable
                                           || linePointAltitudeAutoApplicable
                                           || linePointFlagsApplicable);
    context_.linePointPreviousControlCheck->setVisible(linePointSmoothApplicable);
    context_.linePointPreviousControlCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointPreviousControlCheck->setChecked(linePointSmoothApplicable && linePointPreviousControl);
    context_.linePointSmoothCheck->setVisible(linePointSmoothApplicable);
    context_.linePointSmoothCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointSmoothCheck->setChecked(linePointSmoothApplicable && linePointSmooth);
    context_.linePointNextControlCheck->setVisible(linePointSmoothApplicable);
    context_.linePointNextControlCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointNextControlCheck->setChecked(linePointSmoothApplicable && linePointNextControl);
    context_.orientationEnabledCheck->setVisible(orientationApplicable);
    context_.orientationEnabledCheck->setText(effectiveKind == QStringLiteral("line")
                                                  ? tr("Orientation (-orientation)")
                                                  : tr("Orientation override (-orientation)"));
    context_.orientationEnabledCheck->setEnabled(orientationApplicable);
    context_.orientationSpin->setVisible(orientationApplicable);
    context_.linePointLeftSizeEnabledCheck->setVisible(linePointLeftSizeApplicable);
    context_.linePointLeftSizeEnabledCheck->setEnabled(linePointLeftSizeApplicable);
    context_.linePointLeftSizeSpin->setVisible(linePointLeftSizeApplicable);
    context_.pointAlignEditor->setVisible(pointAlignApplicable);
    context_.pointAlignLabel->setVisible(pointAlignApplicable);
    context_.pointAlignCombo->setVisible(pointAlignApplicable);
    context_.pointAlignCombo->setEnabled(pointAlignApplicable);
    if (orientationApplicable) {
        context_.orientationEnabledCheck->setChecked(orientationDegrees.has_value());
        context_.orientationSpin->setEnabled(context_.orientationEnabledCheck->isChecked());
        context_.orientationSpin->setValue(orientationDegrees.value_or(0.0));
    } else {
        context_.orientationEnabledCheck->setChecked(false);
        context_.orientationSpin->setEnabled(false);
        context_.orientationSpin->setValue(0.0);
    }

    moveConfigureButtonForSelection(effectiveKind, isConfigurableMapObjectKind(effectiveKind));
    if (linePointLeftSizeApplicable) {
        context_.linePointLeftSizeEnabledCheck->setChecked(linePointLeftSize.has_value());
        context_.linePointLeftSizeSpin->setEnabled(context_.linePointLeftSizeEnabledCheck->isChecked());
        context_.linePointLeftSizeSpin->setValue(linePointLeftSize.value_or(40.0));
    } else {
        context_.linePointLeftSizeEnabledCheck->setChecked(false);
        context_.linePointLeftSizeSpin->setEnabled(false);
        context_.linePointLeftSizeSpin->setValue(40.0);
    }
    if (pointAlignApplicable) {
        const int alignIndex = indexForComboUserData(context_.pointAlignCombo, pointAlign);
        context_.pointAlignCombo->setCurrentIndex(alignIndex >= 0 ? alignIndex : 0);
    } else {
        context_.pointAlignCombo->setCurrentIndex(0);
    }
    context_.linePointSegmentSubtypeLabel->setVisible(linePointSegmentSubtypeApplicable);
    context_.linePointSegmentSubtypeCombo->setVisible(linePointSegmentSubtypeApplicable);
    context_.linePointSegmentSubtypeCombo->setEnabled(linePointSegmentSubtypeApplicable);
    if (linePointSegmentSubtypeApplicable) {
        setEditableComboValues(context_.linePointSegmentSubtypeCombo,
                               inspectorSubtypeValuesForCommandTypeWithEmptyChoice(inspectorSymbolCatalog(),
                                                                                   QStringLiteral("line"),
                                                                                   linePointType),
                               linePointSegmentSubtype);
    } else {
        context_.linePointSegmentSubtypeCombo->clear();
    }
    context_.linePointAltitudeAutoCheck->setVisible(linePointAltitudeAutoApplicable);
    context_.linePointAltitudeAutoCheck->setEnabled(linePointAltitudeAutoApplicable);
    context_.linePointAltitudeAutoCheck->setChecked(linePointAltitudeAutoApplicable
                                                    && linePointAltitudeAutoFromStandaloneRows(linePointFlagsRows));
    context_.linePointFlagsEditor->setVisible(linePointFlagsApplicable);
    context_.linePointFlagsEdit->setEnabled(linePointFlagsApplicable);
    {
        const QSignalBlocker blocker(context_.linePointFlagsEdit);
        context_.linePointFlagsEdit->setPlainText(
            linePointRowsWithoutStructuredStandaloneOptions(linePointFlagsRows,
                                                            linePointSegmentSubtypeApplicable,
                                                            linePointAltitudeAutoApplicable).join(QLatin1Char('\n')));
    }
    reorderSelectionControls(context_.lineOptionsEditor,
                             {context_.quickNameEditor,
                              context_.quickTextEditor,
                              context_.quickValueEditor,
                              context_.lineClosedCheck,
                              context_.lineReversedCheck,
                              context_.objectClipDisabledCheck,
                              context_.configureButtonRow});
    reorderSelectionControls(context_.scrapOptionsEditor,
                             {context_.quickNameEditor,
                              context_.quickTextEditor,
                              context_.quickValueEditor,
                              context_.scrapProjectionEditor,
                              context_.configureButtonRow});
    reorderSelectionControls(context_.orientationEditor,
                             {context_.quickNameEditor,
                              context_.quickTextEditor,
                              context_.quickValueEditor,
                              context_.linePointPreviousControlCheck->parentWidget(),
                              context_.orientationEnabledCheck,
                              context_.orientationSpin,
                              context_.linePointLeftSizeEnabledCheck,
                              context_.linePointLeftSizeSpin,
                              context_.linePointSegmentSubtypeLabel->parentWidget(),
                              context_.linePointAltitudeAutoCheck,
                              context_.pointAlignEditor,
                              context_.objectClipDisabledCheck,
                              context_.linePointFlagsEditor,
                              context_.configureButtonRow});
    context_.vertexSection->setVisible(lineVertexActionsAvailable
                                       || objectQuickOptionsVisible
                                       || linePointSmoothApplicable
                                       || orientationApplicable
                                       || pointAlignApplicable
                                       || linePointLeftSizeApplicable
                                       || linePointSegmentSubtypeApplicable
                                       || linePointAltitudeAutoApplicable
                                       || linePointFlagsApplicable);
}
}
