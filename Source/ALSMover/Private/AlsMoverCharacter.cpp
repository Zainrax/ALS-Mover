#include "AlsMoverCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "AlsCharacterMoverComponent.h"
#include "AlsStateLogicTransition.h"
#include "AlsMoverMovementSettings.h"
#include "AlsLayeredMoves.h"
#include "AlsMoverAnimationInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/HUD.h"
#include "DisplayDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverCharacter)

static TAutoConsoleVariable<int32> CVarShowMouseDebug(
    TEXT("ALS.ShowMouseDebug"),
    0,
    TEXT("Show mouse position debug visualization.\n")
    TEXT("0: Disabled\n")
    TEXT("1: Show in Tick only\n")
    TEXT("2: Show in DisplayDebug only\n")
    TEXT("3: Show in both"),
    ECVF_Cheat);

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

void AAlsMoverCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    AnimationInstance = Cast<UAlsMoverAnimationInstance>(GetMesh()->GetAnimInstance());
}

void AAlsMoverCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Register data types with mover component
    if (CharacterMover)
    {
        // Register our custom data types for networking and state management
        CharacterMover->PersistentSyncStateDataTypes.Add(FMoverDataPersistence(FAlsMoverInputs::StaticStruct(), true));
        CharacterMover->PersistentSyncStateDataTypes.Add(
            FMoverDataPersistence(FAlsMoverSyncState::StaticStruct(), true));

        // Log movement settings status
        if (MovementSettings)
        {
            UE_LOG(LogTemp, Log, TEXT("ALS Character: MovementSettings created successfully"));

            // Verify the mover component can find the settings
            if (const UAlsMoverMovementSettings *FoundSettings = CharacterMover->FindSharedSettings<
                UAlsMoverMovementSettings>())
            {
                UE_LOG(LogTemp, Log, TEXT("ALS Character: Movement settings found via FindSharedSettings"));
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ALS Character: Movement settings NOT found via FindSharedSettings"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ALS Character: MovementSettings is null!"));
        }

        UE_LOG(LogTemp, Log, TEXT("ALS Character: BeginPlay setup complete - Transitions=%d, DataTypes=%d"),
               CharacterMover->Transitions.Num(), CharacterMover->PersistentSyncStateDataTypes.Num());
    }
}

void AAlsMoverCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update mouse world position for top-down control
    if (APlayerController *PC = Cast<APlayerController>(GetController()))
    {
        FVector WorldLocation, WorldDirection;
        bool bHit = PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection);

        if (bHit)
        {
            // Trace to ground plane
            FVector TraceStart = WorldLocation;
            FVector TraceEnd = TraceStart + WorldDirection * 10000.0f;

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);

            if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
            {
                CachedMouseWorldPosition = HitResult.Location;
                bHasValidMouseTarget = true;
            }
            else
            {
                // Project to a fixed ground plane if no hit
                FPlane GroundPlane(FVector::ZeroVector, FVector::UpVector);
                FVector PlaneIntersection = FMath::LinePlaneIntersection(TraceStart, TraceEnd, GroundPlane);
                CachedMouseWorldPosition = PlaneIntersection;
                bHasValidMouseTarget = true;
            }
        }
        else
        {
            bHasValidMouseTarget = false;
        }
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
            EnhancedInputComponent->BindAction(InputActions.Walk, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnWalkStarted);
        }

        if (InputActions.Crouch)
        {
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

        if (InputActions.Roll)
        {
            EnhancedInputComponent->BindAction(InputActions.Roll, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnRollStarted);
        }

        if (InputActions.Mantle)
        {
            EnhancedInputComponent->BindAction(InputActions.Mantle, ETriggerEvent::Started, this,
                                               &AAlsMoverCharacter::OnMantleStarted);
        }
    }
}

