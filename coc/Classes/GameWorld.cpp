#include "Base.h"
#include "GameObject.h"
#include "MapManager.h"
#include "GameObjectManager.h"
#include "GameObjectSelectBox.h"
#include "Npc.h"
#include "GameWorld.h"
#include "BulletManager.h"
#include "GameWorldCallBackFunctionsManager.h"
#include "AutoFindPathHelper.h"
#include "Utils.h"
#include "Building.h"
#include "SpecialEffectManager.h"
#include "ForceManager.h"

static Vec2 s_mouseDownPoint;
const float SINGLE_CLICK_AREA = 5.0f;

GameWorld::~GameWorld()
{
    if (_mapManager != nullptr)
    {
        CC_SAFE_DELETE(_mapManager);
    }
}

bool GameWorld::init()
{
    if (!Node::init())
    {
        return false;
    }

    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("BuildingCommon.plist");

    _mapManager = new MapManager();
    _mapManager->init(this, "test3Map.tmx");

    _gameObjectManager = GameObjectManager::getInstance();
    _gameObjectManager->init(this);

    _gameObjectSelectBox = GameObjectSelectBox::create();
    _gameObjectSelectBox->setGlobalZOrder(MAX_GAME_OBJECT_COUNT);
    addChild(_gameObjectSelectBox);

    _bulletManager = BulletManager::getInstance();
    _specialEffectManager = SpecialEffectManager::getInstance();

    GameWorldCallBackFunctionsManager::getInstance()->registerCallBackFunctions(this);

    _forceManager = ForceManager::getInstance();

    auto mouseListener = EventListenerMouse::create();
    mouseListener->onMouseScroll = CC_CALLBACK_1(GameWorld::onMouseScroll, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouseListener, this);

    auto director = Director::getInstance();
    director->getEventDispatcher()->addCustomEventListener("MouseLeftButtonDown", CC_CALLBACK_0(GameWorld::onMouseLeftButtonDown, this));
    director->getEventDispatcher()->addCustomEventListener("MouseLeftButtonUp", CC_CALLBACK_0(GameWorld::onMouseLeftButtonUp, this));
    director->getEventDispatcher()->addCustomEventListener("MouseRightButtonDown", CC_CALLBACK_0(GameWorld::onMouseRightButtonDown, this));
    director->getEventDispatcher()->addCustomEventListener("MouseRightButtonUp", CC_CALLBACK_0(GameWorld::onMouseRightButtonUp, this));
    director->getEventDispatcher()->addCustomEventListener("MouseMove", CC_CALLBACK_1(GameWorld::onMouseMove, this));
    director->getEventDispatcher()->addCustomEventListener("ClearDebugDraw", CC_CALLBACK_0(GameWorld::onClearDebugDraw, this));
    director->getEventDispatcher()->addCustomEventListener("CreateNpcAroundPlayerBaseCamp", CC_CALLBACK_0(GameWorld::createNpcAroundBaseCamp, this, ForceType::Player, "BlueEnchanter", 16));

    //_director->setAlphaBlending(true);

    srand(::timeGetTime());

    initEditedGameObjects();
    initMapGIDTable();

    scheduleUpdate();

    return true;
}

