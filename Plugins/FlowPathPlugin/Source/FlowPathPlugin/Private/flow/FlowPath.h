//
// Created by michael on 17.12.17.
//

#pragma once

#include <memory>
#include "FlowTile.h"

namespace flow {
    
    const BYTE EMPTY = 1;
    const BYTE BLOCKED = 255;

    struct Agent {
        FVector2D acceleration;
        float xPos;
        float yPos;
        float separationForce;

        bool isSeekingTarget;
        FVector2D target;
    };

    class FlowPath {
    private:
        //TODO: use empty data instead of tile
        std::unique_ptr<FlowTile> emptyTile;
        int32 tileLength;
        TMap<FIntPoint, FlowTile *> tileMap;
        TMap<Agent *, std::unique_ptr<Agent>> agents;

        void updatePortals(FIntPoint tileCoordinates);

        FlowTile *getTile(FIntPoint tileCoordinates);

    public:
        explicit FlowPath(int32 tileLength);

        ~FlowPath();

        void updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData);

        //TODO remove, debug only
        void printPortals() const;

        void addAgent(std::unique_ptr<Agent> agent);

        void killAgent(Agent *agent);

        void updateAgents();
    };
}
