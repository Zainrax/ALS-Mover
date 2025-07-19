#include "AlsCharacterMoverComponent.h"
#include "AlsMovementModifiers.h"
#include "AlsMoverData.h"
#include "AlsMoverMovementSettings.h"
#include "AlsGroundMovementMode.h"
#include "AlsStateLogicTransition.h"
#include "MoverDataModelTypes.h"
#include "Utility/AlsGameplayTags.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacterMoverComponent)

UAlsCharacterMoverComponent::UAlsCharacterMoverComponent()
{
    // Set up ALS movement modes - replace default walking with our ALS ground mode
    MovementModes.Add(DefaultModeNames::Walking,
                      CreateDefaultSubobject<UAlsGroundMovementMode>(TEXT("AlsGroundMovementMode")));
    MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UFallingMode>(TEXT("DefaultFallingMode")));
    MovementModes.Add(DefaultModeNames::Flying, CreateDefaultSubobject<UFlyingMode>(TEXT("DefaultFlyingMode")));

    // Add the ALS state logic transition as a global transition
    UAlsStateLogicTransition *StateTransition = CreateDefaultSubobject<UAlsStateLogicTransition>(
        TEXT("AlsStateLogicTransition"));
    Transitions.Add(StateTransition);

    StartingMovementMode = DefaultModeNames::Walking;
}

void UAlsCharacterMoverComponent::BeginPlay()
{
    Super::BeginPlay();

    // Bind our pre-simulation tick handler (like CharacterMoverComponent)
    OnPreSimulationTick.AddDynamic(this, &UAlsCharacterMoverComponent::OnMoverPreSimulationTick);
}

void UAlsCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep &TimeStep,
                                                           const FMoverInputCmdContext &InputCmd)
{
    const FCharacterDefaultInputs *CharacterInputs = InputCmd.InputCollection.FindDataByType<
        FCharacterDefaultInputs>();
    if (CharacterInputs && CharacterInputs->bIsJumpJustPressed && CanJump())
    {
        DoJump();
    }
}

bool UAlsCharacterMoverComponent::DoJump()
{
    if (const UCommonLegacyMovementSettings *CommonSettings = FindSharedSettings<UCommonLegacyMovementSettings>())
    {
        TSharedPtr<FJumpImpulseEffect> JumpMove = MakeShared<FJumpImpulseEffect>();
        JumpMove->UpwardsSpeed = CommonSettings->JumpUpwardsSpeed;

        QueueInstantMovementEffect(JumpMove);

        return true;
    }

    return false;
}

bool UAlsCharacterMoverComponent::CanJump() const
{
    return IsOnGround();
}

bool UAlsCharacterMoverComponent::IsOnGround() const
{
    return HasGameplayTag(Mover_IsOnGround, true);
}

//========================================================================
// Sync State Accessors (for Animation)
//========================================================================

FGameplayTag UAlsCharacterMoverComponent::GetGait() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->Gait;
    }
    return AlsGaitTags::Running; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetStance() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->Stance;
    }
    return AlsStanceTags::Standing; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetRotationMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->RotationMode;
    }
    return AlsRotationModeTags::VelocityDirection; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetLocomotionMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->LocomotionMode;
    }
    return AlsLocomotionModeTags::Grounded; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetOverlayMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->OverlayMode;
    }
    return AlsOverlayModeTags::Default; // Default fallback
}

float UAlsCharacterMoverComponent::GetSpeed() const
{
    return GetVelocity().Size();
}

FVector UAlsCharacterMoverComponent::GetAcceleration() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->Acceleration;
    }
    return FVector::ZeroVector;
}

bool UAlsCharacterMoverComponent::IsMoving(float Threshold) const
{
    return GetSpeed() > Threshold;
}

bool UAlsCharacterMoverComponent::HasMovementInput(float Threshold) const
{
    // Check if there's current movement input
    if (const FAlsMoverInputs *AlsInputs = GetLastInputCmd().InputCollection.FindDataByType<FAlsMoverInputs>())
    {
        return !AlsInputs->MoveInputVector.IsNearlyZero(Threshold);
    }

    // Fallback to character default inputs
    if (const FCharacterDefaultInputs *CharInputs = GetLastInputCmd().InputCollection.FindDataByType<
        FCharacterDefaultInputs>())
    {
        return !CharInputs->GetMoveInput_WorldSpace().IsNearlyZero(Threshold);
    }

    return false;
}

FGameplayTag UAlsCharacterMoverComponent::GetViewMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->ViewMode;
    }
    return AlsViewModeTags::ThirdPerson; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetLocomotionAction() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->LocomotionAction;
    }
    return FGameplayTag::EmptyTag;
}


FRotator UAlsCharacterMoverComponent::GetViewRotation() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->ViewRotation;
    }
    return FRotator::ZeroRotator;
}

float UAlsCharacterMoverComponent::GetYawSpeed() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->YawSpeed;
    }
    return 0.0f;
}

float UAlsCharacterMoverComponent::GetVelocityYawAngle() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->VelocityYawAngle;
    }
    return 0.0f;
}

FVector UAlsCharacterMoverComponent::GetRelativeVelocity() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->RelativeVelocity;
    }
    return FVector::ZeroVector;
}

const UAlsMoverMovementSettings *UAlsCharacterMoverComponent::GetAlsMovementSettings() const
{
    return FindSharedSettings<UAlsMoverMovementSettings>();
}