void GameWorld::initEditedGameObjects()
{
    auto editedGameObjectGroups = _mapManager->getEditedGameObjectGroup();
    auto mapSize = _mapManager->getMapSize();
    auto tileSize = _mapManager->getTileSize();

    for (int i = 0; i < editedGameObjectGroups.size(); i ++)
    {
        ForceType forceType = ForceType::Invalid;
        GameObjectType gameObjectType = GameObjectType::Invalid;

        auto objectLayer = editedGameObjectGroups.at(i);
        auto objectLayerName = objectLayer->getGroupName();
        if (objectLayerName == "playerBuildingEditLayer")
        {
            forceType = ForceType::Player;
            gameObjectType = GameObjectType::Building;
        }
        else if (objectLayerName == "aiBuildingEditLayer")
        {
            forceType = ForceType::AI;
            gameObjectType = GameObjectType::Building;
        }
        else if (objectLayerName == "playerNpcEditLayer")
        {
            forceType = ForceType::Player;
            gameObjectType = GameObjectType::Npc;
        }
        else if (objectLayerName == "aiNpcEditLayer")
        {
            forceType = ForceType::AI;
            gameObjectType = GameObjectType::Npc;
        }
        else
        {
            CCASSERT("Can not parse %s", objectLayerName.c_str());
        }

        auto editedObjectsInfoList = objectLayer->getObjects();
        for (auto& editedObjectInfo : editedObjectsInfoList)
        {
            float inTileMapEditorXPosition = 0.0f;
            float inTileMapEditorYPosition = 0.0f;
            string gameObjectTemplateName;

            auto editedObjectValueMap = editedObjectInfo.asValueMap();
            for (auto& valueIter : editedObjectValueMap)
            {
                if (valueIter.first == "x")
                {
                    inTileMapEditorXPosition = valueIter.second.asFloat();
                }
                else if (valueIter.first == "y")
                {
                    inTileMapEditorYPosition = valueIter.second.asFloat();
                }
                else if (valueIter.first == "name")
                {
                    gameObjectTemplateName = valueIter.second.asString();
                }
            }

            int columnIndex = inTileMapEditorXPosition / tileSize.height;
            int rowIndex = (mapSize.height * tileSize.height - inTileMapEditorYPosition) / tileSize.height;
            auto tileNode = _mapManager->getTileNodeAt(columnIndex, rowIndex);
            auto gameObject = createGameObject(gameObjectType, forceType, gameObjectTemplateName, tileNode->leftTopPosition);

            if (forceType == ForceType::Player && gameObjectType == GameObjectType::Building)
            {
                auto building = static_cast<Building*>(gameObject);
                building->updateStatus(BuildingStatus::Working);
            }

            if (gameObjectTemplateName == "BaseCamp")
            {
                if (forceType == ForceType::Player)
                {
                    _playerBaseCampUniqueID = gameObject->getUniqueID();
                }
                else
                {
                    _aiBaseCampUniqueID = gameObject->getUniqueID();
                }
            }
        }

    }
}

void GameWorld::initMapGIDTable()
{
    auto mapSize = _mapManager->getMapSize();
    _mapGIDTable.resize(mapSize.width);
    for (int i = 0; i < (int)_mapGIDTable.size(); i ++)
    {
        _mapGIDTable[i].resize(mapSize.height);
    }
}

void GameWorld::update(float deltaTime)
{
    if (!_isLeftMouseButtonDown)
    {
        _mapManager->updateMapPosition();
    }

    _gameObjectManager->gameObjectsDepthSort(_mapManager->getTileSize());
    _gameObjectManager->npcMoveToTargetOneByOne();
    _gameObjectManager->removeAllReadyToRemoveGameObjects();

    _forceManager->update(deltaTime);
}

void GameWorld::onMouseScroll(Event* event)
{
    _mapManager->updateMapScale(event);
}

void GameWorld::onMouseLeftButtonDown()
{
    _isLeftMouseButtonDown = true;

    _gameObjectSelectBox->setMouseDownStatus(true);
    _gameObjectSelectBox->setMouseDownPoint(_cursorPoint);

    s_mouseDownPoint = _cursorPoint;

    if (_holdingBuildingID != GAME_OBJECT_UNIQUE_ID_INVALID)
    {
        constructBuilding();
    }
}

void GameWorld::onMouseLeftButtonUp()
{
    _isLeftMouseButtonDown = false;

    _gameObjectSelectBox->setMouseDownStatus(false);
    _gameObjectManager->cancelAllGameObjectSelected();

    if (std::abs(_cursorPoint.x - s_mouseDownPoint.x) < SINGLE_CLICK_AREA && std::abs(_cursorPoint.y - s_mouseDownPoint.y) < SINGLE_CLICK_AREA)
    {
        _gameObjectManager->selectGameObjectBy(_cursorPoint);
    }
    else
    {
        auto selectRect = _gameObjectSelectBox->getRect();
        _gameObjectManager->selectGameObjectsBy(selectRect);
    }
}

