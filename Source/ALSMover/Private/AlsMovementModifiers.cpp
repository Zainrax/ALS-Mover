#include "AlsMovementModifiers.h"
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
// FALSGaitModifier Implementation
//========================================================================

FALSGaitModifier::FALSGaitModifier()
{
    CurrentGait = AlsGaitTags::Running; // Default to running
    CurrentStance = AlsStanceTags::Standing; // Default to standing
}

void FALSGaitModifier::OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep)
{
    if (!MoverComp)
    {
        return;
    }

    // Get the mutable movement settings
    UCommonLegacyMovementSettings *Settings = MoverComp->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();

    if (Settings)
    {
        // Apply speed based on current gait
        float TargetSpeed = RunSpeed; // Default

        if (CurrentGait == AlsGaitTags::Walking)
        {
            TargetSpeed = WalkSpeed;
        }
        else if (CurrentGait == AlsGaitTags::Running)
        {
            TargetSpeed = RunSpeed;
        }
        else if (CurrentGait == AlsGaitTags::Sprinting)
        {
            TargetSpeed = SprintSpeed;
        }

        // Apply crouch multiplier if crouching
        if (CurrentStance == AlsStanceTags::Crouching)
        {
            TargetSpeed *= CrouchSpeedMultiplier;
        }

        // Apply the final speed
        Settings->MaxSpeed = TargetSpeed;

        // Also adjust acceleration/deceleration based on gait and stance
        if (CurrentStance == AlsStanceTags::Crouching)
        {
            Settings->Acceleration = 1000.0f;
            Settings->Deceleration = 1000.0f;
            Settings->TurningRate = 360.0f;
        }
        else if (CurrentGait == AlsGaitTags::Walking)
        {
            Settings->Acceleration = 1500.0f;
            Settings->Deceleration = 1500.0f;
            Settings->TurningRate = 360.0f;
        }
        else if (CurrentGait == AlsGaitTags::Running)
        {
            Settings->Acceleration = 2000.0f;
            Settings->Deceleration = 2000.0f;
            Settings->TurningRate = 500.0f;
        }
        else if (CurrentGait == AlsGaitTags::Sprinting)
        {
            Settings->Acceleration = 2500.0f;
            Settings->Deceleration = 2000.0f;
            Settings->TurningRate = 300.0f; // Slower turning at high speed
        }
    }
}

void FALSGaitModifier::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    if (Ar.IsSaving())
    {
        FString GaitString = CurrentGait.ToString();
        Ar << GaitString;
        FString StanceString = CurrentStance.ToString();
        Ar << StanceString;
    }
    else
    {
        FString GaitString;
        Ar << GaitString;
        CurrentGait = FGameplayTag::RequestGameplayTag(*GaitString);
        FString StanceString;
        Ar << StanceString;
        CurrentStance = FGameplayTag::RequestGameplayTag(*StanceString);
    }

    Ar << WalkSpeed;
    Ar << RunSpeed;
    Ar << SprintSpeed;
    Ar << CrouchSpeedMultiplier;
}

FString FALSGaitModifier::ToSimpleString() const
{
    return FString::Printf(TEXT("Gait=%s Walk=%.0f Run=%.0f Sprint=%.0f"),
                           *CurrentGait.ToString(), WalkSpeed, RunSpeed, SprintSpeed);
}

//========================================================================
// FALSStanceModifier Implementation
//========================================================================

FALSStanceModifier::FALSStanceModifier()
{
    CurrentStance = AlsStanceTags::Standing; // Default to standing
}

void FALSStanceModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
    // Called when the stance modifier is activated
    // The main logic is in OnPostMovement for continuous adjustment
}

void FALSStanceModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }

    // When the modifier ends (e.g., uncrouching), ensure character is fully standing
    if (APawn* Pawn = Cast<APawn>(MoverComp->GetOwner()))
    {
        if (UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
        {
            // Instantly set to standing height
            Capsule->SetCapsuleHalfHeight(StandingCapsuleHalfHeight);

            // Adjust mesh position to keep feet on ground
            if (USkeletalMeshComponent* Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
            {
                FVector MeshRelativeLoc = Mesh->GetRelativeLocation();
                MeshRelativeLoc.Z = -StandingCapsuleHalfHeight;
                Mesh->SetRelativeLocation(MeshRelativeLoc);
            }
        }
    }
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
    // Speed is now handled by the gait modifier which also considers stance
    // This modifier only needs to handle capsule size changes in OnPostMovement
}

void FALSStanceModifier::OnPostMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep,
                                        const FMoverSyncState &SyncState, const FMoverAuxStateContext &AuxState)
{
    if (!MoverComp || !MoverComp->GetOwner())
    {
        return;
    }

    // Handle capsule size changes
    if (APawn *Pawn = Cast<APawn>(MoverComp->GetOwner()))
    {
        if (UCapsuleComponent *Capsule = Cast<UCapsuleComponent>(Pawn->GetRootComponent()))
        {
            float TargetHalfHeight = (CurrentStance == AlsStanceTags::Crouching)
                                         ? CrouchCapsuleHalfHeight
                                         : StandingCapsuleHalfHeight;

            // Smooth capsule height transition
            float CurrentHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
            if (!FMath::IsNearlyEqual(CurrentHalfHeight, TargetHalfHeight, 0.1f))
            {
                float NewHeight = FMath::FInterpTo(CurrentHalfHeight, TargetHalfHeight,
                                                   TimeStep.StepMs * 0.001f, 10.0f);
                Capsule->SetCapsuleHalfHeight(NewHeight);

                // Adjust mesh position to keep feet on ground
                if (USkeletalMeshComponent *Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
                {
                    FVector MeshRelativeLoc = Mesh->GetRelativeLocation();
                    MeshRelativeLoc.Z = -NewHeight;
                    Mesh->SetRelativeLocation(MeshRelativeLoc);
                }
            }
        }
    }
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