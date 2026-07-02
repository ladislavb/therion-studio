#include "MapEditorTab.h"

#include "MapEditorObjectDetailsContext.h"
#include "../TextEditorTab.h"
#include "../../../core/ISessionStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTabWidget>

namespace TherionStudio
{
namespace
{
QString normalizedPendingCommandKind(const QString &commandKind)
{
    const QString normalized = commandKind.trimmed().toLower();
    if (normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area")) {
        return normalized;
    }
    return QString();
}

QString defaultPendingTypeForCommand(const QString &commandKind)
{
    if (commandKind == QStringLiteral("point")) {
        return QStringLiteral("station");
    }
    if (commandKind == QStringLiteral("line")) {
        return QStringLiteral("wall");
    }
    if (commandKind == QStringLiteral("area")) {
        return QStringLiteral("water");
    }
    return QString();
}

bool pendingPointNameVisible(const QString &type)
{
    return type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0;
}

QString nextStationName(const QString &stationName)
{
    static const QRegularExpression numericTailPattern(QStringLiteral("^(.*?)(\\d+)(@.*)?$"));
    static const QRegularExpression alphabeticTailPattern(QStringLiteral("^(.*?)([A-Za-z]+)(@.*)?$"));
    const QString trimmed = stationName.trimmed();
    const QRegularExpressionMatch numericMatch = numericTailPattern.match(trimmed);
    if (numericMatch.hasMatch()) {
        const QString prefix = numericMatch.captured(1);
        const QString numberToken = numericMatch.captured(2);
        const QString suffix = numericMatch.captured(3);
        bool ok = false;
        const qulonglong number = numberToken.toULongLong(&ok);
        if (!ok) {
            return QString();
        }

        QString nextNumber = QString::number(number + 1);
        if (nextNumber.size() < numberToken.size()) {
            nextNumber = QString(numberToken.size() - nextNumber.size(), QLatin1Char('0')) + nextNumber;
        }
        return prefix + nextNumber + suffix;
    }

    const QRegularExpressionMatch alphabeticMatch = alphabeticTailPattern.match(trimmed);
    if (!alphabeticMatch.hasMatch()) {
        return QString();
    }

    const QString prefix = alphabeticMatch.captured(1);
    const QString letterToken = alphabeticMatch.captured(2);
    const QString suffix = alphabeticMatch.captured(3);
    if (prefix.isEmpty()) {
        return QString();
    }

    const bool upperCase = letterToken == letterToken.toUpper();
    QString nextToken = upperCase ? letterToken.toUpper() : letterToken.toLower();
    int index = nextToken.size() - 1;
    while (index >= 0) {
        const QChar current = nextToken.at(index);
        if (current == (upperCase ? QLatin1Char('Z') : QLatin1Char('z'))) {
            nextToken[index] = upperCase ? QLatin1Char('A') : QLatin1Char('a');
            --index;
            continue;
        }

        nextToken[index] = QChar(current.unicode() + 1);
        return prefix + nextToken + suffix;
    }

    nextToken.prepend(upperCase ? QLatin1Char('A') : QLatin1Char('a'));
    return prefix + nextToken + suffix;
}

bool isPendingStationPoint(const QString &commandKind, const QString &type)
{
    return commandKind.trimmed().compare(QStringLiteral("point"), Qt::CaseInsensitive) == 0
        && type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0;
}

bool pendingLabelTextVisible(const QString &commandKind, const QString &type, const QString &text)
{
    const QString normalizedCommand = commandKind.trimmed().toLower();
    return (normalizedCommand == QStringLiteral("point") || normalizedCommand == QStringLiteral("line"))
        && (type.trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0
            || !text.trimmed().isEmpty());
}

bool pendingValueVisible(const InspectorSymbolCatalog &catalog,
                         const QString &commandKind,
                         const QString &type,
                         const QString &value)
{
    return inspectorValueOptionSupportedForCommandType(catalog, commandKind, type)
        || !value.trimmed().isEmpty();
}

bool remembersPendingTypeForCommand(const QString &commandKind)
{
    return commandKind == QStringLiteral("point")
        || commandKind == QStringLiteral("line")
        || commandKind == QStringLiteral("area");
}

QString recentPendingTypeKey(const InspectorObjectQuickFields &fields)
{
    return fields.type.trimmed().toLower() + QLatin1Char('\n') + fields.subtype.trimmed().toLower();
}

void recordRecentPendingInsertFields(QHash<QString, QVector<InspectorObjectQuickFields>> *recentByCommand,
                                     const InspectorObjectQuickFields &fields)
{
    if (recentByCommand == nullptr) {
        return;
    }

    const QString normalizedCommand = normalizedPendingCommandKind(fields.commandKind);
    if (!remembersPendingTypeForCommand(normalizedCommand) || fields.type.trimmed().isEmpty()) {
        return;
    }

    InspectorObjectQuickFields rememberedFields;
    rememberedFields.commandKind = normalizedCommand;
    rememberedFields.type = fields.type.trimmed();
    rememberedFields.subtype = fields.subtype.trimmed();

    QVector<InspectorObjectQuickFields> recent = recentByCommand->value(normalizedCommand);
    const QString key = recentPendingTypeKey(rememberedFields);
    for (auto it = recent.begin(); it != recent.end();) {
        if (recentPendingTypeKey(*it) == key) {
            it = recent.erase(it);
        } else {
            ++it;
        }
    }

    recent.prepend(rememberedFields);
    while (recent.size() > 6) {
        recent.removeLast();
    }
    recentByCommand->insert(normalizedCommand, recent);
}

QString serializeRecentPendingInsertFields(
    const QHash<QString, QVector<InspectorObjectQuickFields>> &recentByCommand)
{
    QJsonObject root;
    const QStringList supportedCommands{QStringLiteral("point"), QStringLiteral("line"), QStringLiteral("area")};
    for (const QString &command : supportedCommands) {
        const QVector<InspectorObjectQuickFields> recentFields = recentByCommand.value(command);
        if (recentFields.isEmpty()) {
            continue;
        }

        QJsonArray commandArray;
        for (const InspectorObjectQuickFields &fields : recentFields) {
            const QString type = fields.type.trimmed();
            if (type.isEmpty()) {
                continue;
            }

            QJsonObject fieldsObject;
            fieldsObject.insert(QStringLiteral("type"), type);
            fieldsObject.insert(QStringLiteral("subtype"), fields.subtype.trimmed());
            commandArray.append(fieldsObject);
        }
        if (!commandArray.isEmpty()) {
            root.insert(command, commandArray);
        }
    }

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QHash<QString, QVector<InspectorObjectQuickFields>> deserializeRecentPendingInsertFields(const QString &json)
{
    QHash<QString, QVector<InspectorObjectQuickFields>> recentByCommand;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return recentByCommand;
    }

    const QJsonObject root = document.object();
    const QStringList supportedCommands{QStringLiteral("point"), QStringLiteral("line"), QStringLiteral("area")};
    for (const QString &command : supportedCommands) {
        const QJsonValue value = root.value(command);
        if (!value.isArray()) {
            continue;
        }

        const QJsonArray commandArray = value.toArray();
        for (int index = commandArray.size() - 1; index >= 0; --index) {
            const QJsonValue entryValue = commandArray.at(index);
            if (!entryValue.isObject()) {
                continue;
            }
            const QJsonObject entryObject = entryValue.toObject();
            InspectorObjectQuickFields fields;
            fields.commandKind = command;
            fields.type = entryObject.value(QStringLiteral("type")).toString().trimmed();
            fields.subtype = entryObject.value(QStringLiteral("subtype")).toString().trimmed();
            if (fields.type.isEmpty()) {
                continue;
            }
            recordRecentPendingInsertFields(&recentByCommand, fields);
        }
    }

    return recentByCommand;
}
}

void MapEditorTab::beginPendingInsertObject(const QString &commandKind)
{
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    InspectorObjectQuickFields fields;
    fields.commandKind = normalizedCommand;
    fields.typeEditable = normalizedCommand != QStringLiteral("scrap");
    fields.type = defaultPendingTypeForCommand(normalizedCommand);
    const QVector<InspectorObjectQuickFields> recentFields =
        interactiveDrawState_.recentPendingInsertFieldsByCommand_.value(normalizedCommand);
    if (!recentFields.isEmpty()) {
        if (!recentFields.constFirst().type.trimmed().isEmpty()) {
            fields.type = recentFields.constFirst().type;
        }
        fields.subtype = recentFields.constFirst().subtype;
    }
    if (normalizedCommand == QStringLiteral("scrap")) {
        fields.identifier = QStringLiteral("new-scrap");
    } else if (normalizedCommand == QStringLiteral("point")) {
        if (fields.type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0) {
            fields.name = nextStationName(interactiveDrawState_.lastPendingStationName_);
        }
        fields.nameVisible = pendingPointNameVisible(fields.type);
    }
    fields.textVisible = pendingLabelTextVisible(normalizedCommand, fields.type, fields.text);
    fields.valueVisible = pendingValueVisible(inspectorSymbolCatalog_, normalizedCommand, fields.type, fields.value);
    interactiveDrawState_.pendingInsertFields_ = fields;
    interactiveDrawState_.pendingInsertFieldsVisible_ = true;
    interactiveDrawState_.pendingTargetScrapIdentifier_.clear();
    if (normalizedCommand == QStringLiteral("line") || normalizedCommand == QStringLiteral("area")) {
        interactiveDrawState_.currentLinePointSegmentSubtype_.clear();
    }
    if (normalizedCommand != QStringLiteral("scrap")) {
        if (const std::optional<InspectorScrapContext> targetContext = pendingInsertTargetScrapContext()) {
            if (!targetContext->willBeCreated) {
                interactiveDrawState_.pendingTargetScrapIdentifier_ = targetContext->identifier.trimmed();
            }
        }
    }
}

void MapEditorTab::clearPendingInsertObject()
{
    interactiveDrawState_.pendingInsertFields_ = InspectorObjectQuickFields{};
    interactiveDrawState_.pendingTargetScrapIdentifier_.clear();
    interactiveDrawState_.pendingInsertFieldsVisible_ = false;
}

std::optional<InspectorObjectQuickFields> MapEditorTab::pendingInsertQuickFields() const
{
    if (!interactiveDrawState_.pendingInsertFieldsVisible_
        || interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return interactiveDrawState_.pendingInsertFields_;
}

std::optional<InspectorScrapContext> MapEditorTab::pendingInsertTargetScrapContext() const
{
    const QVector<TherionParsedLine> parsedLines = parsedLinesForCurrentDocument();
    const QString explicitTarget = interactiveDrawState_.pendingTargetScrapIdentifier_.trimmed();
    if (!explicitTarget.isEmpty()) {
        for (const TherionParsedLine &parsedLine : parsedLines) {
            if (parsedLine.directive == QStringLiteral("scrap")
                && parsedLine.tokens.value(1).compare(explicitTarget, Qt::CaseInsensitive) == 0) {
                InspectorScrapContext context;
                context.identifier = parsedLine.tokens.value(1);
                context.lineNumber = parsedLine.lineNumber;
                return context;
            }
        }

        InspectorScrapContext staleContext;
        staleContext.identifier = explicitTarget;
        return staleContext;
    }

    if (objectSelectionState_.selectedObjectLineNumber_ > 0) {
        if (const std::optional<InspectorScrapContext> selectionContext =
                inspectorScrapContextForSourceLine(parsedLines, objectSelectionState_.selectedObjectLineNumber_)) {
            return selectionContext;
        }
    }

    return inspectorDraftInsertionScrapContext(parsedLines);
}

void MapEditorTab::setPendingInsertTargetScrapIdentifier(const QString &identifier)
{
    interactiveDrawState_.pendingTargetScrapIdentifier_ = identifier.trimmed();
}

void MapEditorTab::setPendingInsertQuickFields(const InspectorObjectQuickFields &fields)
{
    const QString existingCommand = interactiveDrawState_.pendingInsertFields_.commandKind;
    const bool previousStationPoint =
        isPendingStationPoint(existingCommand, interactiveDrawState_.pendingInsertFields_.type);
    const QString normalizedCommand = normalizedPendingCommandKind(fields.commandKind.isEmpty()
                                                                       ? existingCommand
                                                                       : fields.commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    interactiveDrawState_.pendingInsertFields_ = fields;
    interactiveDrawState_.pendingInsertFieldsVisible_ = true;
    interactiveDrawState_.pendingInsertFields_.commandKind = normalizedCommand;
    interactiveDrawState_.pendingInsertFields_.typeEditable = normalizedCommand != QStringLiteral("scrap");
    if (interactiveDrawState_.pendingInsertFields_.type.trimmed().isEmpty()) {
        interactiveDrawState_.pendingInsertFields_.type = defaultPendingTypeForCommand(normalizedCommand);
    }
    if (normalizedCommand == QStringLiteral("point")) {
        const bool currentStationPoint =
            isPendingStationPoint(normalizedCommand, interactiveDrawState_.pendingInsertFields_.type);
        if (currentStationPoint) {
            if (!previousStationPoint
                && interactiveDrawState_.pendingInsertFields_.name.trimmed().isEmpty()) {
                interactiveDrawState_.pendingInsertFields_.name =
                    nextStationName(interactiveDrawState_.lastPendingStationName_);
            }
        } else {
            interactiveDrawState_.pendingInsertFields_.name.clear();
        }
        interactiveDrawState_.pendingInsertFields_.nameVisible =
            pendingPointNameVisible(interactiveDrawState_.pendingInsertFields_.type);
    } else {
        interactiveDrawState_.pendingInsertFields_.nameVisible = false;
        interactiveDrawState_.pendingInsertFields_.name.clear();
    }
    interactiveDrawState_.pendingInsertFields_.textVisible =
        pendingLabelTextVisible(normalizedCommand,
                                interactiveDrawState_.pendingInsertFields_.type,
                                interactiveDrawState_.pendingInsertFields_.text);
    interactiveDrawState_.pendingInsertFields_.valueVisible =
        pendingValueVisible(inspectorSymbolCatalog_,
                            normalizedCommand,
                            interactiveDrawState_.pendingInsertFields_.type,
                            interactiveDrawState_.pendingInsertFields_.value);
    recordRecentPendingInsertFields(&interactiveDrawState_.recentPendingInsertFieldsByCommand_,
                                    interactiveDrawState_.pendingInsertFields_);
    persistRecentPendingInsertQuickFieldsToSession();
}

QVector<InspectorObjectQuickFields> MapEditorTab::recentPendingInsertQuickFields(const QString &commandKind) const
{
    return interactiveDrawState_.recentPendingInsertFieldsByCommand_.value(normalizedPendingCommandKind(commandKind));
}

bool MapEditorTab::pendingInsertLinePointAvailable() const
{
    const QString commandKind = interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower();
    return (commandKind == QStringLiteral("line") || commandKind == QStringLiteral("area"))
        && interactiveDrawState_.pendingInsertFieldsVisible_;
}

bool MapEditorTab::pendingInsertLinePointSmooth() const
{
    return interactiveDrawState_.nextLinePointSmooth_;
}

void MapEditorTab::setPendingInsertLinePointSmooth(bool smooth)
{
    if (!pendingInsertLinePointAvailable()) {
        return;
    }
    interactiveDrawState_.nextLinePointSmooth_ = smooth;
    interactiveDrawState_.nextLinePointIncomingControl_ = smooth;
    interactiveDrawState_.nextLinePointOutgoingControl_ = smooth;
    toolbarStatusNote_ = smooth
        ? tr("Line point smooth enabled for drawing.")
        : tr("Line point smooth disabled for drawing.");
    refreshToolbarSummary();
}

bool MapEditorTab::pendingInsertLinePointIncomingControl() const
{
    return interactiveDrawState_.nextLinePointIncomingControl_;
}

bool MapEditorTab::pendingInsertLinePointOutgoingControl() const
{
    return interactiveDrawState_.nextLinePointOutgoingControl_;
}

void MapEditorTab::setPendingInsertLinePointControl(bool incoming, bool enabled)
{
    if (!pendingInsertLinePointAvailable()) {
        return;
    }
    if (incoming) {
        interactiveDrawState_.nextLinePointIncomingControl_ = enabled;
    } else {
        interactiveDrawState_.nextLinePointOutgoingControl_ = enabled;
    }
    if (!enabled) {
        interactiveDrawState_.nextLinePointSmooth_ = false;
    }
    toolbarStatusNote_ = enabled
        ? tr("Line point control handle enabled for drawing.")
        : tr("Line point control handle disabled for drawing.");
    refreshToolbarSummary();
}

QString MapEditorTab::pendingInsertLinePointSegmentSubtype() const
{
    return interactiveDrawState_.currentLinePointSegmentSubtype_;
}

void MapEditorTab::setPendingInsertLinePointSegmentSubtype(const QString &subtype)
{
    if (!pendingInsertLinePointAvailable()) {
        return;
    }
    const QString normalizedSubtype = subtype.trimmed();
    interactiveDrawState_.currentLinePointSegmentSubtype_ = normalizedSubtype;
    toolbarStatusNote_ = normalizedSubtype.isEmpty()
        ? tr("Line point subtype override cleared for drawing.")
        : tr("Line point subtype override set to %1 for drawing.").arg(normalizedSubtype);
    refreshToolbarSummary();
}

bool MapEditorTab::pendingInsertLinePointOrientationEnabled() const
{
    return interactiveDrawState_.nextLinePointOrientationEnabled_;
}

qreal MapEditorTab::pendingInsertLinePointOrientationDegrees() const
{
    return interactiveDrawState_.nextLinePointOrientationDegrees_;
}

void MapEditorTab::setPendingInsertLinePointOrientation(bool enabled, qreal degrees)
{
    if (!pendingInsertLinePointAvailable()) {
        return;
    }
    interactiveDrawState_.nextLinePointOrientationEnabled_ = enabled;
    interactiveDrawState_.nextLinePointOrientationDegrees_ = normalizeOrientationDegreesForMapDetails(degrees);
    toolbarStatusNote_ = enabled
        ? tr("Line point orientation enabled for drawing.")
        : tr("Line point orientation cleared for drawing.");
    refreshToolbarSummary();
}

bool MapEditorTab::pendingInsertLinePointLeftSizeEnabled() const
{
    return interactiveDrawState_.nextLinePointLeftSizeEnabled_;
}

qreal MapEditorTab::pendingInsertLinePointLeftSize() const
{
    return interactiveDrawState_.nextLinePointLeftSize_;
}

void MapEditorTab::setPendingInsertLinePointLeftSize(bool enabled, qreal leftSize)
{
    if (!pendingInsertLinePointAvailable()) {
        return;
    }
    interactiveDrawState_.nextLinePointLeftSizeEnabled_ = enabled;
    interactiveDrawState_.nextLinePointLeftSize_ = qMax<qreal>(0.1, leftSize);
    toolbarStatusNote_ = enabled
        ? tr("Line point left size enabled for drawing.")
        : tr("Line point left size cleared for drawing.");
    refreshToolbarSummary();
}

void MapEditorTab::applyRecentPendingInsertQuickFields(int index)
{
    std::optional<InspectorObjectQuickFields> pendingFields = pendingInsertQuickFields();
    if (!pendingFields.has_value()) {
        return;
    }

    const QVector<InspectorObjectQuickFields> recentFields = recentPendingInsertQuickFields(pendingFields->commandKind);
    if (index < 0 || index >= recentFields.size()) {
        return;
    }

    pendingFields->type = recentFields.at(index).type;
    pendingFields->subtype = recentFields.at(index).subtype;
    setPendingInsertQuickFields(*pendingFields);
    refreshObjectDetailsPanel();
}

void MapEditorTab::restoreRecentPendingInsertQuickFieldsFromSession()
{
    interactiveDrawState_.recentPendingInsertFieldsByCommand_.clear();
    if (sessionStore_ == nullptr) {
        return;
    }

    interactiveDrawState_.recentPendingInsertFieldsByCommand_ =
        deserializeRecentPendingInsertFields(sessionStore_->therionMapRecentInsertTypeSubtypeHistory());
}

void MapEditorTab::persistRecentPendingInsertQuickFieldsToSession() const
{
    if (sessionStore_ == nullptr) {
        return;
    }

    sessionStore_->setTherionMapRecentInsertTypeSubtypeHistory(
        serializeRecentPendingInsertFields(interactiveDrawState_.recentPendingInsertFieldsByCommand_));
}

TherionDraftObjectOptions MapEditorTab::pendingDraftObjectOptions(const QString &commandKind) const
{
    TherionDraftObjectOptions options;
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()
        || interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != normalizedCommand) {
        return options;
    }

    options.type = interactiveDrawState_.pendingInsertFields_.type;
    options.subtype = interactiveDrawState_.pendingInsertFields_.subtype;
    options.identifier = interactiveDrawState_.pendingInsertFields_.identifier;
    options.targetScrapIdentifier = interactiveDrawState_.pendingTargetScrapIdentifier_;
    if (normalizedCommand == QStringLiteral("point")) {
        options.name = interactiveDrawState_.pendingInsertFields_.name;
        options.nameEnabled = interactiveDrawState_.pendingInsertFields_.nameVisible;
    }
    options.text = interactiveDrawState_.pendingInsertFields_.text;
    options.textEnabled = interactiveDrawState_.pendingInsertFields_.textVisible;
    options.value = interactiveDrawState_.pendingInsertFields_.value;
    options.valueEnabled = interactiveDrawState_.pendingInsertFields_.valueVisible;
    return options;
}

void MapEditorTab::recordCommittedPendingDraftObjectOptions(const QString &commandKind,
                                                            const TherionDraftObjectOptions &options)
{
    const QString normalizedCommand = commandKind.trimmed().toLower();
    if (normalizedCommand == QStringLiteral("line") || normalizedCommand == QStringLiteral("area")) {
        interactiveDrawState_.currentLinePointSegmentSubtype_.clear();
    }
    if (isPendingStationPoint(commandKind, options.type)
        && options.nameEnabled
        && !options.name.trimmed().isEmpty()) {
        const QString committedName = options.name.trimmed();
        interactiveDrawState_.lastPendingStationName_ = committedName;
        if (isPendingStationPoint(interactiveDrawState_.pendingInsertFields_.commandKind,
                                  interactiveDrawState_.pendingInsertFields_.type)
            && interactiveDrawState_.pendingInsertFields_.name.trimmed() == committedName) {
            interactiveDrawState_.pendingInsertFields_.name = nextStationName(committedName);
        }
    }
}

QString MapEditorTab::pendingScrapPreferredName() const
{
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != QStringLiteral("scrap")) {
        return QStringLiteral("new-scrap");
    }

    const QString identifier = interactiveDrawState_.pendingInsertFields_.identifier.trimmed();
    return identifier.isEmpty() ? QStringLiteral("new-scrap") : identifier;
}

QString MapEditorTab::pendingScrapOptions(const QString &scaleOption) const
{
    QStringList options;
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() == QStringLiteral("scrap")) {
        const QString projection = interactiveDrawState_.pendingInsertFields_.projection.trimmed();
        if (!projection.isEmpty()) {
            options.append(QStringLiteral("-projection %1").arg(projection));
        }
    }
    if (!scaleOption.trimmed().isEmpty()) {
        options.append(scaleOption.trimmed());
    }
    return options.join(QLatin1Char(' '));
}

