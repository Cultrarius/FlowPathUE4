// Created by Michael Galetzka - all rights reserved.

#include "FlowPathManager.h"
#include "DrawDebugHelpers.h"
#include "flow/EikonalSolver.h"

DECLARE_CYCLE_STAT(TEXT("FlowPath manager ~ tick"), STAT_ManagerTick, STATGROUP_FlowPath); 
DECLARE_CYCLE_STAT(TEXT("FlowPath manager ~ update data from texture"), STAT_ManagerUpdateFromTexture, STATGROUP_FlowPath);
DECLARE_CYCLE_STAT(TEXT("FlowPath manager ~ update dirty path data"), STAT_ManagerDirtyPaths, STATGROUP_FlowPath);

using namespace flow;

AFlowPathManager::AFlowPathManager()
{
    PrimaryActorTick.bCanEverTick = true;
    WorldToTileScale = FVector2D(0.01f, 0.01f);
    AcceptanceRadius = 10;
    tileLength = 10;
    VelocityBonus = 0.5;
    WaypointBonus = 0.5;
    LookaheadFlowmapGeneration = true;
    MergingPathSearch = true;
    GeneratorThreadPoolSize = 8;
    MaxAsyncFlowMapUpdatesPerTick = 50;
    CleanupFlowmapsAfterTicks = 1000;

#if WITH_EDITOR
    DrawAllBlockedCells = false;
    DrawAllPortals = false;
    DrawFlowMapAroundAgents = false;
    DrawAgentPortalWaypoints = false;
#endif		// WITH_EDITOR

    InitializeTiles();
}

#if WITH_EDITOR

FVector toV3(const FVector2D& vec) {
    return FVector(vec.X, vec.Y, 0);
}

void AFlowPathManager::DebugDrawAllBlockedCells()
{
    auto inverted = WorldToTileTransform.Inverse();
    for (auto& tileCoord : flowPath->getAllValidTileCoordinates()) {
        for (int y = 0; y < tileLength; y++) {
            int32 absoluteY = tileCoord.Y * tileLength + y;
            for (int x = 0; x < tileLength; x++) {
                int32 absoluteX = tileCoord.X * tileLength + x;
                int32 tileIndex = y * tileLength + x;

                if (flowPath->getDataFor({ tileCoord, FIntPoint(x, y) }) == BLOCKED) {
                    FVector2D tileStart(absoluteX, absoluteY);
                    FVector2D tileEnd = tileStart + FVector2D(0.95, 0.95);
                    DrawDebugLine(GetWorld(), toV3(inverted.TransformPoint(tileStart)), toV3(inverted.TransformPoint(tileEnd)), FColor::Red);
                    tileStart.Y += 0.95;
                    tileEnd.Y -= 0.95;
                    DrawDebugLine(GetWorld(), toV3(inverted.TransformPoint(tileStart)), toV3(inverted.TransformPoint(tileEnd)), FColor::Red);
                }
            }
        }
    }
}

FVector2D portalCoordToAbsolute(FIntPoint p, FlowTile* tile, int32 tileLength) {
    auto& tileCoord = tile->getCoordinates();
    int32 absoluteX = tileCoord.X * tileLength + p.X;
    int32 absoluteY = tileCoord.Y * tileLength + p.Y;
    return FVector2D(absoluteX, absoluteY);
}

