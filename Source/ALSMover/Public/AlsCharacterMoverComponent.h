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

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool CanActorJump() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    virtual bool IsOnGround() const;

    // Whether this component should directly handle jumping or not 
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ALS Mover")
    bool bHandleJump = true;

public:
    // ---- ALS State Management ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetCurrentGait() const { return CurrentGait; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetCurrentStance() const { return CurrentStance; }

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetGait(const FGameplayTag &NewGait);

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetStance(const FGameplayTag &NewStance);

    // ---- ALS Movement Settings ----

    // Base movement speeds for each gait
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Speeds")
    float WalkSpeed = 175.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Speeds")
    float RunSpeed = 375.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Speeds")
    float SprintSpeed = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Speeds")
    float CrouchSpeedMultiplier = 0.4f;

    // Acceleration values for each gait
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Acceleration")
    float WalkAcceleration = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Acceleration")
    float RunAcceleration = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Acceleration")
    float SprintAcceleration = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Acceleration")
    float CrouchAccelerationMultiplier = 0.5f;

    // Capsule sizes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Capsule")
    float StandingCapsuleHalfHeight = 92.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Movement|Capsule")
    float CrouchingCapsuleHalfHeight = 60.0f;

protected:
    // Current ALS states
    FGameplayTag CurrentGait;
    FGameplayTag CurrentStance;

    // Movement modifier handles
    FMovementModifierHandle GaitModifierHandle;
    FMovementModifierHandle StanceModifierHandle;

    // Cached state for change detection
    FGameplayTag CachedGait;
    FGameplayTag CachedStance;

private:
    // Modifier management functions
    void ManageGaitModifier();
    void ManageStanceModifier();
};