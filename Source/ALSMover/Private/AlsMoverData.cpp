#include "AlsMoverData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverData)

//========================================================================
// FAlsMoverInputs Implementation
//========================================================================

bool FAlsMoverInputs::NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess)
{
    Super::NetSerialize(Ar, Map, bOutSuccess);

    Ar << MoveInputVector;
    Ar << LookInputVector;
    Ar << MouseWorldPosition;

    uint16 InputFlags = 0;
    if (Ar.IsSaving())
    {
        InputFlags = (bHasValidMouseTarget ? (1 << 0) : 0) |
                     (bIsSprintHeld ? (1 << 1) : 0) |
                     (bWantsToToggleWalk ? (1 << 2) : 0) |
                     (bWantsToToggleCrouch ? (1 << 3) : 0) |
                     (bIsAimingHeld ? (1 << 4) : 0) |
                     (bWantsToRoll ? (1 << 5) : 0) |
                     (bWantsToMantle ? (1 << 6) : 0) |
                     (bUseTopDownView ? (1 << 7) : 0);
    }

    Ar << InputFlags;

    if (Ar.IsLoading())
    {
        bHasValidMouseTarget = (InputFlags & (1 << 0)) != 0;
        bIsSprintHeld = (InputFlags & (1 << 1)) != 0;
        bWantsToToggleWalk = (InputFlags & (1 << 2)) != 0;
        bWantsToToggleCrouch = (InputFlags & (1 << 3)) != 0;
        bIsAimingHeld = (InputFlags & (1 << 4)) != 0;
        bWantsToRoll = (InputFlags & (1 << 5)) != 0;
        bWantsToMantle = (InputFlags & (1 << 6)) != 0;
        bUseTopDownView = (InputFlags & (1 << 7)) != 0;
    }

    bOutSuccess = true;
    return true;
}

void FAlsMoverInputs::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("MoveInput=(%f,%f,%f) LookInput=(%f,%f,%f) MouseWorld=(%f,%f,%f)\n"
                "  Sprint=%d ToggleWalk=%d ToggleCrouch=%d Aim=%d Roll=%d Mantle=%d TopDown=%d",
                MoveInputVector.X, MoveInputVector.Y, MoveInputVector.Z,
                LookInputVector.X, LookInputVector.Y, LookInputVector.Z,
                MouseWorldPosition.X, MouseWorldPosition.Y, MouseWorldPosition.Z,
                bIsSprintHeld ? 1 : 0, bWantsToToggleWalk ? 1 : 0, bWantsToToggleCrouch ? 1 : 0,
                bIsAimingHeld ? 1 : 0, bWantsToRoll ? 1 : 0, bWantsToMantle ? 1 : 0, bUseTopDownView ? 1 : 0);
}

bool FAlsMoverInputs::ShouldReconcile(const FMoverDataStructBase &AuthorityState) const
{
    const FAlsMoverInputs &AuthInputs = static_cast<const FAlsMoverInputs &>(AuthorityState);
    return !MoveInputVector.Equals(AuthInputs.MoveInputVector, KINDA_SMALL_NUMBER) ||
           !LookInputVector.Equals(AuthInputs.LookInputVector, KINDA_SMALL_NUMBER) ||
           !MouseWorldPosition.Equals(AuthInputs.MouseWorldPosition, KINDA_SMALL_NUMBER) ||
           bHasValidMouseTarget != AuthInputs.bHasValidMouseTarget ||
           bIsSprintHeld != AuthInputs.bIsSprintHeld ||
           bWantsToToggleWalk != AuthInputs.bWantsToToggleWalk ||
           bWantsToToggleCrouch != AuthInputs.bWantsToToggleCrouch ||
           bIsAimingHeld != AuthInputs.bIsAimingHeld ||
           bWantsToRoll != AuthInputs.bWantsToRoll ||
           bWantsToMantle != AuthInputs.bWantsToMantle ||
           bUseTopDownView != AuthInputs.bUseTopDownView;
}

