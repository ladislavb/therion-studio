#pragma once

#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QComboBox;
class QIcon;

namespace TherionStudio
{
struct ProjectStructureEntry;
struct TherionParsedLine;
struct TherionSourceLogicalCommand;

struct InspectorObjectQuickFields
{
    QString commandKind;
    QString type;
    QString subtype;
    QString projection;
    QString identifier;
    QString name;
    QString text;
    QString value;
    bool nameVisible = false;
    bool textVisible = false;
    bool valueVisible = false;
    bool typeEditable = true;
};

struct InspectorSymbolCatalog
{
    QHash<QString, QStringList> typeValuesByCommand;
    QHash<QString, QHash<QString, QStringList>> subtypeValuesByCommandAndType;
    QHash<QString, QSet<QString>> valueOptionTypesByCommand;
    QStringList projectionValues;
};

struct InspectorScrapScale
{
    QPointF sourcePoint1;
    QPointF sourcePoint2;
    QPointF realPoint1;
    QPointF realPoint2;
    QString unitToken = QStringLiteral("m");
};

struct InspectorScrapContext
{
    QString identifier;
    int lineNumber = 0;
    bool willBeCreated = false;
};

qreal mapSourceUnitsPerMeterFromParsedLines(const QVector<TherionParsedLine> &parsedLines);
QString xtherionDefaultScrapScaleOption(const QRectF &sourceBounds);
std::optional<InspectorScrapScale> inspectorScrapScaleFromTokens(const QStringList &tokens);
QString scrapScaleExpression(const InspectorScrapScale &scale);
InspectorScrapScale defaultInspectorScrapScale(const QRectF &sourceBounds);
std::optional<InspectorScrapContext> inspectorScrapContextForSourceLine(const QVector<TherionParsedLine> &parsedLines,
                                                                        int lineNumber);
std::optional<InspectorScrapContext> inspectorScrapContextForSourceLine(
    const QVector<TherionSourceLogicalCommand> &commands,
    int lineNumber);
InspectorScrapContext inspectorDraftInsertionScrapContext(const QVector<TherionParsedLine> &parsedLines);
InspectorScrapContext inspectorDraftInsertionScrapContext(const QVector<TherionSourceLogicalCommand> &commands);
QVector<InspectorScrapContext> inspectorScrapContexts(const QVector<TherionParsedLine> &parsedLines);
QVector<InspectorScrapContext> inspectorScrapContexts(const QVector<TherionSourceLogicalCommand> &commands);
QString inspectorMapObjectIconName(const ProjectStructureEntry &entry);
QString inspectorMapObjectItemText(const ProjectStructureEntry &entry, const TherionParsedLine *parsedLine);
QIcon inspectorActionIcon(const QString &iconName);
InspectorSymbolCatalog inspectorSymbolCatalogFromCommandCatalog(const QJsonObject &catalogObject);
QStringList inspectorTypeValuesForCommand(const InspectorSymbolCatalog &catalog, const QString &commandKind);
QStringList inspectorSubtypeValuesForCommandType(const InspectorSymbolCatalog &catalog,
                                                 const QString &commandKind,
                                                 const QString &type);
QStringList inspectorSubtypeValuesForCommandTypeWithEmptyChoice(const InspectorSymbolCatalog &catalog,
                                                                const QString &commandKind,
                                                                const QString &type);
QStringList inspectorProjectionValues(const InspectorSymbolCatalog &catalog);
bool inspectorValueOptionSupportedForCommandType(const InspectorSymbolCatalog &catalog,
                                                 const QString &commandKind,
                                                 const QString &type);
void setEditableComboValues(QComboBox *combo, const QStringList &values, const QString &currentText);
std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromParsedLine(const TherionParsedLine &parsedLine);
std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromLogicalCommand(
    const TherionSourceLogicalCommand &command);
}
