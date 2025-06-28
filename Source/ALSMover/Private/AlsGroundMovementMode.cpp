#include "AlsGroundMovementMode.h"
#include "MoverComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "AlsMoverCharacter.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "AlsMoverMovementSettings.h"
#include "Utility/AlsGameplayTags.h"
#include "MoverTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsGroundMovementMode)

static TAutoConsoleVariable<int32> CVarShowRotationDebug(
    TEXT("ALS.ShowRotationDebug"),
    0,
    TEXT("Show rotation debug logging.\n")
    TEXT("0: Disabled\n")
    TEXT("1: Enabled"),
    ECVF_Cheat);

UAlsGroundMovementMode::UAlsGroundMovementMode(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Declare that this movement mode requires ALS-specific movement settings
    SharedSettingsClasses.Add(UAlsMoverMovementSettings::StaticClass());
}

void UAlsGroundMovementMode::GenerateMove_Implementation(const FMoverTickStartData &StartState,
                                                         const FMoverTimeStep &TimeStep,
                                                         FProposedMove &OutProposedMove) const
{
    const UMoverComponent *MoverComp = GetMoverComponent();
    if (!MoverComp)
    {
        return;
    }

    // Get input from the input collection
    const FCharacterDefaultInputs *CharInput = StartState.InputCmd.InputCollection.FindDataByType<
        FCharacterDefaultInputs>();
    const FAlsMoverInputs *AlsInputs = StartState.InputCmd.InputCollection.FindDataByType<FAlsMoverInputs>();

    if (!CharInput)
    {
        return;
    }

    // Get the current sync state
    const FMoverDefaultSyncState *SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<
        FMoverDefaultSyncState>();
    const FAlsMoverSyncState *AlsState = StartState.SyncState.SyncStateCollection.FindDataByType<FAlsMoverSyncState>();

    if (!SyncState)
    {
        return;
    }

#if WITH_EDITOR
    // Debug to verify this mode is being used
    if (CVarShowRotationDebug.GetValueOnGameThread() > 0)
    {
        static int32 ModeCheckCounter = 0;
        if (ModeCheckCounter++ % 300 == 0) // Log every 5 seconds at 60fps
        {
            UE_LOG(LogMover, Warning, TEXT("UAlsGroundMovementMode::OnGenerateMove is being called!"));
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("ALS Ground Movement Mode Active!"));
            }
        }
    }
