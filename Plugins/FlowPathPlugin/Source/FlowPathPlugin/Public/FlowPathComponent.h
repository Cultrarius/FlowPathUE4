// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlowPathManager.h"
#include "NavAgent.h"
#include "Components/ActorComponent.h"
#include "FlowPathComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class FLOWPATHPLUGIN_API UFlowPathComponent : public UActorComponent, public INavAgent
{
	GENERATED_BODY()

private:
    FAgentInfo agentInfo;

public:

    UFlowPathComponent();

    /** If true, skips TickComponent() if UpdatedComponent was not recently rendered. */
    UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category = SteeringComponent)
    AFlowPathManager* FlowPathManager;

    /**
    * The pawn we move and update.
    * If this is null at startup and bAutoRegisterUpdatedComponent is true, the owning Actor's root component will automatically be set as our UpdatedComponent at startup.
    * @see bAutoRegisterUpdatedComponent, SetUpdatedComponent(), UpdatedPrimitive
    */
    UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category = SteeringComponent)
    APawn* UpdatedPawn;

    /** If true, skips TickComponent() if UpdatedComponent was not recently rendered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SteeringComponent)
    uint32 bUpdateOnlyIfRendered : 1;

    /**
    * If true, after registration we will add a tick dependency to tick before our owner (if we can both tick).
    * This is important when our tick causes an update in the owner's position, so that when the owner ticks it uses the most recent position without lag.
    * Disabling this can improve performance if both objects tick but the order of ticks doesn't matter.
    */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SteeringComponent)
    uint32 bTickBeforeOwner : 1;

    //~ Begin NavAgent Interface 

    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        FAgentInfo GetAgentInfo() const;
        virtual FAgentInfo GetAgentInfo_Implementation() const override;

    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        void UpdateAcceleration(const FVector2D& newAcceleration);
        virtual void UpdateAcceleration_Implementation(const FVector2D& newAcceleration) override;

    UFUNCTION(BlueprintNativeEvent, Category = "FlowPath")
        void TargetReached();
        virtual void TargetReached_Implementation();

    //~ End NavAgent Interface

    //~ Begin ActorComponent Interface 
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
    virtual void RegisterComponentTickFunctions(bool bRegister) override;
    virtual void PostLoad() override;
    virtual void Serialize(FArchive& Ar) override;
    virtual void InitializeComponent() override;
    virtual void OnRegister() override;
    //~ End ActorComponent Interface

    /**
    * Possibly skip update if moved component is not rendered or can't move.
    * @return true if component movement update should be skipped
    */
    virtual bool ShouldSkipUpdate() const;

    /** Assign the pawn we update. */
    UFUNCTION(BlueprintCallable, Category = "Components|Steering")
    virtual void SetUpdatedPawn(APawn* NewUpdatedPawn);

    /** Assign the flow path manager which has the path steering info. */
    UFUNCTION(BlueprintCallable, Category = "Components|Steering")
    virtual void SetFlowPathManager(AFlowPathManager* NewFlowPathManager);

    /** Assign the pawn we update. */
    UFUNCTION(BlueprintCallable, Category = "Components|Steering")
    virtual void MovePawnToTarget(FVector2D Target);
};
