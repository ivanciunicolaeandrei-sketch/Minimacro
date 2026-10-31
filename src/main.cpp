#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    struct MacroEvent {
        uint32_t frame;
        uint8_t button;
        bool player2;
        bool down;
    };

    std::vector<MacroEvent> g_events;
    uint32_t g_frame = 0;
    size_t g_playIndex = 0;
    bool g_recording = false;
    bool g_playing = false;

    std::string jsonEscape(std::string const& value) {
        std::string out;
        for (unsigned char c : value) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c >= 0x20) out += static_cast<char>(c);
                    break;
            }
        }
        return out;
    }

    std::string safeName(std::string name) {
        for (char& c : name) {
            if (c == '/' || c == '\\\\' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        }
        if (name.empty()) name = "Geometry Dash";
        return name;
    }

    void recordInput(PlayerObject* player, PlayerButton button, bool down) {
        if (!g_recording || g_playing) return;
        auto pl = PlayLayer::get();
        if (!pl) return;

        bool p2 = player == pl->m_player2;
        if (player != pl->m_player1 && !p2) return;

        g_events.push_back({
            g_frame,
            static_cast<uint8_t>(button),
            p2,
            down
        });
    }

    bool saveMacro() {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_level || g_events.empty()) return false;

        auto folder = Mod::get()->getSaveDir() / "macros";
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        if (ec) return false;

        auto path = folder / (safeName(pl->m_level->m_levelName) + ".gdr.json");
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;

        const float duration = static_cast<float>(g_frame) / 240.f;
        file << "{\n"
             << "  \"gameVersion\": 2.2081,\n"
             << "  \"description\": \"Recorded with MiniMacro\",\n"
             << "  \"version\": 1.0,\n"
             << "  \"duration\": " << duration << ",\n"
             << "  \"author\": \"MiniMacro\",\n"
             << "  \"bot\": {\"name\": \"MiniMacro\", \"version\": \"0.1.0\"},\n"
             << "  \"level\": {\"id\": " << pl->m_level->m_levelID.value()
             << ", \"name\": \"" << jsonEscape(pl->m_level->m_levelName) << "\"},\n"
             << "  \"framerate\": 240,\n"
             << "  \"inputs\": [\n";

        for (size_t i = 0; i < g_events.size(); ++i) {
            auto const& e = g_events[i];
            file << "    {\"frame\": " << e.frame
                 << ", \"btn\": " << static_cast<int>(e.button)
                 << ", \"2p\": " << (e.player2 ? "true" : "false")
                 << ", \"down\": " << (e.down ? "true" : "false") << "}";
            file << (i + 1 == g_events.size() ? "\n" : ",\n");
        }
        file << "  ],\n  \"lastRecordedFrame\": " << g_frame << "\n}\n";
        return file.good();
    }

    void beginRecording() {
        g_events.clear();
        g_frame = 0;
        g_playIndex = 0;
        g_playing = false;
        g_recording = true;
    }

    void stopRecording() {
        g_recording = false;
    }

    void startPlayback() {
        if (g_events.empty()) return;
        g_recording = false;
        g_playing = true;
        g_frame = 0;
        g_playIndex = 0;
    }

    void tickPlayback() {
        if (!g_playing) return;
        auto pl = PlayLayer::get();
        if (!pl) { g_playing = false; return; }

        while (g_playIndex < g_events.size() && g_events[g_playIndex].frame <= g_frame) {
            auto const& e = g_events[g_playIndex++];
            auto player = e.player2 ? pl->m_player2 : pl->m_player1;
            if (!player) continue;
            auto button = static_cast<PlayerButton>(e.button);
            if (e.down) player->pushButton(button);
            else player->releaseButton(button);
        }

        if (g_playIndex >= g_events.size()) g_playing = false;
    }
}

class $modify(MiniMacroPlayer, PlayerObject) {
    bool pushButton(PlayerButton button) {
        bool result = PlayerObject::pushButton(button);
        recordInput(this, button, true);
        return result;
    }

    bool releaseButton(PlayerButton button) {
        bool result = PlayerObject::releaseButton(button);
        recordInput(this, button, false);
        return result;
    }
};

class $modify(MiniMacroPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        if (g_recording || g_playing) ++g_frame;
        tickPlayback();
    }

    void onQuit() {
        g_recording = false;
        g_playing = false;
        PlayLayer::onQuit();
    }
};

class $modify(MiniMacroPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({winSize.width - 90.f, winSize.height - 35.f});

        auto addButton = [&](char const* label, char const* texture, SEL_MenuHandler handler, float y) {
            auto sprite = ButtonSprite::create(label, "bigFont.fnt", texture, 0.65f);
            sprite->setScale(0.42f);
            auto item = CCMenuItemSpriteExtra::create(sprite, this, handler);
            item->setPosition({0.f, y});
            menu->addChild(item);
        };

        addButton(g_recording ? "Stop Rec" : "Record",
                  g_recording ? "GJ_button_06.png" : "GJ_button_01.png",
                  menu_selector(MiniMacroPauseLayer::onRecord), 0.f);
        addButton("Save GDR", "GJ_button_02.png",
                  menu_selector(MiniMacroPauseLayer::onSave), -34.f);
        addButton("Play", "GJ_button_04.png",
                  menu_selector(MiniMacroPauseLayer::onPlay), -68.f);

        this->addChild(menu, 100);
    }

    void onRecord(CCObject*) {
        if (g_recording) {
            stopRecording();
            FLAlertLayer::create("MiniMacro", "Recording stopped.", "OK")->show();
        } else {
            beginRecording();
            FLAlertLayer::create("MiniMacro", "Recording started. Pause again to stop, then tap Save GDR.", "OK")->show();
        }
    }

    void onSave(CCObject*) {
        if (saveMacro()) {
            FLAlertLayer::create("MiniMacro", "Saved GDR1 JSON in Geode save folder: macros/", "OK")->show();
        } else {
            FLAlertLayer::create("MiniMacro", "Nothing to save, or the file could not be written.", "OK")->show();
        }
    }

    void onPlay(CCObject*) {
        if (g_events.empty()) {
            FLAlertLayer::create("MiniMacro", "Record a macro first in this session.", "OK")->show();
            return;
        }
        startPlayback();
        FLAlertLayer::create("MiniMacro", "Playing the current session recording.", "OK")->show();
    }
};

$on_mod(Loaded) {
    log::info("MiniMacro loaded: session recorder and GDR1 JSON export enabled.");
}
