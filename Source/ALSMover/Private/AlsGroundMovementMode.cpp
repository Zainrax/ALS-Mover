#include "AlsGroundMovementMode.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "AlsMoverCharacter.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsGroundMovementMode)

UAlsGroundMovementMode::UAlsGroundMovementMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UAlsGroundMovementMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
    const UMoverComponent* MoverComp = GetMoverComponent();
    if (!MoverComp)
    {
        return;
    }

    // Get input from the input collection
    const FCharacterDefaultInputs* CharInput = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
    if (!CharInput)
    {
        return;
    }

    // Get the current sync state
    const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    if (!SyncState)
    {
        return;
    }

    // Convert timestep to seconds
    const double DeltaSeconds = static_cast<double>(TimeStep.StepMs) * 0.001;

    // Calculate movement direction from input
    FVector MoveInput = CharInput->GetMoveInput_WorldSpace();
    
    // Apply speed based on current gait
    const float MaxSpeed = GetMaxSpeed();
    FVector DesiredVelocity = MoveInput * MaxSpeed;

    // Get current velocity
    FVector CurrentVelocity = SyncState->GetVelocity_WorldSpace();
    
    // Get acceleration/deceleration from settings if available
    float AccelRate = 10.0f;
    float DecelRate = GroundFriction;
    
    if (const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>())
    {
        AccelRate = Settings->Acceleration / MaxSpeed; // Convert to interp rate
        DecelRate = Settings->Deceleration / MaxSpeed;
    }
    
    // Apply acceleration/friction
    FVector NewVelocity;
    if (DesiredVelocity.SizeSquared() > 0.01f)
    {
        // Accelerate towards desired velocity
        NewVelocity = FMath::VInterpTo(CurrentVelocity, DesiredVelocity, DeltaSeconds, AccelRate);
    }
    else
    {
        // Apply friction when no input
        NewVelocity = FMath::VInterpTo(CurrentVelocity, FVector::ZeroVector, DeltaSeconds, DecelRate);
    }

    // Create the proposed move
    OutProposedMove.LinearVelocity = NewVelocity;
    OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
    OutProposedMove.bHasDirIntent = MoveInput.SizeSquared() > 0.01f;
    
    // Apply gravity
    const FVector Gravity = MoverComp->GetGravityAcceleration();
    OutProposedMove.LinearVelocity.Z = FMath::Max(CurrentVelocity.Z + Gravity.Z * DeltaSeconds, -4000.0f);
}

void UAlsGroundMovementMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
    const UMoverComponent* MoverComp = GetMoverComponent();
    const FMoverTickStartData& StartData = Params.StartState;
    const FMoverDefaultSyncState* StartSyncState = StartData.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

    if (!MoverComp || !StartSyncState)
    {
        return;
    }

    USceneComponent* UpdatedComp = Params.MovingComps.UpdatedComponent.Get();
    if (!UpdatedComp)
    {
        return;
    }

    // Use the proposed linear velocity
    FVector Velocity = Params.ProposedMove.LinearVelocity;
    
    // Convert timestep to seconds
    const double DeltaSeconds = static_cast<double>(Params.TimeStep.StepMs) * 0.001;

    // Compute movement delta
    FVector MoveDelta = Velocity * DeltaSeconds;

    // Record movement details
    FMovementRecord MoveRecord;
    MoveRecord.SetDeltaSeconds(DeltaSeconds);

    FHitResult Hit;
    if (!MoveDelta.IsNearlyZero())
    {
        UMovementUtils::TrySafeMoveUpdatedComponent(
            Params.MovingComps,
            MoveDelta,
            UpdatedComp->GetComponentQuat(),
            true, // bSweep
            Hit,
            ETeleportType::None,
            MoveRecord
        );
    }

    // Handle ground collision
    if (Hit.IsValidBlockingHit() && Hit.Normal.Z > 0.7f) // Ground threshold
    {
        // We hit ground, zero out downward velocity
        if (Velocity.Z < 0)
        {
            Velocity.Z = 0;
        }
    }

    // Update component velocity
    UpdatedComp->ComponentVelocity = Velocity;

    // Save the final state
    FMoverDefaultSyncState& OutSync = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
    
    const FVector FinalLocation = UpdatedComp->GetComponentLocation();
    const FRotator FinalRotation = UpdatedComp->GetComponentRotation();

    OutSync.SetTransforms_WorldSpace(
        FinalLocation,
        FinalRotation,
        Velocity,
        nullptr, // No movement base for now
        NAME_None
    );

    // Store the move record
    OutputState.MoveRecord = MoveRecord;

    // Indicate that all tick time was consumed
    OutputState.MovementEndState.RemainingMs = 0.f;
}

float UAlsGroundMovementMode::GetMaxSpeed() const
{
    // Get speed from CommonLegacyMovementSettings if available
    const UMoverComponent* MoverComp = GetMoverComponent();
    if (MoverComp)
    {
        if (const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>())
        {
            return Settings->MaxSpeed;
        }
    }
    
    // Fallback to hardcoded walk speed
    return MaxWalkSpeed;
}