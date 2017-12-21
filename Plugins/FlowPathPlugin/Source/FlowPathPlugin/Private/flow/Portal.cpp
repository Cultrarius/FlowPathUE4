//
// Created by Michael on 18.12.2017.
//

#include "Portal.h"

using namespace flow;

flow::Portal::Portal(int32 startX, int32 startY, int32 endX, int32 endY, Orientation orientation, FlowTile * parentTile) :
    Portal(FIntPoint(startX, startY), FIntPoint(endX, endY), orientation, parentTile)
{
}

Portal::Portal(FIntPoint start, FIntPoint end, Orientation orientation, FlowTile *parent)
        : start(start), end(end), orientation(orientation), parentTile(parent) {
    center = (end + start) / 2;
}
