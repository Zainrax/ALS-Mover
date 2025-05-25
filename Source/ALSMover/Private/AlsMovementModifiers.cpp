#include "AlsMovementModifiers.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Utility/AlsGameplayTags.h"

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
    CurrentRotationMode = AlsRotationModeTags::VelocityDirection; // Default to velocity direction
}

void FALSRotationModeModifier::OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep)
{
    if (!MoverComp)
    {
    }

    // Handle rotation mode logic here
    // - VelocityDirection: Character faces movement direction
    // - LookingDirection: Character faces aim/camera direction
    // - Aiming: Strafe movement with looking direction
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
}

FString FALSRotationModeModifier::ToSimpleString() const
{
    return FString::Printf(TEXT("RotationMode=%s"), *CurrentRotationMode.ToString());
}