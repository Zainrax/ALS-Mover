#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "MoverTypes.h"
#include "Engine/EngineTypes.h"
#include "AlsLayeredMoves.generated.h"

/**
 * ALS Roll layered move - performs a roll/dodge action
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FLayeredMove_AlsRoll : public FLayeredMoveBase
{
    GENERATED_BODY()

    FLayeredMove_AlsRoll();

    // Roll parameters
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float RollDistance = 200.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float RollDuration = 0.6f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FVector RollDirection = FVector::ForwardVector;

    // Animation to play
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    TObjectPtr<UAnimMontage> RollMontage = nullptr;

    // Whether to use root motion from animation
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    bool bUseRootMotion = true;

    // Play rate for the montage
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float PlayRate = 1.0f;

    // FLayeredMoveBase interface
    virtual bool GenerateMove(const FMoverTickStartData &StartState, const FMoverTimeStep &TimeStep,
                              const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard,
                              FProposedMove &OutProposedMove) override;

    virtual void OnStart(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard) override;
    virtual void OnEnd(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard, float CurrentSimTimeMs) override;

    virtual FLayeredMoveBase *Clone() const override;
    virtual void NetSerialize(FArchive &Ar) override;
    virtual UScriptStruct *GetScriptStruct() const override;
    virtual FString ToSimpleString() const override;
    virtual bool IsFinished(float CurrentSimTimeMs) const override;

protected:
    // Track animation playback
    float AnimStartTime = 0.0f;
    bool bIsPlaying = false;

    // Root motion tracking
    float PreviousMontagePosition = 0.0f;
    float CurrentMontagePosition = 0.0f;
};

/**
 * ALS Mantle layered move - performs mantling/vaulting over obstacles
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FLayeredMove_AlsMantle : public FLayeredMoveBase
{
    GENERATED_BODY()

    FLayeredMove_AlsMantle();

    // Mantle parameters
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FVector MantleStartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    FVector MantleTargetLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float MantleHeight = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float MantleDuration = 1.0f;

    // Animation parameters
    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    TObjectPtr<UAnimMontage> LowMantleMontage = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    TObjectPtr<UAnimMontage> HighMantleMontage = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "ALS Movement")
    float LowMantleThreshold = 125.0f; // Height threshold for low vs high mantle

    // FLayeredMoveBase interface
    virtual bool GenerateMove(const FMoverTickStartData &StartState, const FMoverTimeStep &TimeStep,
                              const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard,
                              FProposedMove &OutProposedMove) override;

    virtual void OnStart(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard) override;
    virtual void OnEnd(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard, float CurrentSimTimeMs) override;

    virtual FLayeredMoveBase *Clone() const override;
    virtual void NetSerialize(FArchive &Ar) override;
    virtual UScriptStruct *GetScriptStruct() const override;
    virtual FString ToSimpleString() const override;
    virtual bool IsFinished(float CurrentSimTimeMs) const override;

protected:
    // Track mantle progress
    float MantleStartTime = 0.0f;
    bool bIsPerformingMantle = false;

    // Helper to interpolate position during mantle
    FVector InterpolateMantlePosition(float Alpha) const;
};

/**
 * Template definitions for type traits
 */
template <>
struct TStructOpsTypeTraits<FLayeredMove_AlsRoll> : TStructOpsTypeTraitsBase2<FLayeredMove_AlsRoll>
{
    enum
    {
        //WithNetSerializer = true,  // Layered moves use custom NetSerialize
        WithCopy = true,
    };
};

template <>
struct TStructOpsTypeTraits<FLayeredMove_AlsMantle> : TStructOpsTypeTraitsBase2<FLayeredMove_AlsMantle>
{
    enum
    {
        //WithNetSerializer = true,  // Layered moves use custom NetSerialize
        WithCopy = true,
    };
};