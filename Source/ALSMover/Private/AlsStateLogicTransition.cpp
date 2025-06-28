#include "AlsStateLogicTransition.h"
#include "AlsMoverData.h"
#include "AlsMovementModifiers.h"
#include "AlsMovementEffects.h"
#include "AlsMoverMovementSettings.h"
#include "MoverComponent.h"
#include "Utility/AlsGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsStateLogicTransition)

UAlsStateLogicTransition::UAlsStateLogicTransition(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
}

FTransitionEvalResult UAlsStateLogicTransition::Evaluate_Implementation(const FSimulationTickParams &Context) const
{
    // Log that this method is being called
    static int32 CallCounter = 0;
    if (CallCounter++ % 300 == 0) // Log every 5 seconds at 60fps
    {
        UE_LOG(LogTemp, Warning, TEXT("ALS StateLogicTransition::Evaluate_Implementation called - Counter: %d"),
               CallCounter);
    }

    // Get ALS-specific input and sync state
    const FAlsMoverInputs *AlsInputs = Context.StartState.InputCmd.InputCollection.FindDataByType<FAlsMoverInputs>();
    FAlsMoverSyncState *AlsSyncState = Context.StartState.SyncState.SyncStateCollection.FindMutableDataByType<
        FAlsMoverSyncState>();

    if (!AlsInputs || !AlsSyncState)
    {
        // Debug: Log if we can't find the required data types
        static int32 MissingDataCounter = 0;
        if (MissingDataCounter++ % 300 == 0) // Log every 5 seconds at 60fps
        {
            UE_LOG(LogTemp, Warning, TEXT("ALS StateLogic: Missing data types - AlsInputs=%s, AlsSyncState=%s"),
                   AlsInputs ? TEXT("Found") : TEXT("MISSING"),
                   AlsSyncState ? TEXT("Found") : TEXT("MISSING"));
        }
        return FTransitionEvalResult::NoTransition;
    }

    UMoverComponent *MoverComp = GetMoverComponent();
    if (!MoverComp)
    {
        return FTransitionEvalResult::NoTransition;
    }

    // Debug: Log transition evaluation periodically or when input events occur
    static int32 EvaluationCounter = 0;
    bool bHasInputEvents = AlsInputs->bWantsToToggleCrouch || AlsInputs->bWantsToToggleWalk ||
                           AlsInputs->bWantsToStartAiming || AlsInputs->bWantsToStopAiming || AlsInputs->bIsSprintHeld;

    if (bHasInputEvents || (EvaluationCounter++ % 300 == 0))
    {
        UE_LOG(LogTemp, Warning,
               TEXT(
                   "ALS StateLogic: Evaluating - Crouch=%s, Walk=%s, Sprint=%s, Aim=%s, CurrentGait=%s, CurrentStance=%s"
               ),
               AlsInputs->bWantsToToggleCrouch ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToToggleWalk ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsSprintHeld ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToStartAiming ? TEXT("YES") : TEXT("no"),
               *AlsSyncState->CurrentGait.ToString(),
               *AlsSyncState->CurrentStance.ToString());
    }

    // Evaluate state logic - order matters!
    EvaluateGaitLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateStanceLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateRotationModeLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateAimingLogic(AlsInputs, AlsSyncState, MoverComp);

    if (bHasInputEvents || (EvaluationCounter++ % 300 == 0))
    {
        UE_LOG(LogTemp, Warning,
               TEXT(
                   "ALS StateLogic: Evaluated - Crouch=%s, Walk=%s, Sprint=%s, Aim=%s, CurrentGait=%s, CurrentStance=%s"
               ),
               AlsInputs->bWantsToToggleCrouch ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToToggleWalk ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsSprintHeld ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToStartAiming ? TEXT("YES") : TEXT("no"),
               *AlsSyncState->CurrentGait.ToString(),
               *AlsSyncState->CurrentStance.ToString());
    }
    // This transition doesn't change movement modes, only modifies state
    return FTransitionEvalResult::NoTransition;
}

