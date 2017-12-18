//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
#include <iostream>

using namespace std;
using namespace flow;

FlowPath::FlowPath(int32 tileLength) : tileLength(tileLength) {
    int size = tileLength * tileLength;
    TArray<BYTE> mapData;
    for (int i = 0; i < size; i++) {
        mapData[i] = 1;
    }
    emptyTile = unique_ptr<FlowTile>(new FlowTile(mapData, tileLength));
}

void FlowPath::updateMapTile(int tileX, int tileY, const TArray<BYTE> &tileData) {
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
    auto coord = make_pair(tileX, tileY);
    const auto &iter = tileMap.find(coord);
    if (iter != tileMap.end()) {
        delete iter->second;
        tileMap.erase(iter);
    }
    tileMap.emplace(coord, tile);
    updatePortals(coord);
}

void FlowPath::addAgent(unique_ptr<Agent> agent) {
    agents.emplace(agent.get(), move(agent));
}

void FlowPath::killAgent(Agent *agent) {
    auto iter = agents.find(agent);
    if (iter != agents.end()) {
        agents.erase(iter);
    }
}

void FlowPath::updateAgents() {

}

void FlowPath::updatePortals(Coordinates tileCoordinates) {
    FlowTile *tile = getTile(tileCoordinates);
    if (tile == nullptr) {
        return;
    }

    Coordinates left = make_pair(tileCoordinates.first - 1, tileCoordinates.second);
    Coordinates right = make_pair(tileCoordinates.first + 1, tileCoordinates.second);
    Coordinates top = make_pair(tileCoordinates.first, tileCoordinates.second - 1);
    Coordinates bottom = make_pair(tileCoordinates.first, tileCoordinates.second + 1);

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
        if (mapPair.second != emptyTile.get()) {
            delete mapPair.second;
        }
    }
}

FlowTile *FlowPath::getTile(Coordinates tileCoordinates) {
    auto iter = tileMap.find(tileCoordinates);
    if (iter == tileMap.end()) {
        return nullptr;
    }
    return iter->second;
}

void FlowPath::printPortals() const {
    for (auto& tilePair : tileMap) {
        cout << tilePair.first.first << ", " << tilePair.first.second << ": \n";
        for (auto& portal : tilePair.second->portals) {
            for (auto& connected : portal.connected) {
                if (connected->parentTile != portal.parentTile) {
                    cout << "  (" << portal.startX << ", " << portal.startY << " - " << portal.endX << ", " << portal.endY << ") -> ("
                            << connected->startX << ", " << connected->startY << " - " << connected->endX << ", " << connected->endY << ")\n";
                }
            }
        }
    }
}

