#include "AlsMoverCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "AlsCharacterMoverComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "AlsGroundMovementMode.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "AlsMoverMovementSettings.h"
#include "AlsMovementModifiers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverCharacter)

//========================================================================
// FAlsMoverInputs Implementation
//========================================================================

bool FAlsMoverInputs::NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess)
{
    Super::NetSerialize(Ar, Map, bOutSuccess);

    Ar << MoveInputVector;
    Ar << LookInputVector;

    uint8 InputFlags = 0;
    if (Ar.IsSaving())
    {
        InputFlags = (bWantsToRun ? 1 : 0) |
                     (bWantsToWalk ? 2 : 0) |
                     (bWantsToCrouch ? 4 : 0) |
                     (bWantsToJump ? 8 : 0) |
                     (bWantsToAim ? 16 : 0);
    }

    Ar << InputFlags;

    if (Ar.IsLoading())
    {
        bWantsToRun = (InputFlags & 1) != 0;
        bWantsToWalk = (InputFlags & 2) != 0;
        bWantsToCrouch = (InputFlags & 4) != 0;
        bWantsToJump = (InputFlags & 8) != 0;
        bWantsToAim = (InputFlags & 16) != 0;
    }

    bOutSuccess = true;
    return true;
}

void FAlsMoverInputs::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("MoveInput=(%f,%f,%f) LookInput=(%f,%f,%f) Run=%d Walk=%d Crouch=%d Jump=%d Aim=%d",
                MoveInputVector.X, MoveInputVector.Y, MoveInputVector.Z,
                LookInputVector.X, LookInputVector.Y, LookInputVector.Z,
                bWantsToRun ? 1 : 0, bWantsToWalk ? 1 : 0, bWantsToCrouch ? 1 : 0,
                bWantsToJump ? 1 : 0, bWantsToAim ? 1 : 0);
}

bool FAlsMoverInputs::ShouldReconcile(const FMoverDataStructBase &AuthorityState) const
{
    const FAlsMoverInputs &AuthInputs = static_cast<const FAlsMoverInputs &>(AuthorityState);
    return !MoveInputVector.Equals(AuthInputs.MoveInputVector, KINDA_SMALL_NUMBER) ||
           !LookInputVector.Equals(AuthInputs.LookInputVector, KINDA_SMALL_NUMBER) ||
           bWantsToRun != AuthInputs.bWantsToRun ||
           bWantsToWalk != AuthInputs.bWantsToWalk ||
           bWantsToCrouch != AuthInputs.bWantsToCrouch ||
           bWantsToJump != AuthInputs.bWantsToJump ||
           bWantsToAim != AuthInputs.bWantsToAim;
}

void FAlsMoverInputs::Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct)
{
    const FAlsMoverInputs &FromInputs = static_cast<const FAlsMoverInputs &>(From);
    const FAlsMoverInputs &ToInputs = static_cast<const FAlsMoverInputs &>(To);

    MoveInputVector = FMath::Lerp(FromInputs.MoveInputVector, ToInputs.MoveInputVector, Pct);
    LookInputVector = FMath::Lerp(FromInputs.LookInputVector, ToInputs.LookInputVector, Pct);

    // Boolean values don't interpolate, use the "To" values
    bWantsToRun = ToInputs.bWantsToRun;
    bWantsToWalk = ToInputs.bWantsToWalk;
    bWantsToCrouch = ToInputs.bWantsToCrouch;
    bWantsToJump = ToInputs.bWantsToJump;
    bWantsToAim = ToInputs.bWantsToAim;
}

//========================================================================
// FAlsMoverSyncState Implementation
//========================================================================

bool FAlsMoverSyncState::NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess)
{
    Super::NetSerialize(Ar, Map, bOutSuccess);

    Ar << CurrentStance;
    Ar << CurrentGait;
    Ar << CurrentRotationMode;
    Ar << CurrentLocomotionMode;

    bOutSuccess = true;
    return true;
}

