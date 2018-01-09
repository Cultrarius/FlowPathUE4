//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
#include "flow/EikonalSolver.h"
#include <iostream>

using namespace std;
using namespace flow;

FlowPath::FlowPath(int32 tileLength) : tileLength(tileLength) {
    int32 size = tileLength * tileLength;
    emptyTileData.AddUninitialized(size);
    fullTileData.AddUninitialized(size);
    for (int32 i = 0; i < size; i++) {
        emptyTileData[i] = EMPTY;
        fullTileData[i] = BLOCKED;
    }
}

bool FlowPath::updateMapTile(int32 tileX, int32 tileY, const TArray<uint8> &tileData) {
    if (tileData.Num() != tileLength * tileLength) {
        return false;
    }
    bool isEmpty = true;
    bool isBlocked = true;
    for (int32 i = 0; i < tileLength * tileLength; i++) {
        if (tileData[i] == 0) {
            return false;
        }
        if (tileData[i] != EMPTY) {
            isEmpty = false;
        }
        if (tileData[i] != BLOCKED) {
            isBlocked = false;
        }
    }

    FIntPoint coord(tileX, tileY);
    FlowTile *tile;
    if (isEmpty) {
        tile = new FlowTile(&emptyTileData, tileLength, coord);
    }
    else if (isBlocked) {
        tile = new FlowTile(&fullTileData, tileLength, coord);
    }
    else {
        tile = new FlowTile(tileData, tileLength, coord);
    }
    auto existingTile = tileMap.Find(coord);
    if (existingTile != nullptr) {
        (*existingTile)->removeConnectedPortals();
        tileMap.Remove(coord);
    }
    tileMap.Add(coord, TUniquePtr<FlowTile>(tile));
    updatePortals(coord);
    return true;
}

uint8 FlowPath::getDataFor(const TilePoint & p) const
{
    auto tile = tileMap.Find(p.tileLocation);
    if (tile == nullptr || !isValidTileLocation(p.pointInTile)) {
        return BLOCKED;
    }
    
    int32 index = (*tile)->toIndex(p.pointInTile);
    return (*tile)->getData()[index];
}

PathSearchResult FlowPath::findDirectPath(FIntPoint start, FIntPoint end)
{
    return (*tileMap.Find(FIntPoint(1, 1)))->findPath(start, end);
}

void FlowPath::updatePortals(FIntPoint tileCoordinates) {
    FlowTile *tile = getTile(tileCoordinates);
    if (tile == nullptr) {
        return;
    }

    FIntPoint left(tileCoordinates.X - 1, tileCoordinates.Y);
    FIntPoint right(tileCoordinates.X + 1, tileCoordinates.Y);
    FIntPoint top(tileCoordinates.X, tileCoordinates.Y - 1);
    FIntPoint bottom(tileCoordinates.X, tileCoordinates.Y + 1);

    FlowTile *leftTile = getTile(left);
    FlowTile *rightTile = getTile(right);
    FlowTile *topTile = getTile(top);
    FlowTile *bottomTile = getTile(bottom);

    if (leftTile != nullptr) {
        tile->connectOverlappingPortals(*leftTile, Orientation::LEFT);
    }
    if (rightTile != nullptr) {
        tile->connectOverlappingPortals(*rightTile, Orientation::RIGHT);
    }
    if (topTile != nullptr) {
        tile->connectOverlappingPortals(*topTile, Orientation::TOP);
    }
    if (bottomTile != nullptr) {
        tile->connectOverlappingPortals(*bottomTile, Orientation::BOTTOM);
    }
}

FlowTile *FlowPath::getTile(FIntPoint tileCoordinates) {
    auto tile = tileMap.Find(tileCoordinates);
    return tile == nullptr ? nullptr : tile->Get();
}

PortalSearchResult FlowPath::findPortalPath(const TileVector& vector, bool useCache)
{
    return findPortalPath(vector.start, vector.end, useCache);
}

void FlowPath::cachePortalPath(const TilePoint & target, TArray<const Portal*> waypoints)
{
    check(waypoints.Num() % 2 == 0);

    int32 absoluteX = target.pointInTile.X + target.tileLocation.X * tileLength;
    int32 absoluteY = target.pointInTile.Y + target.tileLocation.Y * tileLength;
    for (int i = 0; i < waypoints.Num(); i++) {
        auto portal = waypoints[i];
        auto nextPortal = (i == waypoints.Num() - 1) ? nullptr : waypoints[i + 1];
        waypointCache.FindOrAdd(portal).Add({ absoluteX , absoluteY }, nextPortal);
    }
}

