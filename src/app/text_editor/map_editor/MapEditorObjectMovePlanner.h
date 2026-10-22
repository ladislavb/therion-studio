#pragma once

#include <QString>
#include <QVector>

namespace TherionStudio
{
enum class MapEditorObjectMovePosition
{
    BeforeTarget,
    AfterTarget,
    IntoTargetScrap
};

struct MapEditorObjectMovePlan
{
    bool resolved = false;
    bool changed = false;
    int sourceStartLine = 0;
    int sourceEndLine = 0;
    int insertBeforeLineOriginal = 0;
    int insertBeforeLineAfterRemoval = 0;
    QString movedText;
    QString errorMessage;
};

class MapEditorObjectMovePlanner final
{
public:
    [[nodiscard]] static MapEditorObjectMovePlan planMove(const QString &text,
                                                          int sourceLineNumber,
                                                          int targetLineNumber,
                                                          MapEditorObjectMovePosition position);

    // Moves the point, line and area blocks starting at the given lines to the end of the target scrap, keeping their
    // order. insertBeforeLineAfterRemoval is the new start line of the block that began at focusLineNumber.
    [[nodiscard]] static MapEditorObjectMovePlan planMoveIntoScrap(const QString &text,
                                                                   const QVector<int> &objectLineNumbers,
                                                                   int focusLineNumber,
                                                                   int targetScrapLineNumber);

    [[nodiscard]] static bool applyMove(QString *text,
                                        int sourceLineNumber,
                                        int targetLineNumber,
                                        MapEditorObjectMovePosition position,
                                        QString *errorMessage = nullptr);
};
}
