//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
#include <iostream>

using namespace std;
using namespace flow;

FlowPath::FlowPath(int32 tileLength) : tileLength(tileLength) {
    int32 size = tileLength * tileLength;
    TArray<BYTE> mapData;
    for (int32 i = 0; i < size; i++) {
        mapData[i] = 1;
    }
    emptyTile = unique_ptr<FlowTile>(new FlowTile(mapData, tileLength));
}

void FlowPath::updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData) {
    if (tileData.Num() != tileLength * tileLength) {
        throw runtime_error("Invalid tile size");
    }
    bool isEmpty = true;
    for (int32 i = 0; i < tileLength * tileLength; i++) {
        if (tileData[i] == 0) {
            throw runtime_error("Invalid tile value: 0");
        }
        if (tileData[i] != 1) {
            isEmpty = false;
            break;
        }
    }

    FlowTile *tile = isEmpty ? emptyTile.get() : new FlowTile(tileData, tileLength);
    FIntPoint coord(tileX, tileY);
    auto existingTile = tileMap.Find(coord);
    if (existingTile != nullptr) {
        delete *existingTile;
        tileMap.Remove(coord);
    }
    tileMap.Add(coord, tile);
    updatePortals(coord);
}

void FlowPath::addAgent(unique_ptr<Agent> agent) {
    agents.Emplace(agent.get(), move(agent));
}

void FlowPath::killAgent(Agent *agent) {
    agents.Remove(agent);
}

void FlowPath::updateAgents() {

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

FlowPath::~FlowPath() {
    for (auto mapPair : tileMap) {
        if (mapPair.Value != emptyTile.get()) {
            delete mapPair.Value;
        }
    }
}

FlowTile *FlowPath::getTile(FIntPoint tileCoordinates) {
    auto tile = tileMap.Find(tileCoordinates);
    return tile == nullptr ? nullptr : *tile;
}

void FlowPath::printPortals() const {
    for (auto& tilePair : tileMap) {
        cout << tilePair.Key.X << ", " << tilePair.Key.Y << ": \n";
        for (auto& portal : tilePair.Value->portals) {
            for (auto& connected : portal.connected) {
                if (connected->parentTile != portal.parentTile) {
                    cout << "  (" << portal.startX << ", " << portal.startY << " - " << portal.endX << ", " << portal.endY << ") -> ("
                            << connected->startX << ", " << connected->startY << " - " << connected->endX << ", " << connected->endY << ")\n";
                }
            }
        }
    }
}