void FlowPath::deleteFromPathCache(const TilePoint & targetKey)
{
    int32 absoluteX = targetKey.pointInTile.X + targetKey.tileLocation.X * tileLength;
    int32 absoluteY = targetKey.pointInTile.Y + targetKey.tileLocation.Y * tileLength;
    FIntPoint key(absoluteX, absoluteY);
    for (auto& cacheEntry : waypointCache) {
        cacheEntry.Value.Remove(key);
    }
}

PortalSearchResult FlowPath::checkCache(const Portal* start, const FIntPoint& key) const
{
    PortalSearchResult result;
    const CacheEntry* cacheEntry = waypointCache.Find(start);
    if (cacheEntry == nullptr) {
        return result;
    }
    
    if (cacheEntry->Contains(key)) {
        auto nextPortal = (*cacheEntry)[key];
        if (nextPortal != nullptr && nextPortal->tileCoordinates == start->tileCoordinates) {
            start = nextPortal;
            cacheEntry = waypointCache.Find(start);
        }
        
        for (int i = 0; i < 100000; i++) { // just a guard against faulty data
            result.waypoints.Add(start);
            start = (*cacheEntry)[key];
            if (start == nullptr) {
                result.success = result.waypoints.Num() % 2 == 0;
                return result;
            }
            cacheEntry = waypointCache.Find(start);
        }
        ensureMsgf(false, TEXT("Waypoint cache loop count too big; trying to reach target cell (%d, %d)"), key.X, key.Y);
    }
    return result;
}

TArray<const Portal*> createWaypoints(TMap<const Portal*, PortalSearchNode>& searchedNodes, const Portal* startPortal, const Portal* lastPortal) {
    // create waypoints from the portals jumped from start to end
    TArray<const Portal*> allWaypoints;
    PortalSearchNode frontier = searchedNodes[lastPortal];
    while (frontier.nodePortal != startPortal) {
        allWaypoints.Add(frontier.nodePortal);
        frontier = searchedNodes[frontier.parentPortal];
    }

    // reverse the waypoint order and remove unnecessary waypoint-jumps inside the same tile
    TArray<const Portal*> waypoints;
    const Portal* lastWaypoint = nullptr;
    for (int i = allWaypoints.Num() - 1; i >= 0; i--) {
        auto waypoint = allWaypoints[i];
        if (lastWaypoint == nullptr || lastWaypoint->parentTile == waypoint->parentTile) {
            lastWaypoint = waypoint;
        }
        else {
            waypoints.Add(lastWaypoint);
            waypoints.Add(waypoint);
            lastWaypoint = nullptr;
        }
    }
    return waypoints;
}