#endif

    // Convert timestep to seconds
    const double DeltaSeconds = static_cast<double>(TimeStep.StepMs) * 0.001;

    // Get ALS movement settings for gait-specific values
    const UAlsMoverMovementSettings *AlsSettings = MoverComp->FindSharedSettings<UAlsMoverMovementSettings>();

    // Calculate floor normal for movement
    FFloorCheckResult LastFloorResult;
    FVector MovementNormal;
    UMoverBlackboard *SimBlackboard = MoverComp->GetSimBlackboard_Mutable();

    // Try to use the floor as the basis for the intended move direction
    if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.
        IsWalkableFloor())
    {
        MovementNormal = LastFloorResult.HitResult.ImpactNormal;
    }
    else
    {
        MovementNormal = MoverComp->GetUpDirection();
    }

    // Calculate intended orientation (we'll override this with ALS rotation later)
    FRotator IntendedOrientation_WorldSpace;
    if (!CharInput || CharInput->OrientationIntent.IsNearlyZero())
    {
        IntendedOrientation_WorldSpace = SyncState->GetOrientation_WorldSpace();
    }
    else
    {
        IntendedOrientation_WorldSpace = CharInput->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
    }

    // Set up ground movement parameters (like UWalkingMode)
    FGroundMoveParams Params;

    if (CharInput)
    {
        Params.MoveInputType = CharInput->GetMoveInputType();
        constexpr bool bMaintainInputMagnitude = true;
        Params.MoveInput = UPlanarConstraintUtils::ConstrainDirectionToPlane(
            MoverComp->GetPlanarConstraint(), CharInput->GetMoveInput_WorldSpace(), bMaintainInputMagnitude);

#if WITH_EDITOR
        // Debug input
        if (CVarShowRotationDebug.GetValueOnGameThread() > 0 && !Params.MoveInput.IsNearlyZero())
        {
            static int32 InputDebugCounter = 0;
            if (InputDebugCounter++ % 60 == 0) // Log every 60 frames
            {
                UE_LOG(LogMover, Warning, TEXT("Input Debug: CharInput=%s, ProcessedInput=%s"),
                       *CharInput->GetMoveInput_WorldSpace().ToString(), *Params.MoveInput.ToString());
            }
        }
#endif
    }
    else
    {
        Params.MoveInputType = EMoveInputType::Invalid;
        Params.MoveInput = FVector::ZeroVector;

#if WITH_EDITOR
        // Debug no input
        if (CVarShowRotationDebug.GetValueOnGameThread() > 0)
        {
            static int32 NoInputCounter = 0;
            if (NoInputCounter++ % 300 == 0) // Log every 5 seconds
            {
                UE_LOG(LogMover, Warning, TEXT("NO CHARACTER INPUT RECEIVED!"));
            }
        }
#endif
    }

    Params.OrientationIntent = IntendedOrientation_WorldSpace;
    Params.PriorVelocity = FVector::VectorPlaneProject(SyncState->GetVelocity_WorldSpace(), MovementNormal);
    Params.PriorOrientation = SyncState->GetOrientation_WorldSpace();
    Params.GroundNormal = MovementNormal;
    Params.TurningRate = AlsSettings->TurningRate;
    Params.TurningBoost = AlsSettings->TurningBoost;
    Params.DeltaSeconds = DeltaSeconds;

    // Apply ALS gait and stance-based movement settings
    float TargetSpeed = AlsSettings->MaxSpeed;
    float TargetAcceleration = AlsSettings->Acceleration;
    float TargetDeceleration = AlsSettings->Deceleration;

    if (AlsSettings && AlsState)
    {
        // Apply gait-based speed
        if (AlsState->CurrentGait == AlsGaitTags::Walking)
        {
            TargetSpeed = AlsSettings->WalkSpeed;
            TargetAcceleration = AlsSettings->WalkAcceleration;
            TargetDeceleration = AlsSettings->BrakingDecelerationWalking;
        }
        else if (AlsState->CurrentGait == AlsGaitTags::Running)
        {
            TargetSpeed = AlsSettings->RunSpeed;
            TargetAcceleration = AlsSettings->RunAcceleration;
            TargetDeceleration = AlsSettings->BrakingDecelerationRunning;
        }
        else if (AlsState->CurrentGait == AlsGaitTags::Sprinting)
        {
            TargetSpeed = AlsSettings->SprintSpeed;
            TargetAcceleration = AlsSettings->SprintAcceleration;
            TargetDeceleration = AlsSettings->BrakingDecelerationRunning;
        }

        // Apply stance multiplier
        if (AlsState->CurrentStance == AlsStanceTags::Crouching)
        {
            TargetSpeed *= AlsSettings->CrouchSpeedMultiplier;
            TargetAcceleration *= AlsSettings->CrouchAccelerationMultiplier;
        }

        // Debug gait changes
        static FGameplayTag LastGait;
        static FGameplayTag LastStance;
        if (AlsState->CurrentGait != LastGait || AlsState->CurrentStance != LastStance)
        {
            UE_LOG(LogTemp, Warning, TEXT("ALS Ground Movement: Gait=%s, Stance=%s, Speed=%.1f"),
                   *AlsState->CurrentGait.ToString(), *AlsState->CurrentStance.ToString(), TargetSpeed);
            LastGait = AlsState->CurrentGait;
            LastStance = AlsState->CurrentStance;
        }
    }
    else
    {
        // Debug missing settings
        static int32 MissingSettingsCounter = 0;
        if (MissingSettingsCounter++ % 300 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("ALS Ground Movement: Missing settings - AlsSettings=%s, AlsState=%s"),
                   AlsSettings ? TEXT("Found") : TEXT("MISSING"),
                   AlsState ? TEXT("Found") : TEXT("MISSING"));
        }
    }

    Params.MaxSpeed = TargetSpeed;
    Params.Acceleration = TargetAcceleration;
    Params.Deceleration = TargetDeceleration;

    if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(
            Params.PriorVelocity, AlsSettings->MaxSpeed))
    {
        Params.Friction = AlsSettings->GroundFriction;
    }
    else
    {
        Params.Friction = AlsSettings->bUseSeparateBrakingFriction
                              ? AlsSettings->BrakingFriction
                              : AlsSettings->GroundFriction;
        Params.Friction *= AlsSettings->BrakingFrictionFactor;
    }

    // Use Unreal's ground movement utilities (like UWalkingMode)
    OutProposedMove = UGroundMovementUtils::ComputeControlledGroundMove(Params);

