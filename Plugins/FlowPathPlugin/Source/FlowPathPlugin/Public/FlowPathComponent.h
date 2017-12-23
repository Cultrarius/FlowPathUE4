// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "FlowPathComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class FLOWPATHPLUGIN_API UFlowPathComponent : public UActorComponent
{
	GENERATED_BODY()

	
public:

    UFlowPathComponent();

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

    //~ Begin ActorComponent Interface 
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
    virtual void RegisterComponentTickFunctions(bool bRegister) override;
    virtual void PostLoad() override;
    virtual void Serialize(FArchive& Ar) override;

    /** Overridden to auto-register the updated component if it starts NULL, and we can find a root component on our owner. */
    virtual void InitializeComponent() override;

    /** Overridden to update component properties that should be updated while being edited. */
    virtual void OnRegister() override;

    /**
    * Possibly skip update if moved component is not rendered or can't move.
    * @param DeltaTime @todo this parameter is not used in the function.
    * @return true if component movement update should be skipped
    */
    virtual bool ShouldSkipUpdate() const;

    /** Assign the pawn we update. */
    UFUNCTION(BlueprintCallable, Category = "Components|Steering")
    virtual void SetUpdatedPawn(APawn* NewUpdatedPawn);


};