void AAlsMoverCharacter::PossessedBy(AController *NewController)
{
    Super::PossessedBy(NewController);

    // Add input mapping context
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
    // Create or get ALS-specific inputs
    FAlsMoverInputs &AlsInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAlsMoverInputs>();

    // Copy cached input state
    AlsInputs.MoveInputVector = CachedMoveInputVector;
    AlsInputs.LookInputVector = CachedLookInputVector;
    AlsInputs.bIsSprintHeld = bWantsToSprint_Internal;

    // Handle event-based flags (consume them)
    AlsInputs.bWantsToToggleWalk = bWantsToToggleWalk_Internal;
    AlsInputs.bWantsToToggleCrouch = bWantsToToggleCrouch_Internal;
    AlsInputs.bIsAimingHeld = bWantsToAim_Internal;
    AlsInputs.bWantsToRoll = bWantsToRoll_Internal;
    AlsInputs.bWantsToMantle = bWantsToMantle_Internal;

    // Debug logging for input production
    if (AlsInputs.bWantsToToggleCrouch)
    {
        UE_LOG(LogTemp, Warning, TEXT("ALS ProduceInput: Crouch toggle detected, passing to Mover system"));
    }
    if (AlsInputs.bIsSprintHeld)
    {
        UE_LOG(LogTemp, Log, TEXT("ALS ProduceInput: Sprint held, passing to Mover system"));
    }
    if (AlsInputs.bWantsToToggleWalk)
    {
        UE_LOG(LogTemp, Warning, TEXT("ALS ProduceInput: Walk toggle detected, passing to Mover system"));
    }

    // Reset event flags after consuming them
    bWantsToToggleWalk_Internal = false;
    bWantsToToggleCrouch_Internal = false;
    bWantsToRoll_Internal = false;
    bWantsToMantle_Internal = false;

    // Mouse/gamepad detection
    AlsInputs.bHasValidMouseTarget = bHasValidMouseTarget && !bHasGamepadInput();
    AlsInputs.MouseWorldPosition = CachedMouseWorldPosition;

    // Handle action queuing in ProduceInput
    if (AlsInputs.bWantsToRoll && CharacterMover)
    {
        TSharedPtr<FLayeredMove_AlsRoll> RollMove = MakeShared<FLayeredMove_AlsRoll>();
        RollMove->RollDirection = CalculateRollDirection();
        RollMove->RollDistance = RollDistance;
        RollMove->RollDuration = RollDuration;
        RollMove->RollMontage = RollMontage;
        RollMove->bUseRootMotion = (RollMontage != nullptr);

        CharacterMover->QueueLayeredMove(RollMove);

        UE_LOG(LogTemp, Log, TEXT("ALS Character: Queued roll move in direction %s"),
               *RollMove->RollDirection.ToString());
    }

    if (AlsInputs.bWantsToMantle && CharacterMover)
    {
        FVector StartLocation, TargetLocation;
        float MantleHeight;

        if (TryFindMantleTarget(StartLocation, TargetLocation, MantleHeight))
        {
            TSharedPtr<FLayeredMove_AlsMantle> MantleMove = MakeShared<FLayeredMove_AlsMantle>();
            MantleMove->MantleStartLocation = StartLocation;
            MantleMove->MantleTargetLocation = TargetLocation;
            MantleMove->MantleHeight = MantleHeight;
            MantleMove->MantleDuration = FMath::GetMappedRangeValueClamped(
                FVector2D(50.0f, 200.0f), FVector2D(0.8f, 1.5f), MantleHeight);
            MantleMove->LowMantleMontage = LowMantleMontage;
            MantleMove->HighMantleMontage = HighMantleMontage;
            MantleMove->LowMantleThreshold = LowMantleThreshold;

            CharacterMover->QueueLayeredMove(MantleMove);

            UE_LOG(LogTemp, Log, TEXT("ALS Character: Queued mantle move from %s to %s (height %.1f)"),
                   *StartLocation.ToString(), *TargetLocation.ToString(), MantleHeight);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ALS Character: No valid mantle target found"));
        }
    }

    // Also populate standard character inputs for compatibility
    FCharacterDefaultInputs &CharInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<
        FCharacterDefaultInputs>();

    if (!CachedMoveInputVector.IsZero())
    {
        CharInputs.SetMoveInput(EMoveInputType::DirectionalIntent, CachedMoveInputVector);
    }

    CharInputs.bIsJumpPressed = bWantsToJump_Internal;
    CharInputs.bIsJumpJustPressed = bWantsToJump_Internal;

    // Calculate Orientation Intent for look rotation
    FVector LookDirection = FVector::ZeroVector;
    if (bHasGamepadInput())
    {
        // For gamepad input, convert look input to world direction
        // For a fixed top-down camera, map Y to X-axis and X to Y-axis
        if (!CachedLookInputVector.IsNearlyZero())
        {
            LookDirection = FVector(CachedLookInputVector.Y, CachedLookInputVector.X, 0.0f).GetSafeNormal();
        }
    }
    else if (bHasValidMouseTarget)
    {
        // For mouse input, calculate direction from character to mouse world position
        const FVector CurrentLocation = GetActorLocation();
        LookDirection = (CachedMouseWorldPosition - CurrentLocation).GetSafeNormal();
        // Zero out Z component to keep rotation on horizontal plane
        LookDirection.Z = 0.0f;
        LookDirection = LookDirection.GetSafeNormal();
    }

    if (!LookDirection.IsNearlyZero())
    {
        CharInputs.OrientationIntent = LookDirection;
    }
    else
    {
        // If no look input, maintain current forward direction
        CharInputs.OrientationIntent = GetActorForwardVector();
    }

    // Set control rotation
    if (Controller)
    {
        CharInputs.ControlRotation = Controller->GetControlRotation();
    }
}

