// Created by Michael Galetzka - all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "flow/FlowPath.h"
#include "NavAgent.h"
#include "TransformCalculus2D.h"
#include "FlowPathManager.generated.h"

struct AgentData {
    UObject* agent;

    // data from the current tick
    FAgentInfo current;

    // data from the last tick
    FAgentInfo lastTick;

    // pathfinding data
    bool isPathDataDirty = false;
    FVector2D targetAcceleration;
    TArray<const flow::Portal*> waypoints;
    int32 waypointIndex;
};

UCLASS(meta = (BlueprintSpawnableComponent), BlueprintType)
class FLOWPATHPLUGIN_API AFlowPathManager : public AActor
{
    GENERATED_BODY()
private:
    TUniquePtr<flow::FlowPath> flowPath;

    TMap<UObject*, AgentData> agents;

    FTransform2D WorldToTileTransform;

    void updateDirtyPathData();

protected:

    FVector2D toAbsoluteTileLocation(flow::TilePoint p) const;

    FVector2D toTile(FVector2D worldPosition) const;

    flow::TilePoint toTilePoint(FVector2D worldPosition) const;

    flow::TilePoint absoluteTilePosToTilePoint(FVector2D tilePosition) const;

    bool findNextSmoothedWaypoint(const AgentData& data, flow::TilePoint& result) const;

public:	

    /** The cell value of a blocked tile. No movement is allowed through a blocked cell. */
    UPROPERTY(BlueprintReadOnly, Category = FlowPath)
    uint8 BlockedTileValue = 255;

    /** The size of a tile. Each tile consists of individual cells, which have a movement cost attached.
    *   Bigger tiles increase performance and memory needs, but lead to smoother pathfinding.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    int32 tileLength;

    /** The translation to convert a world position into a tile position (e.g. [100, 250] -> [105, 255]). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    FVector2D WorldToTileTranslation;

    /** The scale to convert a world position into a tile position (e.g. [100, 250] -> [1, 2.5]). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    FVector2D WorldToTileScale;

    /** The world-distance to the target at which the pathfinder marks a query as finished. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    float AcceptanceRadius;

    AFlowPathManager();

	virtual void Tick(float DeltaTime) override;
	
    /** Destroys all previous pathing data and recreates new tiles. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void InitializeTiles();

    /** Updates the specified tile with the given data. The data must *not* contain 0 values. The given world vector is transformed and truncated into tile coordinates.*/
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTileWorld(FVector2D worldPosition, const TArray<uint8> &tileData);

    /** Updates the specified tile with the given data. The data must *not* contain 0 values. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTileLocal(int32 tileX, int32 tileY, const TArray<uint8> &tileData);

    /** Updates the tiles with the pixel data from the texture. Each pixel of the texture is converted to a byte value for the tile data. If the texture is not a multiple of the tilesize, the additional cells are marked as blocked. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool UpdateMapTilesFromTexture(int32 tileXUpperLeft, int32 tileYUpperLeft, UTexture2D* texture);

    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    uint8 GetTileDataForWorldPosition(FVector2D worldPosition);

    /** Registers an agent with this path manager, so it can be steered together with other agents. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void RegisterAgent(UObject* agent);

    /** Removes a previously registered agent from the path manager. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void RemoveAgent(UObject* agent);

#if WITH_EDITOR

    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    void DebugDrawAllBlockedCells();

    UFUNCTION(BlueprintCallable, Category = "FlowPath")
     void DebugDrawAllPortals();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath")
    bool DrawFlowMapAroundAgents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath")
    bool DrawAgentPortalWaypoints;

#endif		// WITH_EDITOR

};