void FAlsMoverSyncState::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("Stance=%s Gait=%s Rotation=%s Locomotion=%s",
                *CurrentStance.ToString(),
                *CurrentGait.ToString(),
                *CurrentRotationMode.ToString(),
                *CurrentLocomotionMode.ToString());
}

bool FAlsMoverSyncState::ShouldReconcile(const FMoverDataStructBase &AuthorityState) const
{
    const FAlsMoverSyncState &AuthState = static_cast<const FAlsMoverSyncState &>(AuthorityState);
    return CurrentStance != AuthState.CurrentStance ||
           CurrentGait != AuthState.CurrentGait ||
           CurrentRotationMode != AuthState.CurrentRotationMode ||
           CurrentLocomotionMode != AuthState.CurrentLocomotionMode;
}

void FAlsMoverSyncState::Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct)
{
    // State tags don't interpolate - use the "To" state
    const FAlsMoverSyncState &ToState = static_cast<const FAlsMoverSyncState &>(To);
    CurrentStance = ToState.CurrentStance;
    CurrentGait = ToState.CurrentGait;
    CurrentRotationMode = ToState.CurrentRotationMode;
    CurrentLocomotionMode = ToState.CurrentLocomotionMode;
}

//========================================================================
// AAlsMoverCharacter Implementation
//========================================================================

AAlsMoverCharacter::AAlsMoverCharacter(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set this pawn to call Tick() every frame
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // Create components
    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
    RootComponent = CapsuleComponent;
    CapsuleComponent->SetCapsuleHalfHeight(92.0f);
    CapsuleComponent->SetCapsuleRadius(34.0f);
    CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(CapsuleComponent);
    Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -92.0f));
    Mesh->SetCollisionProfileName(TEXT("CharacterMesh"));

    CharacterMover = CreateDefaultSubobject<UAlsCharacterMoverComponent>(TEXT("CharacterMover"));

    // Create ALS movement settings with proper defaults
    MovementSettings = CreateDefaultSubobject<UAlsMoverMovementSettings>(TEXT("MovementSettings"));

    // Disable automatic replication - Mover handles this
    SetReplicatingMovement(false);

    // Setup input data types
    SetupMoverComponent();
}

void AAlsMoverCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Our AlsCharacterMoverComponent handles all the setup in its BeginPlay
    // We just need to initialize the sync state
    if (CharacterMover)
    {
        // Initialize sync state
        FAlsMoverSyncState InitialAlsState;
        InitialAlsState.CurrentStance = AlsStanceTags::Standing;
        InitialAlsState.CurrentGait = AlsGaitTags::Running;
        InitialAlsState.CurrentRotationMode = AlsRotationModeTags::VelocityDirection;
        InitialAlsState.CurrentLocomotionMode = AlsLocomotionModeTags::Grounded;

        SetAlsSyncState(InitialAlsState);

        // Set initial states in the mover component
        CharacterMover->SetGait(AlsGaitTags::Running);
        CharacterMover->SetStance(AlsStanceTags::Standing);
    }
}

void AAlsMoverCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent *EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Bind movement actions
        if (InputActions.Move)
        {
            EnhancedInputComponent->BindAction(InputActions.Move, ETriggerEvent::Triggered, this,
                                               &AAlsMoverCharacter::OnMoveTriggered);
            EnhancedInputComponent->BindAction(InputActions.Move, ETriggerEvent::Completed, this,
                                               &AAlsMoverCharacter::OnMoveCompleted);
        }

        if (InputActions.Look)
        {
            EnhancedInputComponent->BindAction(InputActions.Look, ETriggerEvent::Triggered, this,
                                               &AAlsMoverCharacter::OnLookTriggered);
            EnhancedInputComponent->BindAction(InputActions.Look, ETriggerEvent::Completed, this,
                                               &AAlsMoverCharacter::OnLookCompleted);
        }

        if (InputActions.Run)
        {
            EnhancedInputComponent->BindAction(InputActions.Run, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnRunStarted);
            EnhancedInputComponent->BindAction(InputActions.Run, ETriggerEvent::Completed, this,
                                               &AAlsMoverCharacter::OnRunCompleted);
        }

        if (InputActions.Walk)
        {
            // Only bind to Started for toggle behavior
            EnhancedInputComponent->BindAction(InputActions.Walk, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnWalkStarted);
        }

        if (InputActions.Crouch)
        {
            // Only bind to Started for toggle behavior
            EnhancedInputComponent->BindAction(InputActions.Crouch, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnCrouchStarted);
        }

        if (InputActions.Jump)
        {
            EnhancedInputComponent->BindAction(InputActions.Jump, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnJumpStarted);
            EnhancedInputComponent->BindAction(InputActions.Jump, ETriggerEvent::Completed, this,
                                               &AAlsMoverCharacter::OnJumpCompleted);
        }

        if (InputActions.Aim)
        {
            EnhancedInputComponent->BindAction(InputActions.Aim, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnAimStarted);
            EnhancedInputComponent->BindAction(InputActions.Aim, ETriggerEvent::Completed, this,
                                               &AAlsMoverCharacter::OnAimCompleted);
        }
    }
}

void AAlsMoverCharacter::PossessedBy(AController *NewController)
{
    Super::PossessedBy(NewController);

    // Setup input mapping context
    if (APlayerController *PC = Cast<APlayerController>(NewController))
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<
            UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (InputMappingContext)
            {
                Subsystem->AddMappingContext(InputMappingContext, 0);
            }
        }
    }
}

void AAlsMoverCharacter::UnPossessed()
{
    // Remove input mapping context
    if (APlayerController *PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem = ULocalPlayer::GetSubsystem<
            UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (InputMappingContext)
            {
                Subsystem->RemoveMappingContext(InputMappingContext);
            }
        }
    }

    Super::UnPossessed();
}

void AAlsMoverCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext &InputCmdResult)

{
    OnProduceInput(static_cast<float>(SimTimeMs), InputCmdResult);
}

void AAlsMoverCharacter::OnProduceInput(float DeltaMs, FMoverInputCmdContext &InputCmdResult)
{
    // Create or get ALS-specific inputs
    FAlsMoverInputs &AlsInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAlsMoverInputs>();

    // Copy cached input state
    AlsInputs.MoveInputVector = CachedMoveInputVector;
    AlsInputs.LookInputVector = CachedLookInputVector;
    AlsInputs.bWantsToRun = bCachedWantsToSprint;
    AlsInputs.bWantsToWalk = bIsWalkToggled;
    AlsInputs.bWantsToCrouch = bIsCrouchToggled;
    AlsInputs.bWantsToJump = bCachedWantsToJump;
    AlsInputs.bWantsToAim = bCachedWantsToAim;

    // Also populate standard character inputs for compatibility with base movement modes
    FCharacterDefaultInputs &CharInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<
        FCharacterDefaultInputs>();

    if (!CachedMoveInputVector.IsZero())
    {
        CharInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputVector);
    }

    CharInputs.bIsJumpPressed = bCachedWantsToJump;
    CharInputs.bIsJumpJustPressed = bCachedWantsToJump;


    // Handle look input for rotation
    if (Controller)
    {
        if (APlayerController *PC = Cast<APlayerController>(Controller))
        {
            if (!CachedLookInputVector.IsZero())
            {
                PC->AddYawInput(CachedLookInputVector.X);
                PC->AddPitchInput(CachedLookInputVector.Y);
            }
        }
        CharInputs.ControlRotation = Controller->GetControlRotation();
    }
}

//========================================================================
// State Accessors
//========================================================================

FGameplayTag AAlsMoverCharacter::GetStance() const
{
    if (const FAlsMoverSyncState *AlsState = GetAlsSyncState())
    {
        return AlsState->CurrentStance;
    }
    return AlsStanceTags::Standing;
}

FGameplayTag AAlsMoverCharacter::GetGait() const
{
    if (const FAlsMoverSyncState *AlsState = GetAlsSyncState())
    {
        return AlsState->CurrentGait;
    }
    return AlsGaitTags::Running;
}

