#pragma once

#include "CoreMinimal.h"
#include "MoverTypes.h"
#include "GameplayTagContainer.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsMoverData.generated.h"

/**
 * ALS-specific input state for Mover system.
 * This struct represents the player's intent for the current frame.
 * It should only contain raw input data, not game state.
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FAlsMoverInputs : public FMoverDataStructBase
{
    GENERATED_USTRUCT_BODY()

    // Raw input from stick/keys, in local space relative to camera/control rotation.
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FVector MoveInputVector{FVector::ZeroVector};

    // Raw look input from mouse/stick. Used for camera control outside Mover,
    // but can be used to determine look direction for rotation modes.
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FVector LookInputVector{FVector::ZeroVector};
    
    // World space position of the mouse cursor on the ground plane.
    // Only valid if bHasValidMouseTarget is true.
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FVector MouseWorldPosition{FVector::ZeroVector};

    // --- Input Flags ---
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bHasValidMouseTarget : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bIsSprintHeld : 1 {false};

    // Event-like flags (consumed after one frame)
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToToggleWalk : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToToggleCrouch : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToStartAiming : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToStopAiming : 1 {false};
    
    // Action flags (consumed after one frame)
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToRoll : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToMantle : 1 {false};
    
    // Note: Jump is handled by the base FCharacterDefaultInputs

    // Required overrides for Mover
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FAlsMoverInputs(*this); }
    virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
    virtual void ToString(FAnsiStringBuilderBase& Out) const override;
    virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
    virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

/**
 * ALS-specific sync state for networking.
 * This is the canonical, networked state of the character.
 * All state modifications must go through the Mover system.
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FAlsMoverSyncState : public FMoverDataStructBase
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentStance { AlsStanceTags::Standing };

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentGait { AlsGaitTags::Running };

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentRotationMode { AlsRotationModeTags::VelocityDirection };
    
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentLocomotionMode { AlsLocomotionModeTags::Grounded };
    
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentOverlayMode { AlsOverlayModeTags::Default };
    
    // Modifier handles for active modifiers
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FMovementModifierHandle CrouchModifierHandle;
    
    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FMovementModifierHandle AimModifierHandle;
    
    // Required overrides for Mover
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FAlsMoverSyncState(*this); }
    virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
    virtual void ToString(FAnsiStringBuilderBase& Out) const override;
    virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;
    virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;
};

/**
 * Template definitions for type traits required by Mover system
 */
template<>
struct TStructOpsTypeTraits<FAlsMoverInputs> : public TStructOpsTypeTraitsBase2<FAlsMoverInputs>
{
    enum
    {
        WithNetSerializer = true,
        WithCopy = true,
    };
};

template<>
struct TStructOpsTypeTraits<FAlsMoverSyncState> : public TStructOpsTypeTraitsBase2<FAlsMoverSyncState>
{
    enum
    {
        WithNetSerializer = true,
        WithCopy = true,
    };
};