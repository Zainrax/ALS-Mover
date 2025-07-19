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
                           AlsInputs->bIsAimingHeld || AlsInputs->bIsSprintHeld;

    if (bHasInputEvents || (EvaluationCounter++ % 300 == 0))
    {
        UE_LOG(LogTemp, Warning,
               TEXT(
                   "ALS StateLogic: Evaluating - Crouch=%s, Walk=%s, Sprint=%s, Aim=%s, CurrentGait=%s, CurrentStance=%s, RotationMode=%s"
               ),
               AlsInputs->bWantsToToggleCrouch ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToToggleWalk ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsSprintHeld ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsAimingHeld ? TEXT("YES") : TEXT("no"),
               *AlsSyncState->Gait.ToString(),
               *AlsSyncState->Stance.ToString(),
               *AlsSyncState->RotationMode.ToString());
    }

    // Evaluate state logic - order matters!
    EvaluateGaitLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateStanceLogic(AlsInputs, AlsSyncState, MoverComp);
    EvaluateAimingLogic(AlsInputs, AlsSyncState, MoverComp);

    if (bHasInputEvents || (EvaluationCounter++ % 300 == 0))
    {
        UE_LOG(LogTemp, Warning,
               TEXT(
                   "ALS StateLogic: Evaluated - Crouch=%s, Walk=%s, Sprint=%s, Aim=%s, CurrentGait=%s, CurrentStance=%s, RotationMode=%s"
               ),
               AlsInputs->bWantsToToggleCrouch ? TEXT("YES") : TEXT("no"),
               AlsInputs->bWantsToToggleWalk ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsSprintHeld ? TEXT("YES") : TEXT("no"),
               AlsInputs->bIsAimingHeld ? TEXT("YES") : TEXT("no"),
               *AlsSyncState->Gait.ToString(),
               *AlsSyncState->Stance.ToString(),
               *AlsSyncState->RotationMode.ToString());
    }
    // This transition doesn't change movement modes, only modifies state
    return FTransitionEvalResult::NoTransition;
}

void UAlsStateLogicTransition::EvaluateGaitLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                 UMoverComponent *MoverComp) const
{
    // --- Part 1: Manage persistent states based on input events ---

    // Handle the Walk TOGGLE
    if (Inputs->bWantsToToggleWalk)
    {
        if (SyncState->WalkModifierHandle.IsValid())
        {
            MoverComp->CancelModifierFromHandle(SyncState->WalkModifierHandle);
            SyncState->WalkModifierHandle.Invalidate();
        }
        else
        {
            auto WalkModifier = MakeShared<FALSWalkStateModifier>();
            SyncState->WalkModifierHandle = MoverComp->QueueMovementModifier(WalkModifier);
        }
    }

    // Handle the Sprint HOLD
    if (Inputs->bIsSprintHeld)
    {
        // If the sprint input is held, we want the sprint modifier to be active.
        // If it's not already active, queue one.
        if (!SyncState->SprintModifierHandle.IsValid())
        {
            auto SprintModifier = MakeShared<FALSSprintStateModifier>();
            SyncState->SprintModifierHandle = MoverComp->QueueMovementModifier(SprintModifier);
        }
    }
    else
    {
        // If the sprint input is NOT held, we want the sprint modifier to be inactive.
        // If it's currently active, cancel it.
        if (SyncState->SprintModifierHandle.IsValid())
        {
            MoverComp->CancelModifierFromHandle(SyncState->SprintModifierHandle);
            SyncState->SprintModifierHandle.Invalidate();
        }
    }

    // --- Part 2: Determine the final, effective gait for THIS FRAME ---
    FGameplayTag TargetGait;

    // The order of checks determines priority.
    // Is a Sprint modifier active?
    if (SyncState->SprintModifierHandle.IsValid())
    {
        TargetGait = AlsGaitTags::Sprinting;
    }
    // Is a Walk modifier active?
    else if (SyncState->WalkModifierHandle.IsValid())
    {
        TargetGait = AlsGaitTags::Walking;
    }
    // Otherwise, the default is Running.
    else
    {
        TargetGait = AlsGaitTags::Running;
    }

    // --- Part 3: Update the CurrentGait tag if it changed ---
    if (SyncState->Gait != TargetGait)
    {
        FGameplayTag OldGait = SyncState->Gait;
        SyncState->Gait = TargetGait;
        UE_LOG(LogTemp, Warning, TEXT("ALS Gait changed from %s to %s"), *OldGait.ToString(), *TargetGait.ToString());
    }
}

