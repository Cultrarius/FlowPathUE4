//
// Created by Michael on 18.12.2017.
//

#pragma once

#include "CoreMinimal.h"
#include <set>

namespace flow {
    enum Orientation {
        TOP, BOTTOM, LEFT, RIGHT
    };

    class FlowTile;

    struct Portal {
        int startX;
        int startY;
        int endX;
        int endY;
        Orientation orientation;
        TSet<Portal *> connected;
        FlowTile *parentTile;

        Portal(int startX, int startY, int endX, int endY, Orientation orientation, FlowTile *parentTile);
    };
}

