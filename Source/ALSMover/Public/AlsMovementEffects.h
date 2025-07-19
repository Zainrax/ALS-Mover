#pragma once

#include "CoreMinimal.h"
#include "InstantMovementEffect.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "AlsMovementEffects.generated.h"

/**
 * Instant movement effect that changes the character's capsule size
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FApplyCapsuleSizeEffect : public FInstantMovementEffect
{
    GENERATED_BODY()

    FApplyCapsuleSizeEffect();

    // Target capsule half height
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float TargetHalfHeight = 92.0f;

    // Target capsule radius (optional, -1 to keep current)
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float TargetRadius = -1.0f;

    // Whether to adjust mesh position to keep feet on ground
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    bool bAdjustMeshPosition = true;

    // FInstantMovementEffect interface
    virtual bool
    ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams, FMoverSyncState &OutputState) override;

    virtual FInstantMovementEffect *Clone() const override;
    virtual void NetSerialize(FArchive &Ar) override;
    virtual UScriptStruct *GetScriptStruct() const override;
    virtual FString ToSimpleString() const override;
};

/**
 * Template trait for type serialization
 */
template <>
struct TStructOpsTypeTraits<FApplyCapsuleSizeEffect> : TStructOpsTypeTraitsBase2<FApplyCapsuleSizeEffect>
{
    enum
    {
        //WithNetSerializer = true,  // Instant effects use custom NetSerialize
        WithCopy = true,
    };
};

/**
 * Instant movement effect that handles root motion crouch state
 * This effect updates the visual component transform to prevent jitter when crouching with root motion
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FAlsApplyCrouchStateEffect : public FInstantMovementEffect
{
    GENERATED_BODY()

    FAlsApplyCrouchStateEffect();

    // Whether we're entering crouch (true) or exiting crouch (false)
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    bool bIsCrouching = true;

    // The height difference for the visual offset calculation
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float HeightDifference = 32.0f;

    // FInstantMovementEffect interface
    virtual bool
    ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams, FMoverSyncState &OutputState) override;

    virtual FInstantMovementEffect *Clone() const override;
    virtual void NetSerialize(FArchive &Ar) override;
    virtual UScriptStruct *GetScriptStruct() const override;
    virtual FString ToSimpleString() const override;
};

/**
 * Instant movement effect that sets the character's locomotion action tag.
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FSetLocomotionActionEffect : public FInstantMovementEffect
{
    GENERATED_BODY()

    FSetLocomotionActionEffect() = default;

    explicit FSetLocomotionActionEffect(const FGameplayTag &InTag) : NewLocomotionAction(InTag)
    {
    }

    // The new tag to set for the locomotion action
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FGameplayTag NewLocomotionAction;

    // FInstantMovementEffect interface
    virtual bool
    ApplyMovementEffect(FApplyMovementEffectParams &ApplyEffectParams, FMoverSyncState &OutputState) override;
    virtual FInstantMovementEffect *Clone() const override;
    virtual void NetSerialize(FArchive &Ar) override;
    virtual UScriptStruct *GetScriptStruct() const override;
    virtual FString ToSimpleString() const override;
};

/**
 * Template trait for type serialization
 */
template <>
struct TStructOpsTypeTraits<FAlsApplyCrouchStateEffect> : TStructOpsTypeTraitsBase2<FAlsApplyCrouchStateEffect>
{
    enum
    {
        //WithNetSerializer = true,  // Instant effects use custom NetSerialize
        WithCopy = true,
    };
};