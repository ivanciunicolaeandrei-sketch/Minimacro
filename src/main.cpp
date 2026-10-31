#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

class $modify(MiniMacroPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({winSize.width - 82.f, winSize.height - 34.f});

        auto sprite = ButtonSprite::create(
            "MiniMacro",
            "bigFont.fnt",
            "GJ_button_01.png",
            0.65f
        );
        sprite->setScale(0.55f);

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(MiniMacroPauseLayer::onMiniMacro)
        );
        menu->addChild(button);
        this->addChild(menu, 100);
    }

    void onMiniMacro(CCObject*) {
        FLAlertLayer::create(
            "MiniMacro",
            "MiniMacro is loading. Recorder, replay import/export, and playback controls are being added.",
            "OK"
        )->show();
    }
};
