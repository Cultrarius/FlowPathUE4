//
// Created by michael on 17.12.17.
//

#pragma once

#include "FlowTile.h"

namespace flow {
    
    const uint8 EMPTY = 1;
    const uint8 BLOCKED = 255;

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

    struct TileVector {
        TilePoint start;
        TilePoint end;
    };

    struct FlowMapExtract {
        float cellValue;
        float neighborCells[8];
    };

    class FlowPath {
    private:
        TArray<uint8> emptyTileData;
        TArray<uint8> fullTileData;

        int32 tileLength;
        TMap<FIntPoint, TUniquePtr<FlowTile>> tileMap;

        void updatePortals(FIntPoint tileCoordinates);

        FlowTile *getTile(FIntPoint tileCoordinates);

        bool isValidTileLocation(const FIntPoint& p) const;

        int32 calcGoalHeuristic(const TilePoint& start, const TilePoint& end) const;

        void extractPartialFlowmap(const TilePoint& p, const TArray<float>& flowMap, Orientation nextPortalOrientation, FlowMapExtract& result) const;

    public:
        explicit FlowPath(int32 tileLength);

        bool updateMapTile(int32 tileX, int32 tileY, const TArray<uint8> &tileData);

        uint8 getDataFor(const TilePoint& p) const;

        PathSearchResult findDirectPath(FIntPoint start, FIntPoint end);

        PortalSearchResult findPortalPath(const TilePoint& start, const TilePoint& end) const;

        PortalSearchResult findPortalPath(const TileVector& vector) const;

        TArray<FIntPoint> getAllValidTileCoordinates() const;

        TArray<const Portal*> getAllPortals() const;

        bool getFlowMapValue(const TileVector& vector, const Portal* nextPortal, const Portal* connectedPortal, FlowMapExtract& result);
    };
}