//========================================================================
// State Accessors (Read-Only, from Mover)
//========================================================================

FGameplayTag AAlsMoverCharacter::GetStance() const
{
    if (CharacterMover)
    {
        return CharacterMover->GetStance();
    }
    return AlsStanceTags::Standing;
}

FGameplayTag AAlsMoverCharacter::GetGait() const
{
    if (CharacterMover)
    {
        return CharacterMover->GetGait();
    }
    return AlsGaitTags::Running;
}

FGameplayTag AAlsMoverCharacter::GetRotationMode() const
{
    if (CharacterMover)
    {
        return CharacterMover->GetRotationMode();
    }
    return AlsRotationModeTags::VelocityDirection;
}

FGameplayTag AAlsMoverCharacter::GetLocomotionMode() const
{
    if (CharacterMover)
    {
        return CharacterMover->GetLocomotionMode();
    }
    return AlsLocomotionModeTags::Grounded;
}

FString AAlsMoverCharacter::GetDebugInfo() const
{
    FString DebugInfo = FString::Printf(TEXT("ALS Mover Character Debug Info:\n"));
    DebugInfo += FString::Printf(TEXT("  Stance: %s\n"), *GetStance().ToString());
    DebugInfo += FString::Printf(TEXT("  Gait: %s\n"), *GetGait().ToString());
    DebugInfo += FString::Printf(TEXT("  Rotation Mode: %s\n"), *GetRotationMode().ToString());
    DebugInfo += FString::Printf(TEXT("  Locomotion Mode: %s\n"), *GetLocomotionMode().ToString());

    if (CharacterMover)
    {
        DebugInfo += FString::Printf(TEXT("  Velocity: %s (Speed: %.1f)\n"),
                                     *CharacterMover->GetVelocity().ToString(),
                                     CharacterMover->GetSpeed());
        DebugInfo += FString::Printf(TEXT("  Movement Mode: %s\n"),
                                     *CharacterMover->GetMovementModeName().ToString());
    }

    DebugInfo += FString::Printf(TEXT("  Input - Move: %s, Jump: %s, Sprint: %s, Walk: %s\n"),
                                 *CachedMoveInputVector.ToString(),
                                 bWantsToJump_Internal ? TEXT("Yes") : TEXT("No"),
                                 bWantsToSprint_Internal ? TEXT("Yes") : TEXT("No"),
                                 bWalkToggled_Internal ? TEXT("ON") : TEXT("OFF"));

    return DebugInfo;
}

FRotator AAlsMoverCharacter::GetControlRotation() const
{
    if (Controller)
    {
        return Controller->GetControlRotation();
    }
    return FRotator::ZeroRotator;
}

//========================================================================
// Input Event Handlers (Pure - Only Set Flags)
//========================================================================

void AAlsMoverCharacter::OnMoveTriggered(const FInputActionValue &Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    CachedMoveInputVector = FVector(MovementVector.Y, MovementVector.X, 0.0f);
}

void AAlsMoverCharacter::OnMoveCompleted(const FInputActionValue &Value)
{
    CachedMoveInputVector = FVector::ZeroVector;
}

void AAlsMoverCharacter::OnLookTriggered(const FInputActionValue &Value)
{
    const FVector2D LookVector = Value.Get<FVector2D>();
    CachedLookInputVector = FVector(LookVector.X, LookVector.Y, 0.0f);
}

