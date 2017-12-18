//
// Created by Michael on 18.12.2017.
//

#include "Portal.h"

using namespace flow;

Portal::Portal(int startX, int startY, int endX, int endY, Orientation orientation, FlowTile *parent)
        : startX(startX), startY(startY), endX(endX), endY(endY), orientation(orientation), parentTile(parent) {}
