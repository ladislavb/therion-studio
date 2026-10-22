#include "MapEditorScrapVisibilityMenu.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QMenu>

namespace TherionStudio
{
namespace
{
void addVisibilitySubmenu(QMenu *menu,
                          const QString &title,
                          MapEditorScrapVisibilityTarget target,
                          const std::optional<MapEditorObjectVisibility> &current,
                          const MapEditorScrapVisibilityApply &apply)
{
    QMenu *submenu = menu->addMenu(title);
    auto *group = new QActionGroup(submenu);
    const struct
    {
        MapEditorObjectVisibility visibility;
        QString label;
    } choices[] = {
        {MapEditorObjectVisibility::Shown,
         QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "Show")},
        {MapEditorObjectVisibility::Faded,
         QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "Fade")},
        {MapEditorObjectVisibility::Hidden,
         QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "Hide")},
    };
    for (const auto &choice : choices) {
        QAction *action = submenu->addAction(choice.label);
        action->setCheckable(true);
        action->setChecked(current.has_value() && current.value() == choice.visibility);
        group->addAction(action);
        const MapEditorObjectVisibility visibility = choice.visibility;
        QObject::connect(action, &QAction::triggered, submenu, [apply, target, visibility]() {
            apply(target, visibility);
        });
    }
}
}

void addMapEditorScrapVisibilityMenus(QMenu *menu,
                                      const MapEditorScrapVisibilityMenuState &state,
                                      const MapEditorScrapVisibilityApply &apply)
{
    if (menu == nullptr || !state.inScrap || !apply) {
        return;
    }

    addVisibilitySubmenu(menu,
                         QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "This Scrap"),
                         MapEditorScrapVisibilityTarget::ThisScrap,
                         state.thisScrap,
                         apply);
    if (state.hasOtherScraps) {
        addVisibilitySubmenu(menu,
                             QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "Other Scraps"),
                             MapEditorScrapVisibilityTarget::OtherScraps,
                             state.otherScraps,
                             apply);
    }
}

void addMapEditorShowAllScrapsAction(QMenu *menu, bool enabled, const std::function<void()> &showAll)
{
    if (menu == nullptr || !showAll) {
        return;
    }

    QAction *action =
        menu->addAction(QCoreApplication::translate("TherionStudio::MapEditorScrapVisibilityMenu", "Show All Scraps"));
    action->setEnabled(enabled);
    QObject::connect(action, &QAction::triggered, menu, [showAll]() {
        showAll();
    });
}
}
