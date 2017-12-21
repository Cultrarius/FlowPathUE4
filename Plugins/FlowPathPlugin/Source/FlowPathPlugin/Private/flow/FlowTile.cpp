//
// Created by Michael on 18.12.2017.
//

#include "FlowTile.h"
#include <queue>
#include "FlowPath.h"

using namespace std;
using namespace flow;

const FIntPoint & flow::FlowTile::getCoordinates() const
{
    return coordinates;
}

FlowTile::FlowTile(const TArray<BYTE> &tileData, int32 tileLength, FIntPoint coordinates) : tileData(tileData), fixedTileData(nullptr), coordinates(coordinates), tileLength(tileLength) {
    initPortalData();
}

flow::FlowTile::FlowTile(TArray<BYTE>* fixedTileData, int32 tileLength, FIntPoint coordinates) : fixedTileData(fixedTileData), coordinates(coordinates), tileLength(tileLength)
{
    initPortalData();
}

void flow::FlowTile::initPortalData()
{
    auto& data = getData();
    int32 maxIndex = tileLength - 1;

    //find left portals
    int32 start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = data[i * tileLength];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(0, start, 0, i - 1, LEFT, this);
            start = -1;
        }
        else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(0, start, 0, maxIndex, LEFT, this);
    }

    //find right portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = data[i * tileLength + tileLength - 1];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(maxIndex, start, maxIndex, i - 1, RIGHT, this);
            start = -1;
        }
        else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(maxIndex, start, maxIndex, maxIndex, RIGHT, this);
    }

    //find top portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = data[i];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(start, 0, i - 1, 0, TOP, this);
            start = -1;
        }
        else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(start, 0, maxIndex, 0, TOP, this);
    }

    //find bottom portals
    start = -1;
    for (int32 i = 0; i < tileLength; i++) {
        auto val = data[i + tileLength * maxIndex];
        if (start != -1 && val == BLOCKED) {
            portals.Emplace(start, maxIndex, i - 1, maxIndex, BOTTOM, this);
            start = -1;
        }
        else if (start == -1 && val != BLOCKED) {
            start = i;
        }
    }
    if (start != -1) {
        portals.Emplace(start, maxIndex, maxIndex, maxIndex, BOTTOM, this);
    }

    // connect the portals via flood fill
    //TODO use more efficient algorithm, e.g. https://www.codeproject.com/Articles/6017/QuickFill-An-efficient-flood-fill-algorithm
    TArray<BYTE> floodData = data;
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
                    if (floodX < 0 || (floodX - tileLength) >= 0 || floodY < 0 || (floodY - tileLength) >= 0 ||
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

float flow::FlowTile::distance(FIntPoint p1, FIntPoint p2)
{
    return FMath::Sqrt((p2 - p1).SizeSquared());
}

int32 flow::FlowTile::fastDistance(FIntPoint p1, FIntPoint p2)
{
    return FMath::Abs(p2.X - p1.X) + FMath::Abs(p2.Y - p1.Y);
}

inline int32 flow::FlowTile::toIndex(int32 x, int32 y) const
{
    return x + y * tileLength;
}

inline int32 flow::FlowTile::toIndex(FIntPoint coordinates) const
{
    return coordinates.X + coordinates.Y * tileLength;
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

PathSearchResult flow::FlowTile::findPath(FIntPoint start, FIntPoint end)
{
    TArray<FIntPoint> wayPoints;
    if (start == end) {
        return { false, wayPoints, 0, 0 };
    }

    // do an improved A* search with waypoint smoothing
    // inspired by https://www.gamasutra.com/view/feature/131505/toward_more_realistic_pathfinding.php?page=1

    int32 tileSize = tileLength * tileLength;
    TArray<bool> initializedTiles;
    initializedTiles.AddZeroed(tileSize);
    TArray<AStarTile> tiles;
    tiles.AddUninitialized(tileSize);

    int32 goalIndex = toIndex(end);
    int32 startIndex = toIndex(start);
    
    tiles[startIndex].pointCost = 0;
    initializedTiles[startIndex] = true;
    tiles[startIndex].location = start;
    tiles[startIndex].goalCost = fastDistance(start, end);
    tiles[startIndex].open = false;

    FIntPoint frontier = start;
    do {
        initializeFrontier(frontier, initializedTiles, tiles, end);
        int32 frontierCost = -1;

        // TODO maintain linked list for faster open search
        for (int y = 0; y < tileLength; y++) {
            for (int x = 0; x < tileLength; x++) {
                int32 i = toIndex(x, y);
                if (initializedTiles[i] && tiles[i].open && (frontierCost < 0 || frontierCost > tiles[i].goalCost)) {
                    frontier.X = x;
                    frontier.Y = y;
                    frontierCost = tiles[i].goalCost;
                }
            }
        }
        //UE_LOG(LogExec, Warning, TEXT("New frontier %d, %d"), frontier.X, frontier.Y);

        if (frontierCost == -1) {
            return{ false, wayPoints, 0, 0 };
        }
    } while (frontier != end);

    // TODO maybe add path smoothing
    int32 pathCost = 0;
    float pathLength = 0;
    while (frontier != start) {
        wayPoints.Add(frontier);
        int32 tileIndex = toIndex(frontier);
        pathCost += getData()[tileIndex];
        auto& parent = tiles[tileIndex].parentTile;
        pathLength += frontier.X == parent.X ? 1 : 1.4f;
        frontier = parent;
    }
    wayPoints.Add(start);
    
    return{ true, wayPoints, pathCost, pathLength };
}

bool flow::FlowTile::isCrossMoveAllowed(const FIntPoint& from, const FIntPoint& to) const
{
    // we do not want to allow cross movements where two obstacles meet, because most likely a unit cannot move there.
    // For example, the move here from start S to target T would not be allowed:
    // . # . .
    // . # T . 
    // . S # #
    // . . . . 
    auto& data = getData();
    int32 deltaX = to.X - from.X;
    int32 deltaY = to.Y - from.Y;
    int32 index1 = toIndex(from + FIntPoint(deltaX, 0));
    int32 index2 = toIndex(from + FIntPoint(0, deltaY));
    return data[index1] != BLOCKED || data[index2] != BLOCKED;
}

void flow::FlowTile::initializeFrontier(const FIntPoint& frontier, TArray<bool>& initializedTiles, TArray<AStarTile>& tiles, const FIntPoint & goal) const
{
    int32 frontierIndex = toIndex(frontier);
    tiles[frontierIndex].open = false;

    // init north tile
    if (frontier.Y > 0) {
        FIntPoint tile = frontier + FIntPoint(0, -1);
        initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
    }

    // init north-west tile
    if (frontier.Y > 0 && frontier.X > 0) {
        FIntPoint tile = frontier + FIntPoint(-1, -1);
        if (isCrossMoveAllowed(tile, frontier)) {
            initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
        }
    }

    // init west tile
    if (frontier.X > 0) {
        FIntPoint tile = frontier + FIntPoint(-1, 0);
        initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
    }

    // init south-west tile
    if (frontier.X > 0 && frontier.Y < (tileLength - 1)) {
        FIntPoint tile = frontier + FIntPoint(-1, 1);
        if (isCrossMoveAllowed(tile, frontier)) {
            initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
        }
    }

    // init south tile
    if (frontier.Y < (tileLength - 1)) {
        FIntPoint tile = frontier + FIntPoint(0, 1);
        initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
    }

    // init south-east tile
    if (frontier.Y < (tileLength - 1) && frontier.X < (tileLength - 1)) {
        FIntPoint tile = frontier + FIntPoint(1, 1);
        if (isCrossMoveAllowed(tile, frontier)) {
            initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
        }
    }

    // init east tile
    if (frontier.X < (tileLength - 1)) {
        FIntPoint tile = frontier + FIntPoint(1, 0);
        initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
    }

    // init north-east tile
    if (frontier.X < (tileLength - 1) && frontier.Y > 0) {
        FIntPoint tile = frontier + FIntPoint(1, -1);
        if (isCrossMoveAllowed(tile, frontier)) {
            initFrontierTile(tile, initializedTiles, tiles, frontierIndex, goal, frontier);
        }
    }
}

void flow::FlowTile::initFrontierTile(const FIntPoint& tile, TArray<bool> &initializedTiles, TArray<AStarTile> &tiles, int32 frontierIndex, const FIntPoint & goal, const FIntPoint& frontier) const
{
    auto& data = getData();
    int32 tileIndex = toIndex(tile);
    int32 pointCost = tiles[frontierIndex].pointCost + data[tileIndex];
    int32 goalCost = pointCost + fastDistance(tile, goal);
    if (!initializedTiles[tileIndex]) {
        initializedTiles[tileIndex] = true;
        if (data[tileIndex] == BLOCKED) {
            tiles[tileIndex].open = false;
        }
        else {
            tiles[tileIndex].open = true;
            tiles[tileIndex].location = tile;
            tiles[tileIndex].pointCost = pointCost;
            tiles[tileIndex].goalCost = goalCost;
            tiles[tileIndex].parentTile = frontier;
        }
    }
    else if (tiles[tileIndex].open && goalCost < tiles[tileIndex].goalCost) {
        tiles[tileIndex].pointCost = pointCost;
        tiles[tileIndex].goalCost = goalCost;
        tiles[tileIndex].parentTile = frontier;
    }
}

const TArray<BYTE>& flow::FlowTile::getData() const
{
    if (fixedTileData != nullptr) {
        return *fixedTileData;
    }
    return tileData;
}