void AFlowPathManager::DebugDrawAllPortals()
{
    auto inverted = WorldToTileTransform.Inverse();
    for (auto& portal : flowPath->getAllPortals()) {

        // draw the portals themselves in green
        FVector2D portalStart = portalCoordToAbsolute(portal->start, portal->parentTile, tileLength) + FVector2D(0.5, 0.5);
        FVector2D portalEnd = portalCoordToAbsolute(portal->end, portal->parentTile, tileLength) + FVector2D(0.5, 0.5);
        DrawDebugLine(GetWorld(), toV3(inverted.TransformPoint(portalStart)), toV3(inverted.TransformPoint(portalEnd)), FColor::Green);

        // draw the connected portals in blue
        FVector2D startCenter = portalCoordToAbsolute(portal->center, portal->parentTile, tileLength) + FVector2D(0.5, 0.5);
        for (auto& connected : portal->connected) {
            FVector2D endCenter = portalCoordToAbsolute(connected.Key->center, connected.Key->parentTile, tileLength) + FVector2D(0.5, 0.5);
            DrawDebugLine(GetWorld(), toV3(inverted.TransformPoint(startCenter)), toV3(inverted.TransformPoint(endCenter)), FColor::Blue);
        }
    }
}

void AFlowPathManager::DebugDrawFlowMaps()
{
    auto inverted = WorldToTileTransform.Inverse();
    for (auto& pair : flowPath->getAllFlowMaps()) {
        auto tileCoord = pair.Key;
        for (auto& flowMap : pair.Value) {
            for (int y = 0; y < tileLength; y++) {
                for (int x = 0; x < tileLength; x++) {
                    auto& val = flowMap[x + y * tileLength];
                    int32 absoluteX = tileCoord.X * tileLength + x;
                    int32 absoluteY = tileCoord.Y * tileLength + y;
                    FVector2D centerPoint(absoluteX + 0.5, absoluteY + 0.5);
                    auto tileCenter = toV3(inverted.TransformPoint(centerPoint));
                    int32 index = val.directionLookupIndex;
                    if (index == -1) {
                        continue;
                    }
                    auto neighborDirection = toV3(inverted.TransformPoint(normalizedNeighbors[index] / 2));
                    DrawDebugDirectionalArrow(GetWorld(), tileCenter, tileCenter + neighborDirection, 100, FColor::Purple, false, -1, 2, 4);
                }
            }
        }
    }
}

#endif		// WITH_EDITOR

void AFlowPathManager::precomputeFlowmaps(const AgentData& data)
{
    if (!Pool.IsValid()) {
        return;
    }

    TilePoint target = toTilePoint(data.current.targetLocation);
    for (int32 i = data.waypointIndex + 2; i < data.waypoints.Num(); i += 2) {
        generatorTasks.emplace_front(target, data.waypoints, i, LookaheadFlowmapGeneration, *flowPath, tileLock);
        Pool->AddQueuedWork(&generatorTasks.front());
    }
}

FlowMapGenerationTask::FlowMapGenerationTask(const TilePoint & target, TArray<const Portal*> waypoints, int32 workIndex, bool lookahead, FlowPath& flowPath, FCriticalSection& tileLock)
    : target(target), waypoints(waypoints), workIndex(workIndex), lookaheadAllowed(lookahead), flowPath(flowPath), tileLock(tileLock)
{
    if (workIndex + 1 >= waypoints.Num()) {
        Abandon();
        return;
    }
    workingTile = waypoints[workIndex]->tileCoordinates;
}

void FlowMapGenerationTask::Abandon()
{
    isAbandoned = true;
}

