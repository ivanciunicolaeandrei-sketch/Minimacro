#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
struct MacroEvent { unsigned frame; unsigned char button; bool p2; bool down; };
std::vector<MacroEvent> events;
unsigned frameNo = 0;
size_t playIndex = 0;
bool recording = false, playing = false, loopPlayback = false;
int playbackTPS = 240;
float playbackAccumulator = 0.f;
CCLayerColor* menuPanel = nullptr;

std::string safeName(std::string s) {
    for (char& c : s) if (c=='/' || c=='\\' || c==':' || c=='*' || c=='?' || c=='"' || c=='<' || c=='>' || c=='|') c='_';
    return s.empty() ? "Geometry Dash" : s;
}
void capture(PlayerObject* p, PlayerButton b, bool down) {
    if (!recording || playing) return;
    auto pl=PlayLayer::get(); if (!pl) return;
    bool p2=(p==pl->m_player2); if (p!=pl->m_player1 && !p2) return;
    events.push_back({frameNo, static_cast<unsigned char>(b), p2, down});
}
void beginRec() { events.clear(); frameNo=0; playIndex=0; playbackAccumulator=0.f; playing=false; recording=true; }
void stopRec() { recording=false; }
bool saveMacro() {
    auto pl=PlayLayer::get(); if (!pl || !pl->m_level || events.empty()) return false;
    auto dir=Mod::get()->getConfigDir()/"mm macro folder"; std::error_code ec;
    std::filesystem::create_directories(dir,ec); if(ec) return false;
    std::ofstream out(dir/(safeName(pl->m_level->m_levelName)+".gdr.json"));
    if(!out) return false;
    out << "{\n  \"format\":\"MiniMacro-session-json\",\n  \"framerate\":" << playbackTPS << ",\n  \"inputs\":[\n";
    for(size_t i=0;i<events.size();++i) {
        auto const& e=events[i];
        out << "    {\"frame\":"<<e.frame<<",\"btn\":"<<int(e.button)<<",\"2p\":"<<(e.p2?"true":"false")<<",\"down\":"<<(e.down?"true":"false")<<"}";
        out << (i+1==events.size()?"\n":",\n");
    }
    out << "  ],\n  \"lastRecordedFrame\":"<<frameNo<<"\n}\n";
    return out.good();
}
void startPlay() { if(events.empty()) return; recording=false; playing=true; frameNo=0; playIndex=0; playbackAccumulator=0.f; }
void playbackTick() {
    if(!playing) return;
    auto pl=PlayLayer::get(); if(!pl) {playing=false;return;}
    while(playIndex<events.size() && events[playIndex].frame<=frameNo) {
        auto const& e=events[playIndex++];
        auto p=e.p2?pl->m_player2:pl->m_player1; if(!p) continue;
        auto b=static_cast<PlayerButton>(e.button);
        if(e.down) p->pushButton(b); else p->releaseButton(b);
    }
    if(playIndex>=events.size()) {
        if(loopPlayback) { frameNo=0; playIndex=0; }
        else playing=false;
    }
}
}

class $modify(MiniMacroPlayer, PlayerObject) {
    bool pushButton(PlayerButton b) { bool r=PlayerObject::pushButton(b); capture(this,b,true); return r; }
    bool releaseButton(PlayerButton b) { bool r=PlayerObject::releaseButton(b); capture(this,b,false); return r; }
};

class $modify(MiniMacroPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        if (!recording && !playing) return;
        playbackAccumulator += std::max(0.f, dt);
        float frameDuration = 1.f / static_cast<float>(std::max(30, std::min(360, playbackTPS)));
        unsigned safety = 0;
        while ((recording || playing) && playbackAccumulator >= frameDuration && safety++ < 24) {
            playbackAccumulator -= frameDuration;
            ++frameNo;
            if (playing) playbackTick();
        }
    }
    void onQuit() {
        recording = false;
        playing = false;
        playbackAccumulator = 0.f;
        PlayLayer::onQuit();
    }
};