#if WITH_EDITOR
    // Debug movement calculation
    if (CVarShowRotationDebug.GetValueOnGameThread() > 0)
    {
        static int32 MoveDebugCounter = 0;
        if (MoveDebugCounter++ % 60 == 0) // Log every 60 frames
        {
            UE_LOG(LogMover, Warning, TEXT("Movement Debug: MoveInput=%s, MaxSpeed=%.1f, ProposedVel=%s"),
                   *Params.MoveInput.ToString(), Params.MaxSpeed, *OutProposedMove.LinearVelocity.ToString());
        }
    }
#endif

    // Override rotation with ALS system
    if (AlsInputs && AlsState)
    {
        FRotator TargetRotation = CalculateTargetRotation(AlsInputs, SyncState);

        if (!TargetRotation.IsNearlyZero())
        {
            const FRotator CurrentRotation = SyncState->GetOrientation_WorldSpace();
            const float DeltaYaw = FRotator::NormalizeAxis(TargetRotation.Yaw - CurrentRotation.Yaw);

            // Calculate rotation speed based on movement state
            const float RotationSpeed = CalculateRotationRate(SyncState, AlsState, DeltaYaw);
            const float MaxDeltaYaw = RotationSpeed * DeltaSeconds;
            const float ClampedDeltaYaw = FMath::Clamp(DeltaYaw, -MaxDeltaYaw, MaxDeltaYaw);

            // Override the angular velocity from ground movement utils
            OutProposedMove.AngularVelocity = FRotator(0, ClampedDeltaYaw / DeltaSeconds, 0);

#if WITH_EDITOR
            // Debug logging
            if (CVarShowRotationDebug.GetValueOnGameThread() > 0)
            {
                static int32 DebugCounter = 0;
                if (DebugCounter++ % 60 == 0) // Log every 60 frames
                {
                    UE_LOG(LogMover, Warning, TEXT("ALS Rotation: Target=%.1f, Current=%.1f, Delta=%.1f, AngVel=%.1f"),
                           TargetRotation.Yaw, CurrentRotation.Yaw, DeltaYaw, OutProposedMove.AngularVelocity.Yaw);
                }
            }
#endif
        }
        else
        {
            OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
        }
    }
    else
    {
        OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
    }
}

void UAlsGroundMovementMode::SimulationTick_Implementation(const FSimulationTickParams &Params,
                                                           FMoverTickEndData &OutputState)
{
    Super::SimulationTick_Implementation(Params, OutputState);

    const FAlsMoverSyncState *InputAlsState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<
        FAlsMoverSyncState>();

    if (InputAlsState)
    {
        FAlsMoverSyncState *OutputAlsState = OutputState.SyncState.SyncStateCollection.FindMutableDataByType<
            FAlsMoverSyncState>();

        if (OutputAlsState)
        {
            *OutputAlsState = *InputAlsState;
        }
    }
}

float UAlsGroundMovementMode::GetMaxSpeed() const
{
    // Get speed from CommonLegacyMovementSettings if available
    const UMoverComponent *MoverComp = GetMoverComponent();
    if (MoverComp)
    {
        if (const UCommonLegacyMovementSettings *Settings = MoverComp->FindSharedSettings<
            UCommonLegacyMovementSettings>())
        {
            return Settings->MaxSpeed;
        }
    }

    // Fallback to hardcoded walk speed
    return MaxWalkSpeed;
}