void MapEditorTab::activateSelectionInspector()
{
    if (mapInspectorTabs_ != nullptr) {
        mapInspectorTabs_->setCurrentIndex(0);
    }
}

MapEditorObjectDetailsContext MapEditorTab::objectDetailsContext()
{
    return MapEditorObjectDetailsContext{
        .callbackContext = this,
        .textEditor = textEditor_,
        .inspectorSymbolCatalog = &inspectorSymbolCatalog_,
        .orientationApplicabilityByCommand = &orientationApplicabilityByCommand_,
        .updatingUi = &objectDetailsUiState_.updatingObjectDetailsUi_,
        .selectedObjectLineNumber = &objectSelectionState_.selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &objectSelectionState_.selectedObjectVertexIndex_,
        .selectedObjectKind = &objectSelectionState_.selectedObjectKind_,
        .objectQuickCommandKind = &objectDetailsUiState_.objectQuickCommandKind_,
        .hiddenInspectorObjectLines = &hiddenInspectorObjectLines_,
        .lastInspectorClickedObjectLineNumber = &lastInspectorClickedObjectLineNumber_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectionLabel = objectDetailsUiState_.objectDetailsSelectionLabel_,
        .emptySelectionSection = objectDetailsUiState_.objectDetailsEmptySelectionSection_,
        .selectionSection = objectDetailsUiState_.objectSelectionSection_,
        .selectionTitleLabel = objectDetailsUiState_.objectSelectionTitleLabel_,
        .vertexSection = objectDetailsUiState_.vertexSelectionSection_,
        .vertexTitleLabel = objectDetailsUiState_.vertexSelectionTitleLabel_,
        .linePointActionsSection = objectDetailsUiState_.linePointActionsSection_,
        .geometrySection = objectDetailsUiState_.geometrySelectionSection_,
        .advancedSection = objectDetailsUiState_.advancedSelectionSection_,
        .deleteButton = objectDetailsUiState_.objectDeleteButton_,
        .areaReferenceLabel = objectDetailsUiState_.objectAreaReferenceLabel_,
        .quickFieldsEditor = objectDetailsUiState_.objectQuickFieldsEditor_,
        .quickIdentifierLabel = objectDetailsUiState_.objectQuickIdentifierLabel_,
        .quickNameLabel = objectDetailsUiState_.objectQuickNameLabel_,
        .quickTextLabel = objectDetailsUiState_.objectQuickTextLabel_,
        .quickValueLabel = objectDetailsUiState_.objectQuickValueLabel_,
        .quickProjectionLabel = objectDetailsUiState_.objectQuickProjectionLabel_,
        .quickTypeLabel = objectDetailsUiState_.objectQuickTypeLabel_,
        .quickSubtypeLabel = objectDetailsUiState_.objectQuickSubtypeLabel_,
        .quickRecentLabel = objectDetailsUiState_.objectQuickRecentLabel_,
        .quickTargetScrapLabel = objectDetailsUiState_.objectQuickTargetScrapLabel_,
        .stylePreviewLabel = objectDetailsUiState_.objectStylePreviewLabel_,
        .quickTypeCombo = objectDetailsUiState_.objectQuickTypeCombo_,
        .quickSubtypeCombo = objectDetailsUiState_.objectQuickSubtypeCombo_,
        .quickProjectionCombo = objectDetailsUiState_.objectQuickProjectionCombo_,
        .quickTargetScrapCombo = objectDetailsUiState_.objectQuickTargetScrapCombo_,
        .quickIdentifierEdit = objectDetailsUiState_.objectQuickIdentifierEdit_,
        .quickNameEditor = objectDetailsUiState_.objectQuickNameEditor_,
        .quickTextEditor = objectDetailsUiState_.objectQuickTextEditor_,
        .quickValueEditor = objectDetailsUiState_.objectQuickValueEditor_,
        .quickRecentEditor = objectDetailsUiState_.objectQuickRecentEditor_,
        .quickRecentButtons = objectDetailsUiState_.objectQuickRecentButtons_,
        .quickNameEdit = objectDetailsUiState_.objectQuickNameEdit_,
        .quickTextEdit = objectDetailsUiState_.objectQuickTextEdit_,
        .quickValueEdit = objectDetailsUiState_.objectQuickValueEdit_,
        .stylePreview = objectDetailsUiState_.objectStylePreview_,
        .vertexActionsEditor = objectDetailsUiState_.vertexActionsEditor_,
        .vertexInsertBeforeButton = objectDetailsUiState_.vertexInsertBeforeButton_,
        .vertexInsertAfterButton = objectDetailsUiState_.vertexInsertAfterButton_,
        .vertexDeleteButton = objectDetailsUiState_.vertexDeleteButton_,
        .vertexSplitButton = objectDetailsUiState_.vertexSplitButton_,
        .metadataLabel = objectDetailsUiState_.objectDetailsMetadataLabel_,
        .orientationEditor = objectDetailsUiState_.objectOrientationEditor_,
        .linePointPreviousControlCheck = objectDetailsUiState_.linePointPreviousControlCheck_,
        .linePointSmoothCheck = objectDetailsUiState_.linePointSmoothCheck_,
        .linePointNextControlCheck = objectDetailsUiState_.linePointNextControlCheck_,
        .orientationEnabledCheck = objectDetailsUiState_.objectOrientationEnabledCheck_,
        .orientationSpin = objectDetailsUiState_.objectOrientationSpin_,
        .linePointLeftSizeEnabledCheck = objectDetailsUiState_.linePointLeftSizeEnabledCheck_,
        .linePointLeftSizeSpin = objectDetailsUiState_.linePointLeftSizeSpin_,
        .linePointSegmentSubtypeLabel = objectDetailsUiState_.linePointSegmentSubtypeLabel_,
        .linePointSegmentSubtypeCombo = objectDetailsUiState_.linePointSegmentSubtypeCombo_,
        .linePointAltitudeAutoCheck = objectDetailsUiState_.linePointAltitudeAutoCheck_,
        .linePointFlagsEditor = objectDetailsUiState_.linePointFlagsEditor_,
        .linePointFlagsEdit = objectDetailsUiState_.linePointFlagsEdit_,
        .lineOptionsEditor = objectDetailsUiState_.lineOptionsEditor_,
        .scrapOptionsEditor = objectDetailsUiState_.scrapOptionsEditor_,
        .scrapProjectionLabel = objectDetailsUiState_.scrapProjectionLabel_,
        .scrapProjectionEditor = objectDetailsUiState_.scrapProjectionEditor_,
        .quickProjectionEditor = objectDetailsUiState_.objectQuickProjectionEditor_,
        .lineClosedCheck = objectDetailsUiState_.lineClosedCheck_,
        .lineReversedCheck = objectDetailsUiState_.lineReversedCheck_,
        .objectClipDisabledCheck = objectDetailsUiState_.objectClipDisabledCheck_,
        .pointAlignEditor = objectDetailsUiState_.pointAlignEditor_,
        .pointAlignLabel = objectDetailsUiState_.pointAlignLabel_,
        .pointAlignCombo = objectDetailsUiState_.pointAlignCombo_,
        .scrapScaleEditor = objectDetailsUiState_.scrapScaleEditor_,
        .scrapScaleSourceX1Spin = objectDetailsUiState_.scrapScaleSourceX1Spin_,
        .scrapScaleSourceY1Spin = objectDetailsUiState_.scrapScaleSourceY1Spin_,
        .scrapScaleSourceX2Spin = objectDetailsUiState_.scrapScaleSourceX2Spin_,
        .scrapScaleSourceY2Spin = objectDetailsUiState_.scrapScaleSourceY2Spin_,
        .scrapScaleRealX1Spin = objectDetailsUiState_.scrapScaleRealX1Spin_,
        .scrapScaleRealY1Spin = objectDetailsUiState_.scrapScaleRealY1Spin_,
        .scrapScaleRealX2Spin = objectDetailsUiState_.scrapScaleRealX2Spin_,
        .scrapScaleRealY2Spin = objectDetailsUiState_.scrapScaleRealY2Spin_,
        .scrapScaleUnitCombo = objectDetailsUiState_.scrapScaleUnitCombo_,
        .configureButtonRow = objectDetailsUiState_.objectConfigureButtonRow_,
        .configureButton = objectDetailsUiState_.objectConfigureButton_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .mapSourceBoundsForCurrentDocument = [this]() {
            return mapSourceBoundsForCurrentDocument();
        },
        .parsedLinesForCurrentDocument = [this]() {
            return parsedLinesForCurrentDocument();
        },
        .logicalCommandsForCurrentDocument = [this]() {
            return logicalCommandsForCurrentDocument();
        },
        .pendingInsertTargetScrapContext = [this]() -> std::optional<InspectorScrapContext> {
            return pendingInsertTargetScrapContext();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .pendingInsertQuickFields = [this]() -> std::optional<InspectorObjectQuickFields> {
            return pendingInsertQuickFields();
        },
        .setPendingInsertQuickFields = [this](const InspectorObjectQuickFields &fields) {
            setPendingInsertQuickFields(fields);
        },
        .recentPendingInsertQuickFields = [this](const QString &commandKind) {
            return recentPendingInsertQuickFields(commandKind);
        },
        .pendingInsertLinePointAvailable = [this]() {
            return pendingInsertLinePointAvailable();
        },
        .pendingInsertLinePointSmooth = [this]() {
            return pendingInsertLinePointSmooth();
        },
        .setPendingInsertLinePointSmooth = [this](bool smooth) {
            setPendingInsertLinePointSmooth(smooth);
        },
        .pendingInsertLinePointIncomingControl = [this]() {
            return pendingInsertLinePointIncomingControl();
        },
        .pendingInsertLinePointOutgoingControl = [this]() {
            return pendingInsertLinePointOutgoingControl();
        },
        .setPendingInsertLinePointControl = [this](bool incoming, bool enabled) {
            setPendingInsertLinePointControl(incoming, enabled);
        },
        .pendingInsertLinePointSegmentSubtype = [this]() {
            return pendingInsertLinePointSegmentSubtype();
        },
        .setPendingInsertLinePointSegmentSubtype = [this](const QString &subtype) {
            setPendingInsertLinePointSegmentSubtype(subtype);
        },
        .pendingInsertLinePointOrientationEnabled = [this]() {
            return pendingInsertLinePointOrientationEnabled();
        },
        .pendingInsertLinePointOrientationDegrees = [this]() {
            return pendingInsertLinePointOrientationDegrees();
        },
        .setPendingInsertLinePointOrientation = [this](bool enabled, qreal degrees) {
            setPendingInsertLinePointOrientation(enabled, degrees);
        },
        .pendingInsertLinePointLeftSizeEnabled = [this]() {
            return pendingInsertLinePointLeftSizeEnabled();
        },
        .pendingInsertLinePointLeftSize = [this]() {
            return pendingInsertLinePointLeftSize();
        },
        .setPendingInsertLinePointLeftSize = [this](bool enabled, qreal leftSize) {
            setPendingInsertLinePointLeftSize(enabled, leftSize);
        },
        .clearInspectorObjectSelection = [this]() {
            clearInspectorObjectSelection();
        },
        .selectMapLine = [this](int lineNumber, bool centerOnSelection) {
            selectMapLine(lineNumber, centerOnSelection);
            syncInspectorObjectSelectionToLine(lineNumber);
        },
        .restorePointSelectionLater = [this](int lineNumber) {
            restorePointSelection(lineNumber);
        },
        .restoreLineAnchorSelectionLater = [this](int lineNumber, int sourceVertexIndex) {
            restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        },
        .applySourceTextChangeWithSnapshot = [this](const QString &label,
                                                    const QString &beforeText,
                                                    const QString &afterText,
                                                    int insertedLineNumber,
                                                    std::function<void()> selectionRestoreHook) {
            return applySourceTextChangeWithSnapshot(label,
                                                     beforeText,
                                                     afterText,
                                                     insertedLineNumber,
                                                     std::move(selectionRestoreHook));
        },
        .insertLineVertexFromSelection = [this](bool before) {
            return insertLineVertexFromSelection(before);
        },
        .splitLineAtSelection = [this]() {
            return splitLineAtSelection();
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
        .setLineVertexSmoothForSelection = [this](bool smooth) {
            return setLineVertexSmoothForSelection(smooth);
        },
        .setLineVertexControlHandleForSelection = [this](bool incoming, bool enabled) {
            return setLineVertexControlHandleForSelection(incoming, enabled);
        },
    };
}
}
