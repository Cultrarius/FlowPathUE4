//
// Created by michael on 17.12.17.
//

#pragma once

#include <set>
#include <map>
#include <memory>
#include "FlowTile.h"

namespace flow {
    
    const BYTE EMPTY = 1;
    const BYTE BLOCKED = 255;
    typedef std::pair<int, int> Coordinates;
    typedef std::pair<float, float> Force;

    struct Agent {
        Force acceleration;
        float xPos;
        float yPos;
        float separationForce;

        bool isSeekingTarget;
        int targetX;
        int targetY;
    };

    class FlowPath {
    private:
        //TODO: use empty data instead of tile
        std::unique_ptr<FlowTile> emptyTile;
        int32 tileLength;
        std::map<Coordinates, FlowTile *> tileMap;
        std::map<Agent *, std::unique_ptr<Agent>> agents;

        void updatePortals(Coordinates tileCoordinates);

        FlowTile *getTile(Coordinates tileCoordinates);

    public:
        explicit FlowPath(int32 tileLength);

        ~FlowPath();

        void updateMapTile(int tileX, int tileY, const TArray<BYTE> &tileData);

        //TODO remove, debug only
        void printPortals() const;

        void addAgent(std::unique_ptr<Agent> agent);

        void killAgent(Agent *agent);

        void updateAgents();
    };
}
