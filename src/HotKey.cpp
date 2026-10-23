#include "HotKey.hpp"

HotKey* HotKey::hotKeysHead = nullptr;
HotKey* HotKey::hotKeysTail = nullptr;

using namespace geode::prelude;

$on_mod(Loaded) {
    KeyboardInputEvent().listen([](KeyboardInputData& data) -> bool {
        if (!GameManager::get()->m_playLayer ||
            !Mod::get()->getSettingValue<bool>("ingame_enable")) return false;
        bool success = false;
        if (data.action == KeyboardInputData::Action::Press) {
            HotKey* hotKey = HotKey::hotKeysHead;
            while (hotKey) {
                if (hotKey->key == data.key) {
                    hotKey->pressed();
                    success = true;
                }
                hotKey = hotKey->next;
            }
        }
        return success;
    }).leak();
}
