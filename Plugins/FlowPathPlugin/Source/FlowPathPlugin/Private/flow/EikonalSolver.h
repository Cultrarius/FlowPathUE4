//
// Created by Michael Galetzka on 06.01.2018.
//

#pragma once

#include "CoreMinimal.h"
#include "FlowTile.h"

namespace flow {

    // Constants
    const uint8 EMPTY = 1;
    const uint8 BLOCKED = 255;
    const int32 MAX_VAL = 10000;
    const int8 xarray[8] = { -1, 0, 1,  0, -1, -1, 1,  1 };
    const int8 yarray[8] = {  0, 1, 0, -1, -1,  1, 1, -1 };
    const int8 reverseLookup[8] = { 2, 3, 0, 1, 6, 7, 4, 5 };
    const FIntPoint neighbors[8] = { FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, -1), FIntPoint(-1, 1), FIntPoint(1, 1), FIntPoint(1, -1) };
    const FVector2D normalizedNeighbors[8] = { FVector2D(-1, 0).GetSafeNormal(), FVector2D(0, 1).GetSafeNormal(), FVector2D(1, 0).GetSafeNormal(), FVector2D(0, -1).GetSafeNormal(), 
        FVector2D(-1, -1).GetSafeNormal(), FVector2D(-1, 1).GetSafeNormal(), FVector2D(1, 1).GetSafeNormal(),  FVector2D(1, -1).GetSafeNormal() };
    const int8 leftDirectionLookup[8] =  { 5, 6, 7, 4, 0, 1, 2, 3 };
    const int8 rightDirectionLookup[8] = { 4, 5, 6, 7, 3, 0, 1, 2 };

    TArray<EikonalCellValue> CreateEikonalSurface(const TArray<uint8>& sourceData, const TArray<FIntPoint> targetPoints);
}

