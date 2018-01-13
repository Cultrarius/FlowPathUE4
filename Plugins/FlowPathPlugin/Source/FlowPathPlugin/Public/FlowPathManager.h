// Created by Michael Galetzka - all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "flow/FlowPath.h"
#include "NavAgent.h"
#include "TransformCalculus2D.h"
#include "QueuedThreadPool.h"
#include "IQueuedWork.h"
#include <list>
#include "ThreadSafeBool.h"
#include "FlowPathManager.generated.h"


struct AgentData {
    UObject* agent;

    // data from the current tick
    FAgentInfo current;
    flow::TilePoint currentLocation;
    flow::TilePoint currentTarget;
        
    // data from the last tick
    FAgentInfo lastTick;
    flow::TilePoint lastLocation;
    flow::TilePoint lastTarget;

    // pathfinding data
    bool isPathDataDirty = false;
    FVector2D targetAcceleration;
    TArray<const flow::Portal*> waypoints;
    int32 waypointIndex;
};

class FlowMapGenerationTask : public IQueuedWork
{
private:
    flow::TilePoint target;
    TArray<const flow::Portal*> waypoints;
    int32 workIndex;
    bool lookaheadAllowed;
    flow::FlowPath& flowPath;
    FCriticalSection& tileLock;

public:
    FIntPoint workingTile;
    FThreadSafeBool isDone;
    FThreadSafeBool isAbandoned;
    TArray<flow::EikonalCellValue> result;
    const flow::Portal * resultStartPortal;
    const flow::Portal * resultEndPortal;

    FlowMapGenerationTask(const flow::TilePoint& target, TArray<const flow::Portal*> waypoints, int32 workIndex, bool lookahead, flow::FlowPath& flowPath, FCriticalSection& tileLock);
    
    /**
    * Tells the queued work that it is being abandoned so that it can do
    * per object clean up as needed. This will only be called if it is being
    * abandoned before completion. NOTE: This requires the object to delete
    * itself using whatever heap it was allocated in.
    */
    void Abandon() override;

    /**
    * This method is also used to tell the object to cleanup but not before
    * the object has finished it's work.
    */
    void DoThreadedWork() override;
};


UCLASS(meta = (BlueprintSpawnableComponent), BlueprintType)
class FLOWPATHPLUGIN_API AFlowPathManager : public AActor
{
    GENERATED_BODY()
private:
    TUniquePtr<flow::FlowPath> flowPath;
    TMap<UObject*, AgentData> agents;
    FTransform2D WorldToTileTransform;
    int32 ticksSinceLastCleanup;

    TUniquePtr<FQueuedThreadPool> Pool;
    std::list<FlowMapGenerationTask> generatorTasks;
    FCriticalSection tileLock;
    
    void updateDirtyPathData();

    void processFlowMapGenerators();

    float calcVelocityBonus(const FVector2D& velocityDirection, int32 i) const;

    float calcWaypointBonus(const FVector2D& waypointDirection, int32 i) const;

protected:

    FVector2D toAbsoluteTileLocation(flow::TilePoint p) const;

    FVector2D toTile(FVector2D worldPosition) const;

    flow::TilePoint toTilePoint(FVector2D worldPosition) const;

    flow::TilePoint absoluteTilePosToTilePoint(FVector2D tilePosition) const;

    bool findClosestWaypoint(const AgentData& data, const flow::TilePoint& agentLocation, flow::TilePoint& result) const;

    void precomputeFlowmaps(const AgentData& data);

    void cleanupOldFlowmaps();

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

    /** 
     * If true then flowmaps can be generated with data from the tiles around them. This leads to higher quality flowmaps,
     * but it also increases the calculation times and a flowmap becomes invalid if a neighbor tile is changed.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    bool LookaheadFlowmapGeneration;

    /**
    * If true then agents with the same goal will reuse each others path search results, which has two benefits:
    * 1. It is faster.
    * 2. Units tend to group together as they try to use similar paths.
    * The drawback is that it can lead to suboptimal paths for some units.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    bool MergingPathSearch;

    /** The better a target cell direction aligns with the current agent velocity, the more its value gets boosted by the bonus (ranges from -1 to 1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    float VelocityBonus;

    /** The better a cell aligns with the next waypoint, the more its value gets boosted by the bonus (ranges from -1 to 1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FlowPath)
    float WaypointBonus;

    /**
    * If true then agents with the same goal will reuse each others path search results, which has two benefits:
    * 1. It is faster.
    * 2. Units tend to group together as they try to use similar paths.
    * The drawback is that it can lead to suboptimal paths for some units.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = FlowPath)
    int32 GeneratorThreadPoolSize;

    /** 
    * The max number of precomputed flowmaps that are written per tick. This is mainly used to smooth the updates over a few tick to prevent lag spikes.
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = FlowPath)
    int32 MaxAsyncFlowMapUpdatesPerTick;

    /**
    * The number of ticks that need to pass before all cached and unused flowmaps are deleted to reclaim memory.
    * A negative value means flowmaps will never be deleted once created (as long as the source data is not changed).
    */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = FlowPath)
    int32 CleanupFlowmapsAfterTicks;

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

    /** Checks if an agent can travel from the given start to the given end. */
    UFUNCTION(BlueprintCallable, Category = "FlowPath")
    bool IsPathPossible(FVector2D worldPositionStart, FVector2D worldPositionEnd) const;

#if WITH_EDITOR

private:
    void DebugDrawAllBlockedCells();

    void DebugDrawAllPortals();

    void DebugDrawFlowMaps();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath Debug")
    bool DrawAllBlockedCells;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath Debug")
    bool DrawAllPortals;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath Debug")
    bool DrawFlowMapAroundAgents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath Debug")
    bool DrawAgentPortalWaypoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowPath Debug")
    bool DrawFlowMaps;

#endif		// WITH_EDITOR

};
