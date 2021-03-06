#include "Base.h"
#include "SelectStageScene.h"
#include "cocostudio/ActionTimeline/CSLoader.h"
#include "GameScene.h"
#include "LoadingScene.h"
#include "audio/include/AudioEngine.h"
#include "SoundManager.h"
#include "GameSetting.h"
#include "StorageManager.h"
#include "GameObject.h"
#include "GameConfigManager.h"
#include "MenuScene.h"
#include "WindowsHelper.h"

cocos2d::Scene* SelectStageScene::createScene()
{
    auto scene = Scene::create();
    auto layer = SelectStageScene::create();
    scene->addChild(layer);

    return scene;
}

bool SelectStageScene::init()
{
    if (!Layer::init())
    {
        return false;
    }

    auto rootNode = CSLoader::createNode("SelectStageScene.csb");
    addChild(rootNode);

    _rootPanel = rootNode->getChildByName<Layout*>("Panel_SelectStage");

    initFrontUI();
    initSelectStageScrollView();

    auto delayPlayMusic = CallFunc::create(CC_CALLBACK_0(SoundManager::playMusic, SoundManager::getInstance(), "SelectStageDialog.mp3", true));
    auto sequenceAction = Sequence::create(DelayTime::create(0.5f), delayPlayMusic, nullptr);
    runAction(sequenceAction);
    // SoundManager::getInstance()->playMusic("SelectStageDialog2.ogg", true);

    WindowsHelper::getInstance()->switchToNormalCursor();
    Director::getInstance()->resume();

    return true;
}

void SelectStageScene::initFrontUI()
{
    _frontUI = _rootPanel->getChildByName<Layout*>("Panel_FrontUI");
    _stageTips = _frontUI->getChildByName<ImageView*>("Image_StageTips");
    // _stageTips->setVisible(false);

    auto easyLevel = _frontUI->getChildByName<CheckBox*>("CheckBox_EasyLevel");
    auto normalLevel = _frontUI->getChildByName<CheckBox*>("CheckBox_NormalLevel");
    auto hardLevel = _frontUI->getChildByName<CheckBox*>("CheckBox_HardLevel");

    easyLevel->addEventListener(CC_CALLBACK_2(SelectStageScene::onSelectDifficultyLevelCheckBoxTouch, this));
    normalLevel->addEventListener(CC_CALLBACK_2(SelectStageScene::onSelectDifficultyLevelCheckBoxTouch, this));
    hardLevel->addEventListener(CC_CALLBACK_2(SelectStageScene::onSelectDifficultyLevelCheckBoxTouch, this));

    _difficultyLevelMap[easyLevel] = DifficultyLevel::Easy;
    _difficultyLevelMap[normalLevel] = DifficultyLevel::Normal;
    _difficultyLevelMap[hardLevel] = DifficultyLevel::Hard;
    
    auto startGameButton = _frontUI->getChildByName<Button*>("Button_GameStart");
    startGameButton->addTouchEventListener(CC_CALLBACK_2(SelectStageScene::onGameStart, this));

    auto backToMenuSceneButton = _frontUI->getChildByName<Button*>("Button_BackToMenuScene");
    backToMenuSceneButton->addTouchEventListener(CC_CALLBACK_2(SelectStageScene::onBackToMenuScene, this));
}

