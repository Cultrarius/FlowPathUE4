// Created by Michael Galetzka - all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "flow/FlowPath.h"
#include "FlowPathManager.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent), BlueprintType)
class FLOWPATHPLUGIN_API AFlowPathManager : public AActor
{
    GENERATED_BODY()
private:
    TUniquePtr<flow::FlowPath> flowPath;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    FVector2D toTile(FVector worldPosition);

public:	

    /** The cell value of a blocked tile. No movement is allowed through a blocked cell. */
    UPROPERTY(BlueprintReadOnly, Category = FlowPath)
    uint8 BlockedTileValue = 255;

    /** The size of a tile. Each tile consists of individual cells, which have a movement cost attached.
    *   Bigger tiles increase performance and memory needs, but lead to smoother pathfinding.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    int32 tileLength = 10;

    /** The transformation to convert a world position into a tile position (e.g. [100, 250, 30] -> [1, 2.5, 0]). The Z value of the result is always discarded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    FTransform WorldToTileTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.1f, 0.1f, 0));

    AFlowPathManager();

	virtual void Tick(float DeltaTime) override;
	
    /** Destroys all previous pathing data and recreates new tiles. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void InitializeTiles();

    /** Updates the specified tile with the given data. The data must *not* contain 0 values. The given world vector is transformed and truncated into tile coordinates.*/
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTileWorld(FVector worldPosition, const TArray<uint8> &tileData);

    /** Updates the specified tile with the given data. The data must *not* contain 0 values. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTileLocal(int32 tileX, int32 tileY, const TArray<uint8> &tileData);

    /** Updates the tiles with the pixel data from the texture. Each pixel of the texture is converted to a byte value for the tile data. If the texture is not a multiple of the tilesize, the additional cells are marked as blocked. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTilesFromTexture(int32 tileXUpperLeft, int32 tileYUpperLeft, UTexture2D* texture);

    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void DebugPrintPortals();
};
