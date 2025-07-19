#include "AlsLayeredMoves.h"
#include "MoverComponent.h"
#include "AlsMoverData.h"
#include "AlsMovementEffects.h"
#include "MoveLibrary/MovementUtils.h"
#include "MotionWarpingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsLayeredMoves)

//========================================================================
// FLayeredMove_AlsRoll Implementation
//========================================================================

FLayeredMove_AlsRoll::FLayeredMove_AlsRoll()
{
    DurationMs = RollDuration * 1000.0f; // Convert to milliseconds
    MixMode = EMoveMixMode::OverrideAll;
    Priority = 1; // Higher than normal movement
}

bool FLayeredMove_AlsRoll::GenerateMove(const FMoverTickStartData &StartState, const FMoverTimeStep &TimeStep,
                                        const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard,
                                        FProposedMove &OutProposedMove)
{
    if (!bIsPlaying)
    {
        return false;
    }

    const float ElapsedTimeSeconds = (TimeStep.BaseSimTimeMs - StartSimTimeMs) * 0.001f;
    const float Alpha = FMath::Clamp(ElapsedTimeSeconds / RollDuration, 0.0f, 1.0f);

    if (bUseRootMotion && RollMontage)
    {
        PreviousMontagePosition = CurrentMontagePosition;
        CurrentMontagePosition = Alpha * RollMontage->GetPlayLength();

        FTransform RootMotionDelta = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(
            RollMontage,
            PreviousMontagePosition,
            CurrentMontagePosition
            );

        if (!RootMotionDelta.Equals(FTransform::Identity))
        {
            const FMoverDefaultSyncState *SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<
                FMoverDefaultSyncState>();
            const FTransform SimActorTransform = SyncState
                                                     ? SyncState->GetTransform_WorldSpace()
                                                     : MoverComp->GetOwner()->GetActorTransform();
            const float DeltaSeconds = TimeStep.StepMs * 0.001f;

            const FTransform WorldSpaceDelta = MoverComp->ConvertLocalRootMotionToWorld(
                RootMotionDelta,
                DeltaSeconds,
                &SimActorTransform
                );

            if (DeltaSeconds > 0.0f)
            {
                OutProposedMove.LinearVelocity = WorldSpaceDelta.GetLocation() / DeltaSeconds;

                FRotator RotationDelta = WorldSpaceDelta.GetRotation().Rotator();
                RotationDelta.Normalize();

                OutProposedMove.AngularVelocity.Pitch = RotationDelta.Pitch / DeltaSeconds;
                OutProposedMove.AngularVelocity.Yaw = RotationDelta.Yaw / DeltaSeconds;
                OutProposedMove.AngularVelocity.Roll = RotationDelta.Roll / DeltaSeconds;
            }
        }
        else
        {
            OutProposedMove.LinearVelocity = FVector::ZeroVector;
            OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
        }
    }
    else
    {
        const float RollSpeed = RollDistance / RollDuration;
        const float SpeedMultiplier = FMath::Pow(1.0f - Alpha, 2.0f);
        OutProposedMove.LinearVelocity = RollDirection * RollSpeed * SpeedMultiplier;
        OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
    }

    return true;
}