void AAlsMoverCharacter::OnLookCompleted(const FInputActionValue &Value)
{
    CachedLookInputVector = FVector::ZeroVector;
}

void AAlsMoverCharacter::OnRunStarted(const FInputActionValue &Value)
{
    bWantsToSprint_Internal = true;
}

void AAlsMoverCharacter::OnRunCompleted(const FInputActionValue &Value)
{
    bWantsToSprint_Internal = false;
}

void AAlsMoverCharacter::OnWalkStarted(const FInputActionValue &Value)
{
    bWantsToToggleWalk_Internal = true; // Will be consumed in ProduceInput
}

void AAlsMoverCharacter::OnWalkCompleted(const FInputActionValue &Value)
{
    // Walk is toggle-based, nothing to do on release
}

void AAlsMoverCharacter::OnCrouchStarted(const FInputActionValue &Value)
{
    UE_LOG(LogTemp, Log, TEXT("ALS Character: Crouch started"));
    bWantsToToggleCrouch_Internal = true; // Will be consumed in ProduceInput
}

void AAlsMoverCharacter::OnJumpStarted(const FInputActionValue &Value)
{
    bWantsToJump_Internal = true;
}

void AAlsMoverCharacter::OnJumpCompleted(const FInputActionValue &Value)
{
    bWantsToJump_Internal = false;
}

void AAlsMoverCharacter::OnAimStarted(const FInputActionValue &Value)
{
    bWantsToAim_Internal = true;
}

void AAlsMoverCharacter::OnAimCompleted(const FInputActionValue &Value)
{
    bWantsToAim_Internal = false;
}

void AAlsMoverCharacter::OnRollStarted(const FInputActionValue &Value)
{
    bWantsToRoll_Internal = true; // Will be consumed in ProduceInput
}

void AAlsMoverCharacter::OnRollCompleted(const FInputActionValue &Value)
{
    // Roll is event-based, nothing to do on release
}

void AAlsMoverCharacter::OnMantleStarted(const FInputActionValue &Value)
{
    bWantsToMantle_Internal = true; // Will be consumed in ProduceInput
}

void AAlsMoverCharacter::OnMantleCompleted(const FInputActionValue &Value)
{
    // Mantle is event-based, nothing to do on release
}

//========================================================================
// Helper Functions
//========================================================================

void AAlsMoverCharacter::SetupMoverComponent()
{
    // Component setup handled in constructor and BeginPlay
}

FVector AAlsMoverCharacter::CalculateRollDirection() const
{
    // Use movement input direction if available
    if (!CachedMoveInputVector.IsNearlyZero())
    {
        return CachedMoveInputVector.GetSafeNormal();
    }

    // Use forward direction if no input
    return GetActorForwardVector();
}

bool AAlsMoverCharacter::TryFindMantleTarget(FVector &OutStartLocation, FVector &OutTargetLocation,
                                             float &OutHeight) const
{
    // Simple mantle trace - in a full implementation, this would be more sophisticated
    FVector StartLocation = GetActorLocation();
    FVector ForwardDirection = GetActorForwardVector();

    // Trace forward to find wall
    FVector WallTraceStart = StartLocation;
    FVector WallTraceEnd = WallTraceStart + ForwardDirection * MantleTraceDistance;

    FHitResult WallHit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    if (!GetWorld()->LineTraceSingleByChannel(WallHit, WallTraceStart, WallTraceEnd, ECC_WorldStatic, QueryParams))
    {
        return false; // No wall found
    }

    // Trace up from wall hit to find ledge
    FVector LedgeTraceStart = WallHit.Location + FVector::UpVector * 200.0f;
    FVector LedgeTraceEnd = LedgeTraceStart - FVector::UpVector * 300.0f;

    FHitResult LedgeHit;
    if (!GetWorld()->LineTraceSingleByChannel(LedgeHit, LedgeTraceStart, LedgeTraceEnd, ECC_WorldStatic, QueryParams))
    {
        return false; // No ledge found
    }

    // Calculate mantle parameters
    OutStartLocation = StartLocation;
    OutTargetLocation = LedgeHit.Location + ForwardDirection * 50.0f; // Move forward a bit from ledge
    OutHeight = OutTargetLocation.Z - OutStartLocation.Z;

    // Check if height is reasonable for mantling
    return OutHeight > 50.0f && OutHeight < 250.0f;
}

