//
// Created by Michael on 18.12.2017.
//

#include "FlowTile.h"
#include <queue>
#include "FlowPath.h"

using namespace std;
using namespace flow;

FlowTile::FlowTile(const TArray<BYTE> &tileData, int32 tileLength) : tileData(tileData) {
    int32 maxIndex = tileLength - 1;

    //find left portals
    int32 start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = tileData[i * tileLength];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(0, start, 0, i - 1, LEFT, this);
            start = -1;
        } else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(0, start, 0, maxIndex, LEFT, this);
    }

    //find right portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = tileData[i * tileLength + tileLength - 1];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(maxIndex, start, maxIndex, i - 1, RIGHT, this);
            start = -1;
        } else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(maxIndex, start, maxIndex, maxIndex, RIGHT, this);
    }

    //find top portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = tileData[i];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(start, 0, i - 1, 0, TOP, this);
            start = -1;
        } else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(start, 0, maxIndex, 0, TOP, this);
    }

    //find bottom portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = tileData[i + tileLength * maxIndex];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(start, maxIndex, i - 1, maxIndex, BOTTOM, this);
            start = -1;
        } else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(start, maxIndex, maxIndex, maxIndex, BOTTOM, this);
    }

    // connect the portals via flood fill
    //TODO use more efficient algorithm, e.g. https://www.codeproject.com/Articles/6017/QuickFill-An-efficient-flood-fill-algorithm
    TArray<BYTE> floodData = tileData;
    for (int32 y = 0; y < tileLength; y++) {
        for (int32 x = 0; x < tileLength; x++) {
            int32 index = x + y * tileLength;
            auto val = floodData[index];
            if (val > 0 && val != BLOCKED) {
                floodData[index] = 0;
                auto indices = getPortalsIndicesFor(x, y);
                TSet<int32> connected;
                connected.Append(indices);
                queue<FIntPoint> todo;
                todo.emplace(x + 1, y);
                todo.emplace(x - 1, y);
                todo.emplace(x, y + 1);
                todo.emplace(x, y - 1);

                for (; !todo.empty(); todo.pop()) {
                    auto &coord = todo.front();
                    int32 floodX = coord.X;
                    int32 floodY = coord.Y;
                    int32 floodIndex = floodX + floodY * tileLength;
                    if (floodX < 0 ||  (floodX - tileLength) >= 0 || floodY < 0 || (floodY - tileLength) >= 0 ||
                        floodData[floodIndex] == 0 || floodData[floodIndex] == BLOCKED) {
                        continue;
                    }
                    floodData[floodIndex] = 0;
                    todo.emplace(floodX + 1, floodY);
                    todo.emplace(floodX - 1, floodY);
                    todo.emplace(floodX, floodY + 1);
                    todo.emplace(floodX, floodY - 1);

                    indices = getPortalsIndicesFor(floodX, floodY);
                    connected.Append(indices);
                }

                TArray<Portal *> connectedPortals;
                for (int32 pIndex : connected) {
                    connectedPortals.Add(&portals[pIndex]);
                }
                for (auto portal : connectedPortals) {
                    portal->connected.Append(connectedPortals);
                    portal->connected.Remove(portal);
                }
            }
        }
    }
}

TArray<int32> FlowTile::getPortalsIndicesFor(int32 x, int32 y) const {
    TArray<int32> result;
    for (int32 i = 0; i < portals.Num(); i++) {
        const Portal &portal = portals[i];
        if (portal.startX <= x && portal.endX >= x && portal.startY <= y && portal.endY >= y) {
            result.Add(i);
        }
    }
    return result;
}

void FlowTile::connectOverlappingPortals(FlowTile &tile, Orientation side) {
    for (auto &otherPortal : tile.portals) {
        if (side == LEFT && otherPortal.orientation != RIGHT ||
            side == RIGHT && otherPortal.orientation != LEFT ||
            side == TOP && otherPortal.orientation != BOTTOM ||
            side == BOTTOM && otherPortal.orientation != TOP) {
            continue;
        }
        for (auto &thisPortal : portals) {
            if (thisPortal.orientation != side) {
                continue;
            }

            int32 startX = max(otherPortal.startX, thisPortal.startX);
            int32 startY = max(otherPortal.startY, thisPortal.startY);
            int32 endX = min(otherPortal.endX, thisPortal.endX);
            int32 endY = min(otherPortal.endY, thisPortal.endY);
            if (((side == TOP || side == BOTTOM) && startX <= endX) ||
                ((side == LEFT || side == RIGHT) && startY <= endY)) {
                otherPortal.connected.Add(&thisPortal);
                thisPortal.connected.Add(&otherPortal);
            }
        }
    }
}