PortalSearchResult FlowPath::findPortalPath(const TilePoint& start, const TilePoint& end, bool useCache)
{
    // This method conducts a modified A* search over the tile portals to find a path to the specified goal.
    // The search can merge with previous search results, which might not always lead to the optimal route,
    // but it speeds up searches from different start cells to the same target and tends to keep groups together.

    PortalSearchResult result;
    auto startTile = tileMap.Find(start.tileLocation);
    auto endTile = tileMap.Find(end.tileLocation);
    FIntPoint startPoint = start.pointInTile;
    FIntPoint endPoint = end.pointInTile;
    int32 absoluteEndX = endPoint.X + end.tileLocation.X * tileLength;
    int32 absoluteEndY = endPoint.Y + end.tileLocation.Y * tileLength;
    FIntPoint absoluteEnd(absoluteEndX, absoluteEndY);

    // sanity checks
    if (startTile == nullptr || endTile == nullptr || !isValidTileLocation(startPoint) || !isValidTileLocation(endPoint) ||
        (*startTile)->getData(startPoint) == BLOCKED || (*endTile)->getData(endPoint) == BLOCKED) {
        return result;
    }

    // check if maybe start and end are already on the same tile
    if (start.tileLocation == end.tileLocation) {
        PathSearchResult directPath = (*startTile)->findPath(start.pointInTile, end.pointInTile);
        if (directPath.success) {
            result.success = true;
            return result;
        }
    }

    // we construct two special portals so they can be inserted into the search queue nodes
    TArray<PortalSearchNode> searchQueue;
    Portal startPortal(startPoint, startPoint, Orientation::NONE, startTile->Get());
    Portal endPortal(endPoint, endPoint, Orientation::NONE, endTile->Get());
    PortalSearchNode startNode = { -1, -1, &startPortal, &startPortal, false };
    TSet<const Portal*> queuedPortals;

    // start by inserting the portals in the start tile into the queue
    for (auto& portal : (*startTile)->getPortals()) {
        PathSearchResult searchResult = (*startTile)->findPath(startPoint, portal.center);
        if (searchResult.success) {
            PortalSearchResult cacheResult = useCache ? checkCache(&portal, absoluteEnd) : PortalSearchResult();
            if (cacheResult.success) {
                return cacheResult;
            }
            TilePoint portalPoint = {start.tileLocation, portal.center};
            int32 goalCost = searchResult.pathCost + calcGoalHeuristic(portalPoint, end);
            PortalSearchNode newNode = {searchResult.pathCost, goalCost, &portal, &startPortal, true};
            searchQueue.HeapPush(newNode);
            queuedPortals.Add(&portal);
        }
    }

    TMap<const Portal*, PortalSearchNode> searchedNodes;
    searchedNodes.Add(&startPortal, startNode);
    PortalSearchNode frontier;
    while (searchQueue.Num() > 0) {
        searchQueue.HeapPop(frontier, false);
        frontier.open = false;
        auto frontierPortal = frontier.nodePortal;
        if (searchedNodes.Contains(frontierPortal)) {
            // we have already searched this portal node
            continue;
        }
        searchedNodes.Add(frontierPortal, frontier);
        
        // check to see if we have reached the goal
        if (frontierPortal == &endPortal) {
            result.success = true;
            result.waypoints = createWaypoints(searchedNodes, &startPortal, frontier.parentPortal);
            if (useCache) {
                cachePortalPath(end, result.waypoints);
            }
            break;
        }

        // Check the cache to merge with previous queries, but only if we are more than one tile away from the start.
        // This is a simple heuristic to prevent suboptimal paths for short distances (where it is the most notable).
        if (useCache && (frontierPortal->tileCoordinates - start.tileLocation).SizeSquared() > 2) {
            PortalSearchResult cacheResult = useCache ? checkCache(frontierPortal, absoluteEnd) : PortalSearchResult();
            if (cacheResult.success) {
                result.waypoints = createWaypoints(searchedNodes, &startPortal, frontierPortal);
                if ((result.waypoints.Num() + cacheResult.waypoints.Num()) % 2 == 0 && !result.waypoints.Contains(cacheResult.waypoints[0])) { // sanity checks
                    result.waypoints.Append(cacheResult.waypoints);
                    cachePortalPath(end, result.waypoints);
                    break;
                }
            }
        }

        // if we are on the goal tile we try to reach the target point from the portal
        if (frontierPortal->tileCoordinates == end.tileLocation) {
            PathSearchResult searchResult = (*endTile)->findPath(frontierPortal->center, end.pointInTile);
            if (searchResult.success) {
                int32 nodeCost = frontier.nodeCost + searchResult.pathCost;
                PortalSearchNode endNode = { nodeCost, nodeCost, &endPortal, frontierPortal, true };
                searchQueue.HeapPush(endNode);
                queuedPortals.Add(&endPortal);
            }
        }

        // check connected portals to reach the goal tile
        for (auto& connected : frontierPortal->connected) {
            Portal* target = connected.Key;
            int32 nodeCost = frontier.nodeCost + connected.Value;
            TilePoint portalPoint = { target->tileCoordinates, target->center };
            int32 goalCost = nodeCost + calcGoalHeuristic(portalPoint, end);
            PortalSearchNode newNode = { nodeCost, goalCost, target, frontierPortal, true };
            searchQueue.HeapPush(newNode);
            queuedPortals.Add(target);
        }
    }

    return result;
}

TArray<FIntPoint> FlowPath::getAllValidTileCoordinates() const
{
    TArray<FIntPoint> result;
    for (auto& pair : tileMap) {
        result.Add(pair.Key);
    }
    return result;
}

