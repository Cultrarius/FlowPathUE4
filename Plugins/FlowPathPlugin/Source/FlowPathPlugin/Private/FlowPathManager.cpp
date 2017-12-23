// Created by Michael Galetzka - all rights reserved.

#include "FlowPathManager.h"
#include "DrawDebugHelpers.h"

using namespace flow;

AFlowPathManager::AFlowPathManager()
{
	PrimaryActorTick.bCanEverTick = true;

    InitializeTiles();
}

void AFlowPathManager::BeginPlay()
{
	Super::BeginPlay();
}

FVector2D AFlowPathManager::toTile(FVector worldPosition)
{
    return FVector2D(WorldToTileTransform.TransformVector(worldPosition) / tileLength);
}

void AFlowPathManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFlowPathManager::InitializeTiles()
{
    flowPath = MakeUnique<FlowPath>(tileLength);
}

bool AFlowPathManager::UpdateMapTileWorld(FVector worldPosition, const TArray<uint8>& tileData)
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

uint8 AFlowPathManager::GetTileDataForWorldPosition(FVector worldPosition)
{
    FVector2D tilePos = toTile(worldPosition);
    int32 tileX = FMath::FloorToInt(tilePos.X);
    int32 tileY = FMath::FloorToInt(tilePos.Y);
    int32 x = FMath::Abs(tilePos.X - tileX) * tileLength;
    int32 y = FMath::Abs(tilePos.Y - tileY) * tileLength;
    TilePoint p = {FIntPoint(tileX, tileY), FIntPoint(x, y)};

    UE_LOG(LogExec, Warning, TEXT("Tile %d, %d: Pos %d, %d"), tileX, tileY, x, y);

    return flowPath->getDataFor(p);
}

#if WITH_EDITOR

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
                    FVector tileStart(absoluteX, absoluteY, 0);
                    FVector tileEnd = tileStart + FVector(0.95, 0.95, 0);
                    DrawDebugLine(GetWorld(), inverted.TransformVector(tileStart), inverted.TransformVector(tileEnd), FColor::Red);
                    tileStart.Y += 0.95;
                    tileEnd.Y -= 0.95;
                    DrawDebugLine(GetWorld(), inverted.TransformVector(tileStart), inverted.TransformVector(tileEnd), FColor::Red);
                }
            }
        }
    }
}

FVector portalCoordToAbsolute(FIntPoint p, FlowTile* tile, int32 tileLength) {
    auto& tileCoord = tile->getCoordinates();
    int32 absoluteX = tileCoord.X * tileLength + p.X;
    int32 absoluteY = tileCoord.Y * tileLength + p.Y;
    return FVector(absoluteX, absoluteY, 0);
}

void AFlowPathManager::DebugDrawAllPortals()
{
    auto inverted = WorldToTileTransform.Inverse();
    for (auto& portal : flowPath->getAllPortals()) {

        // draw the portals themselves in green
        FVector portalStart = portalCoordToAbsolute(portal->start, portal->parentTile, tileLength) + FVector(0.5, 0.5, 0);
        FVector portalEnd = portalCoordToAbsolute(portal->end, portal->parentTile, tileLength) + FVector(0.5, 0.5, 0);
        DrawDebugLine(GetWorld(), inverted.TransformVector(portalStart), inverted.TransformVector(portalEnd), FColor::Green);

        // draw the connected portals in blue
        FVector startCenter = portalCoordToAbsolute(portal->center, portal->parentTile, tileLength) + FVector(0.5, 0.5, 0);
        for (auto& connected : portal->connected) {
            FVector endCenter = portalCoordToAbsolute(connected.Key->center, connected.Key->parentTile, tileLength) + FVector(0.5, 0.5, 0);
            DrawDebugLine(GetWorld(), inverted.TransformVector(startCenter), inverted.TransformVector(endCenter), FColor::Blue);
        }
    }
}

#endif		// WITH_EDITOR
