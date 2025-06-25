#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "AlsMoverMovementSettings.generated.h"

/**
 * ALS Mover-specific movement settings that configure CommonLegacyMovementSettings with proper ALS values
 * This class provides reasonable defaults based on the original ALS movement parameters
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API UAlsMoverMovementSettings : public UCommonLegacyMovementSettings
{
    GENERATED_BODY()

public:
    UAlsMoverMovementSettings();

    // ALS-specific gait speed settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Gait Speeds", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
    float WalkSpeed = 175.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Gait Speeds", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
    float RunSpeed = 375.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Gait Speeds", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
    float SprintSpeed = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Gait Speeds")
    float CrouchSpeedMultiplier = 0.4f;

    // ALS-specific movement parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
    float WalkAcceleration = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
    float RunAcceleration = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
    float SprintAcceleration = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float CrouchAccelerationMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
    float BrakingDecelerationWalking = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s^2"))
    float BrakingDecelerationRunning = 2000.0f;

    // Capsule sizes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Capsule")
    float StandingCapsuleHalfHeight = 92.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Capsule")
    float CrouchingCapsuleHalfHeight = 60.0f;

    // ALS rotation settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "degrees/s"))
    float RotationRate = 720.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "degrees/s"))
    float AimRotationRate = 360.0f;

    // Helper functions to get gait-specific values
    UFUNCTION(BlueprintCallable, Category = "ALS Movement")
    float GetSpeedForGait(const FGameplayTag& GaitTag) const;

    UFUNCTION(BlueprintCallable, Category = "ALS Movement")
    float GetAccelerationForGait(const FGameplayTag& GaitTag) const;


    // Configure the base CommonLegacyMovementSettings based on current gait
    UFUNCTION(BlueprintCallable, Category = "ALS Movement")
    void ApplyGaitSettings(const FGameplayTag& GaitTag, const FGameplayTag& StanceTag);

private:
    void InitializeALSDefaults();
};