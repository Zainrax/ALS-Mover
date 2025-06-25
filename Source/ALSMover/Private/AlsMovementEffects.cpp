#include "AlsMovementEffects.h"
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

bool FApplyCapsuleSizeEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
    if (!ApplyEffectParams.MoverComp || !ApplyEffectParams.MoverComp->GetOwner())
    {
        return false;
    }
    
    APawn* Pawn = ApplyEffectParams.MoverComp->GetOwner<APawn>();
    if (!Pawn)
    {
        return false;
    }
    
    UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent());
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
        if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
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
        FMoverDefaultSyncState* DefaultState = OutputState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
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

FInstantMovementEffect* FApplyCapsuleSizeEffect::Clone() const
{
    return new FApplyCapsuleSizeEffect(*this);
}

void FApplyCapsuleSizeEffect::NetSerialize(FArchive& Ar)
{
    Super::NetSerialize(Ar);
    
    Ar << TargetHalfHeight;
    Ar << TargetRadius;
    
    uint8 AdjustMeshFlag = bAdjustMeshPosition ? 1 : 0;
    Ar << AdjustMeshFlag;
    bAdjustMeshPosition = AdjustMeshFlag != 0;
}

UScriptStruct* FApplyCapsuleSizeEffect::GetScriptStruct() const
{
    return FApplyCapsuleSizeEffect::StaticStruct();
}

FString FApplyCapsuleSizeEffect::ToSimpleString() const
{
    return FString::Printf(TEXT("CapsuleSize Height=%.1f Radius=%.1f"), TargetHalfHeight, TargetRadius);
}