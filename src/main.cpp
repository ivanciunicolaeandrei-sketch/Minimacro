#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
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
void beginRec() { events.clear(); frameNo=0; playIndex=0; playing=false; recording=true; }
void stopRec() { recording=false; }
bool saveMacro() {
    auto pl=PlayLayer::get(); if (!pl || !pl->m_level || events.empty()) return false;
    auto dir=Mod::get()->getSaveDir()/"macros"; std::error_code ec;
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
void startPlay() { if(events.empty()) return; recording=false; playing=true; frameNo=0; playIndex=0; }
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
    void onEnter() {
        PlayLayer::onEnter();
        auto ws=CCDirector::sharedDirector()->getWinSize();
        auto menu=CCMenu::create();
        menu->setPosition({ws.width-38.f, ws.height*0.58f});
        auto spr=ButtonSprite::create("MM", "goldFont.fnt", "GJ_button_05.png", 0.7f);
        spr->setScale(0.55f);
        auto item=CCMenuItemSpriteExtra::create(spr,this,menu_selector(MiniMacroPlayLayer::openMiniMacro));
        menu->addChild(item);
        menu->setID("minimacro-floating-button");
        this->addChild(menu, 1000);
    }
    void update(float dt) {
        PlayLayer::update(dt);
        if(recording || playing) ++frameNo;
        playbackTick();
    }
    void onQuit() { recording=false; playing=false; PlayLayer::onQuit(); }

    void openMiniMacro(CCObject*) {
        if(menuPanel) { menuPanel->removeFromParentAndCleanup(true); menuPanel=nullptr; return; }
        auto ws=CCDirector::sharedDirector()->getWinSize();
        menuPanel=CCLayerColor::create({12,3,28,238});
        menuPanel->setContentSize({ws.width*0.88f, ws.height*0.78f});
        menuPanel->setPosition({ws.width*0.06f,ws.height*0.11f});
        menuPanel->setID("minimacro-eclipse-menu");
        this->addChild(menuPanel,2000);
        auto title=CCLabelBMFont::create("MiniMacro", "bigFont.fnt");
        title->setScale(0.8f); title->setPosition({menuPanel->getContentWidth()/2,menuPanel->getContentHeight()-32});
        menuPanel->addChild(title);
        auto subtitle=CCLabelBMFont::create("ECLIPSE MOBILE STYLE", "goldFont.fnt");
        subtitle->setScale(0.42f); subtitle->setPosition({menuPanel->getContentWidth()/2,menuPanel->getContentHeight()-58});
        menuPanel->addChild(subtitle);
        auto buttons=CCMenu::create();
        buttons->setPosition({menuPanel->getContentWidth()/2,menuPanel->getContentHeight()/2-12});
        auto add=[&](char const* text, SEL_MenuHandler fn, float x,float y) {
            auto s=ButtonSprite::create(text,"bigFont.fnt","GJ_button_05.png",0.65f);
            s->setScale(0.62f);
            auto b=CCMenuItemSpriteExtra::create(s,this,fn); b->setPosition({x,y}); buttons->addChild(b);
        };
        add("Record",menu_selector(MiniMacroPlayLayer::mmRecord),-menuPanel->getContentWidth()*0.28f,58);
        add("Stop",menu_selector(MiniMacroPlayLayer::mmStop),0,58);
        add("Play",menu_selector(MiniMacroPlayLayer::mmPlay),menuPanel->getContentWidth()*0.28f,58);
        add("Save Macro",menu_selector(MiniMacroPlayLayer::mmSave),-menuPanel->getContentWidth()*0.28f,0);
        add("TPS -",menu_selector(MiniMacroPlayLayer::mmTPSDown),0,0);
        add("TPS +",menu_selector(MiniMacroPlayLayer::mmTPSUp),menuPanel->getContentWidth()*0.28f,0);
        add("Loop",menu_selector(MiniMacroPlayLayer::mmLoop),-menuPanel->getContentWidth()*0.18f,-58);
        add("Close",menu_selector(MiniMacroPlayLayer::mmClose),menuPanel->getContentWidth()*0.22f,-58);
        menuPanel->addChild(buttons);
        auto status=CCLabelBMFont::create("Record / Stop / Play  |  Save to Geode/macros", "chatFont.fnt");
        status->setScale(0.42f); status->setPosition({menuPanel->getContentWidth()/2,24}); menuPanel->addChild(status);
    }
    void mmRecord(CCObject*) { beginRec(); }
    void mmStop(CCObject*) { stopRec(); playing=false; }
    void mmPlay(CCObject*) { startPlay(); }
    void mmSave(CCObject*) { if(saveMacro()) FLAlertLayer::create("MiniMacro","Macro saved in Geode save folder /macros.","OK")->show(); else FLAlertLayer::create("MiniMacro","Nothing recorded yet, or save failed.","OK")->show(); }
    void mmTPSDown(CCObject*) { playbackTPS=std::max(30,playbackTPS-30); }
    void mmTPSUp(CCObject*) { playbackTPS=std::min(360,playbackTPS+30); }
    void mmLoop(CCObject*) { loopPlayback=!loopPlayback; }
    void mmClose(CCObject*) { if(menuPanel) {menuPanel->removeFromParentAndCleanup(true);menuPanel=nullptr;} }
};

$on_mod(Loaded) { log::info("MiniMacro loaded: floating in-game menu enabled."); }
