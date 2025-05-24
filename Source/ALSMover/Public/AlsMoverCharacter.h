#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"

// Mover includes
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"

// ALS includes for shared types
#include "Utility/AlsGameplayTags.h"
#include "State/AlsLocomotionState.h"
#include "AlsCameraComponent.h"

#include "AlsMoverCharacter.generated.h"
/**
 * Enhanced Input Action mappings for ALS character
 */
USTRUCT(BlueprintType)
struct FAlsInputActions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Move{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Look{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Run{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Walk{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Crouch{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Jump{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputAction> Aim{nullptr};
};

/**
 * ALS-specific input state for Mover system
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FAlsMoverInputs : public FMoverDataStructBase
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FVector MoveInputVector{FVector::ZeroVector};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FVector LookInputVector{FVector::ZeroVector};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToRun : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToWalk : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToCrouch : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToJump : 1 {false};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    uint8 bWantsToAim : 1 {false};

    // FMoverDataStructBase interface
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FAlsMoverInputs(*this); }
    virtual bool NetSerialize(FArchive &Ar, class UPackageMap *Map, bool &bOutSuccess) override;
    virtual void ToString(FAnsiStringBuilderBase &Out) const override;
    virtual bool ShouldReconcile(const FMoverDataStructBase &AuthorityState) const override;
    virtual void Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct) override;
};

/**
 * ALS-specific sync state for networking
 */
USTRUCT(BlueprintType)
struct ALSMOVER_API FAlsMoverSyncState : public FMoverDataStructBase
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentStance{AlsStanceTags::Standing};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentGait{AlsGaitTags::Running};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentRotationMode{AlsRotationModeTags::VelocityDirection};

    UPROPERTY(BlueprintReadWrite, Category = "ALS Mover")
    FGameplayTag CurrentLocomotionMode{AlsLocomotionModeTags::Grounded};

    // FMoverDataStructBase interface
    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    virtual FMoverDataStructBase* Clone() const override { return new FAlsMoverSyncState(*this); }
    virtual bool NetSerialize(FArchive &Ar, class UPackageMap *Map, bool &bOutSuccess) override;
    virtual void ToString(FAnsiStringBuilderBase &Out) const override;
    virtual bool ShouldReconcile(const FMoverDataStructBase &AuthorityState) const override;
    virtual void Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct) override;
};

/**
 * Base ALS character using the new Mover plugin architecture
 * 
 * This character provides the foundation for ALS locomotion features
 * while leveraging the modular Mover system for movement simulation.
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

    // Native input production override point
    virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext &InputCmdResult);

public:
    // ---- Component Accessors ----

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UCharacterMoverComponent *GetCharacterMover() const { return CharacterMover; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    USkeletalMeshComponent *GetMesh() const { return Mesh; }

    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UCapsuleComponent *GetCapsuleComponent() const { return CapsuleComponent; }
    
    UFUNCTION(BlueprintPure, Category = "ALS Mover")
    UCharacterMoverComponent* GetMoverComponent() const { return CharacterMover; }

    // Camera component getter removed - camera temporarily disabled
    // UFUNCTION(BlueprintPure, Category = "ALS Camera")
    // UAlsCameraComponent* GetCameraComponent() const { return CameraComponent; }

    // ---- State Accessors ----

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

    // ---- State Setters ----

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetStance(const FGameplayTag &NewStance);

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetGait(const FGameplayTag &NewGait);

    UFUNCTION(BlueprintCallable, Category = "ALS Mover")
    void SetRotationMode(const FGameplayTag &NewRotationMode);
    
    // ---- Input Setters (for Controller) ----
    
    void SetCachedMoveInput(const FVector& MoveInput) { CachedMoveInputVector = MoveInput; }
    void SetCachedLookInput(const FVector& LookInput) { CachedLookInputVector = LookInput; }
    void SetCachedJumpInput(bool bWantsToJump) { bCachedWantsToJump = bWantsToJump; }
    void SetCachedSprintInput(bool bWantsToSprint) { bCachedWantsToRun = bWantsToSprint; }
    void SetCachedWalkInput(bool bWantsToWalk) { bCachedWantsToWalk = bWantsToWalk; }
    void SetCachedCrouchInput(bool bWantsToCrouch) { bCachedWantsToCrouch = bWantsToCrouch; }
    void SetCachedAimInput(bool bWantsToAim) { bCachedWantsToAim = bWantsToAim; }

protected:
    // ---- Core Components ----

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USkeletalMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Mover", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCharacterMoverComponent> CharacterMover;

    // Camera component temporarily removed - ALS Camera requires ACharacter owner
    // TODO: Implement proper camera solution for Pawn-based character
    // TObjectPtr<UAlsCameraComponent> CameraComponent;

    // ---- Input System ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    FAlsInputActions InputActions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ALS Input")
    TObjectPtr<class UInputMappingContext> InputMappingContext;

    // ---- Cached Input State ----
    FVector CachedMoveInputVector{FVector::ZeroVector};
    FVector CachedLookInputVector{FVector::ZeroVector};
    uint8 bCachedWantsToRun : 1 {false};
    uint8 bCachedWantsToWalk : 1 {false};
    uint8 bCachedWantsToCrouch : 1 {false};
    uint8 bCachedWantsToJump : 1 {false};
    uint8 bCachedWantsToAim : 1 {false};

    // ---- Input Event Handlers ----

    void OnMoveTriggered(const struct FInputActionValue &Value);
    void OnLookTriggered(const struct FInputActionValue &Value);
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

    // ---- Helper Functions ----

    void SetupMoverComponent();
    void UpdateGaitFromInput();
    FAlsMoverSyncState *GetAlsSyncState() const;
    bool SetAlsSyncState(const FAlsMoverSyncState &NewState);
};