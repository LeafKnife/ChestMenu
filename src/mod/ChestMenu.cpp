#include "mod/ChestMenu.h"
#include "mod/Menu.h"
#include "mod/MyMod.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>

#include <gmlib/gm/enum/ChestType.h>
#include <gmlib/gm/ui/ChestForm.h>
#include <gmlib/mc/world/ItemStack.h>
#include <gmlib/mc/world/actor/Player.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/io/FileUtils.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/utility/MCRESULT.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandVersion.h>
#include <mc/server/commands/MinecraftCommands.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/world/Minecraft.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/item/ItemLockHelper.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/VanillaItemNames.h>

struct MenuCommand {
    std::string id;
};

namespace chest_menu {

namespace {
ll::event::ListenerPtr useItemEv;
ll::event::ListenerPtr playerJoinEv;

std::unordered_map<std::string, Menu> menusMap;

} // namespace

void registerCmd() {
    auto& cmd = ll::command::CommandRegistrar::getInstance()
                    .getOrCreateCommand("menu", "LK-Menu | LeafKnife 菜单插件", CommandPermissionLevel::Any)
                    .alias("cd");

    cmd.overload<MenuCommand>().optional("id").execute(
        [&](CommandOrigin const& origin, CommandOutput& output, MenuCommand const& param) {
            auto* entity = origin.getEntity();
            if (entity == nullptr || !entity->hasType(::ActorType::Player)) {
                output.error("command.error.notplayer");
            }
            auto* player = (Player*)entity;
            sendMenu(*player, param.id);
        }
    );

    auto& cmdReload = ll::command::CommandRegistrar::getInstance().getOrCreateCommand(
        "reloadmenu",
        "LK-Menu | 重新加载菜单",
        CommandPermissionLevel::GameDirectors
    );

    cmdReload.overload().execute([&](CommandOrigin const& origin, CommandOutput& output) {
        loadMenus();
        output.success("已重新加载menu.json文件");
    });
};

void listenEvents() {
    auto& evBus     = ll::event::EventBus::getInstance();
    auto  useItemEv = evBus.emplaceListener<ll::event::PlayerUseItemEvent>([](ll::event::PlayerUseItemEvent& ev) {
        auto& player = ev.self();
        if (ev.item().getTypeName() == VanillaItemNames::Clock().getString()) {
            sendMenu(player, "main");
        }
    });

    auto playerJoinEv = evBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& ev) {
        auto& player    = ev.self();
        auto  inventory = player.mInventory->mInventory.get();
        auto  clock     = ItemStack(VanillaItemNames::Clock().getString());
        clock.mUserData = std::make_unique<CompoundTag>(CompoundTag{
            {"ench",                    ListTag{}                                        },
            {"minecraft:keep_on_death", true                                             },
            {"minecraft:item_lock",     static_cast<uchar>(ItemLockMode::LockInInventory)}
        });
        // ItemLockHelper::setItemLockMode(clock, ::ItemLockMode::LockInInventory);
        if (inventory->getItemCount(clock) < 1) {
            inventory->addItemToFirstEmptySlot(clock);
            player.refreshInventory();
        }
    });
};

void removeListen() {
    auto& evBus = ll::event::EventBus::getInstance();
    evBus.removeListener(useItemEv);
    evBus.removeListener(playerJoinEv);
}

void loadMenus() {
    menusMap.clear();
    auto menu_path = my_mod::MyMod::getInstance().getSelf().getConfigDir() / "menu.json";
    if (!std::filesystem::exists(menu_path)) {
        my_mod::MyMod::getInstance().getSelf().getLogger().warn("未检测到menu.json文件，插件无法正常使用");
        return;
    }
    auto content = ll::file_utils::readFile(menu_path);
    if (!content.has_value()) {
        return;
    }
    auto     menu_data = nlohmann::json::parse(content.value());
    MenuData md        = menu_data;
    menusMap.emplace("main", md.main);
    if (md.menus.is_array() && !md.menus.empty()) {
        for (const auto& it : md.menus) {
            Menu menu = it;
            menusMap.emplace(menu.id, menu);
        }
    }
};

inline void sendChestUI(gmlib::GMPlayer& player, Menu const& menu) {
    auto fm = gmlib::ui::ChestForm(
        menu.title,
        menu.type == 1 ? gmlib::ui::ChestType::BigChest : gmlib::ui::ChestType::SingleChest
    );
    bool isOp = player.isOperator();
    if (menu.permission == 1 && !isOp) {
        player.sendMessage(std::string_view("权限不足，无法打开此菜单"));
        return;
    }
    for (auto& bt : menu.buttons) {
        if (!bt.isShow && !isOp) {
            continue;
        }
        auto                                item = gmlib::GMItemStack(bt.item.type, bt.item.count, bt.item.aux);
        ::Bedrock::Safety::RedactableString name;
        name.set(bt.item.name);
        item.setCustomName(name);
        item.setCustomLore(bt.item.lores);
        auto cmd = bt.command;
        fm.registerSlot(bt.slot, item, [cmd](gmlib::GMPlayer& pl) {
            // CommandContext context =
            //     CommandContext(cmd, std::make_unique<PlayerCommandOrigin>(pl), CommandVersion::CurrentVersion());
            // ll::service::getMinecraft()->mCommands->executeCommand(context, false);
            auto r = pl.executeCommand(std::string_view(cmd));
        });
    }
    fm.sendTo(player);
};

inline void sendForm(gmlib::GMPlayer& player, Menu const& menu) {
    auto fm = ll::form::SimpleForm();
    fm.setTitle(menu.title);
    bool isOp = player.isOperator();
    if (menu.permission == 1 && !isOp) {
        player.sendMessage(std::string_view("权限不足，无法打开此菜单"));
        return;
    }
    for (auto& bt : menu.buttons) {
        if (!bt.isShow && !isOp) {
            continue;
        }
        auto cmd = bt.command;
        fm.appendButton(bt.name, [cmd](Player& pl) {
            CommandContext context =
                CommandContext(cmd, std::make_unique<PlayerCommandOrigin>(pl), CommandVersion::CurrentVersion());
            ll::service::getMinecraft()->mCommands->executeCommand(context, false);
        });
    }
    fm.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason reason) {});
    // player.sendMessage(std::string_view("TODO"));
};

void sendMenu(Player& player, const std::string& id) {
    auto find = menusMap.find(id);
    auto menu = menusMap.at("main");
    if (find != menusMap.end()) {
        menu = find->second;
    }
    auto gmPlayer = gmlib::GMPlayer::getServerPlayer(player.getNetworkIdentifier(), player.getClientSubId());
    sendForm(gmPlayer, menu);
    // if ((int)gmPlayer->mBuildPlatform < 3) {
    //     sendForm(gmPlayer, menu);
    // } else {
    //     sendChestUI(gmPlayer, menu);
    // }
}

} // namespace chest_menu