float UAlsGroundMovementMode::CalculateRotationRate(const FMoverDefaultSyncState *MoverState,
                                                    const FAlsMoverSyncState *AlsState, float AngleDelta) const
{
    float RotationRate = BaseRotationRate;

    // Adjust based on movement speed
    const float CurrentSpeed = MoverState->GetVelocity_WorldSpace().Size2D();
    const float MaxSpeed = GetMaxSpeed();
    const float SpeedRatio = MaxSpeed > 0 ? CurrentSpeed / MaxSpeed : 0.0f;
    const float SpeedMultiplier = FMath::Lerp(1.0f, 0.5f, SpeedRatio); // Slower turn rate at high speeds

    // Adjust based on stance
    float StanceMultiplier = 1.0f;
    if (AlsState && AlsState->CurrentStance == AlsStanceTags::Crouching)
    {
        StanceMultiplier = CrouchRotationMultiplier;
    }

    // Adjust based on gait
    float GaitMultiplier = 1.0f;
    if (AlsState)
    {
        if (AlsState->CurrentGait == AlsGaitTags::Walking)
        {
            GaitMultiplier = WalkRotationMultiplier;
        }
        else if (AlsState->CurrentGait == AlsGaitTags::Sprinting)
        {
            GaitMultiplier = SprintRotationMultiplier;
        }
    }

    // Adjust based on angle difference (faster for small adjustments)
    const float AngleMultiplier = FMath::GetMappedRangeValueClamped(
        FVector2D(0.0f, 180.0f), // Input range
        FVector2D(1.2f, 0.8f), // Output range (faster for small angles)
        FMath::Abs(AngleDelta)
        );

    return RotationRate * SpeedMultiplier * StanceMultiplier * GaitMultiplier * AngleMultiplier;
}

FRotator UAlsGroundMovementMode::CalculateTargetRotation(const FAlsMoverInputs *AlsInputs,
                                                         const FMoverDefaultSyncState *MoverState) const
{
    if (!AlsInputs || !MoverState)
    {
        return FRotator::ZeroRotator;
    }

    FRotator TargetRotation = MoverState->GetOrientation_WorldSpace();

    // Check for gamepad input by detecting look input (as designed in the new system)
    const bool bUsingGamepad = !AlsInputs->LookInputVector.IsNearlyZero(0.1f);

    if (bUsingGamepad)
    {
        // Gamepad control - use right stick input
        // Convert stick input to world rotation
        // Note: LookInputVector.X = right stick horizontal, Y = vertical
        TargetRotation.Yaw = FMath::Atan2(AlsInputs->LookInputVector.X, -AlsInputs->LookInputVector.Y) * 180.0f / PI;
    }
    else if (AlsInputs->bHasValidMouseTarget)
    {
        // Mouse control - face cursor position
        const FVector CurrentLocation = MoverState->GetLocation_WorldSpace();
        const FVector ToMouse = AlsInputs->MouseWorldPosition - CurrentLocation;

        if (!ToMouse.IsNearlyZero())
        {
            TargetRotation = ToMouse.Rotation();
            TargetRotation.Pitch = 0.0f; // Keep only yaw
            TargetRotation.Roll = 0.0f;

#if WITH_EDITOR
            // Debug logging
            if (CVarShowRotationDebug.GetValueOnGameThread() > 0)
            {
                static int32 MouseDebugCounter = 0;
                if (MouseDebugCounter++ % 60 == 0) // Log every 60 frames
                {
                    UE_LOG(LogMover, Warning,
                           TEXT("ALS Mouse Rotation: MousePos=%s, CharPos=%s, ToMouse=%s, TargetYaw=%.1f"),
                           *AlsInputs->MouseWorldPosition.ToString(), *CurrentLocation.ToString(), *ToMouse.ToString(),
                           TargetRotation.Yaw);
                }
            }
#endif
        }
    }

    return TargetRotation;
}