// Created by Michael Galetzka - all rights reserved.

#include "FlowPathManager.h"
#include "DrawDebugHelpers.h"
#include "flow/WaveMarcher.h"

using namespace flow;

AFlowPathManager::AFlowPathManager()
{
    PrimaryActorTick.bCanEverTick = true;
    WorldToTileScale = FVector2D(0.1f, 0.1f);
    AcceptanceRadius = 10;
    tileLength = 10;
    DrawFlowMapAroundAgents = true;
    DrawAgentPortalWaypoints = true;

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

#endif		// WITH_EDITOR

void AFlowPathManager::updateDirtyPathData()
{
    FlowMapExtract flowMap;
    for (auto& agentPair : agents) {
        AgentData& data = agentPair.Value;
        if (!data.current.isPathfindingActive || !data.isPathDataDirty) {
            continue;
        }
        TilePoint location = toTilePoint(data.current.agentLocation);
        TilePoint target = toTilePoint(data.current.targetLocation);

        // check and update the portal waypoint data
        bool isWaypointDataDirty = false;
        if (data.waypoints.Num() > 0) {
            if (data.waypoints[data.waypointIndex + 1]->parentTile->getCoordinates() == location.tileLocation) {
                isWaypointDataDirty = false;
                data.waypointIndex += 2;
            } else if (data.waypoints[data.waypointIndex]->parentTile->getCoordinates() != location.tileLocation) {
                isWaypointDataDirty = true;
            }
        }
        if (isWaypointDataDirty || (data.waypoints.Num() == 0 && location.tileLocation != target.tileLocation)) {
            auto portalSearchResult = flowPath->findPortalPath(location, target);
            if (!portalSearchResult.success) {
                data.current.isPathfindingActive = false;
                data.targetAcceleration = FVector2D::ZeroVector;
                data.isPathDataDirty = false;
                continue;
            }
            data.waypoints = portalSearchResult.waypoints;
            data.waypointIndex = 0;
        }

        bool followingPortals = data.waypointIndex < data.waypoints.Num();
        auto nextPortal = followingPortals ? data.waypoints[data.waypointIndex] : nullptr;
        auto connectedPortal = followingPortals ? data.waypoints[data.waypointIndex + 1] : nullptr;

        if (flowPath->getFlowMapValue({ location, target }, nextPortal, connectedPortal, flowMap)) {
            float bestTarget = MAX_VAL;
            FIntPoint targetDirection = FIntPoint::ZeroValue;
            for (int32 i = 0; i < 8; i++) {
                float cellValue = flowMap.neighborCells[i];
                if (flowMap.neighborCells[i] < bestTarget) {
                    bestTarget = cellValue;
                    targetDirection = neighbors[i];
                }
#if WITH_EDITOR
                if (DrawFlowMapAroundAgents) {
                    auto inverted = WorldToTileTransform.Inverse();
                    int32 absoluteX = location.tileLocation.X * tileLength + location.pointInTile.X + xarray[i];
                    int32 absoluteY = location.tileLocation.Y * tileLength + location.pointInTile.Y + yarray[i];
                    
                    FVector2D centerPoint(absoluteX + 0.5, absoluteY + 0.5);
                    auto scaledRadius = inverted.TransformPoint(FVector2D(0.5, 0.5));
                    FColor color = cellValue == MAX_VAL ? FColor::Black : FMath::Lerp(FLinearColor::Green, FLinearColor::Red, FMath::Clamp(cellValue / 150.0f, 0.0f, 1.0f)).ToFColor(false);
                    DrawDebugCircle(GetWorld(), toV3(inverted.TransformPoint(centerPoint)), FMath::Min(scaledRadius.X, scaledRadius.Y), 16, color, true, -1, 0, 10, FVector(0, 1, 0), FVector(1, 0, 0));
                }
#endif		// WITH_EDITOR
            }

            data.targetAcceleration = FVector2D(targetDirection.X, targetDirection.Y);
        }
        else {
            data.current.isPathfindingActive = false;
            data.targetAcceleration = FVector2D::ZeroVector;
        }

        data.isPathDataDirty = false;
    }
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

void AFlowPathManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
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
            for (int32 i = 1; i < data.waypoints.Num(); i++) {
                auto& previous = data.waypoints[i - 1];
                auto& portal = data.waypoints[i];

                // draw the portal connection in red
                FVector2D startCenter = portalCoordToAbsolute(previous->center, previous->parentTile, tileLength) + FVector2D(0.5, 0.5);
                FVector2D endCenter = portalCoordToAbsolute(portal->center, portal->parentTile, tileLength) + FVector2D(0.5, 0.5);
                DrawDebugLine(GetWorld(), toV3(inverted.TransformPoint(startCenter)), toV3(inverted.TransformPoint(endCenter)), FColor::Red, false, -1, 1, 10);
            }
        }

#endif		// WITH_EDITOR

        data.lastTick = data.current;
        data.current = INavAgent::Execute_GetAgentInfo(agent);
        bool isActive = data.current.isPathfindingActive;
        if (isActive) {
            if ((data.current.targetLocation - data.current.agentLocation).SizeSquared() <= (AcceptanceRadius * AcceptanceRadius)) {
                // agent has reached the goal
                data.current.isPathfindingActive = false;
                data.isPathDataDirty = false;
                data.targetAcceleration = FVector2D::ZeroVector;
                INavAgent::Execute_TargetReached(agent);
            }
            else if (!data.lastTick.isPathfindingActive) {
                // agent just started the pathfinding
                data.isPathDataDirty = true;
                data.targetAcceleration = FVector2D::ZeroVector;
                data.waypoints.Empty();
            }
            else {
                if (!data.isPathDataDirty) {
                    INavAgent::Execute_UpdateAcceleration(agent, data.targetAcceleration);
                }
                data.isPathDataDirty = data.current.agentLocation != data.lastTick.agentLocation || data.current.targetLocation != data.lastTick.targetLocation;
            }
        }
    }

    for (auto agent : agentsToRemove) {
        agents.Remove(agent);
    }

    updateDirtyPathData();
}

void AFlowPathManager::InitializeTiles()
{
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
    return flowPath && flowPath->updateMapTile(tileX, tileY, tileData);
}

bool AFlowPathManager::UpdateMapTilesFromTexture(int32 tileXUpperLeft, int32 tileYUpperLeft, UTexture2D* texture)
{
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

    UE_LOG(LogExec, Warning, TEXT("Tile %d, %d: Pos %d, %d"), tileX, tileY, x, y);

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
