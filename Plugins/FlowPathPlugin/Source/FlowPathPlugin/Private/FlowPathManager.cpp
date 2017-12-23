// Created by Michael Galetzka - all rights reserved.

#include "FlowPathManager.h"

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
    return FVector2D(WorldToTileTransform.TransformVector(worldPosition));
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

void AFlowPathManager::DebugPrintPortals()
{
    if (flowPath) {
        flowPath->printPortals();
    }
}

