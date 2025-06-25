#pragma once

#include "CoreMinimal.h"
#include "MovementModifier.h"
#include "GameplayTagContainer.h"
#include "AlsMovementModifiers.generated.h"

/**
 * ALS Gait Modifier - Changes movement speed based on gait (Walk/Run/Sprint)
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FALSGaitModifier : public FMovementModifierBase
{
    GENERATED_BODY()

    FALSGaitModifier();

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FGameplayTag CurrentGait;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FGameplayTag CurrentStance; // Also track stance to apply combined speed

    // Base speed values for each gait
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float WalkSpeed = 175.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float RunSpeed = 375.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float SprintSpeed = 650.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float CrouchSpeedMultiplier = 0.4f; // Applied when crouching

    // FMovementModifierBase interface
    FString GetDisplayName() const { return TEXT("ALS Gait"); }
    virtual void OnPreMovement(UMoverComponent *MoverComp, const FMoverTimeStep &TimeStep) override;
    virtual UScriptStruct *GetScriptStruct() const override { return StaticStruct(); }
    virtual FMovementModifierBase *Clone() const override { return new FALSGaitModifier(*this); }
    virtual void NetSerialize(FArchive &Ar) override;
    virtual FString ToSimpleString() const override;
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