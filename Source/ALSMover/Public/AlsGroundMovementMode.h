#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "AlsGroundMovementMode.generated.h"

/**
 * ALS-specific ground movement mode that extends the default walking mode
 * Inherits all the complex ground movement logic (floor detection, step-up, sliding)
 * while adding ALS-specific rotation and gait handling
 */
UCLASS(Blueprintable, BlueprintType)
class ALSMOVER_API UAlsGroundMovementMode : public UWalkingMode
{
    GENERATED_UCLASS_BODY()
    // Movement parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float MaxWalkSpeed = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float MaxRunSpeed = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float MaxSprintSpeed = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float GroundFriction = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement")
    float BrakingDeceleration = 2000.0f;

    // Rotation parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation")
    float BaseRotationRate = 720.0f; // Base turn rate in degrees/second

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation")
    float SprintRotationMultiplier = 0.4f; // 40% speed when sprinting

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation")
    float WalkRotationMultiplier = 0.85f; // 85% speed when walking

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Rotation")
    float CrouchRotationMultiplier = 0.65f; // 65% speed when crouching

protected:
    // UBaseMovementMode interface
    virtual void GenerateMove_Implementation(const FMoverTickStartData &StartState, const FMoverTimeStep &TimeStep,
                                             FProposedMove &OutProposedMove) const override;

private:
    float GetMaxSpeed() const;
    float CalculateRotationRate(const struct FMoverDefaultSyncState *MoverState,
                                const struct FAlsMoverSyncState *AlsState, float AngleDelta) const;
    FRotator CalculateTargetRotation(const struct FAlsMoverInputs *AlsInputs,
                                     const FMoverDefaultSyncState *MoverState) const;
};