void FlowMapGenerationTask::DoThreadedWork()
{
    // copy the data while we hold the lock so we do not have to check during the calculation that any pointers or tiles are still valid
    TArray<uint8> sourceData;
    TArray<FIntPoint> targets;
    bool usesLookahead = false;

    {
        //TODO this should be a read-write lock for better performance
        FScopeLock lock(&tileLock);

        if (!isAbandoned) {
            auto nextPortal = waypoints[workIndex];
            auto connectedPortal = waypoints[workIndex + 1];
            auto lookaheadPortal = (lookaheadAllowed && waypoints.Num() > workIndex + 3) ? waypoints[workIndex + 3] : nullptr;
            if (lookaheadPortal != nullptr && waypoints.Num() > workIndex + 4 && waypoints[workIndex + 4]->tileCoordinates == lookaheadPortal->tileCoordinates) {
                lookaheadPortal = waypoints[workIndex + 4];
            }
            TilePoint location = { workingTile, nextPortal->center };
            auto delta = lookaheadPortal == nullptr ? FIntPoint::ZeroValue : lookaheadPortal->tileCoordinates - workingTile;
            usesLookahead = delta.SizeSquared() == 2;

            resultStartPortal = nextPortal;
            resultEndPortal = usesLookahead ? lookaheadPortal : connectedPortal;
            if (flowPath.hasFlowMap(nextPortal, resultEndPortal)) {
                Abandon();
            }
            else {
                nextPortal->parentTile->calculateFlowmapTargets(nextPortal, resultEndPortal, targets);
                if (usesLookahead) {
                    flowPath.createFlowMapSourceData(workingTile, delta, sourceData);
                }
                else {
                    sourceData = nextPortal->parentTile->getData();
                }
            }
        }
    }

    if (sourceData.Num() > 0 && targets.Num() > 0) {
        result = CreateEikonalSurface(sourceData, targets);

        if (result.Num() > 0) {
            int32 tileLength = FMath::Sqrt(result.Num()) / 2;
            if (usesLookahead) {
                // extract the important part from the 2x2 tile
                auto delta = resultEndPortal->tileCoordinates - workingTile;
                TArray<EikonalCellValue> extractedMap;
                extractedMap.AddUninitialized(tileLength * tileLength);
                for (int32 y = 0; y < tileLength; y++) {
                    for (int32 x = 0; x < tileLength; x++) {
                        int32 sourceIndex = toFourTileIndex(delta.X == -1, delta.Y == -1, x, y, tileLength);
                        int32 targetIndex = x + y * tileLength;
                        extractedMap[targetIndex] = result[sourceIndex];
                    }
                }
                result = extractedMap;
            }
            else {
                for (auto p : targets) {
                    // Change values for the portal window, so that an agent will pass to the next tile.
                    int32 index = p.X + p.Y * tileLength;
                    result[index].directionLookupIndex = toDirectionIndex(resultStartPortal->orientation);
                }
            }
        }
    }

    isDone = true;
}

void AFlowPathManager::processFlowMapGenerators()
{
    if (!Pool.IsValid()) {
        return;
    }
     
    int32 count = 0;
    for (auto it = generatorTasks.begin(); it != generatorTasks.end() && count < MaxAsyncFlowMapUpdatesPerTick;) {
        const auto& task = *it;
        if (task.isDone) {
            if (!task.isAbandoned && task.result.Num() > 0) {
                flowPath->cacheFlowMap(task.resultStartPortal, task.resultEndPortal, task.result);
                count++;
            }
            it = generatorTasks.erase(it);
        }
        else {
            it++;
        }
    }
}

