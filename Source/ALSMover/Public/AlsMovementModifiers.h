#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "GameplayTagContainer.h"
#include "AlsMovementModifiers.generated.h"

/**
 * ALS Walk State Modifier - A simple marker modifier that indicates the player wants to walk
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FALSWalkStateModifier : public FMovementModifierBase
{
    GENERATED_BODY()

    FALSWalkStateModifier() 
    { 
        // This modifier should last forever until explicitly cancelled by the player.
        DurationMs = -1.0f; 
    }

    // This modifier has no properties and no logic. Its existence IS the state.

    virtual FMovementModifierBase* Clone() const override { return new FALSWalkStateModifier(*this); }
    virtual UScriptStruct* GetScriptStruct() const override { return FALSWalkStateModifier::StaticStruct(); }
};

/**
 * ALS Sprint State Modifier - A simple marker modifier that indicates the player is sprinting
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FALSSprintStateModifier : public FMovementModifierBase
{
    GENERATED_BODY()

    FALSSprintStateModifier() 
    { 
        // This modifier should last forever until explicitly cancelled.
        DurationMs = -1.0f; 
    }

    // No properties or logic needed. Its existence IS the state.

    virtual FMovementModifierBase* Clone() const override { return new FALSSprintStateModifier(*this); }
    virtual UScriptStruct* GetScriptStruct() const override { return FALSSprintStateModifier::StaticStruct(); }
};

/**
 * ALS Stance Modifier - Changes movement parameters based on stance (Standing/Crouching)
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FALSStanceModifier : public FMovementModifierBase
{
    GENERATED_BODY()

    FALSStanceModifier();

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FGameplayTag CurrentStance;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float CrouchSpeedMultiplier = 0.4f; // Crouching is 40% of base speed

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float CrouchCapsuleHalfHeight = 60.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float StandingCapsuleHalfHeight = 92.0f;

    // Track the base speed from gait modifier
    float BaseSpeedFromGait = 0.0f;

    // FMovementModifierBase interface
    FString GetDisplayName() const { return TEXT("ALS Stance"); }
    virtual void OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
    virtual void OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState) override;
    virtual void OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep) override;
    virtual void OnPostMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep,
                                const FMoverSyncState &SyncState, const FMoverAuxStateContext &AuxState) override;
    virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
    virtual UScriptStruct *GetScriptStruct() const override { return StaticStruct(); }
    virtual FMovementModifierBase *Clone() const override { return new FALSStanceModifier(*this); }
    virtual void NetSerialize(FArchive &Ar) override;
    virtual FString ToSimpleString() const override;
};

/**
 * ALS Rotation Mode Modifier - Changes how character rotates (Velocity Direction vs Looking Direction)
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FALSRotationModeModifier : public FMovementModifierBase
{
    GENERATED_BODY()

    FALSRotationModeModifier();

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FGameplayTag CurrentRotationMode;

    // Rotation settings
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float RotationRate = 720.0f; // Degrees per second

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float AimRotationRate = 360.0f; // Slower rotation when aiming

    // Cached target rotation
    FRotator TargetRotation = FRotator::ZeroRotator;

    // FMovementModifierBase interface
    FString GetDisplayName() const { return TEXT("ALS Rotation Mode"); }
    virtual void OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep) override;
    virtual void OnPostMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep,
                                const FMoverSyncState &SyncState, const FMoverAuxStateContext &AuxState) override;
    virtual UScriptStruct *GetScriptStruct() const override { return StaticStruct(); }
    virtual FMovementModifierBase *Clone() const override { return new FALSRotationModeModifier(*this); }
    virtual void NetSerialize(FArchive &Ar) override;
    virtual FString ToSimpleString() const override;

private:
    void ApplyRotation(APawn *Pawn, const FRotator &TargetRot, float DeltaTime);
};