TArray<const Portal*> FlowPath::getAllPortals() const
{
    TArray<const Portal*> result;

    for (auto& tilePair : tileMap) {
        for (auto& portal : tilePair.Value->getPortals()) {
            result.Add(&portal);
        }
    }

    return result;
}

TMap<FIntPoint, TArray<TArray<EikonalCellValue>>> flow::FlowPath::getAllFlowMaps() const
{
    TMap<FIntPoint, TArray<TArray<EikonalCellValue>>> result;
    for (auto& tile : tileMap) {
        result.Add(tile.Key, tile.Value->getAllFlowMaps());
    }
    return result;
}

int32 FlowPath::fastFlowMapLookup(const TileVector& vector, const Portal* nextPortal, const Portal* connectedPortal, const Portal* lookaheadPortal)
{
    auto& cellLocation = vector.start.pointInTile;
    int32 cellIndex = cellLocation.X + cellLocation.Y * tileLength;
    if (nextPortal == nullptr) {
        check(connectedPortal == nullptr);
        if (vector.start.tileLocation != vector.end.tileLocation) {
            return -1;
        }
        // get flowmap to direct target location
        auto tile = tileMap.Find(vector.end.tileLocation);
        if (tile == nullptr) {
            return -1;
        }
        // TODO add result caching
        // TODO add lookahead if target tile is diagonal start tile
        TArray<FIntPoint> targets = { vector.end.pointInTile };
        const auto& tileFlowMap = (*tile)->createMapToTarget(targets);
        return tileFlowMap[cellIndex].directionLookupIndex;
    }
    else {
        check(connectedPortal != nullptr);

        // get flowmap to target portals
        auto tile = tileMap.Find(vector.start.tileLocation);
        if (tile == nullptr) {
            return -1;
        }

        auto delta = lookaheadPortal == nullptr ? FIntPoint::ZeroValue : lookaheadPortal->tileCoordinates - vector.start.tileLocation;
        if (delta.SizeSquared() != 2) {
            auto& tileFlowMap = (*tile)->createMapToPortal(nextPortal, connectedPortal);
            return tileFlowMap[cellIndex].directionLookupIndex;
        }
        auto dataProvider = [this, &vector, &delta](TArray<uint8>& data) {
            // only create the data when necessary, as the tile might have already cached the flowmap result
            data.AddUninitialized(tileLength * tileLength * 4);
            
            for (int32 i = 0; i < 4; i++) {
                bool xFactor = xFactorArray[i];
                bool yFactor = yFactorArray[i];
                bool isRight = (delta.X == 1) == xFactor;
                bool isDown = (delta.Y == 1) == yFactor;
                int32 deltaX = delta.X * (xFactor ? 1 : 0);
                int32 deltaY = delta.Y * (yFactor ? 1 : 0);                        

                auto tile = tileMap.Find(vector.start.tileLocation + FIntPoint(deltaX, deltaY));
                auto& tileData = tile == nullptr ? fullTileData : (*tile)->getData();                
                for (int32 y = 0; y < tileLength; y++) {
                    for (int32 x = 0; x < tileLength; x++) {
                        int32 sourceIndex = x + y * tileLength;                                    
                        int32 targetIndex = toFourTileIndex(isRight, isDown, x, y, tileLength);
                        data[targetIndex] = tileData[sourceIndex];
                    }
                }
            }
        };
        auto& tileFlowMap = (*tile)->createLookaheadFlowmap(nextPortal, lookaheadPortal, dataProvider);
        return tileFlowMap[cellIndex].directionLookupIndex;
    }
}

bool FlowPath::getFlowMapValue(const TileVector& vector, const Portal* nextPortal, const Portal* connectedPortal, FlowMapExtract & result)
{
    if (nextPortal == nullptr) {
        check(connectedPortal == nullptr);
        if (vector.start.tileLocation != vector.end.tileLocation) {
            return false;
        }
        // get flowmap to direct target location
        auto tile = tileMap.Find(vector.end.tileLocation);
        if (tile == nullptr) {
            return false;
        }
        // TODO add result caching
        TArray<FIntPoint> targets = { vector.end.pointInTile };
        const auto& tileFlowMap = (*tile)->createMapToTarget(targets);
        extractPartialFlowmap(vector.start, tileFlowMap, Orientation::NONE, result);
    }
    else {
        check(connectedPortal != nullptr);
        if (!nextPortal->connected.Contains(connectedPortal) || vector.start.tileLocation != nextPortal->tileCoordinates) {
            return false;
        }

        // get flowmap to target portals
        auto tile = tileMap.Find(vector.start.tileLocation);
        if (tile == nullptr) {
            return false;
        }
        const auto& tileFlowMap = (*tile)->createMapToPortal(nextPortal, connectedPortal);
        extractPartialFlowmap(vector.start, tileFlowMap, nextPortal->orientation, result);
    }
    return true;
}

