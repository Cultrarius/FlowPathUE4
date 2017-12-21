//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
#include <iostream>

using namespace std;
using namespace flow;

FlowPath::FlowPath(int32 tileLength, bool allowCrossGridMovement) : tileLength(tileLength), allowCrossGridMovement(allowCrossGridMovement) {
    int32 size = tileLength * tileLength;
    emptyTileData.AddUninitialized(size);
    fullTileData.AddUninitialized(size);
    for (int32 i = 0; i < size; i++) {
        emptyTileData[i] = EMPTY;
        fullTileData[i] = BLOCKED;
    }
}

void FlowPath::updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData) {
    if (tileData.Num() != tileLength * tileLength) {
        throw runtime_error("Invalid tile size");
    }
    bool isEmpty = true;
    bool isBlocked = true;
    for (int32 i = 0; i < tileLength * tileLength; i++) {
        if (tileData[i] == 0) {
            throw runtime_error("Invalid tile value: 0");
        }
        if (tileData[i] != EMPTY) {
            isEmpty = false;
        }
        if (tileData[i] != BLOCKED) {
            isBlocked = false;
        }
    }

    FIntPoint coord(tileX, tileY);
    FlowTile *tile;
    if (isEmpty) {
        tile = new FlowTile(&emptyTileData, tileLength, coord);
    }
    else if (isBlocked) {
        tile = new FlowTile(&fullTileData, tileLength, coord);
    }
    else {
        tile = new FlowTile(tileData, tileLength, coord);
    }
    auto existingTile = tileMap.Find(coord);
    if (existingTile != nullptr) {
        (*existingTile)->removeConnectedPortals();
        tileMap.Remove(coord);
    }
    tileMap.Add(coord, TUniquePtr<FlowTile>(tile));
    updatePortals(coord);
}

void FlowPath::addAgent(TUniquePtr<Agent> agent) {
    agents.Emplace(agent.Get(), move(agent));
}

void FlowPath::killAgent(Agent *agent) {
    agents.Remove(agent);
}

void FlowPath::updateAgents() {

}

PathSearchResult flow::FlowPath::findDirectPath(FIntPoint start, FIntPoint end)
{
    return (*tileMap.Find(FIntPoint(1, 1)))->findPath(start, end);
}

void FlowPath::updatePortals(FIntPoint tileCoordinates) {
    FlowTile *tile = getTile(tileCoordinates);
    if (tile == nullptr) {
        return;
    }

    FIntPoint left(tileCoordinates.X - 1, tileCoordinates.Y);
    FIntPoint right(tileCoordinates.X + 1, tileCoordinates.Y);
    FIntPoint top(tileCoordinates.X, tileCoordinates.Y - 1);
    FIntPoint bottom(tileCoordinates.X, tileCoordinates.Y + 1);

    FlowTile *leftTile = getTile(left);
    FlowTile *rightTile = getTile(right);
    FlowTile *topTile = getTile(top);
    FlowTile *bottomTile = getTile(bottom);

    if (leftTile != nullptr) {
        tile->connectOverlappingPortals(*leftTile, LEFT);
    }
    if (rightTile != nullptr) {
        tile->connectOverlappingPortals(*rightTile, RIGHT);
    }
    if (topTile != nullptr) {
        tile->connectOverlappingPortals(*topTile, TOP);
    }
    if (bottomTile != nullptr) {
        tile->connectOverlappingPortals(*bottomTile, BOTTOM);
    }
}

FlowTile *FlowPath::getTile(FIntPoint tileCoordinates) {
    auto tile = tileMap.Find(tileCoordinates);
    return tile == nullptr ? nullptr : tile->Get();
}

void FlowPath::printPortals() const {
    for (auto& tilePair : tileMap) {
        UE_LOG(LogExec, Warning, TEXT("Tile %d, %d:"), tilePair.Key.X, tilePair.Key.Y);
        for (auto& portal : tilePair.Value->portals) {
            for (auto& connected : portal.connected) {
                if (connected.Key->parentTile != portal.parentTile) {
                    UE_LOG(LogExec, Warning, TEXT("  From (%d, %d -> %d, %d) To (%d, %d -> %d, %d)"), portal.startX, portal.startY, portal.endX, portal.endY,
                        connected.Key->startX, connected.Key->startY,  connected.Key->endX, connected.Key->endY);
                }
            }
        }
    }
}