void AFlowPathManager::updateDirtyPathData()
{
    SCOPE_CYCLE_COUNTER(STAT_ManagerDirtyPaths);

    FlowMapExtract flowMap;
    for (auto& agentPair : agents) {
        AgentData& data = agentPair.Value;
        if (!data.current.isPathfindingActive || !data.isPathDataDirty) {
            continue;
        }
        auto& location = data.currentLocation;
        auto& target = data.currentTarget;

        // check and update the portal waypoint data
        bool isWaypointDataDirty = target != data.lastTarget;
        if (!isWaypointDataDirty && data.waypoints.Num() > 0 && data.waypoints.Num() > data.waypointIndex) {
            if (data.waypoints[data.waypointIndex + 1]->tileCoordinates == location.tileLocation) {
                data.waypointIndex += 2;
            } else if (data.waypoints[data.waypointIndex]->tileCoordinates != location.tileLocation) {
                isWaypointDataDirty = true;
            }
        }
        if (isWaypointDataDirty || (data.waypoints.Num() == 0 && location.tileLocation != target.tileLocation)) {
            auto portalSearchResult = flowPath->findPortalPath(location, target, MergingPathSearch);
            if (!portalSearchResult.success) {
                data.current.isPathfindingActive = false;
                data.targetAcceleration = FVector2D::ZeroVector;
                data.isPathDataDirty = false;
                data.waypoints.Empty();
                INavAgent::Execute_TargetUnreachable(agentPair.Key);
                continue;
            }
            data.waypoints = portalSearchResult.waypoints;
            data.waypointIndex = 0;
            precomputeFlowmaps(data);
        }

        bool followingPortals = data.waypointIndex < data.waypoints.Num();
        auto nextPortal = followingPortals ? data.waypoints[data.waypointIndex] : nullptr;
        auto connectedPortal = followingPortals ? data.waypoints[data.waypointIndex + 1] : nullptr;
        auto lookaheadPortal = (LookaheadFlowmapGeneration && followingPortals && data.waypoints.Num() > data.waypointIndex + 3) ? data.waypoints[data.waypointIndex + 3] : nullptr;
        if (lookaheadPortal != nullptr && data.waypoints.Num() > data.waypointIndex + 4 && data.waypoints[data.waypointIndex + 4]->tileCoordinates == lookaheadPortal->tileCoordinates) {
            // if possible, jump ahead even more
            lookaheadPortal = data.waypoints[data.waypointIndex + 4];
        }

        int32 lookupIndex = flowPath->fastFlowMapLookup({ location, target }, nextPortal, connectedPortal, lookaheadPortal);
        if (lookupIndex >= 0) {
            data.targetAcceleration = normalizedNeighbors[lookupIndex];
        } else if (flowPath->getFlowMapValue({ location, target }, nextPortal, connectedPortal, flowMap)) {
            float bestTarget = MAX_VAL;
            FVector2D targetDirection = FVector2D::ZeroVector;
            FVector2D agentVelocity = (data.current.agentLocation - data.lastTick.agentLocation).GetSafeNormal();
            TilePoint expectedWaypoint;
            if (!findClosestWaypoint(data, location, expectedWaypoint)) {
                expectedWaypoint = target;
            }
            FVector2D waypointDirection = (toAbsoluteTileLocation(expectedWaypoint) - toAbsoluteTileLocation(location)).GetSafeNormal();
            float min = MAX_VAL;
            float max = 0;
            TArray<int32> candidates;
            //UE_LOG(LogExec, Error, TEXT("-------"));
            for (int32 i = 0; i < 8; i++) {
                float cellValue = flowMap.neighborCells[i];
                if (cellValue == MAX_VAL) {
                    continue;
                }
                if (VelocityBonus != 0 && agentVelocity != FVector2D::ZeroVector) {
                    cellValue -= calcVelocityBonus(agentVelocity, i);
                }
                if (WaypointBonus != 0) {
                    cellValue -= calcWaypointBonus(waypointDirection, i);
                }
                if (cellValue < bestTarget) {
                    bestTarget = cellValue;
                    candidates.Empty(1);
                    candidates.Add(i);
                }
                else if (bestTarget != MAX_VAL && cellValue == bestTarget) {
                    candidates.Add(i);
                }
                min = FMath::Min(min, cellValue);
                max = cellValue == MAX_VAL ? max : FMath::Max(max, cellValue);
            }

            if (candidates.Num() == 1) {
                targetDirection = normalizedNeighbors[candidates[0]];
            }
            else if (candidates.Num() > 1) {
                // we have multiple identical cell values to choose from, pick the one that is best without the applied bonuses
                bestTarget = MAX_VAL;
                for (int32 i = 0; i < candidates.Num(); i++) {
                    int32 index = candidates[i];
                    float cellValue = flowMap.neighborCells[index];
                    if (cellValue < bestTarget) {
                        bestTarget = cellValue;
                        targetDirection = normalizedNeighbors[index];
                    }
                }
            }

#if WITH_EDITOR
            if (DrawFlowMapAroundAgents) {
                auto inverted = WorldToTileTransform.Inverse();
                int32 absoluteX = location.tileLocation.X * tileLength + location.pointInTile.X;
                int32 absoluteY = location.tileLocation.Y * tileLength + location.pointInTile.Y;
                FVector2D centerPoint(absoluteX + 0.5, absoluteY + 0.5);
                auto tileCenter = toV3(inverted.TransformPoint(centerPoint));
                for (int32 i = 0; i < 8; i++) {
                    float cellValue = flowMap.neighborCells[i];
                    auto neighborDirection = toV3(inverted.TransformPoint(normalizedNeighbors[i] / 2));
                    float alpha = FMath::Clamp((cellValue - min) / (max - min + 1), 0.0f, 1.0f);
                    FColor color = cellValue == MAX_VAL ? FColor::Purple : FMath::Lerp(FLinearColor::Green, FLinearColor::Red, alpha).ToFColor(false);
                    DrawDebugDirectionalArrow(GetWorld(), tileCenter, tileCenter + neighborDirection, 100, color, true, -1, 2, 4);
                }
            }
#endif		// WITH_EDITOR

            data.targetAcceleration = FVector2D(targetDirection.X, targetDirection.Y);
        }
        else {
            data.current.isPathfindingActive = false;
            data.targetAcceleration = FVector2D::ZeroVector;
        }

        data.isPathDataDirty = false;
    }
}

