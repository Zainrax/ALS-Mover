#include "AlsStateLogicTransition.h"
#include "AlsMoverData.h"
#include "AlsMovementModifiers.h"
#include "AlsMovementEffects.h"
#include "MoverComponent.h"
#include "Utility/AlsGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsStateLogicTransition)

UAlsStateLogicTransition::UAlsStateLogicTransition(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

FTransitionEvalResult UAlsStateLogicTransition::Evaluate_Implementation(const FSimulationTickParams& Context) const
{
    // Get ALS-specific input and sync state
    const FAlsMoverInputs* AlsInputs = Context.StartState.InputCmd.InputCollection.FindDataByType<FAlsMoverInputs>();
    FAlsMoverSyncState* AlsSyncState = Context.StartState.SyncState.SyncStateCollection.FindMutableDataByType<FAlsMoverSyncState>();
    
    if (!AlsInputs || !AlsSyncState)
    {
        return FTransitionEvalResult::NoTransition;
    }

    UMoverComponent* MoverComp = GetMoverComponent();
    if (!MoverComp)
    {
        return FTransitionEvalResult::NoTransition;
    }

    // Evaluate state logic - order matters!
    EvaluateGaitLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateStanceLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateRotationModeLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateAimingLogic(AlsInputs, AlsSyncState, MoverComp);

    // This transition doesn't change movement modes, only modifies state
    return FTransitionEvalResult::NoTransition;
}

void UAlsStateLogicTransition::EvaluateGaitLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState, 
                                                 UMoverComponent* MoverComp) const
{
    // Determine target gait based on input
    FGameplayTag TargetGait = AlsGaitTags::Running; // Default

    bool bHasMovementInput = !Inputs->MoveInputVector.IsNearlyZero(0.1f);
    
    if (!bHasMovementInput)
    {
        // No movement - maintain current gait but could transition to idle
        return;
    }

    // Sprint takes priority
    if (Inputs->bIsSprintHeld)
    {
        TargetGait = AlsGaitTags::Sprinting;
    }
    // Walking state (toggled)
    else if (Inputs->bWantsToToggleWalk)
    {
        // Toggle between walking and running
        if (SyncState->CurrentGait == AlsGaitTags::Walking)
        {
            TargetGait = AlsGaitTags::Running;
        }
        else
        {
            TargetGait = AlsGaitTags::Walking;
        }
    }
    // Maintain current walk state if already walking
    else if (SyncState->CurrentGait == AlsGaitTags::Walking && !Inputs->bIsSprintHeld)
    {
        TargetGait = AlsGaitTags::Walking;
    }

    // Update gait if changed
    if (SyncState->CurrentGait != TargetGait)
    {
        SyncState->CurrentGait = TargetGait;
        
        // Queue a gait modifier update
        // Note: In a full implementation, we'd update existing modifiers rather than queue new ones
        UE_LOG(LogTemp, Log, TEXT("ALS Gait changed to: %s"), *TargetGait.ToString());
    }
}

void UAlsStateLogicTransition::EvaluateStanceLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState,
                                                   UMoverComponent* MoverComp) const
{
    if (Inputs->bWantsToToggleCrouch)
    {
        bool bIsCrouching = IsModifierActive(MoverComp, SyncState->CrouchModifierHandle);
        
        if (bIsCrouching)
        {
            // Stand up - cancel the crouch modifier
            if (SyncState->CrouchModifierHandle.IsValid())
            {
                MoverComp->CancelModifierFromHandle(SyncState->CrouchModifierHandle);
                SyncState->CrouchModifierHandle = FMovementModifierHandle(); // Reset to invalid
            }
            // Note: The modifier's OnEnd will handle capsule resize and sync state update
            
            UE_LOG(LogTemp, Log, TEXT("ALS Stance Logic: Canceling crouch modifier"));
        }
        else
        {
            // Crouch - queue a new stance modifier
            TSharedPtr<FALSStanceModifier> CrouchModifier = MakeShared<FALSStanceModifier>();
            CrouchModifier->CurrentStance = AlsStanceTags::Crouching;
            SyncState->CrouchModifierHandle = MoverComp->QueueMovementModifier(CrouchModifier);
            // Note: The modifier's OnStart will handle capsule resize and sync state update
            
            UE_LOG(LogTemp, Log, TEXT("ALS Stance Logic: Queuing crouch modifier"));
        }
    }
}

void UAlsStateLogicTransition::EvaluateRotationModeLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState,
                                                         UMoverComponent* MoverComp) const
{
    // For now, rotation mode changes could be triggered by specific inputs
    // In a full implementation, this might be based on aim state, movement direction, etc.
    
    // Example: Switch to view direction when aiming
    if (SyncState->CurrentOverlayMode == AlsOverlayModeTags::Aiming)
    {
        if (SyncState->CurrentRotationMode != AlsRotationModeTags::ViewDirection)
        {
            SyncState->CurrentRotationMode = AlsRotationModeTags::ViewDirection;
            UE_LOG(LogTemp, Log, TEXT("ALS Rotation Mode changed to: View Direction (Aiming)"));
        }
    }
    else if (SyncState->CurrentRotationMode == AlsRotationModeTags::ViewDirection)
    {
        // Return to velocity direction when not aiming
        SyncState->CurrentRotationMode = AlsRotationModeTags::VelocityDirection;
        UE_LOG(LogTemp, Log, TEXT("ALS Rotation Mode changed to: Velocity Direction"));
    }
}

void UAlsStateLogicTransition::EvaluateAimingLogic(const FAlsMoverInputs* Inputs, FAlsMoverSyncState* SyncState,
                                                   UMoverComponent* MoverComp) const
{
    if (Inputs->bWantsToStartAiming && SyncState->CurrentOverlayMode != AlsOverlayModeTags::Aiming)
    {
        // Start aiming
        SyncState->CurrentOverlayMode = AlsOverlayModeTags::Aiming;
        
        // Could queue an aim modifier here if needed
        UE_LOG(LogTemp, Log, TEXT("ALS Started Aiming"));
    }
    else if (Inputs->bWantsToStopAiming && SyncState->CurrentOverlayMode == AlsOverlayModeTags::Aiming)
    {
        // Stop aiming
        SyncState->CurrentOverlayMode = AlsOverlayModeTags::Default;
        
        // Cancel aim modifier if one exists
        if (SyncState->AimModifierHandle.IsValid())
        {
            MoverComp->CancelModifierFromHandle(SyncState->AimModifierHandle);
            SyncState->AimModifierHandle = FMovementModifierHandle(); // Reset to invalid
        }
        
        UE_LOG(LogTemp, Log, TEXT("ALS Stopped Aiming"));
    }
}

bool UAlsStateLogicTransition::IsModifierActive(const UMoverComponent* MoverComp, const FMovementModifierHandle& Handle) const
{
    // In a real implementation, we'd check if the modifier with this handle is still active
    // For now, we use the handle's IsValid() method
    return Handle.IsValid();
}