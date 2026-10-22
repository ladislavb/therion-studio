#include "MapEditorMoveToScrapMenu.h"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>

namespace TherionStudio
{
void addMapEditorMoveToScrapMenu(QMenu *menu,
                                 const QVector<MapEditorMoveToScrapTarget> &targets,
                                 const std::function<void(int targetScrapLineNumber)> &move)
{
    if (menu == nullptr || targets.isEmpty() || !move) {
        return;
    }

    QMenu *submenu =
        menu->addMenu(QCoreApplication::translate("TherionStudio::MapEditorMoveToScrapMenu", "Move to Scrap"));
    submenu->setToolTipsVisible(true);
    for (const MapEditorMoveToScrapTarget &target : targets) {
        const QString label = target.scrapId.isEmpty()
            ? QCoreApplication::translate("TherionStudio::MapEditorMoveToScrapMenu", "Scrap at line %1")
                  .arg(target.scrapLineNumber)
            : target.scrapId;
        QAction *action = submenu->addAction(label);
        action->setEnabled(target.blockedReason.isEmpty());
        action->setToolTip(target.blockedReason);
        const int targetScrapLineNumber = target.scrapLineNumber;
        QObject::connect(action, &QAction::triggered, submenu, [move, targetScrapLineNumber]() {
            move(targetScrapLineNumber);
        });
    }
}
}