void FlowPath::extractPartialFlowmap(const TilePoint & p, const TArray<EikonalCellValue>& flowMap, Orientation nextPortalOrientation, FlowMapExtract & result) const
{
    int32 selfIndex = p.pointInTile.X + p.pointInTile.Y * tileLength;
    float flowVal = flowMap[selfIndex].cellValue;
    result.cellValue = flowVal;
    bool standingOnPortal = flowVal == 0 && nextPortalOrientation != Orientation::NONE;

    for (int32 i = 0; i < 8; i++) {
        auto neighbor = p.pointInTile + neighbors[i];
        int32 tileDeltaX = neighbor.X < 0 ? -1 : (neighbor.X >= tileLength ? 1 : 0);
        int32 tileDeltaY = neighbor.Y < 0 ? -1 : (neighbor.Y >= tileLength ? 1 : 0);
        if (tileDeltaX == 0 && tileDeltaY == 0) {
            int32 index = neighbor.X + neighbor.Y * tileLength;
            result.neighborCells[i] = flowMap[index].cellValue + (standingOnPortal ? 300 : 0);
        }
        else {
            // The neighbor is on another tile, so we have no flowmap for it.
            // We guess a value based on two things:
            // 1. don't unnecessary cross tiles (cause that might trigger a new flowmap calculation)
            // 2. do cross tiles when following portals
            auto neighborTile = tileMap.Find(p.tileLocation + FIntPoint(tileDeltaX, tileDeltaY));
            if (neighborTile == nullptr) {
                result.neighborCells[i] = MAX_VAL;
                continue;
            }

            // get the cell value of the neighbor tile
            neighbor.X = (neighbor.X + tileLength) % tileLength;
            neighbor.Y = (neighbor.Y + tileLength) % tileLength;
            int32 index = neighbor.X + neighbor.Y * tileLength;
            uint8 val = (*neighborTile)->getData()[index];

            // if the cell is blocked then we don't have to check anything
            if (val == BLOCKED) {
                result.neighborCells[i] = MAX_VAL;
                continue;
            }
            
            // check to see if we are following a portal and the neighbor cell is part of the next portal
            if (standingOnPortal) {
                if (nextPortalOrientation == Orientation::TOP && tileDeltaX == 0 && tileDeltaY == -1) {
                    result.neighborCells[i] = val;
                    continue;
                }
                if (nextPortalOrientation == Orientation::BOTTOM && tileDeltaX == 0 && tileDeltaY == 1) {
                    result.neighborCells[i] = val;
                    continue;
                }
                if (nextPortalOrientation == Orientation::RIGHT && tileDeltaX == 1 && tileDeltaY == 0) {
                    result.neighborCells[i] = val;
                    continue;
                }
                if (nextPortalOrientation == Orientation::LEFT && tileDeltaX == -1 && tileDeltaY == 0) {
                    result.neighborCells[i] = val;
                    continue;
                }
            }

            // we use a sufficiently high value so an agent does not go there until it absolutely has to
            result.neighborCells[i] = val + 500;
        }
    }
}

bool FlowPath::isValidTileLocation(const FIntPoint & p) const
{
    return p.X >= 0 && p.Y >= 0 && p.X < tileLength && p.Y < tileLength;
}

int32 FlowPath::calcGoalHeuristic(const TilePoint& start, const TilePoint& end) const
{
    // calculate the absolute distance between start and end across the tiles
    int32 startX = start.tileLocation.X * tileLength + start.pointInTile.X;
    int32 startY = start.tileLocation.Y * tileLength + start.pointInTile.Y;
    int32 endX = end.tileLocation.X * tileLength + end.pointInTile.X;
    int32 endY = end.tileLocation.Y * tileLength + end.pointInTile.Y;
    return FIntPoint(endX - startX, endY - startY).Size();
}


