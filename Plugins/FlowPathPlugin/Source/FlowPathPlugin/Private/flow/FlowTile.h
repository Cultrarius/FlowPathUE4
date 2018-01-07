//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"
#include "Portal.h"

namespace flow {

    struct AStarTile {
        int32 pointCost;
        int32 goalCost;
        FIntPoint location;
        FIntPoint parentTile;
        FIntPoint nextOpen;
        FIntPoint previousOpen;
        bool open;
    };

    struct PathSearchResult {
        bool success;
        TArray<FIntPoint> waypoints;
        int32 pathCost;
    };

    struct FlowPortalKey {
        const Portal* targetPortal;
        const Portal* connectedPortal;

        bool operator==(const FlowPortalKey& Other) const
        {
            return targetPortal == Other.targetPortal && connectedPortal == Other.connectedPortal;
        }
        
        friend uint32 GetTypeHash(const FlowPortalKey& Other)
        {
            return PointerHash(Other.targetPortal, PointerHash(Other.connectedPortal));
        }
    };

    struct EikonalCellValue {
        int8 directionLookupIndex;
        float cellValue;
    };

    class FlowTile {
    private:
        TArray<uint8> tileData;
        TArray<uint8>* fixedTileData;
        FIntPoint coordinates;
        int32 tileLength;
        TArray<Portal> portals;
        TMap<FlowPortalKey, TArray<EikonalCellValue>> eikonalMaps;

        void initPortalData();

        static int32 distance(FIntPoint p1, FIntPoint p2);

        void initializeFrontier(const FIntPoint& frontier, TArray<bool>& initializedTiles, TArray<AStarTile>& tiles, const FIntPoint& goal) const;

        void initFrontierTile(const FIntPoint& tile, TArray<bool> &initializedTiles, TArray<AStarTile> &tiles, int32 frontierIndex, const FIntPoint & goal, const FIntPoint& frontier) const;
        
        bool isCrossMoveAllowed(const FIntPoint& from, const FIntPoint& to) const;

    public:

        int32 toIndex(int32 x, int32 y) const;

        int32 toIndex(const FIntPoint& coordinates) const;

        const TArray<Portal>& getPortals() const;

        const FIntPoint& getCoordinates() const;

        const TArray<uint8>& getData() const;

        uint8 getData(FIntPoint coordinates) const;

        explicit FlowTile(const TArray<uint8> &tileData, int32 tileLength, FIntPoint coordinates);

        explicit FlowTile(TArray<uint8>* fixedTileData, int32 tileLength, FIntPoint coordinates);

        TArray<int32> getPortalsIndicesFor(int32 x, int32 y) const;

        void connectOverlappingPortals(FlowTile &tile, Orientation side);

        void removeConnectedPortals();

        PathSearchResult findPath(FIntPoint start, FIntPoint end);

        const TArray<EikonalCellValue>& createMapToPortal(const Portal* targetPortal, const Portal* connectedPortal);

        TArray<EikonalCellValue> createMapToTarget(const TArray<FIntPoint>& targets);

        TArray<TArray<EikonalCellValue>> getAllFlowMaps() const;
    };
}
