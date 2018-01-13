// Created by Michael Galetzka - all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAgent.generated.h"

USTRUCT(BlueprintType)
struct FAgentInfo
{
    GENERATED_USTRUCT_BODY()

    FAgentInfo()
    {
        isPathfindingActive = false;
    }

    /** If true then this agent tries to reach the targetLocation. */
    UPROPERTY(EditAnywhere, Category = "FlowPath")
        bool isPathfindingActive;
    
    /** The current location of the agent. */
    UPROPERTY(EditAnywhere, Category = "FlowPath")
        FVector2D agentLocation;

    /** The target the agent is trying to reach. */
    UPROPERTY(EditAnywhere, Category = "FlowPath", meta = (EditCondition = "isPathfindingActive"))
        FVector2D targetLocation;
};

UINTERFACE(BlueprintType)
class FLOWPATHPLUGIN_API UNavAgent : public UInterface
{
    GENERATED_UINTERFACE_BODY()
};

class FLOWPATHPLUGIN_API INavAgent
{
    GENERATED_IINTERFACE_BODY()

public:
    /** Returns the up-to-date info on this agent. */
    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        FAgentInfo GetAgentInfo() const;

    /** Updates the direction this agent should move to in order to reach its target. The vector is normalized to a length between 0 and 1. */
    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        void UpdateAcceleration(const FVector2D& newAcceleration);

    /** This method is called if the agent has reached the target location. */
    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        void TargetReached();

    /** This method is called if the agent has a target location that cannot be reached. */
    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        void TargetUnreachable();
};