void FLayeredMove_AlsRoll::OnStart(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard)
{
    bIsPlaying = true;
    PreviousMontagePosition = 0.f;
    CurrentMontagePosition = 0.f;

    // CORRECTED: Queue an effect to change the state tag
    if (UMoverComponent *MutableMover = const_cast<UMoverComponent *>(MoverComp))
    {
        MutableMover->QueueInstantMovementEffect(
            MakeShared<FSetLocomotionActionEffect>(AlsLocomotionActionTags::Rolling));
    }

    if (RollMontage && MoverComp)
    {
        if (APawn *Pawn = MoverComp->GetOwner<APawn>())
        {
            if (USkeletalMeshComponent *Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
            {
                if (UAnimInstance *AnimInstance = Mesh->GetAnimInstance())
                {
                    AnimInstance->Montage_Play(RollMontage, PlayRate);
                }
            }
        }
    }
}

void FLayeredMove_AlsRoll::OnEnd(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard, float CurrentSimTimeMs)
{
    bIsPlaying = false;

    // CORRECTED: Queue an effect to clear the state tag
    if (UMoverComponent *MutableMover = const_cast<UMoverComponent *>(MoverComp))
    {
        MutableMover->QueueInstantMovementEffect(MakeShared<FSetLocomotionActionEffect>(FGameplayTag::EmptyTag));
    }

    // Stop roll animation if still playing
    if (RollMontage && MoverComp)
    {
        if (APawn *Pawn = MoverComp->GetOwner<APawn>())
        {
            if (USkeletalMeshComponent *Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
            {
                if (UAnimInstance *AnimInstance = Mesh->GetAnimInstance())
                {
                    if (AnimInstance->Montage_IsPlaying(RollMontage))
                    {
                        AnimInstance->Montage_Stop(0.2f, RollMontage);
                    }
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("ALS Roll Ended"));
}

FLayeredMoveBase *FLayeredMove_AlsRoll::Clone() const
{
    return new FLayeredMove_AlsRoll(*this);
}

void FLayeredMove_AlsRoll::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    Ar << RollDistance;
    Ar << RollDuration;
    Ar << RollDirection;
    Ar << bUseRootMotion;
    Ar << PlayRate;
    Ar << AnimStartTime;
    Ar << PreviousMontagePosition;
    Ar << CurrentMontagePosition;

    uint8 PlayingFlag = bIsPlaying ? 1 : 0;
    Ar << PlayingFlag;
    bIsPlaying = PlayingFlag != 0;
}

UScriptStruct *FLayeredMove_AlsRoll::GetScriptStruct() const
{
    return StaticStruct();
}

FString FLayeredMove_AlsRoll::ToSimpleString() const
{
    return FString::Printf(TEXT("AlsRoll Dir=%s Dist=%.1f"), *RollDirection.ToString(), RollDistance);
}

bool FLayeredMove_AlsRoll::IsFinished(float CurrentSimTimeMs) const
{
    const float ElapsedMs = CurrentSimTimeMs - StartSimTimeMs;
    return ElapsedMs >= DurationMs;
}

//========================================================================
// FLayeredMove_AlsMantle Implementation
//========================================================================
FLayeredMove_AlsMantle::FLayeredMove_AlsMantle()
{
    // Default values
    MantleDuration = 1.2f;
    DurationMs = MantleDuration * 1000.0f;
    MixMode = EMoveMixMode::OverrideAll;
    Priority = 2; // Higher priority than roll
    bIsPerformingMantle = false;
    LowMantleThreshold = 125.0f;
}

bool FLayeredMove_AlsMantle::GenerateMove(const FMoverTickStartData &StartState, const FMoverTimeStep &TimeStep,
                                          const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard,
                                          FProposedMove &OutProposedMove)
{
    if (!bIsPerformingMantle || !MoverComp)
    {
        return false;
    }

    const float ElapsedTimeSeconds = (TimeStep.BaseSimTimeMs - StartSimTimeMs) * 0.001f;
    const float Alpha = FMath::Clamp(ElapsedTimeSeconds / MantleDuration, 0.0f, 1.0f);

    const FMoverDefaultSyncState *SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<
        FMoverDefaultSyncState>();
    if (!SyncState)
    {
        return false;
    }

    const FVector CurrentPos = SyncState->GetLocation_WorldSpace();
    const FVector TargetPos = InterpolateMantlePosition(Alpha);

    const float DeltaSeconds = TimeStep.StepMs * 0.001f;
    if (DeltaSeconds > 0.0f)
    {
        OutProposedMove.LinearVelocity = (TargetPos - CurrentPos) / DeltaSeconds;
    }

    const FVector MantleDir = (MantleTargetLocation - MantleStartLocation).GetSafeNormal2D();
    if (!MantleDir.IsNearlyZero())
    {
        const FRotator CurrentRot = SyncState->GetOrientation_WorldSpace();
        const FRotator TargetRot = MantleDir.ToOrientationRotator();

        // CORRECTED: Perform component-wise division to calculate angular velocity
        const FRotator DeltaRot = (TargetRot - CurrentRot).GetNormalized();
        if (DeltaSeconds > 0.f)
        {
            OutProposedMove.AngularVelocity.Pitch = DeltaRot.Pitch / DeltaSeconds;
            OutProposedMove.AngularVelocity.Yaw = DeltaRot.Yaw / DeltaSeconds;
            OutProposedMove.AngularVelocity.Roll = DeltaRot.Roll / DeltaSeconds;
        }
    }

    return true;
}

void FLayeredMove_AlsMantle::OnStart(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard)
{
    bIsPerformingMantle = true;

    // CORRECTED: Queue an effect to change the state tag
    if (UMoverComponent *MutableMover = const_cast<UMoverComponent *>(MoverComp))
    {
        MutableMover->QueueInstantMovementEffect(
            MakeShared<FSetLocomotionActionEffect>(AlsLocomotionActionTags::Mantling));
    }

    UAnimMontage *MontageToPlay = (MantleHeight <= LowMantleThreshold) ? LowMantleMontage : HighMantleMontage;
    if (MontageToPlay && MoverComp)
    {
        if (APawn *Pawn = MoverComp->GetOwner<APawn>())
        {
            if (USkeletalMeshComponent *Mesh = Pawn->FindComponentByClass<USkeletalMeshComponent>())
            {
                if (UAnimInstance *AnimInstance = Mesh->GetAnimInstance())
                {
                    AnimInstance->Montage_Play(MontageToPlay, 1.0f);
                    if (UMotionWarpingComponent *MotionWarping = Pawn->FindComponentByClass<UMotionWarpingComponent>())
                    {
                        MotionWarping->AddOrUpdateWarpTargetFromLocation(FName("MantleTarget"), MantleTargetLocation);
                    }
                }
            }
        }
    }
}

void FLayeredMove_AlsMantle::OnEnd(const UMoverComponent *MoverComp, UMoverBlackboard *Blackboard,
                                   float CurrentSimTimeMs)
{
    bIsPerformingMantle = false;

    // CORRECTED: Queue an effect to clear the state tag
    if (UMoverComponent *MutableMover = const_cast<UMoverComponent *>(MoverComp))
    {
        MutableMover->QueueInstantMovementEffect(MakeShared<FSetLocomotionActionEffect>(FGameplayTag::EmptyTag));
    }

    if (MoverComp)
    {
        if (APawn *Pawn = MoverComp->GetOwner<APawn>())
        {
            if (UMotionWarpingComponent *MotionWarping = Pawn->FindComponentByClass<UMotionWarpingComponent>())
            {
                MotionWarping->RemoveWarpTarget(FName("MantleTarget"));
            }
        }
    }
}

FVector FLayeredMove_AlsMantle::InterpolateMantlePosition(float Alpha) const
{
    // Use a curve for more natural movement (e.g., Ease In/Out)
    const float CurvedAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

    // Interpolate horizontal position
    FVector Position = FMath::Lerp(MantleStartLocation, MantleTargetLocation, CurvedAlpha);

    // Add vertical arc for mantling motion. Sin(PI * Alpha) creates a nice arc from 0 to 1 and back to 0.
    const float MaxArcHeight = MantleHeight * 0.5f;
    const float ArcAlpha = FMath::Sin(Alpha * PI);
    Position.Z += MaxArcHeight * ArcAlpha;

    return Position;
}

FLayeredMoveBase *FLayeredMove_AlsMantle::Clone() const
{
    return new FLayeredMove_AlsMantle(*this);
}

void FLayeredMove_AlsMantle::NetSerialize(FArchive &Ar)
{
    Super::NetSerialize(Ar);

    Ar << MantleStartLocation;
    Ar << MantleTargetLocation;
    Ar << MantleHeight;
    Ar << MantleDuration;
    Ar << LowMantleThreshold;

    uint8 PerformingFlag = bIsPerformingMantle ? 1 : 0;
    Ar << PerformingFlag;
    bIsPerformingMantle = PerformingFlag != 0;
}

UScriptStruct *FLayeredMove_AlsMantle::GetScriptStruct() const
{
    return StaticStruct();
}

FString FLayeredMove_AlsMantle::ToSimpleString() const
{
    return FString::Printf(TEXT("AlsMantle Height=%.1f Duration=%.2fs"), MantleHeight, MantleDuration);
}

bool FLayeredMove_AlsMantle::IsFinished(float CurrentSimTimeMs) const
{
    const float ElapsedMs = CurrentSimTimeMs - StartSimTimeMs;
    return ElapsedMs >= DurationMs;
}