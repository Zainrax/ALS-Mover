#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "MovementMode.h"
#include "AlsStateLogicTransition.generated.h"

/**
 * Global transition that handles ALS state changes based on input.
 * This transition manages gait, stance, and other state changes without
 * actually transitioning to a new movement mode.
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API UAlsStateLogicTransition : public UBaseMovementModeTransition
{
    GENERATED_UCLASS_BODY()

public:
    virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Context) const override;

protected:
    // Helper functions for state management
    void EvaluateGaitLogic(const struct FAlsMoverInputs* Inputs, struct FAlsMoverSyncState* SyncState, 
                          class UMoverComponent* MoverComp) const;
    
    void EvaluateStanceLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState, 
                            UMoverComponent* MoverComp) const;
    
    void EvaluateAimingLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState,
                            UMoverComponent* MoverComp) const;
    
    // Helper to check if a modifier is active
    bool IsModifierActive(const UMoverComponent* MoverComp, const FMovementModifierHandle& Handle) const;
};