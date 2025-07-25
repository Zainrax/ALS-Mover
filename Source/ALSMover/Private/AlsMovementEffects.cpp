#include "AlsMovementEffects.h"

#include "AlsMoverData.h"
#include "MoverComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMovementEffects)

//========================================================================
// FApplyCapsuleSizeEffect Implementation
//========================================================================

FApplyCapsuleSizeEffect::FApplyCapsuleSizeEffect()
{
    // Default values set in header
}

bool FApplyCapsuleSizeEffect::ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams,
                                                  FMoverSyncState &OutputState)
{
    if (!ApplyEffectParams.MoverComp || !ApplyEffectParams.MoverComp->GetOwner())
    {
        return false;
    }

    APawn *Pawn = ApplyEffectParams.MoverComp->GetOwner<APawn>();
    if (!Pawn)
    {
        return false;
    }

    UCapsuleComponent *Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent());
    if (!Capsule)
    {
        return false;
    }

    // Apply capsule size change
    const float OldHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float HeightDelta = TargetHalfHeight - OldHalfHeight;

    // Set new capsule size
    Capsule->SetCapsuleHalfHeight(TargetHalfHeight);

    if (TargetRadius > 0.0f)
    {
        Capsule->SetCapsuleRadius(TargetRadius);
    }

    // Adjust mesh position if requested
    if (bAdjustMeshPosition)
    {
        if (USkeletalMeshComponent *Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
        {
            FVector MeshRelativeLoc = Mesh->GetRelativeLocation();
            MeshRelativeLoc.Z = -TargetHalfHeight;
            Mesh->SetRelativeLocation(MeshRelativeLoc);
        }
    }

    // Adjust pawn location to keep feet on ground when standing up
    if (HeightDelta > 0.0f) // Growing taller (standing up)
    {
        // Move the pawn up by half the height change to keep feet in same position
        FMoverDefaultSyncState *DefaultState = OutputState.SyncStateCollection.FindMutableDataByType<
            FMoverDefaultSyncState>();
        if (DefaultState)
        {
            FVector NewLocation = DefaultState->GetLocation_WorldSpace();
            NewLocation.Z += HeightDelta;

            // Use SetTransforms_WorldSpace to update state correctly
            DefaultState->SetTransforms_WorldSpace(
                NewLocation,
                DefaultState->GetOrientation_WorldSpace(),
                DefaultState->GetVelocity_WorldSpace(),
                DefaultState->GetMovementBase(),
                DefaultState->GetMovementBaseBoneName()
                );
        }
    }

    UE_LOG(LogTemp, Log, TEXT("ALS Capsule Size Effect: OldHeight=%.1f, NewHeight=%.1f, Delta=%.1f"),
           OldHalfHeight, TargetHalfHeight, HeightDelta);

    return true; // Effect was successfully applied
}

FInstantMovementEffect *FApplyCapsuleSizeEffect::Clone() const
{
    return new FApplyCapsuleSizeEffect(*this);
}

void FApplyCapsuleSizeEffect::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    Ar << TargetHalfHeight;
    Ar << TargetRadius;

    uint8 AdjustMeshFlag = bAdjustMeshPosition ? 1 : 0;
    Ar << AdjustMeshFlag;
    bAdjustMeshPosition = AdjustMeshFlag != 0;
}

UScriptStruct *FApplyCapsuleSizeEffect::GetScriptStruct() const
{
    return StaticStruct();
}

FString FApplyCapsuleSizeEffect::ToSimpleString() const
{
    return FString::Printf(TEXT("CapsuleSize Height=%.1f Radius=%.1f"), TargetHalfHeight, TargetRadius);
}

//========================================================================
// FAlsApplyCrouchStateEffect Implementation
//========================================================================

FAlsApplyCrouchStateEffect::FAlsApplyCrouchStateEffect()
{
    // Default values set in header
}

bool FAlsApplyCrouchStateEffect::ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams,
                                                     FMoverSyncState &OutputState)
{
    if (!ApplyEffectParams.MoverComp || !ApplyEffectParams.MoverComp->GetOwner())
    {
        return false;
    }

    // For now, we'll handle the visual offset through the mesh component directly
    // This is a temporary solution until we have access to BaseVisualComponentTransform
    APawn *Pawn = ApplyEffectParams.MoverComp->GetOwner<APawn>();
    if (!Pawn)
    {
        return false;
    }

    // The visual offset is already handled by FApplyCapsuleSizeEffect
    // This effect is primarily for future root motion handling
    // When root motion is implemented, we'll need to adjust prediction data here

    UE_LOG(LogTemp, Log,
           TEXT("ALS Crouch State Effect: IsCrouching=%s, HeightDiff=%.1f - Root motion handling placeholder"),
           bIsCrouching ? TEXT("True") : TEXT("False"), HeightDifference);

    return true; // Effect was successfully applied
}

FInstantMovementEffect *FAlsApplyCrouchStateEffect::Clone() const
{
    return new FAlsApplyCrouchStateEffect(*this);
}

void FAlsApplyCrouchStateEffect::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    uint8 CrouchFlag = bIsCrouching ? 1 : 0;
    Ar << CrouchFlag;
    bIsCrouching = CrouchFlag != 0;

    Ar << HeightDifference;
}

UScriptStruct *FAlsApplyCrouchStateEffect::GetScriptStruct() const
{
    return StaticStruct();
}

FString FAlsApplyCrouchStateEffect::ToSimpleString() const
{
    return FString::Printf(TEXT("CrouchState IsCrouching=%s HeightDiff=%.1f"),
                           bIsCrouching ? TEXT("True") : TEXT("False"), HeightDifference);
}

//========================================================================
// FSetLocomotionActionEffect Implementation
//========================================================================

bool FSetLocomotionActionEffect::ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams,
                                                     FMoverSyncState &OutputState)
{
    if (FAlsMoverSyncState *AlsState = OutputState.SyncStateCollection.FindMutableDataByType<FAlsMoverSyncState>())
    {
        AlsState->LocomotionAction = NewLocomotionAction;
        return true;
    }
    return false;
}

FInstantMovementEffect *FSetLocomotionActionEffect::Clone() const
{
    return new FSetLocomotionActionEffect(*this);
}

void FSetLocomotionActionEffect::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);
    Ar << NewLocomotionAction;
}

UScriptStruct *FSetLocomotionActionEffect::GetScriptStruct() const
{
    return StaticStruct();
}

FString FSetLocomotionActionEffect::ToSimpleString() const
{
    return FString::Printf(TEXT("SetLocomotionAction: %s"), *NewLocomotionAction.ToString());
}