void GameWorld::onMouseRightButtonDown()
{
    cancelConstructBuilding();

    _gameObjectManager->cancelEnemySelected();
    auto enemy = _gameObjectManager->selectEnemyBy(_cursorPoint);
    bool hasSelectEnemyBuildingToAttack = false;
    if (enemy)
    {
        _gameObjectManager->setSelectedGameObjectEnemyUniqueID(enemy->getUniqueID());
        if (enemy->getGameObjectType() == GameObjectType::Building)
        {
            hasSelectEnemyBuildingToAttack = true;
        }
    }
    else
    {
        _gameObjectManager->setSelectedGameObjectEnemyUniqueID(ENEMY_UNIQUE_ID_INVALID);
        _gameObjectManager->cancelEnemySelected();
    }


    auto inMapCursorPosition = _mapManager->convertCursorPositionToTileMapSpace();
    if ((_mapManager->isInObstacleTile(inMapCursorPosition) && !hasSelectEnemyBuildingToAttack) ||
        GameUtils::isVec2Equal(_cursorPoint, _previousClickedCursorPoint))
    {
        return;
    }

    _previousClickedCursorPoint = _cursorPoint;

    auto gameObjectSelectedByPlayerCount = _gameObjectManager->getGameObjectSelectedByPlayerCount();
    if (gameObjectSelectedByPlayerCount == 1)
    {
        bool isAllowEndTileNodeToMoveIn = hasSelectEnemyBuildingToAttack;
        _gameObjectManager->npcSelectedByPlayerMoveTo(inMapCursorPosition, isAllowEndTileNodeToMoveIn);
    }
    else
    {
        auto npcMoveTargetList = _mapManager->getNpcMoveTargetListBy(gameObjectSelectedByPlayerCount);
        _gameObjectManager->setSelectedNpcMoveTargetList(ForceType::Player, npcMoveTargetList);
    }
}

void GameWorld::onClearDebugDraw()
{
    _gameObjectManager->clearGameObjectDebugDraw();
}

void GameWorld::onMouseRightButtonUp()
{

}

void GameWorld::onMouseMove(EventCustom* eventCustom)
{
    Vec2* cursorPointVec = (Vec2*)eventCustom->getUserData();
    syncCursorPoint(*cursorPointVec);
}

GameObject* GameWorld::createGameObject(GameObjectType gameObjectType, ForceType forceType, const string& jobName, const Vec2& position)
{
    auto gameObject = _gameObjectManager->createGameObject(gameObjectType, forceType, jobName, position);
    _mapManager->addChildInGameObjectLayer(gameObject);

    if (gameObjectType == GameObjectType::Building && forceType == ForceType::Player)
    {
        _holdingBuildingID = gameObject->getUniqueID();
    }

    return gameObject;
}

void GameWorld::removeGameObjectBy(int uniqueID)
{
    _gameObjectManager->removeGameObjectBy(uniqueID);
}

void GameWorld::createBullet(BulletType bulletType, int attackerID, int attackTargetID)
{
    auto bullet = _bulletManager->createBullet(bulletType, attackerID, attackTargetID);
    if (bullet)
    {
        _mapManager->addChildInGameObjectLayer(bullet);
    }
}

void GameWorld::createSpecialEffect(const string& templateName, const Vec2& inMapPosition, bool isRepeat)
{
    auto specialEffect = _specialEffectManager->createSpecialEffect(templateName, inMapPosition, isRepeat);
    if (specialEffect)
    {
        _mapManager->addChildInGameObjectLayer(specialEffect, 10);
    }
}

void GameWorld::syncCursorPoint(const Vec2& cursorPoint)
{
    _cursorPoint = cursorPoint;
    _mapManager->syncCursorPoint(cursorPoint);
    _gameObjectSelectBox->syncCursorPoint(cursorPoint);
}

list<Vec2> GameWorld::computePathListBetween(const Vec2& inMapStartPosition, const Vec2& inMapEndPosition, bool isAllowEndTileNodeToMoveIn /*= false*/)
{
    list<Vec2> pointPathList;

    auto startTileSubscript = _mapManager->getTileSubscript(inMapStartPosition);
    auto startTileNode = _mapManager->getTileNodeAt((int)startTileSubscript.x, (int)startTileSubscript.y);

    auto endTileSubscript = _mapManager->getTileSubscript(inMapEndPosition);
    auto endTileNode = _mapManager->getTileNodeAt((int)endTileSubscript.x, (int)endTileSubscript.y);
    auto distanceBetweenTileAndNpc = inMapEndPosition - endTileNode->leftTopPosition;

    // 临时允许npc可以移动到最后一个障碍格内
    int originalGID = endTileNode->gid;
    if (isAllowEndTileNodeToMoveIn)
    {
        endTileNode->gid = PASSABLE_ID;
    }

    if (endTileNode->gid != OBSTACLE_ID)
    {
        if (startTileNode != endTileNode)
        {
            auto tileNodePathList = AutoFindPathHelper::computeTileNodePathListBetween(startTileNode, endTileNode);

            for (auto tileNodePath : tileNodePathList)
            {
                pointPathList.push_front(tileNodePath->leftTopPosition + distanceBetweenTileAndNpc);
            }

            if (!pointPathList.empty())
            {
                //第一个点是起点，因此可以忽略
                pointPathList.pop_front();
            }
        }
        else
        {
            if (!GameUtils::isVec2Equal(inMapStartPosition, inMapEndPosition))
            {
                pointPathList.push_front(inMapEndPosition);
            }
        }
    }

    if (isAllowEndTileNodeToMoveIn)
    {
        endTileNode->gid = originalGID;
    }

    return pointPathList;
}

