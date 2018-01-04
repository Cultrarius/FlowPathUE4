//
// Created by Michael Galetzka on 01.01.2018.
//

#pragma once

#include "CoreMinimal.h"
#include "FlowTile.h"

namespace flow {
    const int MAX_VAL = 10000;
    const int yarray[8] = { -1, -1, -1, 0, 1, 1, 1, 0 };
    const int xarray[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
    const FIntPoint neighbors[8] = { FIntPoint(-1, -1), FIntPoint(-1, 0), FIntPoint(-1, 1), FIntPoint(0, 1), 
        FIntPoint(1, 1), FIntPoint(1, 0), FIntPoint(1, -1), FIntPoint(0, -1), };

    TArray<float> CreateEikonalSurface(const TArray<uint8>& sourceData, const TArray<FIntPoint> targetPoints);
}

