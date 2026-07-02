#pragma once

#include <QSet>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionSourceLogicalCommand;

struct MapEditorAreaReference
{
    int areaLineNumber = 0;
    QString areaLabel;
    QString borderLineId;
};

QSet<int> mapEditorBorderLineNumbersForArea(const QString &text, int areaLineNumber);
QSet<int> mapEditorBorderLineNumbersForArea(const QVector<TherionSourceLogicalCommand> &commands, int areaLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QString &text, int borderLineNumber);
QVector<MapEditorAreaReference> mapEditorAreaReferencesForBorderLine(const QVector<TherionSourceLogicalCommand> &commands,
                                                                     int borderLineNumber);
}
