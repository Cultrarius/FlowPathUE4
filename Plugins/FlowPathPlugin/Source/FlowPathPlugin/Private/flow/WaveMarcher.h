//
// Created by Michael Galetzka on 01.01.2018.
//

#pragma once

#include "CoreMinimal.h"
#include "FlowTile.h"

namespace flow {
    const int MAX_VAL = 10000;
    const int xarray[8] = { -1, 0, 1,  0, -1, -1, 1,  1 };
    const int yarray[8] = {  0, 1, 0, -1, -1,  1, 1, -1 };
    const FIntPoint neighbors[8] = { FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(-1, -1), FIntPoint(-1, 1), FIntPoint(1, 1), FIntPoint(1, -1) };
    const FVector2D normalizedNeighbors[8] = { FVector2D(-1, 0).GetSafeNormal(), FVector2D(0, 1).GetSafeNormal(), FVector2D(1, 0).GetSafeNormal(), FVector2D(0, -1).GetSafeNormal(), 
        FVector2D(-1, -1).GetSafeNormal(), FVector2D(-1, 1).GetSafeNormal(), FVector2D(1, 1).GetSafeNormal(),  FVector2D(1, -1).GetSafeNormal() };

    TArray<EikonalCellValue> CreateEikonalSurface(const TArray<uint8>& sourceData, const TArray<FIntPoint> targetPoints);
}

