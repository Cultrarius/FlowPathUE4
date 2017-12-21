//
// Created by michael on 17.12.17.
//

#pragma once

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
        TArray<BYTE> emptyTileData;
        TArray<BYTE> fullTileData;

        int32 tileLength;
        bool allowCrossGridMovement;
        TMap<FIntPoint, TUniquePtr<FlowTile>> tileMap;
        TMap<Agent *, TUniquePtr<Agent>> agents;

        void updatePortals(FIntPoint tileCoordinates);

        FlowTile *getTile(FIntPoint tileCoordinates);

    public:
        explicit FlowPath(int32 tileLength, bool allowCrossGridMovement);

        void updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData);

        //TODO remove, debug only
        void printPortals() const;

        void addAgent(TUniquePtr<Agent> agent);

        void killAgent(Agent *agent);

        void updateAgents();

        TArray<FIntPoint> findDirectPath(FIntPoint start, FIntPoint end, bool crossGridMovement);
    };
}
