//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
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

void FlowPath::addAgent(TUniquePtr<Agent> agent) {
    agents.Emplace(agent.Get(), move(agent));
}

void FlowPath::killAgent(Agent *agent) {
    agents.Remove(agent);
}

void FlowPath::updateAgents() {

}

uint8 flow::FlowPath::getDataFor(const TilePoint & p) const
{
    auto tile = tileMap.Find(p.tileLocation);
    if (tile == nullptr || !isValidTileLocation(p.pointInTile)) {
        return BLOCKED;
    }
    
    int32 index = (*tile)->toIndex(p.pointInTile);
    return (*tile)->getData()[index];
}

PathSearchResult flow::FlowPath::findDirectPath(FIntPoint start, FIntPoint end)
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

PortalSearchResult flow::FlowPath::findPortalPath(const TilePoint& start, const TilePoint& end) const
{
    TArray<const Portal*> waypoints;
    PortalSearchResult result = {false, waypoints, 0};
    auto startTile = tileMap.Find(start.tileLocation);
    auto endTile = tileMap.Find(end.tileLocation);
    FIntPoint startPoint = start.pointInTile;
    FIntPoint endPoint = end.pointInTile;

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
            result.pathCost = directPath.pathCost;
            return result;
        }
    }

    // we construct two special portals so they can be inserted into the search queue nodes
    TArray<PortalSearchNode> searchQueue;
    Portal startPortal(startPoint, startPoint, Orientation::TOP, startTile->Get());
    Portal endPortal(endPoint, endPoint, Orientation::TOP, endTile->Get());
    PortalSearchNode startNode = { -1, -1, &startPortal, &startPortal, false };
    TSet<const Portal*> queuedPortals;

    // start by inserting the portals in the start tile into the queue
    for (auto& portal : (*startTile)->getPortals()) {
        PathSearchResult searchResult = (*startTile)->findPath(startPoint, portal.center);
        if (searchResult.success) {
            TilePoint portalPoint = {start.tileLocation, portal.center};
            int32 goalCost = searchResult.pathCost + calcGoalHeuristic(portalPoint, end);
            UE_LOG(LogExec, Warning, TEXT("  Path to %d, %d costs %d (%d to goal)"), portal.center.X, portal.center.Y, searchResult.pathCost, goalCost);
            PortalSearchNode newNode = {searchResult.pathCost, goalCost, &portal, &startPortal, true};
            searchQueue.HeapPush(newNode);
            queuedPortals.Add(&portal);
        }
    }

    int32 hops = 0;
    TMap<const Portal*, PortalSearchNode> searchedNodes;
    searchedNodes.Add(&startPortal, startNode);
    PortalSearchNode frontier;
    while (searchQueue.Num() > 0) {
        searchQueue.HeapPop(frontier, false);
        frontier.open = false;
        if (searchedNodes.Contains(frontier.nodePortal)) {
            // we have already searched this portal node
            continue;
        }
        hops++;
        searchedNodes.Add(frontier.nodePortal, frontier);
        auto nP = frontier.nodePortal;
        UE_LOG(LogExec, Warning, TEXT("Frontier %d, %d on tile %d, %d, goalCost %d"), nP->center.X, nP->center.Y, nP->parentTile->getCoordinates().X, nP->parentTile->getCoordinates().Y, frontier.goalCost);
        
        // check to see if we have reached the goal
        if (frontier.nodePortal == &endPortal) {
            result.success = true;
            result.pathCost = frontier.nodeCost;

            // create waypoints from the portals jumped from start to end
            frontier = searchedNodes[frontier.parentPortal];
            while (frontier.nodePortal != &startPortal) {
                waypoints.Add(frontier.nodePortal);
                frontier = searchedNodes[frontier.parentPortal];
            }
            result.waypoints = waypoints;
            UE_LOG(LogExec, Warning, TEXT("  -> %d hops"), hops);
            break;
        }

        // if we are on the goal tile we try to reach the target point from the portal
        auto frontierPortal = frontier.nodePortal;
        if (frontier.nodePortal->parentTile->getCoordinates() == end.tileLocation) {
            PathSearchResult searchResult = (*endTile)->findPath(frontierPortal->center, end.pointInTile);
            if (searchResult.success) {
                int32 nodeCost = frontier.nodeCost + searchResult.pathCost;
                PortalSearchNode endNode = { nodeCost, nodeCost, &endPortal, frontierPortal, true };
                searchQueue.HeapPush(endNode);
                queuedPortals.Add(&endPortal);
            }
        }

        // check connected portals to reach the goal tile
        for (auto& connected : frontier.nodePortal->connected) {
            Portal* target = connected.Key;
            int32 nodeCost = frontier.nodeCost + connected.Value;
            TilePoint portalPoint = { target->parentTile->getCoordinates(), target->center };
            int32 goalCost = nodeCost + calcGoalHeuristic(portalPoint, end);
            PortalSearchNode newNode = { nodeCost, goalCost, target, frontierPortal, true };
            searchQueue.HeapPush(newNode);
            queuedPortals.Add(target);
        }
    }

    return result;
}

TArray<FIntPoint> flow::FlowPath::getAllValidTileCoordinates() const
{
    TArray<FIntPoint> result;
    for (auto& pair : tileMap) {
        result.Add(pair.Key);
    }
    return result;
}

TArray<const Portal*> flow::FlowPath::getAllPortals() const
{
    TArray<const Portal*> result;

    for (auto& tilePair : tileMap) {
        for (auto& portal : tilePair.Value->getPortals()) {
            result.Add(&portal);
        }
    }

    return result;
}

bool flow::FlowPath::isValidTileLocation(const FIntPoint & p) const
{
    return p.X >= 0 && p.Y >= 0 && p.X < tileLength && p.Y < tileLength;
}

int32 flow::FlowPath::calcGoalHeuristic(const TilePoint& start, const TilePoint& end) const
{
    // calculate the absolute distance between start and end across the tiles
    int32 startX = start.tileLocation.X * tileLength + start.pointInTile.X;
    int32 startY = start.tileLocation.Y * tileLength + start.pointInTile.Y;
    int32 endX = end.tileLocation.X * tileLength + end.pointInTile.X;
    int32 endY = end.tileLocation.Y * tileLength + end.pointInTile.Y;
    return FIntPoint(endX - startX, endY - startY).Size();
}
