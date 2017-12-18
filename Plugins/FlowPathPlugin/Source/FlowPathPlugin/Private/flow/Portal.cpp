//
// Created by Michael on 18.12.2017.
//

#include "Portal.h"

using namespace flow;

Portal::Portal(int32 startX, int32 startY, int32 endX, int32 endY, Orientation orientation, FlowTile *parent)
        : startX(startX), startY(startY), endX(endX), endY(endY), orientation(orientation), parentTile(parent) {}
