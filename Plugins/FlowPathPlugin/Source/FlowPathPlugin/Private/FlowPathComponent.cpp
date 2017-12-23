// Fill out your copyright notice in the Description page of Project Settings.

#include "FlowPathComponent.h"

UFlowPathComponent::UFlowPathComponent()
{
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    PrimaryComponentTick.bCanEverTick = true;

    bUpdateOnlyIfRendered = false;
    bTickBeforeOwner = true;

    bWantsInitializeComponent = true;
    bAutoActivate = true;
}


void UFlowPathComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (ShouldSkipUpdate())
    {
        return;
    }
    if (UpdatedPawn->IsPendingKill()) {
        // Don't hang on to stale references to a destroyed UpdatedPawn.
        SetUpdatedPawn(nullptr);
        return;
    }

    UpdatedPawn->AddMovementInput(FVector(1, 0, 0));
}

void UFlowPathComponent::RegisterComponentTickFunctions(bool bRegister)
{
    Super::RegisterComponentTickFunctions(bRegister);

    // Super may start up the tick function when we don't want to.
    SetComponentTickEnabled(UpdatedPawn != nullptr && bAutoActivate);

    // If the owner ticks, make sure we tick first
    AActor* Owner = GetOwner();
    if (bTickBeforeOwner && bRegister && PrimaryComponentTick.bCanEverTick && Owner && Owner->CanEverTick())
    {
        Owner->PrimaryActorTick.AddPrerequisite(this, PrimaryComponentTick);
    }
}

void UFlowPathComponent::PostLoad()
{
    Super::PostLoad();

    SetUpdatedPawn(Cast<APawn>(GetOwner()));
}

void UFlowPathComponent::Serialize(FArchive & Ar)
{
    APawn* CurrentUpdatedPawn = UpdatedPawn;
    Super::Serialize(Ar);

    if (Ar.IsLoading())
    {
        // This was marked Transient so it won't be saved out, but we need still to reject old saved values.
        UpdatedPawn = CurrentUpdatedPawn;
    }
}

void UFlowPathComponent::InitializeComponent()
{
    Super::InitializeComponent();

    if (!UpdatedPawn)
    {
        SetUpdatedPawn(Cast<APawn>(GetOwner()));
    }
}

void UFlowPathComponent::OnRegister()
{
    Super::OnRegister();

    const UWorld* world = GetWorld();
    if (world && world->IsGameWorld() && UpdatedPawn == nullptr)
    {
        SetUpdatedPawn(Cast<APawn>(GetOwner()));
    }
}

bool UFlowPathComponent::ShouldSkipUpdate() const
{
    if (UpdatedPawn == nullptr)
    {
        return true;
    }
    USceneComponent* UpdatedComponent = UpdatedPawn->GetRootComponent();
    if (UpdatedComponent == nullptr || UpdatedComponent->Mobility != EComponentMobility::Movable) {
        return true;
    }

    if (bUpdateOnlyIfRendered)
    {
        if (IsNetMode(NM_DedicatedServer))
        {
            // Dedicated servers never render
            return true;
        }

        if (UpdatedComponent == nullptr) {
            return true;
        }
        const float RenderTimeThreshold = 0.41f;
        UWorld* TheWorld = GetWorld();

        // Check attached children render times.
        TArray<USceneComponent*> Children;
        UpdatedComponent->GetChildrenComponents(true, Children);
        for (auto Child : Children)
        {
            const UPrimitiveComponent* PrimitiveChild = Cast<UPrimitiveComponent>(Child);
            if (PrimitiveChild)
            {
                if (PrimitiveChild->IsRegistered() && TheWorld->TimeSince(PrimitiveChild->LastRenderTime) <= RenderTimeThreshold)
                {
                    return false; // Rendered, don't skip it.
                }
            }
        }

        // No children were recently rendered, safely skip the update.
        return true;
    }

    return false;
}

void UFlowPathComponent::SetUpdatedPawn(APawn * NewUpdatedPawn)
{
    // Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
    UpdatedPawn = IsValid(NewUpdatedPawn) ? NewUpdatedPawn : NULL;
}

void UFlowPathComponent::SetFlowPathManager(AFlowPathManager * NewFlowPathManager)
{
    // Don't assign pending kill components, but allow those to null out previous UpdatedComponent.
    FlowPathManager = IsValid(NewFlowPathManager) ? NewFlowPathManager : NULL;
}
