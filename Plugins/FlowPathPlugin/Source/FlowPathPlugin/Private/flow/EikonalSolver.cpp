//
// Created by Michael Galetzka on 06.01.2018.
//

#include "EikonalSolver.h"
#include <list>

using namespace flow;
using namespace std;

enum class State {
    None, Trial, Done
};

struct WaveNode {
    float value = MAX_VAL;
    uint8 surfaceCost = BLOCKED;
    State state = State::None;
    int8 parentDirection = -1;
};

int32 toIndex(int32 x, int32 y, int32 length)
{
    return x + y * length;
}

int32 toIndex(const FIntPoint& coordinates, int32 length)
{
    return coordinates.X + coordinates.Y * length;
}

bool isValidLocation(const FIntPoint & p, int32 length)
{
    return p.X >= 0 && p.Y >= 0 && p.X < length && p.Y < length;
}

TArray<EikonalCellValue> flow::CreateEikonalSurface(const TArray<uint8>& sourceData, const TArray<FIntPoint> targetPoints)
{
    int32 tileSize = sourceData.Num();
    int32 length = FMath::Sqrt(tileSize);
    check(length);

    
    // Data definitions
    TArray<bool> initializedNodes;
    initializedNodes.AddZeroed(tileSize);
    TArray<WaveNode> waveSurface;
    waveSurface.AddDefaulted(tileSize);
    list<FIntPoint> trialNodes;

    // Target point initialization
    for (int32 i = 0; i < targetPoints.Num(); i++) {
        FIntPoint target = targetPoints[i];
        check(target.X >= 0 && target.X < length);
        check(target.Y >= 0 && target.Y < length);
        int32 index = toIndex(target, length);
        waveSurface[index].value = 0.0;
        waveSurface[index].surfaceCost = sourceData[index];
        waveSurface[index].state = State::Trial;
        initializedNodes[index] = true;
        trialNodes.push_back(target);
    }

    // Loop until all nodes are processed
    while (!trialNodes.empty()) {
        auto center = trialNodes.front();
        trialNodes.pop_front();
        int32 centerIndex = toIndex(center, length);
        waveSurface[centerIndex].state = State::Done;
        float centerValue = waveSurface[centerIndex].value;

        // Neighbor value computation
        for (int32 i = 0; i < 8; i++) {
            FIntPoint neighbor = center + neighbors[i];
            if (!isValidLocation(neighbor, length)) {
                continue;
            }

            int32 index = toIndex(neighbor, length);
            auto& node = waveSurface[index];
            if (!initializedNodes[index]) {
                node.surfaceCost = sourceData[index];
                initializedNodes[index] = true;
            }
            if (node.surfaceCost == BLOCKED) {
                continue;
            }

            float newCost;
            if (i < 4) {
                // non-diagonal moves are simple
                newCost = centerValue + node.surfaceCost;
            }
            else {
                // diagonal moves are only allowed when not crossing a blocked cell and they also cost more
                int32 nodeIndex1 = toIndex(center + FIntPoint(xarray[i], 0), length);
                int32 nodeIndex2 = toIndex(center + FIntPoint(0, yarray[i]), length);
                auto& nextNode1 = waveSurface[nodeIndex1];
                auto& nextNode2 = waveSurface[nodeIndex2];
                if (nextNode1.surfaceCost == BLOCKED || nextNode2.surfaceCost == BLOCKED) {
                    continue;
                }
                newCost = centerValue + node.surfaceCost + (nextNode1.surfaceCost + nextNode2.surfaceCost) / 4;
            }

            // Update with new value
            if (newCost < node.value) {
                node.value = newCost;
                node.parentDirection = reverseLookup[i];
                if (node.state != State::Trial) {
                    node.state = State::Trial;
                    trialNodes.push_back(neighbor);
                }
            }
        }
    }

    // copy the result to the output
    TArray<EikonalCellValue> output;
    output.AddUninitialized(sourceData.Num());
    for (int32 i = 0; i < length * length; i++) {
        output[i].cellValue = waveSurface[i].value;
        output[i].directionLookupIndex = waveSurface[i].parentDirection;
    }
    return output;
}
