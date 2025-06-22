#pragma once

#include "nlohmann/detail/macro_scope.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace chest_menu {

struct MItem {
    std::string              name="LeafKnife";
    std::string              type="minecraft:grass";
    int                      aux=0;
    int                      count=1;
    std::vector<std::string> lores;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MItem, name, type, aux, count, lores);
};

struct Button {
    std::string name="LeafKnife";
    MItem       item;
    int         slot=0;
    std::string command="/say test";
    std::string icon;
    bool        isShow=true;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Button, name, item, slot, command, icon, isShow);
};

struct Menu {
    std::string         id="leafknife";
    std::string         title="LeafKnife Test";
    int                 permission=0;
    std::vector<Button> buttons;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Menu, id, title, permission, buttons);
};

struct MenuData {
    int            version=1;
    Menu           main;
    nlohmann::json menus;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MenuData, main, menus);
};

} // namespace chest_menu