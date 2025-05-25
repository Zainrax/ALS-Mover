#include "AlsCharacterMoverComponent.h"
#include "AlsMovementModifiers.h"
#include "Utility/AlsGameplayTags.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacterMoverComponent)

UAlsCharacterMoverComponent::UAlsCharacterMoverComponent()
{
    // Set up default movement modes (from CharacterMoverComponent)
    MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UWalkingMode>(TEXT("DefaultWalkingMode")));
    MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UFallingMode>(TEXT("DefaultFallingMode")));
    MovementModes.Add(DefaultModeNames::Flying, CreateDefaultSubobject<UFlyingMode>(TEXT("DefaultFlyingMode")));

    StartingMovementMode = DefaultModeNames::Falling;

    // Initialize ALS states
    CurrentGait = AlsGaitTags::Running;
    CurrentStance = AlsStanceTags::Standing;
    CachedGait = AlsGaitTags::Running;
    CachedStance = AlsStanceTags::Standing;
}

void UAlsCharacterMoverComponent::BeginPlay()
{
    Super::BeginPlay();

    // Bind our pre-simulation tick handler (like CharacterMoverComponent)
    OnPreSimulationTick.AddDynamic(this, &UAlsCharacterMoverComponent::OnMoverPreSimulationTick);

    // Ensure we have CommonLegacyMovementSettings
    if (!FindSharedSettings<UCommonLegacyMovementSettings>())
    {
        UE_LOG(LogTemp, Warning, TEXT("AlsCharacterMoverComponent: CommonLegacyMovementSettings not found! "
                   "Make sure to add it to SharedSettings in the Blueprint."));
    }

    // Apply initial movement settings
    UpdateMovementSettings();

}

void UAlsCharacterMoverComponent::OnMoverPreSimulationTick(const FMoverTimeStep &TimeStep,
                                                           const FMoverInputCmdContext &InputCmd)
{
    // Handle jump input (from CharacterMoverComponent)
    if (bHandleJump)
    {
        const FCharacterDefaultInputs *CharacterInputs = InputCmd.InputCollection.FindDataByType<
            FCharacterDefaultInputs>();
        if (CharacterInputs && CharacterInputs->bIsJumpJustPressed && CanActorJump())
        {
            Jump();
        }
    }

    // Handle ALS modifier management - only create/update when state changes
    ManageGaitModifier();
    ManageStanceModifier();
}

void UAlsCharacterMoverComponent::ManageGaitModifier()
{
    // Check if gait OR stance has changed - we need to recreate the modifier for either change
    // This ensures the speed is recalculated properly when stance changes
    if (CurrentGait != CachedGait || CurrentStance != CachedStance)
    {
        // Remove old modifier if it exists
        if (GaitModifierHandle.IsValid())
        {
            CancelModifierFromHandle(GaitModifierHandle);
            GaitModifierHandle.Invalidate();
        }

        // Create new gait modifier with current state
        TSharedPtr<FALSGaitModifier> NewGaitModifier = MakeShared<FALSGaitModifier>();
        NewGaitModifier->CurrentGait = CurrentGait;
        NewGaitModifier->CurrentStance = CurrentStance;
        NewGaitModifier->WalkSpeed = WalkSpeed;
        NewGaitModifier->RunSpeed = RunSpeed;
        NewGaitModifier->SprintSpeed = SprintSpeed;
        NewGaitModifier->CrouchSpeedMultiplier = CrouchSpeedMultiplier;

        GaitModifierHandle = QueueMovementModifier(NewGaitModifier);
        CachedGait = CurrentGait;
        CachedStance = CurrentStance;

    }
    else if (!GaitModifierHandle.IsValid())
    {
        // No modifier exists, create one
        TSharedPtr<FALSGaitModifier> NewGaitModifier = MakeShared<FALSGaitModifier>();
        NewGaitModifier->CurrentGait = CurrentGait;
        NewGaitModifier->CurrentStance = CurrentStance;
        NewGaitModifier->WalkSpeed = WalkSpeed;
        NewGaitModifier->RunSpeed = RunSpeed;
        NewGaitModifier->SprintSpeed = SprintSpeed;
        NewGaitModifier->CrouchSpeedMultiplier = CrouchSpeedMultiplier;

        GaitModifierHandle = QueueMovementModifier(NewGaitModifier);
        CachedGait = CurrentGait;
        CachedStance = CurrentStance;

    }
}

void UAlsCharacterMoverComponent::ManageStanceModifier()
{
    // Handle stance-related logic (capsule size changes, etc.)
    if (CurrentStance != CachedStance)
    {
        // Handle capsule size changes
        if (auto *PawnOwner = GetOwner())
        {
            if (UCapsuleComponent *Capsule = Cast<UCapsuleComponent>(PawnOwner->GetRootComponent()))
            {
                float TargetHalfHeight = (CurrentStance == AlsStanceTags::Crouching)
                                             ? CrouchingCapsuleHalfHeight
                                             : StandingCapsuleHalfHeight;

                Capsule->SetCapsuleHalfHeight(TargetHalfHeight);

                // Adjust mesh position if needed
                if (USkeletalMeshComponent *Mesh = PawnOwner->FindComponentByClass<USkeletalMeshComponent>())
                {
                    FVector MeshRelativeLoc = Mesh->GetRelativeLocation();
                    MeshRelativeLoc.Z = -TargetHalfHeight;
                    Mesh->SetRelativeLocation(MeshRelativeLoc);
                }
            }
        }

        // Update cached stance - this is now handled by ManageGaitModifier since it tracks both
    }
}

void UAlsCharacterMoverComponent::SetGait(const FGameplayTag &NewGait)
{
    if (CurrentGait != NewGait)
    {
        CurrentGait = NewGait;
        // Modifier management happens in OnMoverPreSimulationTick
    }
}

void UAlsCharacterMoverComponent::SetStance(const FGameplayTag &NewStance)
{
    if (CurrentStance != NewStance)
    {
        CurrentStance = NewStance;
        // Modifier management happens in OnMoverPreSimulationTick
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