float AFlowPathManager::calcVelocityBonus(const FVector2D& velocityDirection, int32 i) const
{
    float distance = (velocityDirection - normalizedNeighbors[i]).Size();
    float bonus = (-distance + 1) * VelocityBonus;
    //UE_LOG(LogExec, Error, TEXT("Velocity %f, %f / Vector %f, %f / Bonus %f / Distance %f"), velocityDirection.X, velocityDirection.Y, normalizedNeighbors[i].X, normalizedNeighbors[i].Y, bonus, distance);
    return bonus;
}

float AFlowPathManager::calcWaypointBonus(const FVector2D& waypointDirection, int32 i) const
{
    float distance = (waypointDirection - normalizedNeighbors[i]).Size();
    float bonus = (-distance + 1) * WaypointBonus;
    //UE_LOG(LogExec, Error, TEXT("Waypoint %f, %f / Vector %f, %f / Bonus %f / Distance %f"), waypointDirection.X, waypointDirection.Y, normalizedNeighbors[i].X, normalizedNeighbors[i].Y, bonus, distance);
    return bonus;
}

FVector2D AFlowPathManager::toAbsoluteTileLocation(flow::TilePoint p) const
{
    int32 absoluteX = p.tileLocation.X * tileLength + p.pointInTile.X;
    int32 absoluteY = p.tileLocation.Y * tileLength + p.pointInTile.Y;
    return FVector2D(absoluteX, absoluteY);
}

FVector2D AFlowPathManager::toTile(FVector2D worldPosition) const
{
    return FVector2D(WorldToTileTransform.TransformPoint(worldPosition) / tileLength);
}

TilePoint AFlowPathManager::toTilePoint(FVector2D worldPosition) const
{
    FVector2D tilePos = toTile(worldPosition);
    return absoluteTilePosToTilePoint(tilePos);
}

TilePoint AFlowPathManager::absoluteTilePosToTilePoint(FVector2D tilePos) const
{
    int32 tileX = FMath::FloorToInt(tilePos.X);
    int32 tileY = FMath::FloorToInt(tilePos.Y);
    int32 x = FMath::Abs(tilePos.X - tileX) * tileLength;
    int32 y = FMath::Abs(tilePos.Y - tileY) * tileLength;
    return { FIntPoint(tileX, tileY), FIntPoint(x, y) };
}

