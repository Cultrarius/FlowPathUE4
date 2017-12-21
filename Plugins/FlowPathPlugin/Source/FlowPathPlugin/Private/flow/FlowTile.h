//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"
#include "Portal.h"

namespace flow {

    enum GridDirection {
        North, NorthWest, West, SouthWest, South, SouthEast, East, NorthEast
    };

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

    class FlowTile {
    private:
        TArray<BYTE> tileData;
        TArray<BYTE>* fixedTileData;
        FIntPoint coordinates;
        int32 tileLength;
        TArray<Portal> portals;

        void initPortalData();

        static int32 fastDistance(FIntPoint p1, FIntPoint p2);

        inline int32 toIndex(int32 x, int32 y) const;

        inline int32 toIndex(FIntPoint coordinates) const;

        void initializeFrontier(const FIntPoint& frontier, TArray<bool>& initializedTiles, TArray<AStarTile>& tiles, const FIntPoint& goal) const;

        void initFrontierTile(const FIntPoint& tile, TArray<bool> &initializedTiles, TArray<AStarTile> &tiles, int32 frontierIndex, const FIntPoint & goal, const FIntPoint& frontier) const;

        const TArray<BYTE>& getData() const;

        bool isCrossMoveAllowed(const FIntPoint& from, const FIntPoint& to) const;

    public:

        const TArray<Portal>& getPortals() const;

        const FIntPoint& getCoordinates() const;

        explicit FlowTile(const TArray<BYTE> &tileData, int32 tileLength, FIntPoint coordinates);

        explicit FlowTile(TArray<BYTE>* fixedTileData, int32 tileLength, FIntPoint coordinates);

        TArray<int32> getPortalsIndicesFor(int32 x, int32 y) const;

        void connectOverlappingPortals(FlowTile &tile, Orientation side);

        void removeConnectedPortals();

        PathSearchResult findPath(FIntPoint start, FIntPoint end);
    };
}
