#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "State/AlsControlRigInput.h"
#include "State/AlsCrouchingState.h"
#include "State/AlsDynamicTransitionsState.h"
#include "State/AlsFeetState.h"
#include "State/AlsGroundedState.h"
#include "State/AlsInAirState.h"
#include "State/AlsLayeringState.h"
#include "State/AlsLeanState.h"
#include "State/AlsLocomotionAnimationState.h"
#include "State/AlsLookState.h"
#include "State/AlsMovementBaseState.h"
#include "State/AlsPoseState.h"
#include "State/AlsRagdollingAnimationState.h"
#include "State/AlsRotateInPlaceState.h"
#include "State/AlsSpineState.h"
#include "State/AlsStandingState.h"
#include "State/AlsTransitionsState.h"
#include "State/AlsTurnInPlaceState.h"
#include "State/AlsViewAnimationState.h"
#include "Settings/AlsAnimationInstanceSettings.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsMoverAnimationInstance.generated.h"

class UAlsCharacterMoverComponent;
class UAlsAnimationInstanceSettings;

/**
 * Base animation instance class for ALS Mover characters
 * Provides a clean interface for reading state from the Mover system
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API UAlsMoverAnimationInstance : public UAnimInstance
{
    GENERATED_BODY()

    friend class UAlsMoverLinkedAnimationInstance;

protected:
    UFUNCTION(BlueprintCallable, Category = "ALS Mover Animation")
    UAlsCharacterMoverComponent *GetAlsMoverComponent() const;

    // ---- Settings & State ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
    TObjectPtr<UAlsAnimationInstanceSettings> Settings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
    uint8 bPendingUpdate : 1 {true};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient, Meta = (ClampMin = 0))
    double TeleportedTime{0.0f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
    uint8 bDisplayDebugTraces : 1 {false};

    mutable TArray<TFunction<void()>> DisplayDebugTracesQueue;
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag ViewMode{AlsViewModeTags::ThirdPerson};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag LocomotionMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag RotationMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag Stance;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag Gait;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag OverlayMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag LocomotionAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FGameplayTag GroundedEntryMode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsMovementBaseState MovementBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsLayeringState LayeringState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsPoseState PoseState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsViewAnimationState ViewState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsSpineState SpineState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsLookState LookState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsLocomotionAnimationState LocomotionState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsLeanState LeanState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsGroundedState GroundedState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsStandingState StandingState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsCrouchingState CrouchingState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsInAirState InAirState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsFeetState FeetState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsTransitionsState TransitionsState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsDynamicTransitionsState DynamicTransitionsState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsRotateInPlaceState RotateInPlaceState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsTurnInPlaceState TurnInPlaceState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|ALS", Transient)
    FAlsRagdollingAnimationState RagdollingState;

    // Add for ragdolling snapshot
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
    FPoseSnapshot FinalRagdollPose;

    // Helper boolean variables for common state checks
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Helper", Transient)
    uint8 bIsWalking : 1 {false};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Helper", Transient)
    uint8 bIsRunning : 1 {false};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Helper", Transient)
    uint8 bIsSprinting : 1 {false};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Helper", Transient)
    uint8 bIsCrouching : 1 {false};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Helper", Transient)
    uint8 bIsAiming : 1 {false};

public:
    // Core
    UFUNCTION(BlueprintPure, Category = "ALS|Animation Instance",
        Meta = (BlueprintThreadSafe, ReturnDisplayName = "Setting"))
    UAlsAnimationInstanceSettings *GetSettingsUnsafe() const;

    UFUNCTION(BlueprintPure, Category = "ALS|Animation Instance",
        Meta = (BlueprintThreadSafe, ReturnDisplayName = "Rig Input"))
    FAlsControlRigInput GetControlRigInput() const;

    void MarkPendingUpdate();
    void MarkTeleported();

    // View
    virtual bool IsSpineRotationAllowed();

    // ~Begin UAnimInstance Interface
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual void NativeBeginPlay() override;
    virtual void NativeThreadSafeUpdateAnimation(float DeltaTime) override;
    virtual void NativePostUpdateAnimation();

protected:
    virtual FAnimInstanceProxy *CreateAnimInstanceProxy() override;
    // ~End UAnimInstance Interface

    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void InitializeLook();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshLook();

    // Locomotion & Grounded
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void InitializeLean();
    void SetGroundedEntryMode(const FGameplayTag &NewGroundedEntryMode);
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void ResetGroundedEntryMode();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void InitializeGrounded();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshGrounded();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshGroundedMovement();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void SetHipsDirection(EAlsHipsDirection NewHipsDirection);
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void InitializeStandingMovement();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshStandingMovement();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void ActivatePivot();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void ResetPivot();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshCrouchingMovement();

    // In Air
    void Jump();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshInAir();

    // Transitions
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void PlayQuickStopAnimation();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void PlayTransitionAnimation(UAnimSequenceBase *Sequence, float BlendInDuration = 0.2f,
                                 float BlendOutDuration = 0.2f, float PlayRate = 1.0f, float StartTime = 0.0f,
                                 bool bFromStandingIdleOnly = false);
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void PlayTransitionLeftAnimation(float BlendInDuration = 0.2f, float BlendOutDuration = 0.2f, float PlayRate = 1.0f,
                                     float StartTime = 0.0f, bool bFromStandingIdleOnly = false);

    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void PlayTransitionRightAnimation(float BlendInDuration = 0.2f, float BlendOutDuration = 0.2f,
                                      float PlayRate = 1.0f,
                                      float StartTime = 0.0f, bool bFromStandingIdleOnly = false);
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void StopTransitionAndTurnInPlaceAnimations(float BlendOutDuration = 0.2f);
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshDynamicTransitions();

    // Rotate/Turn In Place
    virtual bool IsRotateInPlaceAllowed();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshRotateInPlace();
    virtual bool IsTurnInPlaceAllowed();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void InitializeTurnInPlace();
    UFUNCTION(BlueprintCallable, Category = "ALS|Animation Instance", Meta = (BlueprintThreadSafe))
    void RefreshTurnInPlace();

    // Ragdolling
    FPoseSnapshot &SnapshotFinalRagdollPose();

    // Utility
    float GetCurveValueClamped01(const FName &CurveName) const;

private:
    void UpdateStateFromMover(float DeltaTime);
    void UpdateHelperVariables();
    void RefreshMovementBaseOnGameThread();
    void RefreshLayering();
    void RefreshPose();
    void RefreshViewOnGameThread();
    void RefreshView(float DeltaTime);
    void RefreshSpine(float SpineBlendAmount, float DeltaTime);
    void RefreshInAirOnGameThread();
    void RefreshFeetOnGameThread();
    void RefreshFeet(float DeltaTime);
    void RefreshTransitions();
    void RefreshRagdollingOnGameThread();
    FVector3f GetRelativeVelocity() const;
    FVector2f GetRelativeAccelerationAmount() const;
    void RefreshVelocityBlend();
    void RefreshGroundedLean();
    void RefreshMovementDirection(float ViewRelativeVelocityYawAngle);
    void RefreshRotationYawOffsets(float ViewRelativeVelocityYawAngle);
    void RefreshGroundPrediction();
    void RefreshInAirLean();
    void RefreshFoot(FAlsFootState &FootState, const FName &IkCurveName, const FName &LockCurveName,
                     const FTransform &ComponentTransformInverse, float DeltaTime) const;
    void ProcessFootLockTeleport(float IkAmount, FAlsFootState &FootState) const;
    void ProcessFootLockBaseChange(float IkAmount, FAlsFootState &FootState,
                                   const FTransform &ComponentTransformInverse) const;
    void RefreshFootLock(float IkAmount, FAlsFootState &FootState, const FName &LockCurveName,
                         const FTransform &ComponentTransformInverse, float DeltaTime) const;
    void PlayQueuedTransitionAnimation();
    void PlayQueuedTurnInPlaceAnimation();
    void StopQueuedTransitionAndTurnInPlaceAnimations();

    UPROPERTY()
    TObjectPtr<UAlsCharacterMoverComponent> CachedMoverComponent = nullptr;
    FVector PreviousLocation{ForceInit};
};

inline UAlsAnimationInstanceSettings *UAlsMoverAnimationInstance::GetSettingsUnsafe() const { return Settings; }
inline void UAlsMoverAnimationInstance::MarkPendingUpdate() { bPendingUpdate |= true; }
inline void UAlsMoverAnimationInstance::MarkTeleported() { TeleportedTime = GetWorld()->GetTimeSeconds(); }

inline void UAlsMoverAnimationInstance::SetGroundedEntryMode(const FGameplayTag &NewGroundedEntryMode)
{
    GroundedEntryMode = NewGroundedEntryMode;
}

inline void UAlsMoverAnimationInstance::ResetGroundedEntryMode() { GroundedEntryMode = FGameplayTag::EmptyTag; }

inline void UAlsMoverAnimationInstance::SetHipsDirection(EAlsHipsDirection NewHipsDirection)
{
    GroundedState.HipsDirection = NewHipsDirection;
}

inline void UAlsMoverAnimationInstance::ResetPivot() { StandingState.bPivotActive = false; }
inline void UAlsMoverAnimationInstance::Jump() { InAirState.bJumpRequested = true; }