void UAlsStateLogicTransition::EvaluateGaitLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                 UMoverComponent *MoverComp) const
{
    // Handle walk toggle first - this is an event that changes our walking state
    if (Inputs->bWantsToToggleWalk)
    {
        SyncState->bWantToWalk = !SyncState->bWantToWalk;
        UE_LOG(LogTemp, Warning, TEXT("ALS Gait Logic: Walk toggled to %s"),
               SyncState->bWantToWalk ? TEXT("ON") : TEXT("OFF"));
    }

    // Determine target gait based on current state and input
    FGameplayTag TargetGait;

    // Sprint takes priority over all other gaits
    if (Inputs->bIsSprintHeld)
    {
        TargetGait = AlsGaitTags::Sprinting;
    }
    // Check if we're in walking mode
    else if (SyncState->bWantToWalk)
    {
        TargetGait = AlsGaitTags::Walking;
    }
    // Default to running
    else
    {
        TargetGait = AlsGaitTags::Running;
    }

    // Update gait if changed
    if (SyncState->CurrentGait != TargetGait)
    {
        FGameplayTag OldGait = SyncState->CurrentGait;
        SyncState->CurrentGait = TargetGait;

        UE_LOG(LogTemp, Warning, TEXT("ALS Gait changed from %s to %s"),
               *OldGait.ToString(), *TargetGait.ToString());
    }
}

void UAlsStateLogicTransition::EvaluateStanceLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                   UMoverComponent *MoverComp) const
{
    if (Inputs->bWantsToToggleCrouch)
    {
        // Check if we are already crouching by checking current stance
        if (SyncState->CurrentStance == AlsStanceTags::Crouching)
        {
            // We are crouching, so stand up
            SyncState->CurrentStance = AlsStanceTags::Standing;

            // Cancel the crouch modifier if it exists
            if (SyncState->CrouchModifierHandle.IsValid())
            {
                MoverComp->CancelModifierFromHandle(SyncState->CrouchModifierHandle);
                SyncState->CrouchModifierHandle.Invalidate();
            }

            UE_LOG(LogTemp, Warning, TEXT("ALS Stance Logic: Standing up (was crouching)"));
        }
        else
        {
            // We are standing, so crouch
            SyncState->CurrentStance = AlsStanceTags::Crouching;

            // Queue a crouch modifier for the physical changes (capsule height, etc.)
            TSharedPtr<FALSStanceModifier> CrouchModifier = MakeShared<FALSStanceModifier>();

            // Get movement settings for capsule heights
            if (const UAlsMoverMovementSettings *MovementSettings = MoverComp->FindSharedSettings<
                UAlsMoverMovementSettings>())
            {
                CrouchModifier->StandingCapsuleHalfHeight = MovementSettings->StandingCapsuleHalfHeight;
                CrouchModifier->CrouchCapsuleHalfHeight = MovementSettings->CrouchingCapsuleHalfHeight;
                CrouchModifier->CrouchSpeedMultiplier = 0.5f; // This will be handled by gait modifier
            }
            else
            {
                // Use defaults
                CrouchModifier->StandingCapsuleHalfHeight = 92.0f;
                CrouchModifier->CrouchCapsuleHalfHeight = 60.0f;
                CrouchModifier->CrouchSpeedMultiplier = 0.5f;
            }

            // Set the modifier to crouch state
            CrouchModifier->CurrentStance = AlsStanceTags::Crouching;

            // Queue the modifier and store its handle in the sync state
            SyncState->CrouchModifierHandle = MoverComp->QueueMovementModifier(CrouchModifier);

            UE_LOG(LogTemp, Warning, TEXT("ALS Stance Logic: Crouching (Height: %.1f -> %.1f)"),
                   CrouchModifier->StandingCapsuleHalfHeight, CrouchModifier->CrouchCapsuleHalfHeight);
        }
    }
}

void UAlsStateLogicTransition::EvaluateRotationModeLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                         UMoverComponent *MoverComp) const
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

void UAlsStateLogicTransition::EvaluateAimingLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                   UMoverComponent *MoverComp) const
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

bool UAlsStateLogicTransition::IsModifierActive(const UMoverComponent *MoverComp,
                                                const FMovementModifierHandle &Handle) const
{
    // In a real implementation, we'd check if the modifier with this handle is still active
    // For now, we use the handle's IsValid() method
    return Handle.IsValid();
}