MapManager* GameWorld::getMapManager()
{
    return _mapManager;
}

void GameWorld::constructBuilding()
{
    auto holdingObject = _gameObjectManager->getGameObjectBy(_holdingBuildingID);
    auto holdingBuilding = dynamic_cast<Building*>(holdingObject);
    if (holdingBuilding &&
        holdingBuilding->getBuildingStatus() == BuildingStatus::PrepareToBuild &&
        holdingBuilding->canBuild())
    {
        holdingBuilding->updateStatus(BuildingStatus::BeingBuilt);

        _holdingBuildingID = GAME_OBJECT_UNIQUE_ID_INVALID;
    }
}

void GameWorld::cancelConstructBuilding()
{
    if (_holdingBuildingID != GAME_OBJECT_UNIQUE_ID_INVALID)
    {
        auto holdingObject = _gameObjectManager->getGameObjectBy(_holdingBuildingID);
        auto holdingBuilding = dynamic_cast<Building*>(holdingObject);
        if (holdingBuilding && holdingBuilding->getBuildingStatus() == BuildingStatus::PrepareToBuild)
        {
            _gameObjectManager->removeGameObjectBy(_holdingBuildingID);
        }

        _holdingBuildingID = GAME_OBJECT_UNIQUE_ID_INVALID;
    }
}

