#include "AlsMoverMovementSettings.h"
#include "Utility/AlsGameplayTags.h"

UAlsMoverMovementSettings::UAlsMoverMovementSettings()
{
    // Set base properties from UCommonLegacyMovementSettings to reasonable ALS defaults
    MaxSpeed = RunSpeed;
    MaxAcceleration = RunAcceleration;
    BrakingDeceleration = BrakingDecelerationRunning;
    GroundFriction = 8.0f;
    JumpUpwardsSpeed = 420.0f; // Example value
}

float UAlsMoverMovementSettings::GetSpeedForGait(const FGameplayTag& GaitTag) const
{
    if (GaitTag == AlsGaitTags::Sprinting)
    {
        return SprintSpeed;
    }
    if (GaitTag == AlsGaitTags::Running)
    {
        return RunSpeed;
    }
    if (GaitTag == AlsGaitTags::Walking)
    {
        return WalkSpeed;
    }
    return RunSpeed; // Default
}

float UAlsMoverMovementSettings::GetAccelerationForGait(const FGameplayTag& GaitTag) const
{
    if (GaitTag == AlsGaitTags::Sprinting)
    {
        return SprintAcceleration;
    }
    if (GaitTag == AlsGaitTags::Running)
    {
        return RunAcceleration;
    }
    if (GaitTag == AlsGaitTags::Walking)
    {
        return WalkAcceleration;
    }
    return RunAcceleration; // Default
}

void UAlsMoverMovementSettings::ApplyGaitSettings(const FGameplayTag& GaitTag, const FGameplayTag& StanceTag)
{
    // This function updates the base UCommonLegacyMovementSettings properties that the default modes use.
    // This is a temporary measure; ideally, the movement modes would read directly from this class.

    float TargetSpeed = GetSpeedForGait(GaitTag);
    float TargetAcceleration = GetAccelerationForGait(GaitTag);
    float TargetDeceleration = BrakingDecelerationRunning;

    if (GaitTag == AlsGaitTags::Walking)
    {
        TargetDeceleration = BrakingDecelerationWalking;
    }

    if (StanceTag == AlsStanceTags::Crouching)
    {
        TargetSpeed *= CrouchSpeedMultiplier;
        TargetAcceleration *= CrouchAccelerationMultiplier;
    }

    // Apply to base class properties
    MaxSpeed = TargetSpeed;
    MaxAcceleration = TargetAcceleration;
    BrakingDeceleration = TargetDeceleration;
}

