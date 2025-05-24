#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "MovementMode.h"
#include "AlsGroundMovementMode.generated.h"

/**
 * Simple ground movement mode for ALS Mover testing
 * This is a basic implementation to get movement working in the editor
 */
UCLASS(Blueprintable, BlueprintType)
class ALSMOVER_API UAlsGroundMovementMode : public UBaseMovementMode
{
    GENERATED_UCLASS_BODY()

public:
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

protected:
    // UBaseMovementMode interface
    virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
    virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

private:
    float GetMaxSpeed() const;
};