vector<Vec2> GameWorld::computeNpcCreatePointList(ForceType forceType, int readyToCreateNpcCount)
{
    vector<Vec2> npcCreatePointList;

    int baseCampUniqueID = GAME_OBJECT_UNIQUE_ID_INVALID;
    if (forceType == ForceType::Player)
    {
        baseCampUniqueID = _playerBaseCampUniqueID;
    }
    else
    {
        baseCampUniqueID = _aiBaseCampUniqueID;
    }

    auto mapSize = _mapManager->getMapSize();
    for (int columnIndex = 0; columnIndex < (int)mapSize.width; columnIndex++)
    {
        for (int rowIndex = 0; rowIndex < (int)mapSize.height; rowIndex ++)
        {
            auto tileNode = _mapManager->getTileNodeAt(columnIndex, rowIndex);
            _mapGIDTable[columnIndex][rowIndex] = tileNode->gid;
        }
    }

    // 计算场景中已存在的npc占用的格子，这样做是为了避免新创建的npc创建在已被其他npc占用的格子上，出现重合现象
    auto& gameObjectMap = _gameObjectManager->getGameObjectMap();
    for (auto& gameObjectIter : gameObjectMap)
    {
        auto gameObject = gameObjectIter.second;
        if (gameObject->getGameObjectType() == GameObjectType::Npc)
        {
            auto gameObjectPosition = gameObject->getPosition();
            auto gameObjectOccupyTileNodeSubscript = _mapManager->getTileSubscript(gameObjectPosition);
            _mapGIDTable[(int)gameObjectOccupyTileNodeSubscript.x][(int)gameObjectOccupyTileNodeSubscript.y] = OBSTACLE_ID;
        }
    }

    // 这里需要寻找围绕基地的bottomGrid的那些可以作为npc创建点的节点，此时需要遍历*标识的节点
    // * * * * *
    // * # # # *
    // * # # # *
    // * # # # *
    // * * * * *
    // 因此需要找到处于左上角*的下标，和处于右下角*的下标，然后以此为依据进行遍历
    auto baseCamp = _gameObjectManager->getGameObjectBy(baseCampUniqueID);
    if (baseCamp)
    {
        auto baseCampObject = static_cast<Building*>(baseCamp);
        auto& baseCampBottomGridPositionList = baseCampObject->getBottomGridInMapPositionList();
        auto firstBottomGridPosition = baseCampBottomGridPositionList.front();
        auto lastBottomGridPosition = baseCampBottomGridPositionList.back();

        auto firstBottomGridOccupyTileNodeSubscript = _mapManager->getTileSubscript(firstBottomGridPosition);
        auto lastBottomGridOccupyTileNodeSubscript = _mapManager->getTileSubscript(lastBottomGridPosition);

        int leftTopStartSearchColumnIndex = std::max(0, (int)(firstBottomGridOccupyTileNodeSubscript.x - 1));
        int leftTopStartSearchRowIndex = std::max(0, (int)(firstBottomGridOccupyTileNodeSubscript.y - 1));

        int rightBottomStartSearchColumnIndex = std::min((int)(lastBottomGridOccupyTileNodeSubscript.x + 1), (int)(mapSize.width - 1));
        int rightBottomStartSearchRowIndex = std::min((int)(lastBottomGridOccupyTileNodeSubscript.y + 1), (int)(mapSize.height - 1));

        while ((int)npcCreatePointList.size() < readyToCreateNpcCount)
        {
            for (int columnIndex = leftTopStartSearchColumnIndex; columnIndex <= rightBottomStartSearchColumnIndex; columnIndex ++)
            {
                if (_mapGIDTable[columnIndex][leftTopStartSearchRowIndex] != OBSTACLE_ID &&
                    (int)npcCreatePointList.size() < readyToCreateNpcCount)
                {
                    auto tileNodeInTopLine = _mapManager->getTileNodeAt(columnIndex, leftTopStartSearchRowIndex);
                    npcCreatePointList.push_back(tileNodeInTopLine->leftTopPosition);

                    _mapGIDTable[columnIndex][leftTopStartSearchRowIndex] = OBSTACLE_ID;
                }

                if (_mapGIDTable[columnIndex][rightBottomStartSearchRowIndex] != OBSTACLE_ID &&
                    (int)npcCreatePointList.size() < readyToCreateNpcCount)
                {
                    auto tileNodeInBottomLine = _mapManager->getTileNodeAt(columnIndex, rightBottomStartSearchRowIndex);
                    npcCreatePointList.push_back(tileNodeInBottomLine->leftTopPosition);

                    _mapGIDTable[columnIndex][rightBottomStartSearchRowIndex] = OBSTACLE_ID;
                }
            }

            for (int rowIndex = leftTopStartSearchRowIndex; rowIndex <= rightBottomStartSearchRowIndex; rowIndex ++)
            {
                if (_mapGIDTable[leftTopStartSearchColumnIndex][rowIndex] != OBSTACLE_ID &&
                    (int)npcCreatePointList.size() < readyToCreateNpcCount)
                {
                    auto tileNodeInLeftLine = _mapManager->getTileNodeAt(leftTopStartSearchColumnIndex, rowIndex);
                    npcCreatePointList.push_back(tileNodeInLeftLine->leftTopPosition);

                    _mapGIDTable[leftTopStartSearchColumnIndex][rowIndex] = OBSTACLE_ID;
                }

                if (_mapGIDTable[rightBottomStartSearchColumnIndex][rowIndex] != OBSTACLE_ID &&
                    (int)npcCreatePointList.size() < readyToCreateNpcCount)
                {
                    auto tileNodeInRightLine = _mapManager->getTileNodeAt(rightBottomStartSearchColumnIndex, rowIndex);
                    npcCreatePointList.push_back(tileNodeInRightLine->leftTopPosition);

                    _mapGIDTable[rightBottomStartSearchColumnIndex][rowIndex] = OBSTACLE_ID;
                }
            }

            leftTopStartSearchColumnIndex = std::max(leftTopStartSearchColumnIndex - 1, 0);
            leftTopStartSearchRowIndex = std::max(leftTopStartSearchRowIndex - 1, 0);

            rightBottomStartSearchColumnIndex = std::min(rightBottomStartSearchColumnIndex + 1, (int)(mapSize.width - 1));
            rightBottomStartSearchRowIndex = std::min(rightBottomStartSearchRowIndex + 1, (int)(mapSize.height - 1));
        }
    }

    return npcCreatePointList;
}

void GameWorld::createNpcAroundBaseCamp(ForceType forceType, const string& npcTemplateName, int npcCount)
{
    auto npcCreatePointList = computeNpcCreatePointList(forceType, npcCount);
    for (auto& point : npcCreatePointList)
    {
        createGameObject(GameObjectType::Npc, forceType, npcTemplateName, point);
    }
}

const DebugInfo& GameWorld::getDebugInfo()
{
    _debugInfo.mapDebugInfo = _mapManager->getMapDebugInfo(TileMapLayerType::GameObjcetLayer);
    _debugInfo.gameObjectCount = (int)_gameObjectManager->getGameObjectMap().size();

    return _debugInfo;
}

int GameWorld::getEnemyBaseCampUniqueID()
{
    return _aiBaseCampUniqueID;
}

int GameWorld::getPlayerBaseCampUniqueID()
{
    return _playerBaseCampUniqueID;
}
