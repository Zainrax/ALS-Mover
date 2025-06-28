#include "AlsMovementModifiers.h"

#include "AlsMovementEffects.h"
#include "AlsMoverData.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsMoverCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMovementModifiers)

//========================================================================
// FALSStanceModifier Implementation
//========================================================================

FALSStanceModifier::FALSStanceModifier()
{
    CurrentStance = AlsStanceTags::Standing; // Default to standing
}

void FALSStanceModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }

    // This modifier is now active. Set the persistent state tag.
    if (FAlsMoverSyncState* AlsState = const_cast<FMoverSyncState&>(SyncState).SyncStateCollection.FindMutableDataByType<FAlsMoverSyncState>())
    {
        AlsState->CurrentStance = AlsStanceTags::Crouching;
    }

    // Apply the immediate capsule size change via an Instant Effect
    TSharedPtr<FApplyCapsuleSizeEffect> CapsuleEffect = MakeShared<FApplyCapsuleSizeEffect>();
    CapsuleEffect->TargetHalfHeight = this->CrouchCapsuleHalfHeight;
    CapsuleEffect->bAdjustMeshPosition = true;
    MoverComp->QueueInstantMovementEffect(CapsuleEffect);
}

void FALSStanceModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }
    
    // This modifier is being removed. Revert the persistent state tag.
    if (FAlsMoverSyncState* AlsState = const_cast<FMoverSyncState&>(SyncState).SyncStateCollection.FindMutableDataByType<FAlsMoverSyncState>())
    {
        AlsState->CurrentStance = AlsStanceTags::Standing;
    }

    // Apply the immediate capsule size change back to standing
    TSharedPtr<FApplyCapsuleSizeEffect> StandEffect = MakeShared<FApplyCapsuleSizeEffect>();
    StandEffect->TargetHalfHeight = this->StandingCapsuleHalfHeight;
    StandEffect->bAdjustMeshPosition = true;
    MoverComp->QueueInstantMovementEffect(StandEffect);
}

bool FALSStanceModifier::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
    if (CurrentStance == AlsStanceTags::Crouching)
    {
        if (bExactMatch)
        {
            return TagToFind.MatchesTagExact(Mover_IsCrouching);
        }
        return TagToFind.MatchesTag(Mover_IsCrouching);
    }
    return false;
}

void FALSStanceModifier::OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep)
{
    // No pre-movement processing needed
}

void FALSStanceModifier::OnPostMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep,
                                        const FMoverSyncState &SyncState, const FMoverAuxStateContext &AuxState)
{
    FMovementModifierBase::OnPostMovement(MoverComp, TimeStep, SyncState, AuxState);
}

void FALSStanceModifier::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    if (Ar.IsSaving())
    {
        FString TagString = CurrentStance.ToString();
        Ar << TagString;
    }
    else
    {
        FString TagString;
        Ar << TagString;
        CurrentStance = FGameplayTag::RequestGameplayTag(*TagString);
    }

    Ar << CrouchSpeedMultiplier;
    Ar << CrouchCapsuleHalfHeight;
    Ar << StandingCapsuleHalfHeight;
}

FString FALSStanceModifier::ToSimpleString() const
{
    return FString::Printf(TEXT("Stance=%s CrouchSpeed=%.0f%% CrouchHeight=%.0f"),
                           *CurrentStance.ToString(), CrouchSpeedMultiplier * 100.0f, CrouchCapsuleHalfHeight);
}

//========================================================================
// FALSRotationModeModifier Implementation
//========================================================================

FALSRotationModeModifier::FALSRotationModeModifier()
{
    CurrentRotationMode = AlsRotationModeTags::ViewDirection; // Default to view direction for top-down
}

void FALSRotationModeModifier::OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }

    // Get the target rotation from the character
    if (AAlsMoverCharacter *AlsChar = Cast<AAlsMoverCharacter>(MoverComp->GetOwner()))
    {
        TargetRotation = AlsChar->GetControlRotation();
    }
}

void FALSRotationModeModifier::OnPostMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep,
                                              const FMoverSyncState &SyncState, const FMoverAuxStateContext &AuxState)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }

    APawn *Pawn = Cast<APawn>(MoverComp->GetOwner());
    if (!Pawn)
    {
        return;
    }

    // Apply rotation smoothly
    const float DeltaTime = TimeStep.StepMs * 0.001f;
    ApplyRotation(Pawn, TargetRotation, DeltaTime);
}

// Calculation methods removed - now handled in AAlsMoverCharacter

void FALSRotationModeModifier::ApplyRotation(APawn *Pawn, const FRotator &TargetRot, float DeltaTime)
{
    if (!Pawn)
    {
        return;
    }

    FRotator CurrentRotation = Pawn->GetActorRotation();

    // Determine rotation rate based on mode
    float CurrentRotationRate = RotationRate;
    if (CurrentRotationMode == AlsRotationModeTags::Aiming)
    {
        CurrentRotationRate = AimRotationRate;
    }

    // Interpolate rotation
    FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRot, DeltaTime, CurrentRotationRate / 360.0f);

    // Apply the rotation
    Pawn->SetActorRotation(NewRotation);

#if WITH_EDITOR
    // Debug visualization - set to true to enable debug drawing
    static bool bShowRotationDebug = false;
    if (bShowRotationDebug)
    {
        // Get eye height for debug visualization
        float EyeHeight = 150.0f; // Default
        if (const AAlsMoverCharacter *AlsChar = Cast<AAlsMoverCharacter>(Pawn))
        {
            if (const UCapsuleComponent *Capsule = AlsChar->GetCapsuleComponent())
            {
                EyeHeight = Capsule->GetScaledCapsuleHalfHeight() * 1.6f;
            }
        }

        FVector StartPos = Pawn->GetActorLocation();
        FVector EyePos = StartPos + FVector(0, 0, EyeHeight);

        // Draw target rotation direction from eye level
        FVector EndPos = EyePos + TargetRot.Vector() * 200.0f;
        DrawDebugLine(Pawn->GetWorld(), EyePos, EndPos, FColor::Red, false, -1.0f, 0, 2.0f);

        // Draw current rotation direction from eye level
        EndPos = EyePos + NewRotation.Vector() * 150.0f;
        DrawDebugLine(Pawn->GetWorld(), EyePos, EndPos, FColor::Green, false, -1.0f, 0, 2.0f);

        // Draw a sphere at eye level
        DrawDebugSphere(Pawn->GetWorld(), EyePos, 5.0f, 8, FColor::Yellow, false, -1.0f, 0, 1.0f);

        // Draw ground projection lines
        DrawDebugLine(Pawn->GetWorld(), StartPos, EyePos, FColor::Blue, false, -1.0f, 0, 1.0f);
    }
#endif
}

void FALSRotationModeModifier::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    if (Ar.IsSaving())
    {
        FString TagString = CurrentRotationMode.ToString();
        Ar << TagString;
    }
    else
    {
        FString TagString;
        Ar << TagString;
        CurrentRotationMode = FGameplayTag::RequestGameplayTag(*TagString);
    }

    Ar << RotationRate;
    Ar << AimRotationRate;
    Ar << TargetRotation;
}

FString FALSRotationModeModifier::ToSimpleString() const
{
    return FString::Printf(TEXT("RotationMode=%s Rate=%.0f"), *CurrentRotationMode.ToString(), RotationRate);
}