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

bool flow::TilePoint::operator==(const TilePoint & other) const
{
    return tileLocation == other.tileLocation && pointInTile == other.pointInTile;
}

bool flow::TilePoint::operator!=(const TilePoint & other) const
{
    return tileLocation != other.tileLocation || pointInTile != other.pointInTile;
}

bool FlowPath::updateMapTile(int32 tileX, int32 tileY, const TArray<uint8> &tileData) {
    if (tileData.Num() != tileLength * tileLength) {
        return false;
    }
    bool isEmpty = true;
    bool isBlocked = true;
    for (int32 i = 0; i < tileData.Num(); i++) {
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
        clearTileFromWaypointCache(**existingTile);
        (*existingTile)->removeConnectedPortals();
        // TODO invalidate lookahead flowmaps
        tileMap.Remove(coord);
    }
    tileMap.Add(coord, TUniquePtr<FlowTile>(tile));
    updatePortals(coord);
    return true;
}

void flow::FlowPath::clearTileFromWaypointCache(const FlowTile & tile)
{
    // see which portals we have to remove from the cache
    TSet<const Portal*> portalsToRemove;
    for (auto& portal : tile.getPortals()) {
        portalsToRemove.Add(&portal);
        for (auto& connected : portal.connected) {
            portalsToRemove.Add(connected.Key);
        }
    }

    //remove the portal entries from the cache
    for (auto& portal : portalsToRemove) {
        auto cacheEntry = waypointCache.Find(portal);
        if (cacheEntry == nullptr) {
            continue;
        }

        for (auto& goal : *cacheEntry) {
            auto targetToRemove = goal.Key;

            // remove portals up the chain
            auto parent = goal.Value.fromPortal;
            while (parent != nullptr) {
                auto nextParent = waypointCache[parent][targetToRemove];
                waypointCache[parent].Remove(targetToRemove);
                parent = nextParent.fromPortal;
            }

            // remove portals down the chain
            auto child = goal.Value.toPortal;
            while (child != nullptr) {
                auto nextChild = waypointCache[child][targetToRemove];
                waypointCache[child].Remove(targetToRemove);
                child = nextChild.toPortal;
            }
        }

        waypointCache.Remove(portal);
    }
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
        auto prevPortal = i == 0 ? nullptr : waypoints[i - 1];
        auto portal = waypoints[i];
        auto nextPortal = (i == waypoints.Num() - 1) ? nullptr : waypoints[i + 1];
        waypointCache.FindOrAdd(portal).Add({ absoluteX , absoluteY }, { prevPortal, nextPortal });
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
        auto nextPortal = (*cacheEntry)[key].toPortal;
        if (nextPortal != nullptr && nextPortal->tileCoordinates == start->tileCoordinates) {
            start = nextPortal;
            cacheEntry = waypointCache.Find(start);
        }
        
        for (int i = 0; i < 100000; i++) { // just a guard against faulty data
            result.waypoints.Add(start);
            start = (*cacheEntry)[key].toPortal;
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

std::function<void(TArray<uint8>&)> flow::FlowPath::createFlowmapDataProvider(FIntPoint startTile, FIntPoint delta) const
{
    return [this, startTile, delta](TArray<uint8>& data) {
        data.AddUninitialized(tileLength * tileLength * 4);

        for (int32 i = 0; i < 4; i++) {
            bool xFactor = xFactorArray[i];
            bool yFactor = yFactorArray[i];
            bool isRight = (delta.X == 1) == xFactor;
            bool isDown = (delta.Y == 1) == yFactor;
            int32 deltaX = delta.X * (xFactor ? 1 : 0);
            int32 deltaY = delta.Y * (yFactor ? 1 : 0);

            auto tile = tileMap.Find(startTile + FIntPoint(deltaX, deltaY));
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
                    result.success = true;
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

TArray<const Portal*> flow::FlowPath::getAllTilePortals(FIntPoint tileCoordinates) const
{
    TArray<const Portal*> result;

    auto tile = tileMap.Find(tileCoordinates);
    if (tile != nullptr) {
        for (auto& portal : (*tile)->getPortals()) {
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
            UE_LOG(LogExec, Warning, TEXT("End location tile != start tile location for simple path."));
            return -1;
        }
        // get flowmap to direct target location
        auto tile = tileMap.Find(vector.end.tileLocation);
        if (tile == nullptr) {
            UE_LOG(LogExec, Warning, TEXT("End location tile (%d, %d) not found."), vector.end.tileLocation.X, vector.end.tileLocation.Y);
            return -1;
        }
        // TODO add lookahead if target tile is diagonal start tile
        TArray<FIntPoint> targets = { vector.end.pointInTile };
        const auto& tileFlowMap = (*tile)->createMapToTarget(targets, true);
        int32 direction = tileFlowMap[cellIndex].directionLookupIndex;
        if (direction == -1) {
            UE_LOG(LogExec, Warning, TEXT("Unable to calculate flowmap value for agent"));
        }
        return direction;
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
            int32 direction = tileFlowMap[cellIndex].directionLookupIndex;
            if (direction == -1) {
                for (int32 i = 0; i < 10; i++) {
                    const EikonalCellValue* v = &tileFlowMap[i * 10];
                    UE_LOG(LogExec, Warning, TEXT("%f %f %f %f %f %f %f %f %f %f"), v[0].cellValue, v[1].cellValue, v[2].cellValue, v[3].cellValue, v[4].cellValue, v[5].cellValue, v[6].cellValue, v[7].cellValue, v[8].cellValue, v[9].cellValue);
                }
                UE_LOG(LogExec, Warning, TEXT("---------------------"));
                for (int32 i = 0; i < 10; i++) {
                    const EikonalCellValue* v = &tileFlowMap[i * 10];
                    UE_LOG(LogExec, Warning, TEXT("%d %d %d %d %d %d %d %d %d %d"), v[0].directionLookupIndex, v[1].directionLookupIndex, v[2].directionLookupIndex, v[3].directionLookupIndex, v[4].directionLookupIndex,
                        v[5].directionLookupIndex, v[6].directionLookupIndex, v[7].directionLookupIndex, v[8].directionLookupIndex, v[9].directionLookupIndex);
                }
                UE_LOG(LogExec, Warning, TEXT("Unable to calculate flowmap value for agent"));
            }
            return direction;
        }
        auto dataProvider = createFlowmapDataProvider(vector.start.tileLocation, delta);
        auto& tileFlowMap = (*tile)->createLookaheadFlowmap(nextPortal, lookaheadPortal, dataProvider);
        int32 direction = tileFlowMap[cellIndex].directionLookupIndex;
        if (direction == -1) {
            UE_LOG(LogExec, Warning, TEXT("Unable to calculate flowmap value for agent"));
        }
        return direction;
    }
}

bool flow::FlowPath::hasFlowMap(const Portal * startPortal, const Portal * targetPortal) const
{
    if (startPortal == nullptr || targetPortal == nullptr) {
        return false;
    }
    auto tile = tileMap.Find(startPortal->tileCoordinates);
    if (tile == nullptr) {
        return false;
    }
    return (*tile)->hasFlowMap(startPortal, targetPortal);
}

void flow::FlowPath::createFlowMapSourceData(FIntPoint startTile, FIntPoint delta, TArray<uint8>& result)
{
    auto dataProvider = createFlowmapDataProvider(startTile, delta);
    dataProvider(result);
}

void flow::FlowPath::cacheFlowMap(const Portal * resultStartPortal, const Portal * resultEndPortal, const TArray<flow::EikonalCellValue>& result)
{
    if (resultStartPortal == nullptr || resultEndPortal == nullptr) {
        return;
    }
    auto tile = tileMap.Find(resultStartPortal->tileCoordinates);
    if (tile == nullptr || (*tile)->hasFlowMap(resultStartPortal, resultEndPortal)) {
        return;
    }
    (*tile)->cacheFlowMap(resultStartPortal, resultEndPortal, result);
}

void flow::FlowPath::deleteFlowMapsFromTile(const FIntPoint & tileCoordinates)
{
    auto tile = tileMap.Find(tileCoordinates);
    if (tile == nullptr) {
        return;
    }
    return (*tile)->deleteAllFlowMaps();
}

int32 flow::FlowPath::getTileLength() const
{
    return tileLength;
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