void SelectStageScene::initSelectStageScrollView()
{
    _selectStageScrollView = _rootPanel->getChildByName<ScrollView*>("ScrollView_SelectStage");

    for (int i = 1; i < INT_MAX; i ++)
    {
        auto stageCheckBoxNode = _selectStageScrollView->getChildByName(StringUtils::format("CheckBox_Stage%d", i));
        if (stageCheckBoxNode)
        {
            auto stageCheckBox = static_cast<CheckBox*>(stageCheckBoxNode);
            stageCheckBox->addEventListener(CC_CALLBACK_2(SelectStageScene::onStageCheckBoxTouch, this, i));

            if (i <= StorageManager::getInstance()->_stageData.maxPlayableStage)
            {
                stageCheckBox->setEnabled(true);
                stageCheckBox->setBright(true);
            }

            _stageCheckBoxList.push_back(stageCheckBox);
        }
        else
        {
            break;
        }
    }

    g_setting.maxStage = (int)_stageCheckBoxList.size();

    int selectedStage = StorageManager::getInstance()->_stageData.playerSelectedStage;
    auto selectedCheckBox = _stageCheckBoxList[selectedStage - 1];
    onStageCheckBoxTouch(selectedCheckBox, CheckBox::EventType::SELECTED, selectedStage);

    auto visibleSize = Director::getInstance()->getVisibleSize();
    auto innerContainer = _selectStageScrollView->getInnerContainer();
    auto selectedCheckBoxWorldPosition = innerContainer->convertToWorldSpace(selectedCheckBox->getPosition());
    float yDelta = visibleSize.height / 2.0f - selectedCheckBoxWorldPosition.y;

    auto innerContainerPosition = innerContainer->getPosition();
    auto innerContainerSize = innerContainer->getContentSize();

    float newInnerContainerYPosition = innerContainerPosition.y + yDelta;
    newInnerContainerYPosition = std::max(visibleSize.height - innerContainerSize.height, newInnerContainerYPosition);
    newInnerContainerYPosition = std::min(0.0f, newInnerContainerYPosition);
    innerContainer->setPosition(Vec2(innerContainerPosition.x, newInnerContainerYPosition));
}

void SelectStageScene::onStageCheckBoxTouch(Ref* sender, CheckBox::EventType type, int stageID)
{
    if (type == CheckBox::EventType::SELECTED || type == CheckBox::EventType::UNSELECTED)
    {
        auto currentCheckBox = static_cast<CheckBox*>(sender);
        for (auto stageCheckBox : _stageCheckBoxList)
        {
            if (stageCheckBox == currentCheckBox)
            {
                stageCheckBox->setSelected(true);
                StorageManager::getInstance()->_stageData.playerSelectedStage = stageID;

                auto introduction = GameConfigManager::getInstance()->getStageIntroductionBy(stageID);
                auto introductionLabel = _stageTips->getChildByName<Text*>("Text_StageTips");
                introductionLabel->setString(introduction);
            }
            else
            {
                stageCheckBox->setSelected(false);
            }
        }
    }
}

void SelectStageScene::onSelectDifficultyLevelCheckBoxTouch(Ref* sender, CheckBox::EventType type)
{
    if (type == CheckBox::EventType::SELECTED || type == CheckBox::EventType::UNSELECTED)
    {
        auto currentCheckBox = static_cast<CheckBox*>(sender);
        for (auto& iter : _difficultyLevelMap)
        {
            if (iter.first == currentCheckBox)
            {
                iter.first->setSelected(true);
            }
            else
            {
                iter.first->setSelected(false);
            }
        }
    }
}

void SelectStageScene::onGameStart(Ref* sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED)
    {
        SoundManager::getInstance()->stopAll();

        g_setting.difficultyLevel = getSelectedDifficultyLevel();

        auto loadingScene = LoadingScene::createScene();
        Director::getInstance()->replaceScene(loadingScene);
    }
}

void SelectStageScene::onBackToMenuScene(Ref* sender, Widget::TouchEventType type)
{
    if (type == Widget::TouchEventType::ENDED)
    {
        SoundManager::getInstance()->stopAll();

        auto menuScene = MenuScene::createScene();
        Director::getInstance()->replaceScene(menuScene);
    }
}

DifficultyLevel SelectStageScene::getSelectedDifficultyLevel()
{
    DifficultyLevel result = DifficultyLevel::Normal;

    for (auto& iter : _difficultyLevelMap)
    {
        if (iter.first->isSelected())
        {
            result = iter.second;
            break;
        }
    }

    return result;
}
