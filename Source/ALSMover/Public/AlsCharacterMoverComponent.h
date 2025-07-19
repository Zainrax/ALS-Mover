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
    virtual bool DoJump();

public:
    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool CanJump() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool IsOnGround() const;

    // ---- Sync State Accessors (for Animation) ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetGait() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetStance() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetRotationMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetLocomotionMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetOverlayMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|Movement")
    float GetSpeed() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|Movement")
    FVector GetAcceleration() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FRotator GetViewRotation() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|Movement")
    float GetYawSpeed() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|Movement")
    float GetVelocityYawAngle() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|Movement")
    FVector GetRelativeVelocity() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    bool IsMoving(float Threshold = 10.0f) const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    bool HasMovementInput(float Threshold = 0.1f) const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetViewMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover|State")
    FGameplayTag GetLocomotionAction() const;

    // Get the ALS movement settings
    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    const class UAlsMoverMovementSettings *GetAlsMovementSettings() const;
};