void AAlsMoverCharacter::ToggleRotationMode()
{
    // Debug function - in the new system, rotation modes are managed by state transitions
    UE_LOG(LogTemp, Log, TEXT("ALS Character: Rotation mode changes are now handled by the Mover state system"));
}

void AAlsMoverCharacter::DisplayDebug(UCanvas *Canvas, const FDebugDisplayInfo &DebugDisplay, float &YL, float &YPos)
{
    Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

    if (!Canvas || !CharacterMover)
    {
        return;
    }

    // Static variables to track our debug categories
    static const FName NAME_AlsMover("AlsMover");
    static const FName NAME_AlsMoverInput("AlsMover.Input");
    static const FName NAME_AlsMoverStates("AlsMover.States");
    static const FName NAME_AlsMoverMovement("AlsMover.Movement");

    const bool bShowAlsMover = DebugDisplay.IsDisplayOn(NAME_AlsMover);
    const bool bShowInput = DebugDisplay.IsDisplayOn(NAME_AlsMoverInput);
    const bool bShowStates = DebugDisplay.IsDisplayOn(NAME_AlsMoverStates);
    const bool bShowMovement = DebugDisplay.IsDisplayOn(NAME_AlsMoverMovement);

    if (!bShowAlsMover && !bShowInput && !bShowStates && !bShowMovement)
    {
        return;
    }

    UFont *Font = GEngine->GetMediumFont();
    const FColor HeaderColor = FColor::Green;
    const FColor ValueColor = FColor::White;

    // Draw header
    Canvas->SetDrawColor(HeaderColor);
    Canvas->DrawText(Font, TEXT("=== ALS MOVER DEBUG (REFACTORED) ==="), 4.0f, YPos);
    YPos += YL;

    // Show states (read from Mover)
    if (bShowAlsMover || bShowStates)
    {
        Canvas->SetDrawColor(HeaderColor);
        Canvas->DrawText(Font, TEXT("[States - From Mover]"), 4.0f, YPos);
        YPos += YL;

        Canvas->SetDrawColor(ValueColor);
        Canvas->DrawText(Font, FString::Printf(TEXT("Stance: %s"), *GetStance().ToString()), 20.0f, YPos);
        YPos += YL;
        Canvas->DrawText(Font, FString::Printf(TEXT("Gait: %s"), *GetGait().ToString()), 20.0f, YPos);
        YPos += YL;
        Canvas->DrawText(Font, FString::Printf(TEXT("Rotation Mode: %s"), *GetRotationMode().ToString()), 20.0f, YPos);
        YPos += YL;
    }

    // Show input state (internal flags)
    if (bShowAlsMover || bShowInput)
    {
        Canvas->SetDrawColor(HeaderColor);
        Canvas->DrawText(Font, TEXT("[Input State - Internal Flags]"), 4.0f, YPos);
        YPos += YL;

        Canvas->SetDrawColor(ValueColor);
        Canvas->DrawText(Font, FString::Printf(TEXT("Move Input: %s"), *CachedMoveInputVector.ToString()), 20.0f, YPos);
        YPos += YL;
        Canvas->DrawText(Font, FString::Printf(TEXT("Sprint: %s | Current Gait: %s | Current Stance: %s"),
                                               bWantsToSprint_Internal ? TEXT("ON") : TEXT("OFF"),
                                               *GetGait().ToString(),
                                               *GetStance().ToString()), 20.0f, YPos);
        YPos += YL;
    }

    // Show movement info
    if (bShowAlsMover || bShowMovement)
    {
        Canvas->SetDrawColor(HeaderColor);
        Canvas->DrawText(Font, TEXT("[Movement - From Mover]"), 4.0f, YPos);
        YPos += YL;

        Canvas->SetDrawColor(ValueColor);
        if (CharacterMover)
        {
            Canvas->DrawText(Font, FString::Printf(TEXT("Speed: %.1f"), CharacterMover->GetSpeed()), 20.0f, YPos);
            YPos += YL;
            Canvas->DrawText(Font, FString::Printf(TEXT("Velocity: %s"), *CharacterMover->GetVelocity().ToString()),
                             20.0f, YPos);
            YPos += YL;
        }
    }
}