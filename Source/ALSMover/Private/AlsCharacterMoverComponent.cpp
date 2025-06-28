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
    UAlsStateLogicTransition* StateTransition = CreateDefaultSubobject<UAlsStateLogicTransition>(TEXT("AlsStateLogicTransition"));
    Transitions.Add(StateTransition);

    StartingMovementMode = DefaultModeNames::Walking;

    // Initialize ALS states
    CurrentGait = AlsGaitTags::Running;
    CurrentStance = AlsStanceTags::Standing;
    CurrentRotationMode = AlsRotationModeTags::ViewDirection; // Default for top-down
    CachedGait = AlsGaitTags::Running;
    CachedStance = AlsStanceTags::Standing;
    CachedRotationMode = AlsRotationModeTags::ViewDirection;
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
    if (CharacterInputs && CharacterInputs->bIsJumpJustPressed && CanActorJump())
    {
        Jump();
    }

    // Handle ALS modifier management - only create/update when state changes
    ManageStanceModifier();
    // Rotation is now handled entirely by the movement mode
    // ManageRotationModifier();
}

void UAlsCharacterMoverComponent::ManageStanceModifier()
{
    if (const UAlsMoverMovementSettings *MovementSettings = GetAlsMovementSettings())
    {
        // Get current stance from SyncState
        FGameplayTag SyncStateStance = GetSyncStateStance();
        
        // Handle stance-related logic (capsule size changes, etc.)
        // Note: The actual crouching logic is now handled by the FALSStanceModifier 
        // This method now mainly serves to keep the local member variable in sync
        if (SyncStateStance != CurrentStance)
        {
            CurrentStance = SyncStateStance;
        }
    }
}

void UAlsCharacterMoverComponent::SetGait(const FGameplayTag &NewGait)
{
    if (CurrentGait != NewGait)
    {
        CurrentGait = NewGait;
    }
}

void UAlsCharacterMoverComponent::SetStance(const FGameplayTag &NewStance)
{
    if (CurrentStance != NewStance)
    {
        CurrentStance = NewStance;
    }
}

bool UAlsCharacterMoverComponent::Jump()
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

bool UAlsCharacterMoverComponent::CanActorJump() const
{
    return IsOnGround();
}

bool UAlsCharacterMoverComponent::IsOnGround() const
{
    return HasGameplayTag(Mover_IsOnGround, true);
}

void UAlsCharacterMoverComponent::SetRotationMode(const FGameplayTag &NewRotationMode)
{
    if (CurrentRotationMode != NewRotationMode)
    {
        CurrentRotationMode = NewRotationMode;
    }
}

void UAlsCharacterMoverComponent::ManageRotationModifier()
{
    if (const UAlsMoverMovementSettings *MovementSettings = GetAlsMovementSettings())
    {
        // Check if rotation mode has changed
        if (CurrentRotationMode != CachedRotationMode || !RotationModifierHandle.IsValid())
        {
            // Remove old modifier if it exists
            if (RotationModifierHandle.IsValid())
            {
                CancelModifierFromHandle(RotationModifierHandle);
                RotationModifierHandle.Invalidate();
            }

            // Create new rotation modifier
            TSharedPtr<FALSRotationModeModifier> NewRotationModifier = MakeShared<FALSRotationModeModifier>();
            NewRotationModifier->CurrentRotationMode = CurrentRotationMode;
            NewRotationModifier->RotationRate = MovementSettings->RotationRate;
            NewRotationModifier->AimRotationRate = MovementSettings->AimRotationRate;

            RotationModifierHandle = QueueMovementModifier(NewRotationModifier);
            CachedRotationMode = CurrentRotationMode;
        }
    }
}

//========================================================================
// Sync State Accessors (for Animation)
//========================================================================

FGameplayTag UAlsCharacterMoverComponent::GetSyncStateGait() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->CurrentGait;
    }
    return AlsGaitTags::Running; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetSyncStateStance() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->CurrentStance;
    }
    return AlsStanceTags::Standing; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetSyncStateRotationMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->CurrentRotationMode;
    }
    return AlsRotationModeTags::VelocityDirection; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetSyncStateLocomotionMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->CurrentLocomotionMode;
    }
    return AlsLocomotionModeTags::Grounded; // Default fallback
}

FGameplayTag UAlsCharacterMoverComponent::GetSyncStateOverlayMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetSyncState().SyncStateCollection.FindDataByType<FAlsMoverSyncState>())
    {
        return AlsState->CurrentOverlayMode;
    }
    return AlsOverlayModeTags::Default; // Default fallback
}

float UAlsCharacterMoverComponent::GetSpeed() const
{
    return GetVelocity().Size();
}

FVector UAlsCharacterMoverComponent::GetVelocity() const
{
    if (const FMoverDefaultSyncState *DefaultState = GetSyncState().SyncStateCollection.FindDataByType<
        FMoverDefaultSyncState>())
    {
        return DefaultState->GetVelocity_WorldSpace();
    }
    return FVector::ZeroVector;
}

FVector UAlsCharacterMoverComponent::GetAcceleration() const
{
    // Calculate acceleration from velocity change
    // This would need to be tracked over time for accurate calculation
    // For now, return zero as a placeholder
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

const UAlsMoverMovementSettings *UAlsCharacterMoverComponent::GetAlsMovementSettings() const
{
    return FindSharedSettings<UAlsMoverMovementSettings>();
}