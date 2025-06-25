#include "AlsMoverAnimationInterface.h"
#include "AlsCharacterMoverComponent.h"
#include "AlsMoverCharacter.h"
#include "Utility/AlsGameplayTags.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverAnimationInterface)

UAlsMoverAnimationInterface::UAlsMoverAnimationInterface()
{
    // Initialize default values
    Gait = AlsGaitTags::Running;
    Stance = AlsStanceTags::Standing;
    RotationMode = AlsRotationModeTags::VelocityDirection;
    LocomotionMode = AlsLocomotionModeTags::Grounded;
    OverlayMode = AlsOverlayModeTags::Default;
}

void UAlsMoverAnimationInterface::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    
    // Cache the mover component reference
    CachedMoverComponent = GetAlsMoverComponent();
}

void UAlsMoverAnimationInterface::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    
    // Update state from the mover component
    UpdateStateFromMover();
    
    // Update helper variables
    UpdateHelperVariables();
}

UAlsCharacterMoverComponent* UAlsMoverAnimationInterface::GetAlsMoverComponent() const
{
    if (CachedMoverComponent)
    {
        return CachedMoverComponent;
    }
    
    // Try to get from the owning pawn
    if (APawn* OwningPawn = TryGetPawnOwner())
    {
        return OwningPawn->FindComponentByClass<UAlsCharacterMoverComponent>();
    }
    
    return nullptr;
}

void UAlsMoverAnimationInterface::UpdateStateFromMover()
{
    UAlsCharacterMoverComponent* MoverComp = GetAlsMoverComponent();
    if (!MoverComp)
    {
        return;
    }
    
    // Update state variables from sync state
    Gait = MoverComp->GetSyncStateGait();
    Stance = MoverComp->GetSyncStateStance();
    RotationMode = MoverComp->GetSyncStateRotationMode();
    LocomotionMode = MoverComp->GetSyncStateLocomotionMode();
    OverlayMode = MoverComp->GetSyncStateOverlayMode();
    
    // Update movement variables
    Speed = MoverComp->GetSpeed();
    Velocity = MoverComp->GetVelocity();
    Acceleration = MoverComp->GetAcceleration();
    bIsMoving = MoverComp->IsMoving(MovingThreshold);
    bHasMovementInput = MoverComp->HasMovementInput(InputThreshold);
    bIsOnGround = MoverComp->IsOnGround();
}

void UAlsMoverAnimationInterface::UpdateHelperVariables()
{
    // Update helper boolean variables based on current state
    bIsWalking = (Gait == AlsGaitTags::Walking);
    bIsRunning = (Gait == AlsGaitTags::Running);
    bIsSprinting = (Gait == AlsGaitTags::Sprinting);
    bIsCrouching = (Stance == AlsStanceTags::Crouching);
    bIsAiming = (OverlayMode == AlsRotationModeTags::Aiming);
}