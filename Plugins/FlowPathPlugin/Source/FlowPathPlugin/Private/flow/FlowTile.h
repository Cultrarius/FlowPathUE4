//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"
#include "Portal.h"

namespace flow {
    class FlowTile {
    private:
        TArray<BYTE> tileData;
        TArray<BYTE>* fixedTileData;

        void initPortalData(int32 tileLength);

    public:
        TArray<Portal> portals;

        explicit FlowTile(const TArray<BYTE> &tileData, int32 tileLength);

        explicit FlowTile(TArray<BYTE>* fixedTileData, int32 tileLength);

        TArray<int32> getPortalsIndicesFor(int32 x, int32 y) const;

        void connectOverlappingPortals(FlowTile &tile, Orientation side);
    };
}
