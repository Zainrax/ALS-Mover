#include "AlsCharacterExample.h"

#include "AlsCameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Utility/AlsVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacterExample)

AAlsCharacterExample::AAlsCharacterExample()
{
    Camera = CreateDefaultSubobject<UAlsCameraComponent>(FName{TEXTVIEW("Camera")});
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(FName{TEXTVIEW("SpringArm")});
    Camera->SetupAttachment(SpringArm);
    Camera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}

void AAlsCharacterExample::NotifyControllerChanged()
{
    const auto *PreviousPlayer{Cast<APlayerController>(PreviousController)};
    if (IsValid(PreviousPlayer))
    {
        auto *InputSubsystem{
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PreviousPlayer->GetLocalPlayer())};
        if (IsValid(InputSubsystem))
        {
            InputSubsystem->RemoveMappingContext(InputMappingContext);
        }
    }

    auto *NewPlayer{Cast<APlayerController>(GetController())};
    if (IsValid(NewPlayer))
    {
        NewPlayer->InputYawScale_DEPRECATED = 1.0f;
        NewPlayer->InputPitchScale_DEPRECATED = 1.0f;
        NewPlayer->InputRollScale_DEPRECATED = 1.0f;

        auto *InputSubsystem{
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(NewPlayer->GetLocalPlayer())};
        if (IsValid(InputSubsystem))
        {
            FModifyContextOptions Options;
            Options.bNotifyUserSettings = true;

            InputSubsystem->AddMappingContext(InputMappingContext, 0, Options);
        }
    }

    Super::NotifyControllerChanged();
}

void AAlsCharacterExample::CalcCamera(const float DeltaTime, FMinimalViewInfo &ViewInfo)
{
    if (Camera->IsActive())
    {
        Camera->GetViewInfo(ViewInfo);
        return;
    }

    Super::CalcCamera(DeltaTime, ViewInfo);
}

void AAlsCharacterExample::SetupPlayerInputComponent(UInputComponent *Input)
{
    Super::SetupPlayerInputComponent(Input);

    auto *EnhancedInput{Cast<UEnhancedInputComponent>(Input)};
    if (IsValid(EnhancedInput))
    {
        EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLookMouse);
        EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnLookMouse);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLook);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnLook);
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnMove);
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnMove);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnSprint);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnSprint);
        EnhancedInput->BindAction(WalkAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnWalk);
        EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnCrouch);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnJump);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnJump);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnAim);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnAim);
        EnhancedInput->BindAction(RagdollAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRagdoll);
        EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRoll);
        EnhancedInput->BindAction(RotationModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRotationMode);
        EnhancedInput->BindAction(ViewModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnViewMode);
        EnhancedInput->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this,
                                  &ThisClass::Input_OnSwitchShoulder);
    }
}

void AAlsCharacterExample::Input_OnLookMouse(const FInputActionValue &ActionValue)
{
    const FVector2f Value{ActionValue.Get<FVector2D>()};

    AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
    AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void AAlsCharacterExample::Input_OnLook(const FInputActionValue &ActionValue)
{
    const FVector2f Value{ActionValue.Get<FVector2D>()};

    AddControllerPitchInput(Value.Y * LookUpRate);
    AddControllerYawInput(Value.X * LookRightRate);
}

void AAlsCharacterExample::Input_OnMove(const FInputActionValue &ActionValue)
{
    const auto Value{UAlsVector::ClampMagnitude012D(ActionValue.Get<FVector2D>())};

    auto ViewRotation{GetViewState().Rotation};

    if (IsValid(GetController()))
    {
        // Use exact camera rotation instead of target rotation whenever possible.

        FVector ViewLocation;
        GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }

    const auto ForwardDirection{UAlsVector::AngleToDirectionXY(UE_REAL_TO_FLOAT(ViewRotation.Yaw))};
    const auto RightDirection{UAlsVector::PerpendicularCounterClockwiseXY(ForwardDirection)};

    AddMovementInput(ForwardDirection * Value.Y + RightDirection * Value.X);
}

void AAlsCharacterExample::Input_OnSprint(const FInputActionValue &ActionValue)
{
    SetDesiredGait(ActionValue.Get<bool>() ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void AAlsCharacterExample::Input_OnWalk()
{
    if (GetDesiredGait() == AlsGaitTags::Walking)
    {
        SetDesiredGait(AlsGaitTags::Running);
    }
    else if (GetDesiredGait() == AlsGaitTags::Running)
    {
        SetDesiredGait(AlsGaitTags::Walking);
    }
}

void AAlsCharacterExample::Input_OnCrouch()
{
    if (GetDesiredStance() == AlsStanceTags::Standing)
    {
        SetDesiredStance(AlsStanceTags::Crouching);
    }
    else if (GetDesiredStance() == AlsStanceTags::Crouching)
    {
        SetDesiredStance(AlsStanceTags::Standing);
    }
}

void AAlsCharacterExample::Input_OnJump(const FInputActionValue &ActionValue)
{
    if (ActionValue.Get<bool>())
    {
        if (StopRagdolling())
        {
            return;
        }

        if (StartMantlingGrounded())
        {
            return;
        }

        if (GetStance() == AlsStanceTags::Crouching)
        {
            SetDesiredStance(AlsStanceTags::Standing);
            return;
        }

        Jump();
    }
    else
    {
        StopJumping();
    }
}

void AAlsCharacterExample::Input_OnAim(const FInputActionValue &ActionValue)
{
    SetDesiredAiming(ActionValue.Get<bool>());
}

void AAlsCharacterExample::Input_OnRagdoll()
{
    if (!StopRagdolling())
    {
        StartRagdolling();
    }
}

void AAlsCharacterExample::Input_OnRoll()
{
    static constexpr auto PlayRate{1.3f};

    StartRolling(PlayRate);
}

void AAlsCharacterExample::Input_OnRotationMode()
{
    SetDesiredRotationMode(GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
                               ? AlsRotationModeTags::ViewDirection
                               : AlsRotationModeTags::VelocityDirection);
}

void AAlsCharacterExample::Input_OnViewMode()
{
    SetViewMode(GetViewMode() == AlsViewModeTags::ThirdPerson
                    ? AlsViewModeTags::FirstPerson
                    : AlsViewModeTags::ThirdPerson);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void AAlsCharacterExample::Input_OnSwitchShoulder()
{
    Camera->SetRightShoulder(!Camera->IsRightShoulder());
}

void AAlsCharacterExample::DisplayDebug(UCanvas *Canvas, const FDebugDisplayInfo &DisplayInfo, float &Unused,
                                        float &VerticalLocation)
{
    if (Camera->IsActive())
    {
        Camera->DisplayDebug(Canvas, DisplayInfo, VerticalLocation);
    }

    Super::DisplayDebug(Canvas, DisplayInfo, Unused, VerticalLocation);
}