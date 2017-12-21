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

    struct PortalSearchNode {
        int32 nodeCost;
        int32 goalCost;
        const Portal* nodePortal;
        const Portal* parentPortal;
        bool open;

        bool operator<(const PortalSearchNode& other) const
        {
            if (goalCost == other.goalCost) {
                return nodeCost > other.nodeCost;
            }
            return goalCost < other.goalCost;
        }
    };

    struct PortalSearchResult {
        bool success;
        TArray<const Portal*> waypoints;
        int32 pathCost;
    };

    struct TilePoint {
        FIntPoint tileLocation;
        FIntPoint pointInTile;
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

        bool isValidTileLocation(const FIntPoint& p) const;

        int32 calcGoalHeuristic(const TilePoint& start, const TilePoint& end) const;

    public:
        explicit FlowPath(int32 tileLength, bool allowCrossGridMovement);

        void updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData);

        //TODO remove, debug only
        void printPortals() const;

        void addAgent(TUniquePtr<Agent> agent);

        void killAgent(Agent *agent);

        void updateAgents();

        PathSearchResult findDirectPath(FIntPoint start, FIntPoint end);

        PortalSearchResult findPortalPath(const TilePoint& start, const TilePoint& end) const;
    };
}
