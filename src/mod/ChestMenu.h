#pragma once

#include <mc/world/actor/player/Player.h>
#include <string>

namespace chest_menu {

void registerCmd();
void listenEvents();
void removeListen();
void loadMenus();
void renderMenu(Player& player, const std::string& id);

} // namespace chest_menu