void UAlsStateLogicTransition::EvaluateStanceLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                   UMoverComponent *MoverComp) const
{
    if (!Inputs->bWantsToToggleCrouch)
    {
        return;
    }

    const UAlsMoverMovementSettings *MovementSettings = MoverComp->FindSharedSettings<UAlsMoverMovementSettings>();
    if (!MovementSettings)
    {
        UE_LOG(LogTemp, Warning, TEXT("ALS Stance Logic: UAlsMoverMovementSettings not found!"));
        return;
    }

    const float HeightDifference = MovementSettings->StandingCapsuleHalfHeight - MovementSettings->
                                   CrouchingCapsuleHalfHeight;

    if (SyncState->CrouchModifierHandle.IsValid())
    {
        // Requesting to UNCROUCH
        MoverComp->CancelModifierFromHandle(SyncState->CrouchModifierHandle);
        SyncState->CrouchModifierHandle.Invalidate();

        // Queue effects for standing
        auto StandEffect = MakeShared<FApplyCapsuleSizeEffect>();
        StandEffect->TargetHalfHeight = MovementSettings->StandingCapsuleHalfHeight;
        MoverComp->QueueInstantMovementEffect(StandEffect);

        auto CrouchStateEffect = MakeShared<FAlsApplyCrouchStateEffect>();
        CrouchStateEffect->bIsCrouching = false;
        CrouchStateEffect->HeightDifference = HeightDifference;
        MoverComp->QueueInstantMovementEffect(CrouchStateEffect);

        UE_LOG(LogTemp, Log, TEXT("ALS State Logic: Uncrouch requested."));
    }
    else
    {
        // Requesting to CROUCH
        auto CrouchModifier = MakeShared<FALSStanceModifier>();
        SyncState->CrouchModifierHandle = MoverComp->QueueMovementModifier(CrouchModifier);

        // Queue effects for crouching
        auto CrouchEffect = MakeShared<FApplyCapsuleSizeEffect>();
        CrouchEffect->TargetHalfHeight = MovementSettings->CrouchingCapsuleHalfHeight;
        MoverComp->QueueInstantMovementEffect(CrouchEffect);

        auto CrouchStateEffect = MakeShared<FAlsApplyCrouchStateEffect>();
        CrouchStateEffect->bIsCrouching = true;
        CrouchStateEffect->HeightDifference = HeightDifference;
        MoverComp->QueueInstantMovementEffect(CrouchStateEffect);

        UE_LOG(LogTemp, Log, TEXT("ALS State Logic: Crouch requested. Handle: %s"),
               *SyncState->CrouchModifierHandle.ToString());
    }
}


void UAlsStateLogicTransition::EvaluateAimingLogic(const FAlsMoverInputs *Inputs, FAlsMoverSyncState *SyncState,
                                                   UMoverComponent *MoverComp) const
{
    const UAlsMoverMovementSettings *AlsSettings = MoverComp->FindSharedSettings<UAlsMoverMovementSettings>();
    if (!AlsSettings)
    {
        // Log a warning if settings are missing, but only once to avoid spam
        static bool bLoggedMissingSettings = false;
        if (!bLoggedMissingSettings)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT(
                       "AlsStateLogicTransition: UAlsMoverMovementSettings not found on Mover Component. Rotation rates will use defaults."
                   ));
            bLoggedMissingSettings = true;
        }
    }

    const bool bShouldBeAiming = Inputs->bIsAimingHeld;
    const bool bIsCurrentlyAiming = SyncState->OverlayMode == AlsOverlayModeTags::Aiming;

    // Only perform actions if the state needs to change
    if (bShouldBeAiming != bIsCurrentlyAiming)
    {
        if (bShouldBeAiming)
        {
            // --- Transition TO Aiming State ---
            SyncState->OverlayMode = AlsOverlayModeTags::Aiming;
            SyncState->RotationMode = AlsRotationModeTags::Aiming; // Aiming uses Aiming rotation mode

            if (!SyncState->AimModifierHandle.IsValid())
            {
                auto AimModifier = MakeShared<FALSRotationRateModifier>();
                AimModifier->NewRotationRate = AlsSettings ? AlsSettings->AimRotationRate : 360.0f;
                // Use settings or fallback
                SyncState->AimModifierHandle = MoverComp->QueueMovementModifier(AimModifier);
                UE_LOG(LogTemp, Log, TEXT("ALS State Logic: Started Aiming. Queued Aim Modifier. Handle: %s"),
                       *SyncState->AimModifierHandle.ToString());
            }
        }
        else
        {
            // --- Transition FROM Aiming State ---
            SyncState->OverlayMode = AlsOverlayModeTags::Default;
            SyncState->RotationMode = AlsRotationModeTags::ViewDirection; // Revert to default for top-down

            // If the aim modifier handle is valid, cancel it.
            if (SyncState->AimModifierHandle.IsValid())
            {
                MoverComp->CancelModifierFromHandle(SyncState->AimModifierHandle);
                SyncState->AimModifierHandle.Invalidate(); // Crucial to reset the handle
                UE_LOG(LogTemp, Log, TEXT("ALS State Logic: Stopped Aiming. Cancelled Aim Modifier."));
            }
        }
    }
}

bool UAlsStateLogicTransition::IsModifierActive(const UMoverComponent *MoverComp,
                                                const FMovementModifierHandle &Handle) const
{
    // In a real implementation, we'd check if the modifier with this handle is still active
    // For now, we use the handle's IsValid() method
    return Handle.IsValid();
}