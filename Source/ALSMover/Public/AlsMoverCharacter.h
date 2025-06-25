#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"

// Mover includes
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "AlsCharacterMoverComponent.h"
#include "AlsMoverData.h"
#include "AlsLayeredMoves.h"

// ALS includes for shared types
#include "Utility/AlsGameplayTags.h"

#include "AlsMoverCharacter.generated.h"

/**
 * Enhanced Input Action mappings for ALS character
 */
USTRUCT(BlueprintType)
struct FAlsCharacterInputActions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Move{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Look{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Run{nullptr}; // Sprint action (hold to sprint)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Walk{nullptr}; // Walk toggle (press to toggle walk on/off)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Crouch{nullptr}; // Crouch toggle (press to toggle crouch on/off)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Jump{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Aim{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Roll{nullptr}; // Roll/dodge action

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Mantle{nullptr}; // Mantle/vault action
};

/**
 * ALS character using pure input producer pattern
 * 
 * This character is a pure input producer and does NOT manage any game state.
 * All state is managed by the Mover system through data-driven architecture.
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API AAlsMoverCharacter : public APawn, public IMoverInputProducerInterface
{
    GENERATED_BODY()

public:
    AAlsMoverCharacter(const FObjectInitializer &ObjectInitializer = FObjectInitializer::Get());

protected:
    //~ Begin APawn interface
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent *PlayerInputComponent) override;
    virtual void PossessedBy(AController *NewController) override;
    virtual void UnPossessed() override;
    //~ End APawn interface

    //~ Begin IMoverInputProducerInterface
    virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext &InputCmdResult) override;
    //~ End IMoverInputProducerInterface

public:
    // ---- Component Accessors ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UAlsCharacterMoverComponent *GetCharacterMover() const { return CharacterMover; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    USkeletalMeshComponent *GetMesh() const { return Mesh; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UCapsuleComponent *GetCapsuleComponent() const { return CapsuleComponent; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UAlsCharacterMoverComponent *GetMoverComponent() const { return CharacterMover; }

    // ---- State Accessors (Read-Only) ----
    // These read from the Mover sync state

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetStance() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetGait() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetRotationMode() const;

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    FGameplayTag GetLocomotionMode() const;

    // Debug helpers
    UFUNCTION(BlueprintCallable, Category = "ALS Mover|Debug")
    FString GetDebugInfo() const;

    // Debug functions
    UFUNCTION(Exec, Category = "ALS Mover Debug")
    void ToggleRotationMode();

    // ShowDebug support
    virtual void DisplayDebug(class UCanvas *Canvas, const FDebugDisplayInfo &DebugDisplay, float &YL,
                              float &YPos) override;

    // ---- Input State Getters (for debug) ----

    FVector GetCachedMoveInputVector() const { return CachedMoveInputVector; }
    FVector GetCachedLookInputVector() const { return CachedLookInputVector; }
    bool IsSprinting() const { return bWantsToSprint_Internal; }
    bool IsWalkToggled() const { return bWalkToggled_Internal; }
    bool IsCrouchToggled() const { return bCrouchToggled_Internal; }
    bool IsJumping() const { return bWantsToJump_Internal; }
    bool IsAiming() const { return bWantsToAim_Internal; }
    FRotator GetControlRotation() const;

protected:
    // ---- Core Components ----

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UAlsCharacterMoverComponent> CharacterMover;

    // ---- Movement Settings ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Movement", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<class UAlsMoverMovementSettings> MovementSettings;

    // ---- Input System ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    FAlsCharacterInputActions InputActions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputMappingContext> InputMappingContext;

    // ---- Roll Action Settings ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    TObjectPtr<UAnimMontage> RollMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    float RollDistance = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    float RollDuration = 0.6f;

    // ---- Mantle Action Settings ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    TObjectPtr<UAnimMontage> LowMantleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    TObjectPtr<UAnimMontage> HighMantleMontage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    float MantleTraceDistance = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Actions")
    float LowMantleThreshold = 125.0f;

    // ---- Cached Input State (Internal Only) ----
    // These are NOT replicated and only used for input production

    FVector CachedMoveInputVector{FVector::ZeroVector};
    FVector CachedLookInputVector{FVector::ZeroVector};
    FVector CachedMouseWorldPosition{FVector::ZeroVector};

    // Sprint state (held input)
    uint8 bWantsToSprint_Internal : 1 {false};

    // Toggle states (event-based)
    uint8 bWalkToggled_Internal : 1 {false};
    uint8 bCrouchToggled_Internal : 1 {false};

    // Hold states
    uint8 bWantsToJump_Internal : 1 {false};
    uint8 bWantsToAim_Internal : 1 {false};

    // Action events (consumed in ProduceInput)
    uint8 bWantsToRoll_Internal : 1 {false};
    uint8 bWantsToMantle_Internal : 1 {false};

    // Toggle events (consumed in ProduceInput)
    uint8 bWantsToToggleWalk_Internal : 1 {false};
    uint8 bWantsToToggleCrouch_Internal : 1 {false};
    uint8 bWantsToStartAiming_Internal : 1 {false};
    uint8 bWantsToStopAiming_Internal : 1 {false};

    // Mouse state
    uint8 bHasValidMouseTarget : 1 {false};

    // ---- Input Event Handlers ----
    // These only set internal flags, no state changes

    void OnMoveTriggered(const struct FInputActionValue &Value);
    void OnMoveCompleted(const struct FInputActionValue &Value);
    void OnLookTriggered(const struct FInputActionValue &Value);
    void OnLookCompleted(const struct FInputActionValue &Value);
    void OnRunStarted(const struct FInputActionValue &Value);
    void OnRunCompleted(const struct FInputActionValue &Value);
    void OnWalkStarted(const struct FInputActionValue &Value);
    void OnWalkCompleted(const struct FInputActionValue &Value);
    void OnCrouchStarted(const struct FInputActionValue &Value);
    void OnCrouchCompleted(const struct FInputActionValue &Value);
    void OnJumpStarted(const struct FInputActionValue &Value);
    void OnJumpCompleted(const struct FInputActionValue &Value);
    void OnAimStarted(const struct FInputActionValue &Value);
    void OnAimCompleted(const struct FInputActionValue &Value);
    void OnRollStarted(const struct FInputActionValue &Value);
    void OnRollCompleted(const struct FInputActionValue &Value);
    void OnMantleStarted(const struct FInputActionValue &Value);
    void OnMantleCompleted(const struct FInputActionValue &Value);

    // ---- Helper Functions ----

    void SetupMoverComponent();

    // Action helpers
    FVector CalculateRollDirection() const;
    bool TryFindMantleTarget(FVector &OutStartLocation, FVector &OutTargetLocation, float &OutHeight) const;

    FORCEINLINE bool bHasGamepadInput() const { return !CachedLookInputVector.IsNearlyZero(0.1f); }
};