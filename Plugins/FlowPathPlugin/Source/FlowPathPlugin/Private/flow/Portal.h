//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"

namespace flow {
    enum class Orientation {
        NONE, TOP, BOTTOM, LEFT, RIGHT
    };

    class FlowTile;

    struct Portal {
        FIntPoint start;
        FIntPoint end;
        FIntPoint center;
        Orientation orientation;
        TMap<Portal *, int32> connected;
        FlowTile *parentTile;

        Portal(int32 startX, int32 startY, int32 endX, int32 endY, Orientation orientation, FlowTile *parentTile);

        Portal(FIntPoint start, FIntPoint end, Orientation orientation, FlowTile *parentTile);
    };
}