FGameplayTag AAlsMoverCharacter::GetRotationMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetAlsSyncState())
    {
        return AlsState->CurrentRotationMode;
    }
    return AlsRotationModeTags::VelocityDirection;
}

FGameplayTag AAlsMoverCharacter::GetLocomotionMode() const
{
    if (const FAlsMoverSyncState *AlsState = GetAlsSyncState())
    {
        return AlsState->CurrentLocomotionMode;
    }
    return AlsLocomotionModeTags::Grounded;
}

FString AAlsMoverCharacter::GetDebugInfo() const
{
    FString DebugInfo = FString::Printf(TEXT("ALS Mover Debug Info:\n"));
    DebugInfo += FString::Printf(TEXT("  Stance: %s\n"), *GetStance().ToString());
    DebugInfo += FString::Printf(TEXT("  Gait: %s\n"), *GetGait().ToString());
    DebugInfo += FString::Printf(TEXT("  Rotation Mode: %s\n"), *GetRotationMode().ToString());
    DebugInfo += FString::Printf(TEXT("  Locomotion Mode: %s\n"), *GetLocomotionMode().ToString());

    if (CharacterMover)
    {
        DebugInfo += FString::Printf(TEXT("  Velocity: %s (Speed: %.1f)\n"),
                                     *CharacterMover->GetVelocity().ToString(),
                                     CharacterMover->GetVelocity().Size());
        DebugInfo += FString::Printf(TEXT("  Movement Mode: %s\n"),
                                     *CharacterMover->GetMovementModeName().ToString());

        // Show current max speed from settings
        if (const UCommonLegacyMovementSettings *Settings = CharacterMover->FindSharedSettings<
            UCommonLegacyMovementSettings>())
        {
            DebugInfo += FString::Printf(TEXT("  MaxSpeed Setting: %.1f\n"), Settings->MaxSpeed);
        }

        // Show basic movement info
    }

    DebugInfo += FString::Printf(TEXT("  Input - Move: %s, Jump: %s, Sprint: %s, Walk: %s\n"),
                                 *CachedMoveInputVector.ToString(),
                                 bCachedWantsToJump ? TEXT("Yes") : TEXT("No"),
                                 bCachedWantsToSprint ? TEXT("Yes") : TEXT("No"),
                                 bIsWalkToggled ? TEXT("ON") : TEXT("OFF"));

    return DebugInfo;
}

//========================================================================
// State Setters
//========================================================================

void AAlsMoverCharacter::SetStance(const FGameplayTag &NewStance)
{
    if (CharacterMover)
    {
        // Let the mover component handle the stance change
        CharacterMover->SetStance(NewStance);

        // Update sync state
        if (FAlsMoverSyncState *AlsState = GetAlsSyncState())
        {
            AlsState->CurrentStance = NewStance;
            SetAlsSyncState(*AlsState);
        }
    }
}

void AAlsMoverCharacter::SetGait(const FGameplayTag &NewGait)
{
    if (CharacterMover)
    {
        // Let the mover component handle the gait change
        CharacterMover->SetGait(NewGait);

        // Update sync state
        if (FAlsMoverSyncState *AlsState = GetAlsSyncState())
        {
            AlsState->CurrentGait = NewGait;
            SetAlsSyncState(*AlsState);
        }
    }
}

void AAlsMoverCharacter::SetRotationMode(const FGameplayTag &NewRotationMode)
{
    if (FAlsMoverSyncState *AlsState = GetAlsSyncState())
    {
        if (AlsState->CurrentRotationMode != NewRotationMode)
        {
            AlsState->CurrentRotationMode = NewRotationMode;
            SetAlsSyncState(*AlsState);
        }
    }
}

//========================================================================
// Input Event Handlers
//========================================================================

void AAlsMoverCharacter::OnMoveTriggered(const FInputActionValue &Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    CachedMoveInputVector = FVector(MovementVector.Y, MovementVector.X, 0.0f);
}

