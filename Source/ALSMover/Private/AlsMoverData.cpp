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
                     (bWantsToMantle ? (1 << 6) : 0);
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
    }

    bOutSuccess = true;
    return true;
}

void FAlsMoverInputs::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("MoveInput=(%f,%f,%f) LookInput=(%f,%f,%f) Sprint=%d ToggleWalk=%d ToggleCrouch=%d Aim=%d",
                MoveInputVector.X, MoveInputVector.Y, MoveInputVector.Z,
                LookInputVector.X, LookInputVector.Y, LookInputVector.Z,
                bIsSprintHeld ? 1 : 0, bWantsToToggleWalk ? 1 : 0, bWantsToToggleCrouch ? 1 : 0, bIsAimingHeld ? 1 : 0);
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
           bWantsToMantle != AuthInputs.bWantsToMantle;
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
    Ar << CurrentOverlayMode;
    
    bOutSuccess = true;
    return true;
}

void FAlsMoverSyncState::ToString(FAnsiStringBuilderBase &Out) const
{
    Super::ToString(Out);
    Out.Appendf("Stance=%s Gait=%s Rotation=%s Locomotion=%s Overlay=%s",
                *CurrentStance.ToString(),
                *CurrentGait.ToString(),
                *CurrentRotationMode.ToString(),
                *CurrentLocomotionMode.ToString(),
                *CurrentOverlayMode.ToString());

    if (CrouchModifierHandle.IsValid())
    {
        Out.Appendf(" CrouchMod=%s", *CrouchModifierHandle.ToString());
    }
    if (AimModifierHandle.IsValid())
    {
        Out.Appendf(" AimMod=%s", *AimModifierHandle.ToString());
    }
}

bool FAlsMoverSyncState::ShouldReconcile(const FMoverDataStructBase &AuthorityState) const
{
    const FAlsMoverSyncState &AuthState = static_cast<const FAlsMoverSyncState &>(AuthorityState);
    return CurrentStance != AuthState.CurrentStance ||
           CurrentGait != AuthState.CurrentGait ||
           CurrentRotationMode != AuthState.CurrentRotationMode ||
           CurrentLocomotionMode != AuthState.CurrentLocomotionMode ||
           CurrentOverlayMode != AuthState.CurrentOverlayMode ||
           false; // Modifier handles are runtime-only and not compared for reconciliation
}

void FAlsMoverSyncState::Interpolate(const FMoverDataStructBase &From, const FMoverDataStructBase &To, float Pct)
{
    // State tags don't interpolate - use the "To" state
    const FAlsMoverSyncState &ToState = static_cast<const FAlsMoverSyncState &>(To);
    CurrentStance = ToState.CurrentStance;
    CurrentGait = ToState.CurrentGait;
    CurrentRotationMode = ToState.CurrentRotationMode;
    CurrentLocomotionMode = ToState.CurrentLocomotionMode;
    CurrentOverlayMode = ToState.CurrentOverlayMode;
    // Note: Modifier handles are runtime-only and not interpolated
    WalkModifierHandle = ToState.WalkModifierHandle;
    SprintModifierHandle = ToState.SprintModifierHandle;
    CrouchModifierHandle = ToState.CrouchModifierHandle;
    AimModifierHandle = ToState.AimModifierHandle;
}