bool AFlowPathManager::findClosestWaypoint(const AgentData& data, const flow::TilePoint& agentLocation, TilePoint& result) const
{
    int32 index = data.waypointIndex;
    int32 count = data.waypoints.Num();
    if (count > index) {
        auto portal = data.waypoints[index];
        result.tileLocation = portal->tileCoordinates;
        if (result.tileLocation == agentLocation.tileLocation) {
            result.pointInTile = portal->start;
            int32 bestDistance = (result.pointInTile - agentLocation.pointInTile).SizeSquared();
            for (int x = portal->start.X; x <= portal->end.X; x++) {
                for (int y = portal->start.Y; y <= portal->end.Y; y++) {
                    FIntPoint p(x, y);
                    int32 distance = (p - agentLocation.pointInTile).SizeSquared();
                    if (distance < bestDistance) {
                        result.pointInTile = p;
                        bestDistance = distance;
                    }
                }
            }
        }
        else {
            result.pointInTile = portal->center;
        }
        return true;
    }
    return false;
}

void AFlowPathManager::Tick(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_ManagerTick);

    Super::Tick(DeltaTime);

    processFlowMapGenerators();

#if WITH_EDITOR
    if (DrawAllBlockedCells) {
        DebugDrawAllBlockedCells();
    }
    if (DrawAllPortals) {
        DebugDrawAllPortals();
    }
    if (DrawFlowMaps) {
        DebugDrawFlowMaps();
    }
#endif		// WITH_EDITOR

    TArray<UObject*> agentsToRemove;
    for (auto& agentPair : agents) {
        UObject* agent = agentPair.Key;
        AgentData& data = agentPair.Value;

        if (!agent->IsValidLowLevelFast() || agent->IsPendingKill()) {
            agentsToRemove.Add(agent);
            continue;
        }

#if WITH_EDITOR
        if (DrawAgentPortalWaypoints) {
            auto inverted = WorldToTileTransform.Inverse();
            if (data.waypoints.Num() > data.waypointIndex) {
                auto& first = data.waypoints[data.waypointIndex];
                auto& last = data.waypoints.Last();
                FVector2D firstWaypoint = portalCoordToAbsolute(first->center, first->parentTile, tileLength) + FVector2D(0.5, 0.5);
                FVector2D lastWaypoint = portalCoordToAbsolute(last->center, last->parentTile, tileLength) + FVector2D(0.5, 0.5);
                DrawDebugDirectionalArrow(GetWorld(), toV3(data.current.agentLocation), toV3(inverted.TransformPoint(firstWaypoint)), 100, FColor::Red, false, -1, 2, 4);
                DrawDebugDirectionalArrow(GetWorld(), toV3(inverted.TransformPoint(lastWaypoint)), toV3(data.current.targetLocation), 100, FColor::Red, false, -1, 2, 4);
            }
            for (int32 i = data.waypointIndex + 1; i < data.waypoints.Num(); i++) {
                auto& previous = data.waypoints[i - 1];
                auto& portal = data.waypoints[i];

                // draw the portal connection in red
                FVector2D startCenter = portalCoordToAbsolute(previous->center, previous->parentTile, tileLength) + FVector2D(0.5, 0.5);
                FVector2D endCenter = portalCoordToAbsolute(portal->center, portal->parentTile, tileLength) + FVector2D(0.5, 0.5);
                DrawDebugDirectionalArrow(GetWorld(), toV3(inverted.TransformPoint(startCenter)), toV3(inverted.TransformPoint(endCenter)), 100, FColor::Red, false, -1, 2, 4);
            }
        }
#endif		// WITH_EDITOR

        data.lastTick = data.current;
        data.lastLocation = data.currentLocation;
        data.lastTarget = data.currentTarget;
        data.current = INavAgent::Execute_GetAgentInfo(agent);
        data.currentLocation = toTilePoint(data.current.agentLocation);
        data.currentTarget = toTilePoint(data.current.targetLocation);

        bool isActive = data.current.isPathfindingActive;
        if (isActive) {
            if ((data.current.targetLocation - data.current.agentLocation).SizeSquared() <= (AcceptanceRadius * AcceptanceRadius)) {
                // agent has reached the goal
                data.current.isPathfindingActive = false;
                data.isPathDataDirty = false;
                data.targetAcceleration = FVector2D::ZeroVector;
                data.waypoints.Empty();
                INavAgent::Execute_TargetReached(agent);
            }
            else if (!data.lastTick.isPathfindingActive) {
                // agent just started the pathfinding
                data.isPathDataDirty = true;
                data.targetAcceleration = FVector2D::ZeroVector;
                data.waypoints.Empty();
            }
            else if (!data.isPathDataDirty) {
                INavAgent::Execute_UpdateAcceleration(agent, data.targetAcceleration);
                data.isPathDataDirty = data.currentLocation != data.lastLocation || data.currentTarget != data.lastTarget;
            }
        }
    }

    for (auto agent : agentsToRemove) {
        agents.Remove(agent);
    }

    updateDirtyPathData();
    cleanupOldFlowmaps();
    
}

