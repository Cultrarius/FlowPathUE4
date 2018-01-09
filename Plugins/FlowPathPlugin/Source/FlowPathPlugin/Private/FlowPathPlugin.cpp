// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FlowPathPlugin.h"
#include "flow/FlowPath.h"
#include "flow/EikonalSolver.h"

#define LOCTEXT_NAMESPACE "FFlowPathPluginModule"

using namespace flow;

void FFlowPathPluginModule::StartupModule()
{
    return;
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    
    FlowPath path(4);
    TArray<uint8> tile11 = {
        1, 1, 1, 1,
        2, 4, 255, 1,
        1, 2, 1, 255,
        1, 1, 1, 1
    };
    TArray<uint8> tile21 = {
        255, 1, 255, 1,
        255, 1, 255, 1,
        255, 1, 255, 255,
        255, 1, 1, 1
    };
    TArray<uint8> tile12 = {
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 255, 255, 255,
        1, 1, 1, 1
    };
    TArray<uint8> tile22 = {
        255, 1, 1, 1,
        255, 1, 1, 1,
        255, 255, 1, 1,
        1, 1, 1, 1
    };
    path.updateMapTile(1, 1, tile11);
    path.updateMapTile(1, 2, tile12);
    path.updateMapTile(2, 1, tile21);
    path.updateMapTile(2, 2, tile22);

    tile21 = {
        255, 1, 255, 1,
        3, 1, 255, 1,
        255, 1, 255, 255,
        255, 1, 1, 1
    };
    path.updateMapTile(2, 1, tile21);
    UE_LOG(LogExec, Warning, TEXT("---------------------"));

    PathSearchResult result = path.findDirectPath(FIntPoint(1, 0), FIntPoint(3, 3));
    UE_LOG(LogExec, Warning, TEXT("Success: %d, Cost: %d"), result.success, result.pathCost);
    for (FIntPoint p : result.waypoints) {
        UE_LOG(LogExec, Warning, TEXT("Waypoint %d, %d:"), p.X, p.Y);
    }
    UE_LOG(LogExec, Warning, TEXT("---------------------"));

    TilePoint searchStart = { FIntPoint(1, 1), FIntPoint(1, 0) };
    TilePoint searchEnd = { FIntPoint(2, 2), FIntPoint(1, 3) };
    PortalSearchResult portalResult = path.findPortalPath(searchStart, searchEnd, false);
    //TODO: test cache
    UE_LOG(LogExec, Warning, TEXT("Success: %d"), portalResult.success);
    UE_LOG(LogExec, Warning, TEXT("Portal search [start]"));
    for (int i = 0; i < portalResult.waypoints.Num(); i++) {
        const Portal* p = portalResult.waypoints[i];
        UE_LOG(LogExec, Warning, TEXT("Portal waypoint %d, %d on tile %d, %d:"), p->center.X, p->center.Y, p->tileCoordinates.X, p->tileCoordinates.Y);
    }
    UE_LOG(LogExec, Warning, TEXT("Portal search [end]"));

    UE_LOG(LogExec, Warning, TEXT("---------------------"));
    portalResult.waypoints.RemoveAt(0, 2);
    path.cachePortalPath(searchEnd, portalResult.waypoints);
    portalResult = path.findPortalPath(searchStart, searchEnd, false);
    UE_LOG(LogExec, Warning, TEXT("Testing cached path... Success: %d"), portalResult.success);
    UE_LOG(LogExec, Warning, TEXT("Portal search [start]"));
    for (int i = 0; i < portalResult.waypoints.Num(); i++) {
        const Portal* p = portalResult.waypoints[i];
        UE_LOG(LogExec, Warning, TEXT("Portal waypoint %d, %d on tile %d, %d:"), p->center.X, p->center.Y, p->tileCoordinates.X, p->tileCoordinates.Y);
    }
    UE_LOG(LogExec, Warning, TEXT("Portal search [end]"));
    UE_LOG(LogExec, Warning, TEXT("---------------------"));
    
    TArray<uint8> bigTile = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 255, 255,
        1, 1, 1, 1, 1, 1, 1, 1, 255, 255,
        1, 1, 1, 1, 1, 1, 1, 1, 255, 255,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    TArray<FIntPoint> targets;
    targets.Emplace(0, 9);
    targets.Emplace(1, 9);
    targets.Emplace(2, 9);
    targets.Emplace(3, 9);
    targets.Emplace(4, 9);
    targets.Emplace(5, 9);
    targets.Emplace(6, 9);
    targets.Emplace(7, 9);
    targets.Emplace(8, 9);
    targets.Emplace(9, 9);
    TArray<EikonalCellValue> eikonal = CreateEikonalSurface(bigTile, targets);

    for (int32 i = 0; i < 10; i++) {
        EikonalCellValue* v = &eikonal[i * 10];
        UE_LOG(LogExec, Warning, TEXT("%f %f %f %f %f %f %f %f %f %f"), v[0].cellValue, v[1].cellValue, v[2].cellValue, v[3].cellValue, v[4].cellValue, v[5].cellValue, v[6].cellValue, v[7].cellValue, v[8].cellValue, v[9].cellValue);
    }
    UE_LOG(LogExec, Warning, TEXT("---------------------"));
    for (int32 i = 0; i < 10; i++) {
        EikonalCellValue* v = &eikonal[i * 10];
        UE_LOG(LogExec, Warning, TEXT("%d %d %d %d %d %d %d %d %d %d"), v[0].directionLookupIndex, v[1].directionLookupIndex, v[2].directionLookupIndex, v[3].directionLookupIndex, v[4].directionLookupIndex,
            v[5].directionLookupIndex, v[6].directionLookupIndex, v[7].directionLookupIndex, v[8].directionLookupIndex, v[9].directionLookupIndex);
    }

    UE_LOG(LogExec, Warning, TEXT("Meh"));
}

void FFlowPathPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFlowPathPluginModule, FlowPathPlugin)