void AAlsMoverCharacter::OnMoveCompleted(const FInputActionValue &Value)
{
    // Clear movement input when keys are released
    CachedMoveInputVector = FVector::ZeroVector;
}

void AAlsMoverCharacter::OnLookTriggered(const FInputActionValue &Value)
{
    const FVector2D LookVector = Value.Get<FVector2D>();
    CachedLookInputVector = FVector(LookVector.X, LookVector.Y, 0.0f);
}

void AAlsMoverCharacter::OnLookCompleted(const FInputActionValue &Value)
{
    // Clear look input when released (for continuous look input)
    CachedLookInputVector = FVector::ZeroVector;
}

void AAlsMoverCharacter::OnRunStarted(const FInputActionValue &Value)
{
    bCachedWantsToSprint = true;
    UpdateGaitFromInput();
}

void AAlsMoverCharacter::OnRunCompleted(const FInputActionValue &Value)
{
    bCachedWantsToSprint = false;
    UpdateGaitFromInput();
}

void AAlsMoverCharacter::OnWalkStarted(const FInputActionValue &Value)
{
    bIsWalkToggled = !bIsWalkToggled;
    UpdateGaitFromInput();
}

void AAlsMoverCharacter::OnWalkCompleted(const FInputActionValue &Value)
{
    // Walk is toggle-based, so we don't need to do anything on release
}

void AAlsMoverCharacter::OnCrouchStarted(const FInputActionValue &Value)
{
    bIsCrouchToggled = !bIsCrouchToggled;

    if (bIsCrouchToggled)
    {
        SetStance(AlsStanceTags::Crouching);
    }
    else
    {
        SetStance(AlsStanceTags::Standing);
    }
}

void AAlsMoverCharacter::OnCrouchCompleted(const FInputActionValue &Value)
{
    // Crouch is toggle-based, so we don't need to do anything on release
}

void AAlsMoverCharacter::OnJumpStarted(const FInputActionValue &Value)
{
    bCachedWantsToJump = true;
}

void AAlsMoverCharacter::OnJumpCompleted(const FInputActionValue &Value)
{
    bCachedWantsToJump = false;
}

void AAlsMoverCharacter::OnAimStarted(const FInputActionValue &Value)
{
    bCachedWantsToAim = true;
}

void AAlsMoverCharacter::OnAimCompleted(const FInputActionValue &Value)
{
    bCachedWantsToAim = false;
}

//========================================================================
// Helper Functions
//========================================================================

void AAlsMoverCharacter::SetupMoverComponent()
{
    // Movement modes should be set up in BeginPlay or through Blueprint configuration
    // The CharacterMoverComponent expects movement modes to be configured before runtime
}

void AAlsMoverCharacter::UpdateGaitFromInput()
{
    FGameplayTag DesiredGait = AlsGaitTags::Running; // Default gait

    // Walk toggle takes priority
    if (bIsWalkToggled)
    {
        DesiredGait = AlsGaitTags::Walking;
    }
    // Sprint only when sprinting and moving
    else if (bCachedWantsToSprint && !CachedMoveInputVector.IsZero())
    {
        DesiredGait = AlsGaitTags::Sprinting;
    }
    // Otherwise use default Running gait


    // Update gait
    SetGait(DesiredGait);
}


FAlsMoverSyncState *AAlsMoverCharacter::GetAlsSyncState() const
{
    if (CharacterMover)
    {
        // Note: const_cast needed since we need to modify the data, but GetSyncState() returns const reference
        FMoverSyncState &SyncState = const_cast<FMoverSyncState &>(CharacterMover->GetSyncState());
        return SyncState.SyncStateCollection.FindMutableDataByType<FAlsMoverSyncState>();
    }
    return nullptr;
}

bool AAlsMoverCharacter::SetAlsSyncState(const FAlsMoverSyncState &NewState)
{
    if (FAlsMoverSyncState *CurrentState = GetAlsSyncState())
    {
        *CurrentState = NewState;
        return true;
    }
    return false;
}