void AFlowPathManager::cleanupOldFlowmaps()
{
    if (CleanupFlowmapsAfterTicks < 0) {
        return;
    }
    ticksSinceLastCleanup++;
    if (ticksSinceLastCleanup < CleanupFlowmapsAfterTicks) {
        return;
    }
    //gather currently used tiles from the agent data
    TSet<FIntPoint> usedTiles;
    for (auto& agentPair : agents) {
        AgentData& data = agentPair.Value;
        usedTiles.Add(data.currentLocation.tileLocation);
        usedTiles.Add(data.currentTarget.tileLocation);
        for (auto& waypoint : data.waypoints) {
            usedTiles.Add(waypoint->tileCoordinates);
        }
    }

    // clear flowmap caches from unused tiles
    TArray<FIntPoint> flowMapTiles;
    flowPath->getAllFlowMaps().GetKeys(flowMapTiles);
    for (auto& tile : flowMapTiles) {
        if (!usedTiles.Contains(tile)) {
            flowPath->deleteFlowMapsFromTile(tile);
        }
    }
    ticksSinceLastCleanup = 0;
}

void AFlowPathManager::InitializeTiles()
{
    if (GeneratorThreadPoolSize > 0) {
        Pool.Reset(FQueuedThreadPool::Allocate());
        if (!Pool->Create(GeneratorThreadPoolSize, 32 * 1024, TPri_BelowNormal)) {
            Pool.Reset(nullptr);
        }
    }
    else {
        Pool.Reset(nullptr);
    }
    generatorTasks.clear();

    FMatrix2x2 scaleMatrix(WorldToTileScale.X, 0, 0, WorldToTileScale.Y);
    WorldToTileTransform = FTransform2D(scaleMatrix, WorldToTileTranslation);
    flowPath = MakeUnique<FlowPath>(tileLength);
}

bool AFlowPathManager::UpdateMapTileWorld(FVector2D worldPosition, const TArray<uint8>& tileData)
{
    FVector2D tilePos = toTile(worldPosition);
    return UpdateMapTileLocal(FMath::FloorToInt(tilePos.X), FMath::FloorToInt(tilePos.Y), tileData);
}

bool AFlowPathManager::UpdateMapTileLocal(int32 tileX, int32 tileY, const TArray<uint8>& tileData)
{
    FScopeLock lock(&tileLock);

    FIntPoint tileCoord(tileX, tileY);
    auto originalTilePortals = flowPath->getAllTilePortals(tileCoord);
    bool success = flowPath->updateMapTile(tileX, tileY, tileData);

    if (success) {
        // remove invalidated waypoint data
        for (auto& data : agents) {
            TSet<const flow::Portal*> waypointPortals;
            waypointPortals.Append(data.Value.waypoints);
            for (auto& oldPortal : originalTilePortals) {
                if (waypointPortals.Contains(oldPortal)) {
                    data.Value.waypoints.Empty();
                    data.Value.isPathDataDirty = true;
                    break;
                }
            }
        }

        if (Pool.IsValid()) {
            // stop async flowmap tasks working on that tile
            for (auto& task : generatorTasks) {
                if (task.workingTile == tileCoord) {
                    task.Abandon();
                }
            }
        }
    }
    return success;
}

