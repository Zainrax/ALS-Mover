#pragma once

#include "CoreMinimal.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "GameplayTagContainer.h"
#include "AlsCharacterMoverComponent.generated.h"

struct FALSGaitModifier;
struct FALSStanceModifier;
struct FALSRotationModeModifier;

/**
 * ALS-specific Character Mover Component
 * Extends the base CharacterMoverComponent to handle ALS movement mechanics
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ALSMOVER_API UAlsCharacterMoverComponent : public UMoverComponent
{
    GENERATED_BODY()

public:
    UAlsCharacterMoverComponent();

protected:
    // ~Begin UMoverComponent Interface
    virtual void BeginPlay() override;
    // ~End UMoverComponent Interface

    // Override the pre-simulation tick to handle ALS-specific logic
    UFUNCTION()
    virtual void OnMoverPreSimulationTick(const FMoverTimeStep &TimeStep, const FMoverInputCmdContext &InputCmd);

    // ALS Jump functionality (from CharacterMoverComponent)
    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    virtual bool Jump();

public:
    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool CanActorJump() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool IsOnGround() const;
    // ---- ALS State Management ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetCurrentGait() const { return CurrentGait; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetCurrentStance() const { return CurrentStance; }

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetGait(const FGameplayTag &NewGait);

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetStance(const FGameplayTag &NewStance);

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetCurrentRotationMode() const { return CurrentRotationMode; }

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetRotationMode(const FGameplayTag &NewRotationMode);

    // ---- Sync State Accessors (for Animation) ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetSyncStateGait() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetSyncStateStance() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetSyncStateRotationMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetSyncStateLocomotionMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetSyncStateOverlayMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    float GetSpeed() const;

    FVector GetVelocity() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FVector GetAcceleration() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    bool IsMoving(float Threshold = 10.0f) const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    bool HasMovementInput(float Threshold = 0.1f) const;

    // Get the ALS movement settings
    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    const class UAlsMoverMovementSettings *GetAlsMovementSettings() const;

protected:
    // Current ALS states
    FGameplayTag CurrentGait;
    FGameplayTag CurrentStance;
    FGameplayTag CurrentRotationMode;

    // Movement modifier handles
    FMovementModifierHandle StanceModifierHandle;
    FMovementModifierHandle RotationModifierHandle;

    // Cached state for change detection
    FGameplayTag CachedGait;
    FGameplayTag CachedStance;
    FGameplayTag CachedRotationMode;

private:
    // Modifier management functions
    void ManageStanceModifier();
    void ManageRotationModifier();
};