void FAlsMoverInputs::Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct)
{
    const FAlsMoverInputs &FromInputs = static_cast<const FAlsMoverInputs &>(From);
    const FAlsMoverInputs &ToInputs = static_cast<const FAlsMoverInputs &>(To);

    MoveInputVector = FMath::Lerp(FromInputs.MoveInputVector, ToInputs.MoveInputVector, Pct);
    LookInputVector = FMath::Lerp(FromInputs.LookInputVector, ToInputs.LookInputVector, Pct);
    MouseWorldPosition = FMath::Lerp(FromInputs.MouseWorldPosition, ToInputs.MouseWorldPosition, Pct);

    // Boolean values don't interpolate, use the "To" values
    bHasValidMouseTarget = ToInputs.bHasValidMouseTarget;
    bIsSprintHeld = ToInputs.bIsSprintHeld;
    bWantsToToggleWalk = ToInputs.bWantsToToggleWalk;
    bWantsToToggleCrouch = ToInputs.bWantsToToggleCrouch;
    bIsAimingHeld = ToInputs.bIsAimingHeld;
    bWantsToRoll = ToInputs.bWantsToRoll;
    bWantsToMantle = ToInputs.bWantsToMantle;
    bUseTopDownView = ToInputs.bUseTopDownView;
}

//========================================================================
// FAlsMoverSyncState Implementation
//========================================================================

bool FAlsMoverSyncState::NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess)
{
    Super::NetSerialize(Ar, Map, bOutSuccess);

    Ar << Stance;
    Ar << Gait;
    Ar << RotationMode;
    Ar << ViewMode;
    Ar << LocomotionMode;
    Ar << OverlayMode;
    Ar << LocomotionAction;
    Ar << Acceleration;
    Ar << ViewRotation;
    Ar << YawSpeed;
    Ar << PreviousVelocity;
    Ar << PreviousRotation;
    Ar << RelativeVelocity;
    Ar << VelocityYawAngle;

    uint8 StateFlags = 0;
    if (Ar.IsSaving())
    {
        StateFlags = (bHasMovementInput ? (1 << 0) : 0);
    }
    Ar << StateFlags;
    if (Ar.IsLoading())
    {
        bHasMovementInput = (StateFlags & (1 << 0)) != 0;
    }

    bOutSuccess = true;
    return true;
}

void FAlsMoverSyncState::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("Stance=%s Gait=%s Rotation=%s View=%s Locomotion=%s Overlay=%s Action=%s\n"
                "  Accel=(%f,%f,%f) ViewRot=(%f,%f,%f) YawSpeed=%f\n"
                "  PrevVel=(%f,%f,%f) PrevRot=(%f,%f,%f)\n"
                "  RelVel=(%f,%f,%f) VelYawAngle=%f HasInput=%d",
                *Stance.ToString(),
                *Gait.ToString(),
                *RotationMode.ToString(),
                *ViewMode.ToString(),
                *LocomotionMode.ToString(),
                *OverlayMode.ToString(),
                *LocomotionAction.ToString(),
                Acceleration.X, Acceleration.Y, Acceleration.Z,
                ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll,
                YawSpeed,
                PreviousVelocity.X, PreviousVelocity.Y, PreviousVelocity.Z,
                PreviousRotation.Pitch, PreviousRotation.Yaw, PreviousRotation.Roll,
                RelativeVelocity.X, RelativeVelocity.Y, RelativeVelocity.Z,
                VelocityYawAngle,
                bHasMovementInput ? 1 : 0);

    if (WalkModifierHandle.IsValid())
    {
        Out.Appendf(" WalkMod=%s", *WalkModifierHandle.ToString());
    }
    if (SprintModifierHandle.IsValid())
    {
        Out.Appendf(" SprintMod=%s", *SprintModifierHandle.ToString());
    }
    if (CrouchModifierHandle.IsValid())
    {
        Out.Appendf(" CrouchMod=%s", *CrouchModifierHandle.ToString());
    }
    if (AimModifierHandle.IsValid())
    {
        Out.Appendf(" AimMod=%s", *AimModifierHandle.ToString());
    }
    if (RotationModeModifierHandle.IsValid())
    {
        Out.Appendf(" RotationMod=%s", *RotationModeModifierHandle.ToString());
    }
}

