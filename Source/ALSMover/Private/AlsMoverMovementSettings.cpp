#include "AlsMoverMovementSettings.h"

#include "Utility/AlsGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverMovementSettings)

UAlsMoverMovementSettings::UAlsMoverMovementSettings()
{
    InitializeALSDefaults();
}

void UAlsMoverMovementSettings::InitializeALSDefaults()
{
    // Configure base CommonLegacyMovementSettings with ALS-appropriate defaults

    // Set default to running speed (ALS default gait)
    MaxSpeed = RunSpeed; // 375.0f cm/s

    // ALS uses higher acceleration for responsive movement
    Acceleration = RunAcceleration; // 2000.0f cm/s^2
    Deceleration = BrakingDecelerationRunning; // 2000.0f cm/s^2

    // Ground friction - ALS uses 8.0f for good responsiveness
    GroundFriction = 8.0f;

    // Braking settings for quick stops
    bUseSeparateBrakingFriction = true;
    BrakingFriction = 8.0f;
    BrakingFrictionFactor = 2.0f;

    // Turning - ALS uses responsive turning
    TurningRate = RunTurningRate; // 500.0f degrees/s
    TurningBoost = 8.0f; // Default UE value works well

    // Jump settings - ALS standard jump
    JumpUpwardsSpeed = 420.0f; // Slightly lower than UE default for more realistic feel

    // Step height - ALS characters can step up reasonably high obstacles
    MaxStepHeight = 45.0f; // Slightly higher than default 40cm

    // Floor detection
    FloorSweepDistance = 50.0f; // Slightly longer than default for better ground detection

    // Slope walking - standard ALS slope tolerance
    MaxWalkSlopeCosine = 0.71f; // ~45 degree slopes (cos(45°) ≈ 0.707)
}

float UAlsMoverMovementSettings::GetSpeedForGait(const FGameplayTag &GaitTag) const
{
    if (GaitTag == AlsGaitTags::Walking)
    {
        return WalkSpeed;
    }
    if (GaitTag == AlsGaitTags::Running)
    {
        return RunSpeed;
    }
    if (GaitTag == AlsGaitTags::Sprinting)
    {
        return SprintSpeed;
    }

    // Default to running speed
    return RunSpeed;
}

float UAlsMoverMovementSettings::GetAccelerationForGait(const FGameplayTag &GaitTag) const
{
    if (GaitTag == AlsGaitTags::Walking)
    {
        return WalkAcceleration;
    }
    if (GaitTag == AlsGaitTags::Running)
    {
        return RunAcceleration;
    }
    if (GaitTag == AlsGaitTags::Sprinting)
    {
        return SprintAcceleration;
    }

    // Default to running acceleration
    return RunAcceleration;
}

float UAlsMoverMovementSettings::GetTurningRateForGait(const FGameplayTag &GaitTag) const
{
    if (GaitTag == AlsGaitTags::Walking)
    {
        return WalkTurningRate;
    }
    if (GaitTag == AlsGaitTags::Running)
    {
        return RunTurningRate;
    }
    if (GaitTag == AlsGaitTags::Sprinting)
    {
        return SprintTurningRate;
    }

    // Default to running turning rate
    return RunTurningRate;
}

void UAlsMoverMovementSettings::ApplyGaitSettings(const FGameplayTag &GaitTag, const FGameplayTag &StanceTag)
{
    // Apply speed based on gait and stance
    float TargetSpeed = GetSpeedForGait(GaitTag);

    // Modify speed for crouching stance
    if (StanceTag == AlsStanceTags::Crouching)
    {
        TargetSpeed = CrouchSpeed;
    }

    // Update the base movement settings
    MaxSpeed = TargetSpeed;
    Acceleration = GetAccelerationForGait(GaitTag);
    TurningRate = GetTurningRateForGait(GaitTag);

    // Adjust deceleration based on gait (faster gaits have higher braking)
    if (GaitTag == AlsGaitTags::Walking)
    {
        Deceleration = BrakingDecelerationWalking;
    }
    else
    {
        Deceleration = BrakingDecelerationRunning;
    }
}