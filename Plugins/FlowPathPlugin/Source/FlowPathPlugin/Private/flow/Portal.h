//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"

namespace flow {
    enum Orientation {
        TOP, BOTTOM, LEFT, RIGHT
    };

    class FlowTile;

    struct Portal {
        int32 startX;
        int32 startY;
        int32 endX;
        int32 endY;
        Orientation orientation;
        TMap<Portal *, float> connected;
        FlowTile *parentTile;

        Portal(int32 startX, int32 startY, int32 endX, int32 endY, Orientation orientation, FlowTile *parentTile);
    };
}