bool AFlowPathManager::UpdateMapTilesFromTexture(int32 tileXUpperLeft, int32 tileYUpperLeft, UTexture2D* texture)
{
    SCOPE_CYCLE_COUNTER(STAT_ManagerUpdateFromTexture);

    if (texture == nullptr || texture->PlatformData == nullptr) {
        return false;
    }
    auto data = &texture->PlatformData->Mips[0];
    int32 sizeX = data->SizeX;
    int32 sizeY = data->SizeY;
    auto bulkData = &data->BulkData;
    auto imageData = static_cast<const FColor*>(bulkData->LockReadOnly());
    if (imageData == nullptr) {
        UE_LOG(LogExec, Error, TEXT("No image data for flow path update! Be sure to disable mipmaps and compression for the texture."));
        bulkData->Unlock();
        return false;
    }

    int32 tilesX = FMath::CeilToInt(sizeX * 1.0 / tileLength);
    int32 tilesY = FMath::CeilToInt(sizeY * 1.0 / tileLength);

    TArray<uint8> tileData;
    tileData.AddUninitialized(tileLength * tileLength);
    for (int tileY = 0; tileY < tilesY; tileY++) {
        for (int tileX = 0; tileX < tilesX; tileX++) {
            for (int y = 0; y < tileLength; y++) {
                int32 textureY = tileY * tileLength + y;
                for (int x = 0; x < tileLength; x++) {
                    int32 textureX = tileX * tileLength + x;
                    int32 tileIndex = y * tileLength + x;
                    if (textureY >= sizeY || textureX >= sizeX) {
                        tileData[tileIndex] = BLOCKED;
                    }
                    else {
                        int32 textureIndex = textureY * sizeX + textureX;
                        const FColor& colorVal = imageData[textureIndex];
                        uint32 combined = colorVal.R | colorVal.G | colorVal.B;
                        tileData[tileIndex] = combined == 0 ? BLOCKED : combined;
                    }
                }
            }
            UpdateMapTileLocal(tileX + tileXUpperLeft, tileY + tileYUpperLeft, tileData);
        }
    }

    bulkData->Unlock();
    return true;
}

uint8 AFlowPathManager::GetTileDataForWorldPosition(FVector2D worldPosition)
{
    FVector2D tilePos = toTile(worldPosition);
    int32 tileX = FMath::FloorToInt(tilePos.X);
    int32 tileY = FMath::FloorToInt(tilePos.Y);
    int32 x = FMath::Abs(tilePos.X - tileX) * tileLength;
    int32 y = FMath::Abs(tilePos.Y - tileY) * tileLength;
    TilePoint p = { FIntPoint(tileX, tileY), FIntPoint(x, y) };

    //UE_LOG(LogExec, Warning, TEXT("Tile %d, %d: Pos %d, %d"), tileX, tileY, x, y);

    return flowPath->getDataFor(p);
}

void AFlowPathManager::RegisterAgent(UObject* agent)
{
    check(agent);
    check(agent->IsValidLowLevel());
    if (!agent->GetClass()->ImplementsInterface(UNavAgent::StaticClass())) {
        UE_LOG(LogExec, Error, TEXT("Agents registered with flow path manager must implement the INavAgent interface. (%s)"), *agent->GetFName().ToString());
        return;
    }
    agents.Add(agent);
}

void AFlowPathManager::RemoveAgent(UObject * agent)
{
    check(agent);
    agents.Remove(agent);
}

bool AFlowPathManager::IsPathPossible(FVector2D worldPositionStart, FVector2D worldPositionEnd) const
{
    TilePoint start = toTilePoint(worldPositionStart);
    TilePoint end = toTilePoint(worldPositionEnd);
    return flowPath->findPortalPath({ start, end }, true).success;
}

