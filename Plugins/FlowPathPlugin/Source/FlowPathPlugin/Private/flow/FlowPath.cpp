//
// Created by michael on 17.12.17.
//

#include "FlowPath.h"
#include <iostream>

using namespace std;
using namespace flow;

FlowPath::FlowPath(int32 tileLength, bool allowCrossGridMovement) : tileLength(tileLength), allowCrossGridMovement(allowCrossGridMovement) {
    int32 size = tileLength * tileLength;
    emptyTileData.AddUninitialized(size);
    fullTileData.AddUninitialized(size);
    for (int32 i = 0; i < size; i++) {
        emptyTileData[i] = EMPTY;
        fullTileData[i] = BLOCKED;
    }
}

void FlowPath::updateMapTile(int32 tileX, int32 tileY, const TArray<BYTE> &tileData) {
    if (tileData.Num() != tileLength * tileLength) {
        throw runtime_error("Invalid tile size");
    }
    bool isEmpty = true;
    bool isBlocked = true;
    for (int32 i = 0; i < tileLength * tileLength; i++) {
        if (tileData[i] == 0) {
            throw runtime_error("Invalid tile value: 0");
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
}

void FlowPath::addAgent(TUniquePtr<Agent> agent) {
    agents.Emplace(agent.Get(), move(agent));
}

void FlowPath::killAgent(Agent *agent) {
    agents.Remove(agent);
}

void FlowPath::updateAgents() {

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
        tile->connectOverlappingPortals(*leftTile, LEFT);
    }
    if (rightTile != nullptr) {
        tile->connectOverlappingPortals(*rightTile, RIGHT);
    }
    if (topTile != nullptr) {
        tile->connectOverlappingPortals(*topTile, TOP);
    }
    if (bottomTile != nullptr) {
        tile->connectOverlappingPortals(*bottomTile, BOTTOM);
    }
}

FlowTile *FlowPath::getTile(FIntPoint tileCoordinates) {
    auto tile = tileMap.Find(tileCoordinates);
    return tile == nullptr ? nullptr : tile->Get();
}

void FlowPath::printPortals() const {
    for (auto& tilePair : tileMap) {
        UE_LOG(LogExec, Warning, TEXT("Tile %d, %d:"), tilePair.Key.X, tilePair.Key.Y);
        for (auto& portal : tilePair.Value->getPortals()) {
            for (auto& connected : portal.connected) {
                if (connected.Key->parentTile != portal.parentTile) {
                    UE_LOG(LogExec, Warning, TEXT("  From (%d, %d -> %d, %d) To (%d, %d -> %d, %d)"), portal.start.X, portal.start.Y, portal.end.X, portal.end.Y,
                        connected.Key->start.X, connected.Key->start.Y, connected.Key->end.X, connected.Key->end.Y);
                }
            }
        }
    }
}

PortalSearchResult flow::FlowPath::findPortalPath(TilePoint start, TilePoint end)
{
    TArray<const Portal*> waypoints;
    PortalSearchResult result = {false, waypoints, 0};
    auto startTile = tileMap.Find(start.tileLocation);
    auto endTile = tileMap.Find(end.tileLocation);
    FIntPoint startPoint = start.pointInTile;
    FIntPoint endPoint = end.pointInTile;

    if (startTile == nullptr || endTile == nullptr || !isValidTileLocation(startPoint) || !isValidTileLocation(endPoint)) {
        return result;
    }

    // check if start and end are on the same tile
    if (start.tileLocation == end.tileLocation) {
        PathSearchResult directPath = (*startTile)->findPath(start.pointInTile, end.pointInTile);
        if (directPath.success) {
            result.success = true;
            result.pathCost = directPath.pathCost;
            return result;
        }
    }

    // we construct two special portals so they can be inserted into the queue
    TArray<PortalSearchNode> searchQueue;
    Portal startPortal(startPoint, startPoint, TOP, startTile->Get());
    Portal endPortal(endPoint, endPoint, TOP, endTile->Get());
    PortalSearchNode startNode = { -1, -1, &startPortal, &startPortal, false };
    TSet<const Portal*> queuedPortals;

    // start by inserting the portals in the start tile into the queue
    for (auto& portal : (*startTile)->getPortals()) {
        PathSearchResult searchResult = (*startTile)->findPath(startPoint, portal.center);
        if (searchResult.success) {
            UE_LOG(LogExec, Warning, TEXT("  Path to %d, %d costs %d"), portal.center.X, portal.center.Y, searchResult.pathCost);
            int32 goalCost = searchResult.pathCost + calcGoalHeuristic(portal.orientation, **startTile, **endTile);
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
        searchedNodes.Add(frontier.nodePortal, frontier);
        auto nP = frontier.nodePortal;
        UE_LOG(LogExec, Warning, TEXT("Frontier %d, %d on tile %d, %d, goalCost %d"), nP->center.X, nP->center.Y, nP->parentTile->getCoordinates().X, nP->parentTile->getCoordinates().Y, frontier.goalCost);
        
        // check to see if we have reached the goal
        if (frontier.nodePortal == &endPortal) {
            result.success = true;
            result.pathCost = frontier.nodeCost;

            // create waypoints, don't the start and end since they are not real portals
            frontier = searchedNodes[frontier.parentPortal];
            while (frontier.nodePortal != &startPortal) {
                waypoints.Add(frontier.nodePortal);
                frontier = searchedNodes[frontier.parentPortal];
            }
            result.waypoints = waypoints;
        }

        // if we are on the goal tile we try to reach the target point from the portal
        auto frontierPortal = frontier.nodePortal;
        if (frontier.nodePortal->parentTile->getCoordinates() == end.tileLocation) {
            PathSearchResult searchResult = (*endTile)->findPath(frontierPortal->center, end.pointInTile);
            if (searchResult.success) {
                int32 nodeCost = frontier.nodeCost + searchResult.pathCost;
                if (queuedPortals.Contains(&endPortal)) {
                    for (auto& node : searchQueue) {
                        if (node.nodePortal == &endPortal && node.nodeCost > nodeCost) {
                            node.nodeCost = nodeCost;
                            node.goalCost = nodeCost;
                            node.parentPortal = frontierPortal;
                        }
                    }
                }
                else {
                    PortalSearchNode endNode = { nodeCost, nodeCost, &endPortal, frontierPortal, true };
                    searchQueue.HeapPush(endNode);
                    queuedPortals.Add(&endPortal);
                }
            }
        }

        // check connected portals to reach the goal tile
        for (auto& connected : frontier.nodePortal->connected) {
            Portal* target = connected.Key;
            int32 nodeCost = frontier.nodeCost + connected.Value;
            int32 goalCost = nodeCost + calcGoalHeuristic(target->orientation, *target->parentTile, **endTile);
            if (queuedPortals.Contains(target)) {
                for (auto& node : searchQueue) {
                    if (node.open && node.nodePortal == target && node.goalCost > goalCost) {
                        node.nodeCost = nodeCost;
                        node.goalCost = goalCost;
                        node.parentPortal = frontierPortal;
                    }
                }
            }
            else {
                PortalSearchNode newNode = { nodeCost, goalCost, target, frontierPortal, true };
                searchQueue.HeapPush(newNode);
                queuedPortals.Add(target);
            }
        }
    }

    return result;
}

bool flow::FlowPath::isValidTileLocation(const FIntPoint & p) const
{
    return p.X >= 0 && p.Y >= 0 && p.X < tileLength && p.Y < tileLength;
}

int32 flow::FlowPath::calcGoalHeuristic(Orientation startOrientation, const FlowTile & fromTile, const FlowTile & toTile) const
{
    FIntPoint delta = toTile.getCoordinates() - fromTile.getCoordinates();
    int32 jumpsX = FMath::Abs(delta.X);
    int32 jumpsY = FMath::Abs(delta.Y);

    if (delta.X > 0 && startOrientation == LEFT) {
        jumpsX++;
    }
    else if (delta.X > 0 && startOrientation == RIGHT) {
        jumpsX--;
    }
    else if (delta.X < 0 && startOrientation == RIGHT) {
        jumpsX++;
    }
    else if (delta.X < 0 && startOrientation == LEFT) {
        jumpsX--;
    }
    
    if (delta.Y > 0 && startOrientation == BOTTOM) {
        jumpsY++;
    }
    else if (delta.Y > 0 && startOrientation == TOP) {
        jumpsY--;
    }
    else if (delta.Y < 0 && startOrientation == TOP) {
        jumpsY++;
    }
    else if (delta.Y < 0 && startOrientation == BOTTOM) {
        jumpsY--;
    }

    int32 result = FIntPoint(jumpsX * tileLength, jumpsY * tileLength).Size();
    const char* orientStr = startOrientation == TOP ? "top" : (startOrientation == BOTTOM ? "bottom" : (startOrientation == RIGHT ? "right" : "left"));
    UE_LOG(LogExec, Warning, TEXT("Heuristic for %d, %d (%s) -> %d, %d: %d, %d = %d"), 
        fromTile.getCoordinates().X, fromTile.getCoordinates().Y, ANSI_TO_TCHAR(orientStr),
        toTile.getCoordinates().X, toTile.getCoordinates().Y, jumpsX, jumpsY, result);
    
    return result;
}