class $modify(MiniMacroPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto ws = this->getContentSize();
        auto menu = CCMenu::create();
        menu->setPosition({ws.width * 0.5f, ws.height * 0.22f});
        auto spr = ButtonSprite::create("MM", "goldFont.fnt", "GJ_button_05.png", 0.7f);
        auto item = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MiniMacroPauseLayer::openMiniMacro));
        menu->addChild(item);
        menu->setID("minimacro-pause-button");
        this->addChild(menu, 1000);
    }

    void openMiniMacro(CCObject*) {
        if (menuPanel) { menuPanel->removeFromParentAndCleanup(true); menuPanel = nullptr; return; }
        auto ws = this->getContentSize();
        menuPanel = CCLayerColor::create({12, 3, 28, 245});
        menuPanel->setContentSize({ws.width * 0.82f, ws.height * 0.62f});
        menuPanel->setPosition({ws.width * 0.09f, ws.height * 0.19f});
        menuPanel->setID("minimacro-pause-panel");
        this->addChild(menuPanel, 2000);
        auto title = CCLabelBMFont::create("MiniMacro", "bigFont.fnt");
        title->setPosition({menuPanel->getContentWidth()/2, menuPanel->getContentHeight()-30});
        menuPanel->addChild(title);
        auto buttons = CCMenu::create();
        buttons->setPosition({menuPanel->getContentWidth()/2, menuPanel->getContentHeight()/2});
        auto add = [&](char const* label, SEL_MenuHandler fn, float x, float y) {
            auto s = ButtonSprite::create(label, "bigFont.fnt", "GJ_button_05.png", 0.65f);
            auto b = CCMenuItemSpriteExtra::create(s, this, fn);
            b->setPosition({x,y});
            buttons->addChild(b);
        };
        add("Record", menu_selector(MiniMacroPauseLayer::mmRecord), -menuPanel->getContentWidth()*0.28f, 35);
        add("Stop", menu_selector(MiniMacroPauseLayer::mmStop), 0, 35);
        add("Play", menu_selector(MiniMacroPauseLayer::mmPlay), menuPanel->getContentWidth()*0.28f, 35);
        add("Save", menu_selector(MiniMacroPauseLayer::mmSave), -menuPanel->getContentWidth()*0.28f, -35);
        add("Import", menu_selector(MiniMacroPauseLayer::mmImport), menuPanel->getContentWidth()*0.28f, -35);
        add("TPS -", menu_selector(MiniMacroPauseLayer::mmTPSDown), -menuPanel->getContentWidth()*0.28f, -78);
        add("TPS +", menu_selector(MiniMacroPauseLayer::mmTPSUp), 0, -78);
        add("Loop", menu_selector(MiniMacroPauseLayer::mmLoop), menuPanel->getContentWidth()*0.28f, -78);
        add("Close", menu_selector(MiniMacroPauseLayer::mmClose), 0, -112);
        menuPanel->addChild(buttons);
    }
    void mmRecord(CCObject*) { beginRec(); }
    void mmStop(CCObject*) { stopRec(); playing = false; }
    void mmPlay(CCObject*) { startPlay(); }
    void mmSave(CCObject*) {
        if (saveMacro()) FLAlertLayer::create("MiniMacro", "Macro saved in config/mm macro folder.", "OK")->show();
        else FLAlertLayer::create("MiniMacro", "Nothing recorded yet, or save failed.", "OK")->show();
    }
    void mmTPSDown(CCObject*) { playbackTPS = std::max(30, playbackTPS - 30); }
    void mmTPSUp(CCObject*) { playbackTPS = std::min(360, playbackTPS + 30); }
    void mmLoop(CCObject*) { loopPlayback = !loopPlayback; }
    void mmImport(CCObject*) {
        geode::utils::file::FilePickOptions options;
        options.defaultPath = Mod::get()->getConfigDir() / "mm macro folder";
        options.filters.push_back({"MiniMacro JSON", {"json"}});
        geode::async::spawn(
            geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, std::move(options)),
            [](geode::utils::file::PickResult result) {
                if (!result.isOk()) {
                    FLAlertLayer::create("MiniMacro", "The file picker could not open.", "OK")->show();
                    return;
                }
                auto selected = result.unwrap();
                if (!selected.has_value()) return;
                std::ifstream input(selected->string());
                if (!input) {
                    FLAlertLayer::create("MiniMacro", "Could not open that file.", "OK")->show();
                    return;
                }
                auto parsed = matjson::parse(input);
                if (!parsed) {
                    FLAlertLayer::create("MiniMacro", "Invalid JSON macro file.", "OK")->show();
                    return;
                }
                auto json = parsed.unwrap();
                if (!json["inputs"].isArray()) {
                    FLAlertLayer::create("MiniMacro", "This JSON does not contain MiniMacro inputs.", "OK")->show();
                    return;
                }
                std::vector<MacroEvent> imported;
                bool valid = true;
                for (auto const& item : json["inputs"]) {
                    auto frame = item["frame"].asUInt();
                    auto button = item["btn"].asUInt();
                    auto p2 = item["2p"].asBool();
                    auto down = item["down"].asBool();
                    if (!frame || !button || !p2 || !down ||
                        frame.unwrap() > 4294967295ULL || button.unwrap() > 255) {
                        valid = false;
                        break;
                    }
                    imported.push_back({
                        static_cast<unsigned>(frame.unwrap()),
                        static_cast<unsigned char>(button.unwrap()),
                        p2.unwrap(),
                        down.unwrap()
                    });
                }
                if (!valid || imported.empty()) {
                    FLAlertLayer::create("MiniMacro", "Macro data is invalid or contains no inputs.", "OK")->show();
                    return;
                }
                std::stable_sort(imported.begin(), imported.end(), [](auto const& a, auto const& b) {
                    return a.frame < b.frame;
                });
                events = std::move(imported);
                frameNo = 0;
                playIndex = 0;
                recording = false;
                playing = false;
                auto tps = json["framerate"].asInt();
                if (tps && tps.unwrap() >= 30 && tps.unwrap() <= 360) {
                    playbackTPS = static_cast<int>(tps.unwrap());
                }
                FLAlertLayer::create("MiniMacro", "Macro imported successfully!", "OK")->show();
            }
        );
    }
    void mmClose(CCObject*) { if (menuPanel) { menuPanel->removeFromParentAndCleanup(true); menuPanel = nullptr; } }
};

$on_mod(Loaded) {
    std::error_code ec;
    auto macroFolder = Mod::get()->getConfigDir() / "mm macro folder";
    std::filesystem::create_directories(macroFolder, ec);
    if (ec) {
        log::error("MiniMacro: could not create config folder: {}", ec.message());
    } else {
        log::info("MiniMacro: config folder ready at {}", macroFolder.string());
    }
    log::info("MiniMacro loaded: pause-menu button enabled.");
}
