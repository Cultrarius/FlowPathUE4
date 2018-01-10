//
// Created by michael on 17.12.17.
//

#pragma once

#include "FlowTile.h"

namespace flow {

    const bool xFactorArray[4] = { false, true, false, true };
    const bool yFactorArray[4] = { false, false, true, true };
    typedef TMap<FIntPoint, TUniquePtr<FlowTile>> TileMap;
    
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
        bool success = false;
        TArray<const Portal*> waypoints;
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

    typedef TMap<FIntPoint, const Portal*> CacheEntry;
    typedef TMap<const Portal*, CacheEntry> WaypointCache;

    class FlowPath {
    private:
        TArray<uint8> emptyTileData;
        TArray<uint8> fullTileData;

        int32 tileLength;
        TileMap tileMap;
        WaypointCache waypointCache;

        void updatePortals(FIntPoint tileCoordinates);

        FlowTile *getTile(FIntPoint tileCoordinates);

        bool isValidTileLocation(const FIntPoint& p) const;

        int32 calcGoalHeuristic(const TilePoint& start, const TilePoint& end) const;

        void extractPartialFlowmap(const TilePoint& p, const TArray<EikonalCellValue>& flowMap, Orientation nextPortalOrientation, FlowMapExtract& result) const;

        PortalSearchResult checkCache(const Portal* start, const FIntPoint& absoluteTarget) const;

        std::function<void(TArray<uint8>&)> createFlowmapDataProvider(FIntPoint startTile, FIntPoint delta) const;

    public:
        explicit FlowPath(int32 tileLength);

        bool updateMapTile(int32 tileX, int32 tileY, const TArray<uint8> &tileData);

        uint8 getDataFor(const TilePoint& p) const;

        PathSearchResult findDirectPath(FIntPoint start, FIntPoint end);

        PortalSearchResult findPortalPath(const TilePoint& start, const TilePoint& end, bool useCache);

        PortalSearchResult findPortalPath(const TileVector& vector, bool useCache);

        void cachePortalPath(const TilePoint& targetKey, TArray<const Portal*> waypoints);

        void deleteFromPathCache(const TilePoint& targetKey);

        TArray<FIntPoint> getAllValidTileCoordinates() const;

        TArray<const Portal*> getAllPortals() const;

        TMap<FIntPoint, TArray<TArray<EikonalCellValue>>> getAllFlowMaps() const;

        bool getFlowMapValue(const TileVector& vector, const Portal* nextPortal, const Portal* connectedPortal, FlowMapExtract& result);

        int32 fastFlowMapLookup(const TileVector& vector, const Portal* nextPortal, const Portal* connectedPortal, const Portal* lookaheadPortal);

        bool hasFlowMap(const Portal* startPortal, const Portal* targetPortal) const;

        void createFlowMapSourceData(FIntPoint startTile, FIntPoint delta, TArray<uint8>& result);

        void cacheFlowMap(const Portal * resultStartPortal, const Portal * resultEndPortal, const TArray<flow::EikonalCellValue>& result);
    };
}