bool FAlsMoverSyncState::ShouldReconcile(const FMoverDataStructBase &AuthorityState) const
{
    const FAlsMoverSyncState &AuthState = static_cast<const FAlsMoverSyncState &>(AuthorityState);
    return Stance != AuthState.Stance ||
           Gait != AuthState.Gait ||
           RotationMode != AuthState.RotationMode ||
           ViewMode != AuthState.ViewMode ||
           LocomotionMode != AuthState.LocomotionMode ||
           OverlayMode != AuthState.OverlayMode ||
           LocomotionAction != AuthState.LocomotionAction ||
           !Acceleration.Equals(AuthState.Acceleration, KINDA_SMALL_NUMBER) ||
           !ViewRotation.Equals(AuthState.ViewRotation, KINDA_SMALL_NUMBER) ||
           !FMath::IsNearlyEqual(YawSpeed, AuthState.YawSpeed, KINDA_SMALL_NUMBER) ||
           !PreviousVelocity.Equals(AuthState.PreviousVelocity, KINDA_SMALL_NUMBER) ||
           !PreviousRotation.Equals(AuthState.PreviousRotation, KINDA_SMALL_NUMBER) ||
           !RelativeVelocity.Equals(AuthState.RelativeVelocity, KINDA_SMALL_NUMBER) ||
           !FMath::IsNearlyEqual(VelocityYawAngle, AuthState.VelocityYawAngle, KINDA_SMALL_NUMBER) ||
           bHasMovementInput != AuthState.bHasMovementInput ||
           false; // Modifier handles are runtime-only and not compared for reconciliation
}

void FAlsMoverSyncState::Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct)
{
    // State tags don't interpolate - use the "To" state
    const FAlsMoverSyncState &FromState = static_cast<const FAlsMoverSyncState &>(From);
    const FAlsMoverSyncState &ToState = static_cast<const FAlsMoverSyncState &>(To);

    Stance = ToState.Stance;
    Gait = ToState.Gait;
    RotationMode = ToState.RotationMode;
    ViewMode = ToState.ViewMode;
    LocomotionMode = ToState.LocomotionMode;
    OverlayMode = ToState.OverlayMode;
    LocomotionAction = ToState.LocomotionAction;

    // Interpolate continuous values
    Acceleration = FMath::Lerp(FromState.Acceleration, ToState.Acceleration, Pct);
    ViewRotation = FMath::Lerp(FromState.ViewRotation, ToState.ViewRotation, Pct);
    YawSpeed = FMath::Lerp(FromState.YawSpeed, ToState.YawSpeed, Pct);
    PreviousVelocity = FMath::Lerp(FromState.PreviousVelocity, ToState.PreviousVelocity, Pct);
    PreviousRotation = FMath::Lerp(FromState.PreviousRotation, ToState.PreviousRotation, Pct);
    RelativeVelocity = FMath::Lerp(FromState.RelativeVelocity, ToState.RelativeVelocity, Pct);
    VelocityYawAngle = FMath::Lerp(FromState.VelocityYawAngle, ToState.VelocityYawAngle, Pct);

    // Boolean values don't interpolate, use the "To" values
    bHasMovementInput = ToState.bHasMovementInput;

    // Note: Modifier handles are runtime-only and not interpolated
    WalkModifierHandle = ToState.WalkModifierHandle;
    SprintModifierHandle = ToState.SprintModifierHandle;
    CrouchModifierHandle = ToState.CrouchModifierHandle;
    AimModifierHandle = ToState.AimModifierHandle;
    RotationModeModifierHandle = ToState.RotationModeModifierHandle;
}