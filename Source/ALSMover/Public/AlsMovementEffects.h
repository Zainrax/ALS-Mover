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