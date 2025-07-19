#include "AlsMoverAnimationInstance.h"
#include "AlsCharacterMoverComponent.h"
#include "AlsMoverCharacter.h"
#include "AlsAnimationInstanceProxy.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/SkeletalMesh.h"
#include "Settings/AlsAnimationInstanceSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsPrivateMemberAccessor.h"
#include "Utility/AlsRotation.h"
#include "Utility/AlsUtility.h"
#include "Utility/AlsVector.h"
#include "Utility/AlsMath.h"
#include "Utility/AlsGameplayTags.h"
#include "GameFramework/Pawn.h"
#if WITH_EDITORONLY_DATA && ENABLE_DRAW_DEBUG
#include "Utility/AlsDebugUtility.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverAnimationInstance)

ALS_DEFINE_PRIVATE_MEMBER_ACCESSOR(AlsGetAnimationCurvesAccessor, &FAnimInstanceProxy::GetAnimationCurves,
                                   const TMap<FName, float>& (FAnimInstanceProxy::*)(EAnimCurveType) const)

void UAlsMoverAnimationInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

#if WITH_EDITOR
    const auto *World{GetWorld()};
    if (IsValid(World) && !World->IsGameWorld() && !IsValid(CachedMoverComponent))
    {
        // Use default for editor preview
        CachedMoverComponent = GetMutableDefault<AAlsMoverCharacter>()->GetMoverComponent();
    }
#endif

    CachedMoverComponent = GetAlsMoverComponent();

    const auto *Mesh{GetSkelMeshComponent()};
    if (IsValid(Mesh) && IsValid(Mesh->GetSkinnedAsset()))
    {
        const auto &ReferenceSkeleton{Mesh->GetSkinnedAsset()->GetRefSkeleton()};
        const auto PelvisBoneIndex{ReferenceSkeleton.FindBoneIndex(UAlsConstants::PelvisBoneName())};

        static const auto GetThighAxis{
            [](const FReferenceSkeleton &ReferenceSkeleton, const int32 PelvisBoneIndex, const FName &FootBoneName,
               FVector3f &ThighAxis)
            {
                auto ParentBoneIndex{ReferenceSkeleton.FindBoneIndex(FootBoneName)};
                if (ParentBoneIndex < 0)
                {
                    return;
                }

                while (true)
                {
                    const auto NextParentBoneIndex{ReferenceSkeleton.GetParentIndex(ParentBoneIndex)};
                    if (NextParentBoneIndex <= 0)
                    {
                        return;
                    }
                    if (NextParentBoneIndex == PelvisBoneIndex)
                    {
                        break;
                    }
                    ParentBoneIndex = NextParentBoneIndex;
                }

                const auto &ThighTransform{ReferenceSkeleton.GetRefBonePose()[ParentBoneIndex]};
                ThighAxis = FVector3f{ThighTransform.GetLocation()};
                ThighAxis.Normalize();
            }
        };

        GetThighAxis(ReferenceSkeleton, PelvisBoneIndex, UAlsConstants::FootLeftBoneName(), FeetState.Left.ThighAxis);
        GetThighAxis(ReferenceSkeleton, PelvisBoneIndex, UAlsConstants::FootRightBoneName(), FeetState.Right.ThighAxis);
    }
}

void UAlsMoverAnimationInstance::NativeBeginPlay()
{
    Super::NativeBeginPlay();
    ALS_ENSURE(IsValid(Settings));
}

FAnimInstanceProxy *UAlsMoverAnimationInstance::CreateAnimInstanceProxy()
{
    return new FAlsAnimationInstanceProxy{this};
}

UAlsCharacterMoverComponent *UAlsMoverAnimationInstance::GetAlsMoverComponent() const
{
    if (CachedMoverComponent)
    {
        return CachedMoverComponent;
    }

    if (APawn *OwningPawn = TryGetPawnOwner())
    {
        return OwningPawn->FindComponentByClass<UAlsCharacterMoverComponent>();
    }

    return nullptr;
}

// =================================================================================================
// Update
// =================================================================================================

void UAlsMoverAnimationInstance::NativeUpdateAnimation(const float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);

    if (!CachedMoverComponent)
    {
        CachedMoverComponent = GetAlsMoverComponent();
    }

    if (!IsValid(Settings) || !IsValid(CachedMoverComponent))
    {
        return;
    }

    UpdateStateFromMover(DeltaTime);
    UpdateHelperVariables();

    RefreshMovementBaseOnGameThread();
    RefreshFeetOnGameThread();
    RefreshInAirOnGameThread();
    RefreshRagdollingOnGameThread();
}

void UAlsMoverAnimationInstance::UpdateStateFromMover(float DeltaTime)
{
    PreviousLocation = LocomotionState.Location;

    Gait = CachedMoverComponent->GetGait();
    Stance = CachedMoverComponent->GetStance();
    RotationMode = CachedMoverComponent->GetRotationMode();
    LocomotionMode = CachedMoverComponent->GetLocomotionMode();
    OverlayMode = CachedMoverComponent->GetOverlayMode();
    LocomotionAction = CachedMoverComponent->GetLocomotionAction();

    RefreshViewOnGameThread();

    const auto &Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();
    const auto &ActorTransform = Proxy.GetActorTransform();
    const auto PreviousYawAngle = LocomotionState.Rotation.Yaw;

    LocomotionState.Velocity = CachedMoverComponent->GetVelocity();
    LocomotionState.Speed = LocomotionState.Velocity.Size2D();
    LocomotionState.bHasInput = LocomotionState.Speed > UE_KINDA_SMALL_NUMBER;
    if (LocomotionState.bHasInput)
    {
        LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(
            UAlsVector::DirectionToAngleXY(LocomotionState.Velocity));
    }

    LocomotionState.Acceleration = CachedMoverComponent->GetAcceleration();
    LocomotionState.bMoving = CachedMoverComponent->IsMoving();
    LocomotionState.bMovingSmooth = LocomotionState.Speed > (Settings
                                                                 ? Settings->General.MovingSmoothSpeedThreshold
                                                                 : 150.0f);

    LocomotionState.bHasInput = CachedMoverComponent->HasMovementInput();
    if (const FAlsMoverInputs *AlsInputs = CachedMoverComponent->GetLastInputCmd().InputCollection.FindDataByType<
        FAlsMoverInputs>())
    {
        LocomotionState.InputYawAngle = UAlsVector::DirectionToAngleXY(AlsInputs->MoveInputVector);
    }

    // Add network smoothing from original
    static const auto *EnableListenServerSmoothingConsoleVariable = IConsoleManager::Get().FindConsoleVariable(
        TEXT("p.NetEnableListenServerSmoothing"));
    if (
        (CachedMoverComponent->GetOwnerRole() != ROLE_SimulatedProxy &&
         !(CachedMoverComponent->GetOwner()->IsNetMode(NM_ListenServer) && EnableListenServerSmoothingConsoleVariable->
           GetBool())))
    {
        LocomotionState.Location = ActorTransform.GetLocation();
        LocomotionState.Rotation = ActorTransform.Rotator();
        LocomotionState.RotationQuaternion = ActorTransform.GetRotation();
    }
    else
    {
        const auto MeshRelativeTransform{Proxy.GetComponentRelativeTransform()};
        const auto SmoothTransform{
            ActorTransform * FTransform{MeshRelativeTransform.GetRotation() * FQuat::Identity,
                                        MeshRelativeTransform.GetLocation()}};
        LocomotionState.Location = SmoothTransform.GetLocation();
        LocomotionState.Rotation = SmoothTransform.Rotator();
        LocomotionState.RotationQuaternion = SmoothTransform.GetRotation();
    }

    LocomotionState.YawSpeed = DeltaTime > UE_SMALL_NUMBER
                                   ? FMath::UnwindDegrees(
                                         UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw - PreviousYawAngle)) /
                                     DeltaTime
                                   : 0.0f;
    LocomotionState.Scale = ActorTransform.GetScale3D().Z;

    if (const auto *Capsule = GetOwningComponent()->GetOwner()->FindComponentByClass<UCapsuleComponent>())
    {
        LocomotionState.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
        LocomotionState.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    }

    constexpr float TeleportDistanceThreshold = 300.0f;
    if (!bPendingUpdate && FVector::DistSquared(PreviousLocation, LocomotionState.Location) > FMath::Square(
            TeleportDistanceThreshold))
    {
        MarkTeleported();
    }
}

void UAlsMoverAnimationInstance::UpdateHelperVariables()
{
    bIsWalking = (Gait == AlsGaitTags::Walking);
    bIsRunning = (Gait == AlsGaitTags::Running);
    bIsSprinting = (Gait == AlsGaitTags::Sprinting);
    bIsCrouching = (Stance == AlsStanceTags::Crouching);
    bIsAiming = (OverlayMode == AlsOverlayModeTags::Aiming);
}

void UAlsMoverAnimationInstance::NativeThreadSafeUpdateAnimation(const float DeltaTime)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaTime);

    if (!IsValid(Settings) || !IsValid(CachedMoverComponent))
    {
        return;
    }

    DynamicTransitionsState.bUpdatedThisFrame = false;
    RotateInPlaceState.bUpdatedThisFrame = false;
    TurnInPlaceState.bUpdatedThisFrame = false;

    RefreshLayering();
    RefreshPose();
    RefreshView(DeltaTime);
    RefreshFeet(DeltaTime);
    RefreshTransitions();
    RefreshTurnInPlace();
}

void UAlsMoverAnimationInstance::NativePostUpdateAnimation()
{
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsMoverAnimationInstance::NativePostUpdateAnimation"),
                                STAT_UAlsAnimationInstance_NativePostUpdateAnimation, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings) || !IsValid(CachedMoverComponent))
    {
        return;
    }

    PlayQueuedTransitionAnimation();
    PlayQueuedTurnInPlaceAnimation();
    StopQueuedTransitionAndTurnInPlaceAnimations();

#if WITH_EDITORONLY_DATA && ENABLE_DRAW_DEBUG
    if (!bPendingUpdate)
    {
        for (const auto &DisplayDebugTraceFunction : DisplayDebugTracesQueue)
        {
            DisplayDebugTraceFunction();
        }
    }
    DisplayDebugTracesQueue.Reset();
#endif

    bPendingUpdate = false;
}

// =================================================================================================
// Ported Animation Logic
// =================================================================================================

void UAlsMoverAnimationInstance::RefreshMovementBaseOnGameThread()
{
    check(IsInGameThread());

    FRelativeBaseInfo LastFoundBase;
    if (CachedMoverComponent->GetSimBlackboard()->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, LastFoundBase))
    {
        const FQuat PreviousRotation = MovementBase.Rotation;

        MovementBase.Primitive = LastFoundBase.MovementBase.Get();
        MovementBase.BoneName = LastFoundBase.BoneName;

        bool bBaseChanged = MovementBase.Primitive != LastFoundBase.MovementBase || MovementBase.BoneName !=
                            LastFoundBase.BoneName;
        MovementBase.bBaseChanged = bBaseChanged;

        MovementBase.bHasRelativeLocation = MovementBase.Primitive != nullptr;
        MovementBase.bHasRelativeRotation = MovementBase.bHasRelativeLocation;

        MovementBase.Location = LastFoundBase.Location;
        MovementBase.Rotation = LastFoundBase.Rotation;

        MovementBase.DeltaRotation = MovementBase.bHasRelativeLocation && !MovementBase.bBaseChanged
                                         ? (MovementBase.Rotation * PreviousRotation.Inverse()).Rotator()
                                         : FRotator::ZeroRotator;
    }
    else
    {
        MovementBase = FAlsMovementBaseState();
    }
}

void UAlsMoverAnimationInstance::RefreshLayering()
{
    const auto &Curves{
        AlsGetAnimationCurvesAccessor::Access(GetProxyOnAnyThread<FAnimInstanceProxy>(),
                                              EAnimCurveType::AttributeCurve)};

    static const auto GetCurveValue{
        [](const TMap<FName, float> &Curves, const FName &CurveName) -> float
        {
            const auto *Value{Curves.Find(CurveName)};
            return Value != nullptr ? *Value : 0.0f;
        }
    };

    LayeringState.HeadBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerHeadCurveName());
    LayeringState.HeadAdditiveBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerHeadAdditiveCurveName());
    LayeringState.HeadSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerHeadSlotCurveName());

    // The mesh space blend will always be 1 unless the local space blend is 1.

    LayeringState.ArmLeftBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmLeftCurveName());
    LayeringState.ArmLeftAdditiveBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmLeftAdditiveCurveName());
    LayeringState.ArmLeftSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmLeftSlotCurveName());
    LayeringState.ArmLeftLocalSpaceBlendAmount =
        GetCurveValue(Curves, UAlsConstants::LayerArmLeftLocalSpaceCurveName());
    LayeringState.ArmLeftMeshSpaceBlendAmount = !FAnimWeight::IsFullWeight(LayeringState.ArmLeftLocalSpaceBlendAmount);

    // The mesh space blend will always be 1 unless the local space blend is 1.

    LayeringState.ArmRightBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmRightCurveName());
    LayeringState.ArmRightAdditiveBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmRightAdditiveCurveName());
    LayeringState.ArmRightSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerArmRightSlotCurveName());
    LayeringState.ArmRightLocalSpaceBlendAmount = GetCurveValue(
        Curves, UAlsConstants::LayerArmRightLocalSpaceCurveName());
    LayeringState.ArmRightMeshSpaceBlendAmount = !
        FAnimWeight::IsFullWeight(LayeringState.ArmRightLocalSpaceBlendAmount);

    LayeringState.HandLeftBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerHandLeftCurveName());
    LayeringState.HandRightBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerHandRightCurveName());

    LayeringState.SpineBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerSpineCurveName());
    LayeringState.SpineAdditiveBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerSpineAdditiveCurveName());
    LayeringState.SpineSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerSpineSlotCurveName());

    LayeringState.PelvisBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerPelvisCurveName());
    LayeringState.PelvisSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerPelvisSlotCurveName());

    LayeringState.LegsBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerLegsCurveName());
    LayeringState.LegsSlotBlendAmount = GetCurveValue(Curves, UAlsConstants::LayerLegsSlotCurveName());
}

void UAlsMoverAnimationInstance::RefreshPose()
{
    const auto &Curves{
        AlsGetAnimationCurvesAccessor::Access(GetProxyOnAnyThread<FAnimInstanceProxy>(),
                                              EAnimCurveType::AttributeCurve)};

    static const auto GetCurveValue{
        [](const TMap<FName, float> &Curves, const FName &CurveName) -> float
        {
            const auto *Value{Curves.Find(CurveName)};
            return Value != nullptr ? *Value : 0.0f;
        }
    };

    PoseState.GroundedAmount = GetCurveValue(Curves, UAlsConstants::PoseGroundedCurveName());
    PoseState.InAirAmount = GetCurveValue(Curves, UAlsConstants::PoseInAirCurveName());

    PoseState.StandingAmount = GetCurveValue(Curves, UAlsConstants::PoseStandingCurveName());
    PoseState.CrouchingAmount = GetCurveValue(Curves, UAlsConstants::PoseCrouchingCurveName());

    PoseState.MovingAmount = GetCurveValue(Curves, UAlsConstants::PoseMovingCurveName());

    PoseState.GaitAmount = FMath::Clamp(GetCurveValue(Curves, UAlsConstants::PoseGaitCurveName()), 0.0f, 3.0f);
    PoseState.GaitWalkingAmount = UAlsMath::Clamp01(PoseState.GaitAmount);
    PoseState.GaitRunningAmount = UAlsMath::Clamp01(PoseState.GaitAmount - 1.0f);
    PoseState.GaitSprintingAmount = UAlsMath::Clamp01(PoseState.GaitAmount - 2.0f);

    PoseState.UnweightedGaitAmount = PoseState.GroundedAmount > UE_SMALL_NUMBER
                                         ? PoseState.GaitAmount / PoseState.GroundedAmount
                                         : PoseState.GaitAmount;

    PoseState.UnweightedGaitWalkingAmount = UAlsMath::Clamp01(PoseState.UnweightedGaitAmount);
    PoseState.UnweightedGaitRunningAmount = UAlsMath::Clamp01(PoseState.UnweightedGaitAmount - 1.0f);
    PoseState.UnweightedGaitSprintingAmount = UAlsMath::Clamp01(PoseState.UnweightedGaitAmount - 2.0f);
}

void UAlsMoverAnimationInstance::RefreshViewOnGameThread()
{
    check(IsInGameThread());

    if (const APawn *OwningPawn = TryGetPawnOwner())
    {
        if (const APlayerController *PC = Cast<APlayerController>(OwningPawn->GetController()))
        {
            ViewState.Rotation = PC->GetControlRotation();
        }
        else
        {
            if (const auto *MoverSyncState = CachedMoverComponent->GetSyncState().SyncStateCollection.FindDataByType<
                FMoverDefaultSyncState>())
            {
                ViewState.Rotation = MoverSyncState->GetOrientation_WorldSpace();
            }
        }
    }

    ViewState.YawSpeed = LocomotionState.YawSpeed;
}

void UAlsMoverAnimationInstance::RefreshView(const float DeltaTime)
{
    if (!LocomotionAction.IsValid())
    {
        ViewState.YawAngle = FMath::UnwindDegrees(
            UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - LocomotionState.Rotation.Yaw));
        ViewState.PitchAngle = FMath::UnwindDegrees(
            UE_REAL_TO_FLOAT(ViewState.Rotation.Pitch - LocomotionState.Rotation.Pitch));

        ViewState.PitchAmount = 0.5f - ViewState.PitchAngle / 180.0f;
    }

    const auto ViewAmount{1.0f - GetCurveValueClamped01(UAlsConstants::ViewBlockCurveName())};
    const auto AimingAmount{GetCurveValueClamped01(UAlsConstants::AllowAimingCurveName())};

    ViewState.LookAmount = ViewAmount * (1.0f - AimingAmount);

    RefreshSpine(ViewAmount * AimingAmount, DeltaTime);
}

bool UAlsMoverAnimationInstance::IsSpineRotationAllowed()
{
    return RotationMode == AlsRotationModeTags::Aiming;
}

void UAlsMoverAnimationInstance::RefreshSpine(const float SpineBlendAmount, const float DeltaTime)
{
    if (SpineState.bSpineRotationAllowed != IsSpineRotationAllowed())
    {
        SpineState.bSpineRotationAllowed = !SpineState.bSpineRotationAllowed;

        if (SpineState.bSpineRotationAllowed)
        {
            if (FAnimWeight::IsFullWeight(SpineState.SpineAmount))
            {
                SpineState.SpineAmountScale = 1.0f;
                SpineState.SpineAmountBias = 0.0f;
            }
            else
            {
                SpineState.SpineAmountScale = 1.0f / (1.0f - SpineState.SpineAmount);
                SpineState.SpineAmountBias = -SpineState.SpineAmount * SpineState.SpineAmountScale;
            }
        }
        else
        {
            SpineState.SpineAmountScale = !FAnimWeight::IsRelevant(SpineState.SpineAmount)
                                              ? 1.0f
                                              : 1.0f / SpineState.SpineAmount;

            SpineState.SpineAmountBias = 0.0f;
        }

        SpineState.LastYawAngle = SpineState.CurrentYawAngle;
        SpineState.LastActorYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
    }

    if (SpineState.bSpineRotationAllowed)
    {
        if (bPendingUpdate || FAnimWeight::IsFullWeight(SpineState.SpineAmount))
        {
            SpineState.SpineAmount = 1.0f;
            SpineState.CurrentYawAngle = ViewState.YawAngle;
        }
        else
        {
            static constexpr auto InterpolationSpeed{20.0f};

            SpineState.SpineAmount = UAlsMath::ExponentialDecay(SpineState.SpineAmount, 1.0f, DeltaTime,
                                                                InterpolationSpeed);

            SpineState.CurrentYawAngle = UAlsRotation::LerpAngle(SpineState.LastYawAngle, ViewState.YawAngle,
                                                                 SpineState.SpineAmount * SpineState.SpineAmountScale +
                                                                 SpineState.SpineAmountBias);
        }
    }
    else
    {
        if (bPendingUpdate || !FAnimWeight::IsRelevant(SpineState.SpineAmount))
        {
            SpineState.SpineAmount = 0.0f;
            SpineState.CurrentYawAngle = UAlsRotation::LerpAngle(SpineState.LastYawAngle,
                                                                 SpineState.LastActorYawAngle - UE_REAL_TO_FLOAT(
                                                                     LocomotionState.Rotation.Yaw),
                                                                 SpineState.SpineAmountBias);
        }
        else
        {
            static constexpr auto InterpolationSpeed{1.0f}; // Change to 1.0f as original
            static constexpr auto ReferenceViewYawSpeed{40.0f};

            // Add from original
            const auto InterpolationSpeedMultiplier = FMath::Max(
                1.0f, FMath::Abs(ViewState.YawSpeed) / ReferenceViewYawSpeed);
            SpineState.SpineAmount = UAlsMath::ExponentialDecay(SpineState.SpineAmount, 0.0f, DeltaTime,
                                                                InterpolationSpeed * InterpolationSpeedMultiplier);

            // Offset logic from original
            if (MovementBase.bHasRelativeRotation)
            {
                SpineState.LastActorYawAngle = FMath::UnwindDegrees(
                    UE_REAL_TO_FLOAT(SpineState.LastActorYawAngle + MovementBase.DeltaRotation.Yaw));
            }

            auto YawAngleOffset = FMath::UnwindDegrees(
                UE_REAL_TO_FLOAT(SpineState.LastActorYawAngle - LocomotionState.Rotation.Yaw));
            static constexpr auto MaxYawAngleOffset{30.0f};
            YawAngleOffset = FMath::Clamp(YawAngleOffset, -MaxYawAngleOffset, MaxYawAngleOffset);

            SpineState.LastActorYawAngle = FMath::UnwindDegrees(
                UE_REAL_TO_FLOAT(YawAngleOffset + LocomotionState.Rotation.Yaw));

            SpineState.CurrentYawAngle = UAlsRotation::LerpAngle(0.0f, SpineState.LastYawAngle + YawAngleOffset,
                                                                 SpineState.SpineAmount * SpineState.SpineAmountScale +
                                                                 SpineState.SpineAmountBias);
        }
    }

    SpineState.YawAngle = UAlsRotation::LerpAngle(0.0f, SpineState.CurrentYawAngle, SpineBlendAmount);
}

void UAlsMoverAnimationInstance::RefreshInAirOnGameThread()
{
    check(IsInGameThread());

    InAirState.bJumped = !bPendingUpdate && (InAirState.bJumped || InAirState.bJumpRequested);
    InAirState.bJumpRequested = false;
}

void UAlsMoverAnimationInstance::RefreshFeetOnGameThread()
{
    check(IsInGameThread());
    const auto *Mesh{GetSkelMeshComponent()};

    FeetState.PelvisRotation = FQuat4f{
        Mesh->GetSocketTransform(UAlsConstants::PelvisBoneName(), RTS_Component).GetRotation()};

    const auto FootLeftTargetTransform{
        Mesh->GetSocketTransform(Settings->General.bUseFootIkBones
                                     ? UAlsConstants::FootLeftIkBoneName()
                                     : UAlsConstants::FootLeftVirtualBoneName())
    };
    FeetState.Left.TargetLocation = FootLeftTargetTransform.GetLocation();
    FeetState.Left.TargetRotation = FootLeftTargetTransform.GetRotation();

    const auto FootRightTargetTransform{
        Mesh->GetSocketTransform(Settings->General.bUseFootIkBones
                                     ? UAlsConstants::FootRightIkBoneName()
                                     : UAlsConstants::FootRightVirtualBoneName())
    };
    FeetState.Right.TargetLocation = FootRightTargetTransform.GetLocation();
    FeetState.Right.TargetRotation = FootRightTargetTransform.GetRotation();
}

void UAlsMoverAnimationInstance::RefreshFeet(const float DeltaTime)
{
    const auto ComponentTransformInverse{GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform().Inverse()};

    RefreshFoot(FeetState.Left, UAlsConstants::FootLeftIkCurveName(), UAlsConstants::FootLeftLockCurveName(),
                ComponentTransformInverse, DeltaTime);
    RefreshFoot(FeetState.Right, UAlsConstants::FootRightIkCurveName(), UAlsConstants::FootRightLockCurveName(),
                ComponentTransformInverse, DeltaTime);
}

void UAlsMoverAnimationInstance::RefreshTransitions()
{
    TransitionsState.bTransitionsAllowed = FAnimWeight::IsFullWeight(
        GetCurveValue(UAlsConstants::AllowTransitionsCurveName()));
}

void UAlsMoverAnimationInstance::RefreshRagdollingOnGameThread()
{
    check(IsInGameThread());

    if (LocomotionAction != AlsLocomotionActionTags::Ragdolling)
    {
        return;
    }

    static constexpr auto ReferenceSpeed{1000.0f};

    // Adapt: Use Mover velocity since no GetRagdollingState
    FVector RagdollVelocity = CachedMoverComponent->GetVelocity(); // Or from sync state PreviousVelocity
    RagdollingState.FlailPlayRate = UAlsMath::Clamp01(UE_REAL_TO_FLOAT(RagdollVelocity.Size() / ReferenceSpeed));
}

FVector3f UAlsMoverAnimationInstance::GetRelativeVelocity() const
{
    return FVector3f{LocomotionState.RotationQuaternion.UnrotateVector(LocomotionState.Velocity)};
}

FVector2f UAlsMoverAnimationInstance::GetRelativeAccelerationAmount() const
{
    constexpr float MaxAcceleration = 2000.0f;
    constexpr float MaxBrakingDeceleration = 2000.0f;

    const auto TargetMaxAcceleration{
        (LocomotionState.Acceleration | LocomotionState.Velocity) >= 0.0f
            ? MaxAcceleration
            : MaxBrakingDeceleration
    };

    if (TargetMaxAcceleration <= UE_KINDA_SMALL_NUMBER)
    {
        return FVector2f::ZeroVector;
    }

    const FVector3f RelativeAcceleration{
        LocomotionState.RotationQuaternion.UnrotateVector(LocomotionState.Acceleration)};

    return FVector2f{UAlsVector::ClampMagnitude01(RelativeAcceleration / TargetMaxAcceleration)};
}

void UAlsMoverAnimationInstance::RefreshVelocityBlend()
{
    const auto RelativeVelocity{GetRelativeVelocity()};
    const auto RelativeDirection{RelativeVelocity.GetSafeNormal()};

    auto &VelocityBlend{GroundedState.VelocityBlend};

    const auto VelocityBlendInterpolationSpeed = [&]()
    {
        if (VelocityBlend.bInitializationRequired || bPendingUpdate)
        {
            VelocityBlend.bInitializationRequired = false;
            return 100000.0f;
        }

        return Settings->Grounded.VelocityBlendInterpolationSpeed;
    }();

    VelocityBlend.ForwardAmount = FMath::FInterpTo(VelocityBlend.ForwardAmount,
                                                   UAlsMath::Clamp01(RelativeDirection.X),
                                                   GetDeltaSeconds(), VelocityBlendInterpolationSpeed);

    VelocityBlend.BackwardAmount = FMath::FInterpTo(VelocityBlend.BackwardAmount,
                                                    FMath::Abs(FMath::Clamp(RelativeDirection.X, -1.0f, 0.0f)),
                                                    GetDeltaSeconds(), VelocityBlendInterpolationSpeed);

    VelocityBlend.LeftAmount = FMath::FInterpTo(VelocityBlend.LeftAmount,
                                                FMath::Abs(FMath::Clamp(RelativeDirection.Y, -1.0f, 0.0f)),
                                                GetDeltaSeconds(), VelocityBlendInterpolationSpeed);

    VelocityBlend.RightAmount = FMath::FInterpTo(VelocityBlend.RightAmount,
                                                 UAlsMath::Clamp01(RelativeDirection.Y),
                                                 GetDeltaSeconds(), VelocityBlendInterpolationSpeed);
}

void UAlsMoverAnimationInstance::RefreshGroundedLean()
{
    const auto TargetLeanAmount{GetRelativeAccelerationAmount()};

    if (bPendingUpdate || Settings->General.LeanInterpolationSpeed <= 0.0f)
    {
        LeanState.RightAmount = TargetLeanAmount.Y;
        LeanState.ForwardAmount = TargetLeanAmount.X;
    }
    else
    {
        const auto InterpolationAmount{
            UAlsMath::ExponentialDecay(GetDeltaSeconds(), Settings->General.LeanInterpolationSpeed)};

        LeanState.RightAmount = FMath::Lerp(LeanState.RightAmount, TargetLeanAmount.Y, InterpolationAmount);
        LeanState.ForwardAmount = FMath::Lerp(LeanState.ForwardAmount, TargetLeanAmount.X, InterpolationAmount);
    }
}

void UAlsMoverAnimationInstance::RefreshMovementDirection(const float ViewRelativeVelocityYawAngle)
{
    if (RotationMode == AlsRotationModeTags::VelocityDirection || Gait == AlsGaitTags::Sprinting)
    {
        GroundedState.MovementDirection = EAlsMovementDirection::Forward;
        return;
    }

    static constexpr auto ForwardHalfAngle{70.0f};
    static constexpr auto AngleThreshold{5.0f};

    GroundedState.MovementDirection = UAlsMath::CalculateMovementDirection(
        ViewRelativeVelocityYawAngle, ForwardHalfAngle, AngleThreshold);
}

void UAlsMoverAnimationInstance::RefreshRotationYawOffsets(const float ViewRelativeVelocityYawAngle)
{
    auto &RotationYawOffsets{GroundedState.RotationYawOffsets};

    RotationYawOffsets.ForwardAngle = Settings->Grounded.RotationYawOffsetForwardCurve->GetFloatValue(
        ViewRelativeVelocityYawAngle);
    RotationYawOffsets.BackwardAngle = Settings->Grounded.RotationYawOffsetBackwardCurve->GetFloatValue(
        ViewRelativeVelocityYawAngle);
    RotationYawOffsets.LeftAngle = Settings->Grounded.RotationYawOffsetLeftCurve->GetFloatValue(
        ViewRelativeVelocityYawAngle);
    RotationYawOffsets.RightAngle = Settings->Grounded.RotationYawOffsetRightCurve->GetFloatValue(
        ViewRelativeVelocityYawAngle);
}

void UAlsMoverAnimationInstance::RefreshGroundPrediction()
{
    InAirState.GroundPredictionAmount = FMath::GetMappedRangeValueClamped(
        FVector2f{50.0f, 200.0f}, FVector2f{0.0f, 1.0f},
        FVector3f{LocomotionState.Velocity}.Z);
}

void UAlsMoverAnimationInstance::RefreshInAirLean()
{
    // Use the relative velocity direction and amount to determine how much the character should lean
    // while in air. The lean amount curve gets the vertical velocity and is used as a multiplier to
    // smoothly reverse the leaning direction when transitioning from moving upwards to moving downwards.

    static constexpr auto ReferenceSpeed{350.0f};

    const auto TargetLeanAmount{
        GetRelativeVelocity() / ReferenceSpeed * Settings->InAir.LeanAmountCurve->GetFloatValue(
            InAirState.VerticalVelocity)
    };

    if (bPendingUpdate || Settings->General.LeanInterpolationSpeed <= 0.0f)
    {
        LeanState.RightAmount = TargetLeanAmount.Y;
        LeanState.ForwardAmount = TargetLeanAmount.X;
    }
    else
    {
        const auto InterpolationAmount{
            UAlsMath::ExponentialDecay(GetDeltaSeconds(), Settings->General.LeanInterpolationSpeed)};

        LeanState.RightAmount = FMath::Lerp(LeanState.RightAmount, TargetLeanAmount.Y, InterpolationAmount);
        LeanState.ForwardAmount = FMath::Lerp(LeanState.ForwardAmount, TargetLeanAmount.X, InterpolationAmount);
    }
}

void UAlsMoverAnimationInstance::RefreshFoot(FAlsFootState &FootState, const FName &IkCurveName,
                                             const FName &LockCurveName,
                                             const FTransform &ComponentTransformInverse, const float DeltaTime) const
{
    const auto IkAmount{GetCurveValueClamped01(IkCurveName)};

    ProcessFootLockTeleport(IkAmount, FootState);
    ProcessFootLockBaseChange(IkAmount, FootState, ComponentTransformInverse);
    RefreshFootLock(IkAmount, FootState, LockCurveName, ComponentTransformInverse, DeltaTime);
}

void UAlsMoverAnimationInstance::ProcessFootLockTeleport(const float IkAmount, FAlsFootState &FootState) const
{
    // Due to network smoothing, we assume that teleportation occurs over a short period of time, not
    // in one frame, since after accepting the teleportation event, the character can still be moved for
    // some indefinite time, and this must be taken into account in order to avoid foot lock glitches.

    if (bPendingUpdate || GetWorld()->TimeSince(TeleportedTime) > 0.2f || !FAnimWeight::IsRelevant(
            IkAmount * FootState.LockAmount))
    {
        return;
    }

    const auto &ComponentTransform{GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform()};

    FootState.LockLocation = ComponentTransform.TransformPosition(FVector{FootState.LockComponentRelativeLocation});
    FootState.LockRotation = ComponentTransform.TransformRotation(FQuat{FootState.LockComponentRelativeRotation});

    if (MovementBase.bHasRelativeLocation)
    {
        const auto BaseRotationInverse{MovementBase.Rotation.Inverse()};

        FootState.LockMovementBaseRelativeLocation =
            FVector3f{BaseRotationInverse.RotateVector(FootState.LockLocation - MovementBase.Location)};

        FootState.LockMovementBaseRelativeRotation = FQuat4f{BaseRotationInverse * FootState.LockRotation};
    }
}

void UAlsMoverAnimationInstance::ProcessFootLockBaseChange(const float IkAmount, FAlsFootState &FootState,
                                                           const FTransform &ComponentTransformInverse) const
{
    if ((!bPendingUpdate && !MovementBase.bBaseChanged) || !FAnimWeight::IsRelevant(IkAmount * FootState.LockAmount))
    {
        return;
    }

    if (bPendingUpdate)
    {
        FootState.LockLocation = FootState.TargetLocation;
        FootState.LockRotation = FootState.TargetRotation;
    }

    FootState.LockComponentRelativeLocation = FVector3f{
        ComponentTransformInverse.TransformPosition(FootState.LockLocation)};
    FootState.LockComponentRelativeRotation = FQuat4f{
        ComponentTransformInverse.TransformRotation(FootState.LockRotation)};

    if (MovementBase.bHasRelativeLocation)
    {
        const auto BaseRotationInverse{MovementBase.Rotation.Inverse()};

        FootState.LockMovementBaseRelativeLocation =
            FVector3f{BaseRotationInverse.RotateVector(FootState.LockLocation - MovementBase.Location)};

        FootState.LockMovementBaseRelativeRotation = FQuat4f{BaseRotationInverse * FootState.LockRotation};
    }
    else
    {
        FootState.LockMovementBaseRelativeLocation = FVector3f::ZeroVector;
        FootState.LockMovementBaseRelativeRotation = FQuat4f::Identity;
    }
}

void UAlsMoverAnimationInstance::RefreshFootLock(const float IkAmount, FAlsFootState &FootState,
                                                 const FName &LockCurveName,
                                                 const FTransform &ComponentTransformInverse,
                                                 const float DeltaTime) const
{
    auto NewLockAmount{GetCurveValueClamped01(LockCurveName)};

    if (LocomotionState.bMovingSmooth || LocomotionMode != AlsLocomotionModeTags::Grounded)
    {
        // Smoothly disable foot lock if the character is moving or in the air,
        // instead of relying on the curve value from the animation blueprint.

        static constexpr auto MovingDecreaseSpeed{5.0f};
        static constexpr auto NotGroundedDecreaseSpeed{0.6f};

        NewLockAmount = bPendingUpdate
                            ? 0.0f
                            : FMath::Max(0.0f, FMath::Min(
                                             NewLockAmount,
                                             FootState.LockAmount - DeltaTime *
                                             (LocomotionState.bMovingSmooth
                                                  ? MovingDecreaseSpeed
                                                  : NotGroundedDecreaseSpeed)));
    }

    if (Settings->Feet.bDisableFootLock || !FAnimWeight::IsRelevant(IkAmount * NewLockAmount))
    {
        if (FootState.LockAmount > 0.0f)
        {
            FootState.LockAmount = 0.0f;

            FootState.LockLocation = FVector::ZeroVector;
            FootState.LockRotation = FQuat::Identity;

            FootState.LockComponentRelativeLocation = FVector3f::ZeroVector;
            FootState.LockComponentRelativeRotation = FQuat4f::Identity;

            FootState.LockMovementBaseRelativeLocation = FVector3f::ZeroVector;
            FootState.LockMovementBaseRelativeRotation = FQuat4f::Identity;
        }

        FootState.FinalLocation = FVector3f{ComponentTransformInverse.TransformPosition(FootState.TargetLocation)};
        FootState.FinalRotation = FQuat4f{ComponentTransformInverse.TransformRotation(FootState.TargetRotation)};
        return;
    }

    const auto bNewAmountEqualOne{FAnimWeight::IsFullWeight(NewLockAmount)};
    const auto bNewAmountGreaterThanPrevious{NewLockAmount > FootState.LockAmount};

    // Update the foot lock amount only if the new amount is less than the current amount or equal to 1. This
    // allows the foot to blend out from a locked location or lock to a new location, but never blend in.

    if (bNewAmountEqualOne)
    {
        if (bNewAmountGreaterThanPrevious)
        {
            // If the new foot lock amount is 1 and the previous amount is less than 1, then save the new foot lock location and rotation.

            if (FootState.LockAmount <= 0.9f)
            {
                // Keep the same lock location and rotation when the previous lock
                // amount is close to 1 to get rid of the foot "teleportation" issue.

                FootState.LockLocation = FootState.TargetLocation;
                FootState.LockRotation = FootState.TargetRotation;

                FootState.LockComponentRelativeLocation = FVector3f{
                    ComponentTransformInverse.TransformPosition(FootState.LockLocation)};
                FootState.LockComponentRelativeRotation = FQuat4f{
                    ComponentTransformInverse.TransformRotation(FootState.LockRotation)};
            }

            if (MovementBase.bHasRelativeLocation)
            {
                const auto BaseRotationInverse{MovementBase.Rotation.Inverse()};

                FootState.LockMovementBaseRelativeLocation =
                    FVector3f{BaseRotationInverse.RotateVector(FootState.TargetLocation - MovementBase.Location)};

                FootState.LockMovementBaseRelativeRotation = FQuat4f{BaseRotationInverse * FootState.TargetRotation};
            }
            else
            {
                FootState.LockMovementBaseRelativeLocation = FVector3f::ZeroVector;
                FootState.LockMovementBaseRelativeRotation = FQuat4f::Identity;
            }
        }

        FootState.LockAmount = 1.0f;
    }
    else if (!bNewAmountGreaterThanPrevious)
    {
        FootState.LockAmount = NewLockAmount;
    }

    if (MovementBase.bHasRelativeLocation)
    {
        FootState.LockLocation = MovementBase.Location +
                                 MovementBase.Rotation.
                                              RotateVector(FVector{FootState.LockMovementBaseRelativeLocation});

        FootState.LockRotation = MovementBase.Rotation * FQuat{FootState.LockMovementBaseRelativeRotation};
    }

    FootState.LockComponentRelativeLocation = FVector3f{
        ComponentTransformInverse.TransformPosition(FootState.LockLocation)};
    FootState.LockComponentRelativeRotation = FQuat4f{
        ComponentTransformInverse.TransformRotation(FootState.LockRotation)};

    // Limit the foot lock location so that legs do not twist into a spiral when the actor rotates quickly.

    const auto ComponentRelativeThighAxis{FeetState.PelvisRotation.RotateVector(FootState.ThighAxis)};
    const auto LockAngle{
        UAlsVector::AngleBetweenSignedXY(ComponentRelativeThighAxis, FootState.LockComponentRelativeLocation)};

    if (FMath::Abs(LockAngle) > Settings->Feet.FootLockAngleLimit + UE_KINDA_SMALL_NUMBER)
    {
        const auto ConstrainedLockAngle{
            FMath::Clamp(LockAngle, -Settings->Feet.FootLockAngleLimit, Settings->Feet.FootLockAngleLimit)};
        const FQuat4f OffsetRotation{FVector3f::UpVector, FMath::DegreesToRadians(ConstrainedLockAngle - LockAngle)};

        FootState.LockComponentRelativeLocation = OffsetRotation.RotateVector(FootState.LockComponentRelativeLocation);
        FootState.LockComponentRelativeRotation = OffsetRotation * FootState.LockComponentRelativeRotation;
        FootState.LockComponentRelativeRotation.Normalize();

        const auto &ComponentTransform{GetProxyOnAnyThread<FAnimInstanceProxy>().GetComponentTransform()};

        FootState.LockLocation = ComponentTransform.TransformPosition(FVector{FootState.LockComponentRelativeLocation});
        FootState.LockRotation = ComponentTransform.TransformRotation(FQuat{FootState.LockComponentRelativeRotation});

        if (MovementBase.bHasRelativeLocation)
        {
            const auto BaseRotationInverse{MovementBase.Rotation.Inverse()};

            FootState.LockMovementBaseRelativeLocation =
                FVector3f{BaseRotationInverse.RotateVector(FootState.LockLocation - MovementBase.Location)};

            FootState.LockMovementBaseRelativeRotation = FQuat4f{BaseRotationInverse * FootState.LockRotation};
        }
    }

    const auto FinalLocation{FMath::Lerp(FootState.TargetLocation, FootState.LockLocation, FootState.LockAmount)};

    auto FinalRotation{FQuat::FastLerp(FootState.TargetRotation, FootState.LockRotation, FootState.LockAmount)};
    FinalRotation.Normalize();

    FootState.FinalLocation = FVector3f{ComponentTransformInverse.TransformPosition(FinalLocation)};
    FootState.FinalRotation = FQuat4f{ComponentTransformInverse.TransformRotation(FinalRotation)};
}

void UAlsMoverAnimationInstance::PlayQueuedTransitionAnimation()
{
    check(IsInGameThread())

    if (TransitionsState.bStopTransitionsQueued || !IsValid(TransitionsState.QueuedTransitionSequence))
    {
        return;
    }

    PlaySlotAnimationAsDynamicMontage(TransitionsState.QueuedTransitionSequence, UAlsConstants::TransitionSlotName(),
                                      TransitionsState.QueuedTransitionBlendInDuration,
                                      TransitionsState.QueuedTransitionBlendOutDuration,
                                      TransitionsState.QueuedTransitionPlayRate, 1, 0.0f,
                                      TransitionsState.QueuedTransitionStartTime);

    TransitionsState.QueuedTransitionSequence = nullptr;
    TransitionsState.QueuedTransitionBlendInDuration = 0.0f;
    TransitionsState.QueuedTransitionBlendOutDuration = 0.0f;
    TransitionsState.QueuedTransitionPlayRate = 1.0f;
    TransitionsState.QueuedTransitionStartTime = 0.0f;
}

void UAlsMoverAnimationInstance::PlayQueuedTurnInPlaceAnimation()
{
    check(IsInGameThread());

    if (TransitionsState.bStopTransitionsQueued || !IsValid(TurnInPlaceState.QueuedSettings))
    {
        return;
    }

    const auto *TurnInPlaceSettings{TurnInPlaceState.QueuedSettings.Get()};

    PlaySlotAnimationAsDynamicMontage(TurnInPlaceSettings->Sequence, TurnInPlaceState.QueuedSlotName,
                                      Settings->TurnInPlace.BlendDuration, Settings->TurnInPlace.BlendDuration,
                                      TurnInPlaceSettings->PlayRate, 1, 0.0f);

    TurnInPlaceState.PlayRate = TurnInPlaceSettings->PlayRate;

    if (TurnInPlaceSettings->bScalePlayRateByAnimatedTurnAngle)
    {
        TurnInPlaceState.PlayRate *= FMath::Abs(
            TurnInPlaceState.QueuedTurnYawAngle / TurnInPlaceSettings->AnimatedTurnAngle);
    }

    TurnInPlaceState.QueuedSettings = nullptr;
    TurnInPlaceState.QueuedSlotName = NAME_None;
    TurnInPlaceState.QueuedTurnYawAngle = 0.0f;
}

void UAlsMoverAnimationInstance::StopQueuedTransitionAndTurnInPlaceAnimations()
{
    check(IsInGameThread())

    if (!TransitionsState.bStopTransitionsQueued)
    {
        return;
    }

    StopSlotAnimation(TransitionsState.QueuedStopTransitionsBlendOutDuration, UAlsConstants::TransitionSlotName());
    StopSlotAnimation(TransitionsState.QueuedStopTransitionsBlendOutDuration,
                      UAlsConstants::TurnInPlaceStandingSlotName());
    StopSlotAnimation(TransitionsState.QueuedStopTransitionsBlendOutDuration,
                      UAlsConstants::TurnInPlaceCrouchingSlotName());

    TransitionsState.bStopTransitionsQueued = false;
    TransitionsState.QueuedStopTransitionsBlendOutDuration = 0.0f;
}

// =================================================================================================
// Animation Instance Functions
// =================================================================================================

void UAlsMoverAnimationInstance::InitializeLook()
{
    const auto TargetYawAngle{ViewState.YawAngle};

    LookState.WorldYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw) + TargetYawAngle;
    LookState.YawAngle = TargetYawAngle;
}

void UAlsMoverAnimationInstance::RefreshLook()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshLook"), STAT_UAlsAnimationInstance_RefreshLook,
                                STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings))
    {
        return;
    }

    const auto ActorYawAngle{UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw)};

    if (MovementBase.bHasRelativeRotation)
    {
        // Offset the angle to keep it relative to the movement base.
        LookState.WorldYawAngle = FMath::UnwindDegrees(
            UE_REAL_TO_FLOAT(LookState.WorldYawAngle + MovementBase.DeltaRotation.Yaw));
    }

    float TargetYawAngle;
    float TargetPitchAngle;
    float InterpolationSpeed;

    if (RotationMode == AlsRotationModeTags::VelocityDirection)
    {
        // Look towards input direction.

        TargetYawAngle = FMath::UnwindDegrees(
            (LocomotionState.bHasInput ? LocomotionState.InputYawAngle : LocomotionState.TargetYawAngle) -
            ActorYawAngle);

        TargetPitchAngle = 0.0f;
        InterpolationSpeed = Settings->View.LookTowardsInputYawAngleInterpolationSpeed;
    }
    else
    {
        // Look towards view direction.

        TargetYawAngle = ViewState.YawAngle;
        TargetPitchAngle = ViewState.PitchAngle;
        InterpolationSpeed = Settings->View.LookTowardsCameraRotationInterpolationSpeed;
    }

    if (LookState.bInitializationRequired || InterpolationSpeed <= 0.0f)
    {
        LookState.YawAngle = TargetYawAngle;
        LookState.PitchAngle = TargetPitchAngle;

        LookState.bInitializationRequired = false;
    }
    else
    {
        const auto YawAngle{FMath::UnwindDegrees(LookState.WorldYawAngle - ActorYawAngle)};
        auto DeltaYawAngle{FMath::UnwindDegrees(TargetYawAngle - YawAngle)};

        if (DeltaYawAngle > 180.0f - UAlsRotation::CounterClockwiseRotationAngleThreshold)
        {
            DeltaYawAngle -= 360.0f;
        }
        else if (FMath::Abs(LocomotionState.YawSpeed) > UE_SMALL_NUMBER && FMath::Abs(TargetYawAngle) > 90.0f)
        {
            // When interpolating yaw angle, favor the character rotation direction, over the shortest rotation
            // direction, so that the rotation of the head remains synchronized with the rotation of the body.

            DeltaYawAngle = LocomotionState.YawSpeed > 0.0f ? FMath::Abs(DeltaYawAngle) : -FMath::Abs(DeltaYawAngle);
        }

        const auto InterpolationAmount{UAlsMath::ExponentialDecay(GetDeltaSeconds(), InterpolationSpeed)};

        LookState.YawAngle = FMath::UnwindDegrees(YawAngle + DeltaYawAngle * InterpolationAmount);
        LookState.PitchAngle = UAlsRotation::LerpAngle(LookState.PitchAngle, TargetPitchAngle, InterpolationAmount);
    }

    LookState.WorldYawAngle = FMath::UnwindDegrees(ActorYawAngle + LookState.YawAngle);

    // Separate the yaw angle into 3 separate values. These 3 values are used to improve the
    // blending of the view when rotating completely around the character. This allows to
    // keep the view responsive but still smoothly blend from left to right or right to left.

    LookState.YawForwardAmount = LookState.YawAngle / 360.0f + 0.5f;
    LookState.YawLeftAmount = 0.5f - FMath::Abs(LookState.YawForwardAmount - 0.5f);
    LookState.YawRightAmount = 0.5f + FMath::Abs(LookState.YawForwardAmount - 0.5f);
}

void UAlsMoverAnimationInstance::InitializeLean()
{
    LeanState.RightAmount = 0.0f;
    LeanState.ForwardAmount = 0.0f;
}

void UAlsMoverAnimationInstance::InitializeGrounded()
{
    GroundedState.VelocityBlend.bInitializationRequired = true;
}

void UAlsMoverAnimationInstance::RefreshGrounded()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsMoverAnimationInstance::RefreshGrounded"),
                                STAT_UAlsAnimationInstance_RefreshGrounded, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings))
    {
        return;
    }

    RefreshVelocityBlend();
    RefreshGroundedLean();
}

void UAlsMoverAnimationInstance::RefreshGroundedMovement()
{
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsMoverAnimationInstance::RefreshGroundedMovement"),
                                STAT_UAlsAnimationInstance_RefreshGroundedMovement, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings))
    {
        return;
    }

    GroundedState.HipsDirectionLockAmount = FMath::Clamp(GetCurveValue(UAlsConstants::HipsDirectionLockCurveName()),
                                                         -1.0f, 1.0f);

    const auto ViewRelativeVelocityYawAngle{
        FMath::UnwindDegrees(UE_REAL_TO_FLOAT(LocomotionState.VelocityYawAngle - ViewState.Rotation.Yaw))
    };

    RefreshMovementDirection(ViewRelativeVelocityYawAngle);
    RefreshRotationYawOffsets(ViewRelativeVelocityYawAngle);
}

void UAlsMoverAnimationInstance::InitializeStandingMovement()
{
    StandingState.PlayRate = 1.0f;
}

void UAlsMoverAnimationInstance::RefreshStandingMovement()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshStandingMovement"),
                                STAT_UAlsAnimationInstance_RefreshStandingMovement, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings))
    {
        return;
    }

    const auto Speed{LocomotionState.Speed / LocomotionState.Scale};

    // Calculate the stride blend amount. This value is used within the blend spaces to scale the stride (distance feet travel)
    // so that the character can walk or run at different movement speeds. It also allows the walk or run gait animations to
    // blend independently while still matching the animation speed to the movement speed, preventing the character from needing
    // to play a half walk + half run blend. The curves are used to map the stride amount to the speed for maximum control.

    StandingState.StrideBlendAmount = FMath::Lerp(Settings->Standing.StrideBlendAmountWalkCurve->GetFloatValue(Speed),
                                                  Settings->Standing.StrideBlendAmountRunCurve->GetFloatValue(Speed),
                                                  PoseState.UnweightedGaitRunningAmount);

    // Calculate the walk run blend amount. This value is used within the blend spaces to blend between walking and running.

    StandingState.WalkRunBlendAmount = Gait == AlsGaitTags::Walking ? 0.0f : 1.0f;

    // Calculate the standing play rate by dividing the character's speed by the animated speed for each gait.
    // The interpolation is determined by the gait amount curve that exists on every locomotion cycle so that
    // the play rate is always in sync with the currently blended animation. The value is also divided by the
    // stride blend and the capsule scale so that the play rate increases as the stride or scale gets smaller.

    // TODO Automatically calculate the play rate, such as is done in the UAnimDistanceMatchingLibrary::SetPlayrateToMatchSpeed() function.

    const auto WalkRunSpeedAmount{
        FMath::Lerp(Speed / Settings->Standing.AnimatedWalkSpeed,
                    Speed / Settings->Standing.AnimatedRunSpeed,
                    PoseState.UnweightedGaitRunningAmount)
    };

    const auto WalkRunSprintSpeedAmount{
        FMath::Lerp(WalkRunSpeedAmount,
                    Speed / Settings->Standing.AnimatedSprintSpeed,
                    PoseState.UnweightedGaitSprintingAmount)
    };

    // Do not let the play rate be exactly zero, otherwise animation notifies
    // may start to be triggered every frame until the play rate is changed.
    // TODO Check the need for this hack in future engine versions.

    StandingState.PlayRate = FMath::Clamp(WalkRunSprintSpeedAmount / StandingState.StrideBlendAmount,
                                          UE_KINDA_SMALL_NUMBER, 3.0f);

    StandingState.SprintBlockAmount = GetCurveValueClamped01(UAlsConstants::SprintBlockCurveName());

    if (Gait != AlsGaitTags::Sprinting)
    {
        StandingState.SprintTime = 0.0f;
        StandingState.SprintAccelerationAmount = 0.0f;
        return;
    }

    // Use the relative acceleration as the sprint relative acceleration if less than 0.5 seconds has
    // elapsed since the start of the sprint, otherwise set the sprint relative acceleration to zero.
    // This is necessary to apply the acceleration animation only at the beginning of the sprint.

    static constexpr auto SprintTimeThreshold{0.5f};

    StandingState.SprintTime = bPendingUpdate
                                   ? SprintTimeThreshold
                                   : StandingState.SprintTime + GetDeltaSeconds();

    StandingState.SprintAccelerationAmount = StandingState.SprintTime >= SprintTimeThreshold
                                                 ? 0.0f
                                                 : GetRelativeAccelerationAmount().X;
}

void UAlsMoverAnimationInstance::ActivatePivot()
{
    StandingState.bPivotActive = true;
}

void UAlsMoverAnimationInstance::RefreshCrouchingMovement()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshCrouchingMovement"),
                                STAT_UAlsAnimationInstance_RefreshCrouchingMovement, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (!IsValid(Settings))
    {
        return;
    }

    const auto Speed{LocomotionState.Speed / LocomotionState.Scale};

    CrouchingState.StrideBlendAmount = Settings->Crouching.StrideBlendAmountCurve->GetFloatValue(Speed);

    CrouchingState.PlayRate = FMath::Clamp(
        Speed / (Settings->Crouching.AnimatedCrouchSpeed * CrouchingState.StrideBlendAmount),
        UE_KINDA_SMALL_NUMBER, 2.0f);
}

void UAlsMoverAnimationInstance::RefreshInAir()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    if (!bPendingUpdate && InAirState.bJumped)
    {
        static constexpr auto ReferenceSpeed{600.0f};
        static constexpr auto MinPlayRate{1.2f};
        static constexpr auto MaxPlayRate{1.5f};

        InAirState.JumpPlayRate = UAlsMath::LerpClamped(MinPlayRate, MaxPlayRate,
                                                        LocomotionState.Speed / ReferenceSpeed);
        InAirState.bJumped = false; // Add reset
    }

    InAirState.VerticalVelocity = UE_REAL_TO_FLOAT(LocomotionState.Velocity.Z); // Add if needed, original has it

    RefreshGroundPrediction();
    RefreshInAirLean();
}

void UAlsMoverAnimationInstance::PlayQuickStopAnimation()
{
    if (!IsValid(Settings))
    {
        return;
    }

    if (RotationMode != AlsRotationModeTags::VelocityDirection)
    {
        PlayTransitionLeftAnimation(Settings->Transitions.QuickStopBlendInDuration,
                                    Settings->Transitions.QuickStopBlendOutDuration,
                                    Settings->Transitions.QuickStopPlayRate.X,
                                    Settings->Transitions.QuickStopStartTime);
        return;
    }

    auto RemainingYawAngle{
        FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
            (LocomotionState.bHasInput ? LocomotionState.InputYawAngle : LocomotionState.TargetYawAngle) -
            LocomotionState.Rotation.Yaw))
    };

    RemainingYawAngle = UAlsRotation::RemapAngleForCounterClockwiseRotation(RemainingYawAngle);

    // Scale quick stop animation play rate based on how far the character
    // is going to rotate. At 180 degrees, the play rate will be maximal.

    if (RemainingYawAngle <= 0.0f)
    {
        PlayTransitionLeftAnimation(Settings->Transitions.QuickStopBlendInDuration,
                                    Settings->Transitions.QuickStopBlendOutDuration,
                                    FMath::Lerp(Settings->Transitions.QuickStopPlayRate.X,
                                                Settings->Transitions.QuickStopPlayRate.Y,
                                                FMath::Abs(RemainingYawAngle) / 180.0f),
                                    Settings->Transitions.QuickStopStartTime);
    }
    else
    {
        PlayTransitionRightAnimation(Settings->Transitions.QuickStopBlendInDuration,
                                     Settings->Transitions.QuickStopBlendOutDuration,
                                     FMath::Lerp(Settings->Transitions.QuickStopPlayRate.X,
                                                 Settings->Transitions.QuickStopPlayRate.Y,
                                                 FMath::Abs(RemainingYawAngle) / 180.0f),
                                     Settings->Transitions.QuickStopStartTime);
    }
}

void UAlsMoverAnimationInstance::PlayTransitionAnimation(UAnimSequenceBase *Sequence, float BlendInDuration,
                                                         float BlendOutDuration, float PlayRate, float StartTime,
                                                         bool bFromStandingIdleOnly)
{
    if (bFromStandingIdleOnly && (LocomotionState.bMoving || Stance != AlsStanceTags::Standing))
    {
        return;
    }

    // Animation montages can't be played in the worker thread, so queue them up to play later in the game thread.

    TransitionsState.QueuedTransitionSequence = Sequence;
    TransitionsState.QueuedTransitionBlendInDuration = BlendInDuration;
    TransitionsState.QueuedTransitionBlendOutDuration = BlendOutDuration;
    TransitionsState.QueuedTransitionPlayRate = PlayRate;
    TransitionsState.QueuedTransitionStartTime = StartTime;

    if (IsInGameThread())
    {
        PlayQueuedTransitionAnimation();
    }
}

void UAlsMoverAnimationInstance::PlayTransitionLeftAnimation(const float BlendInDuration, const float BlendOutDuration,
                                                             const float PlayRate,
                                                             const float StartTime, const bool bFromStandingIdleOnly)
{
    if (!IsValid(Settings))
    {
        return;
    }

    PlayTransitionAnimation(Stance == AlsStanceTags::Crouching
                                ? Settings->Transitions.CrouchingLeftSequence
                                : Settings->Transitions.StandingLeftSequence,
                            BlendInDuration, BlendOutDuration, PlayRate, StartTime, bFromStandingIdleOnly);
}

void UAlsMoverAnimationInstance::PlayTransitionRightAnimation(const float BlendInDuration,
                                                              const float BlendOutDuration, const float PlayRate,
                                                              const float StartTime, const bool bFromStandingIdleOnly)
{
    if (!IsValid(Settings))
    {
        return;
    }

    PlayTransitionAnimation(Stance == AlsStanceTags::Crouching
                                ? Settings->Transitions.CrouchingRightSequence
                                : Settings->Transitions.StandingRightSequence,
                            BlendInDuration, BlendOutDuration, PlayRate, StartTime, bFromStandingIdleOnly);
}

void UAlsMoverAnimationInstance::StopTransitionAndTurnInPlaceAnimations(float BlendOutDuration)
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshDynamicTransitions"),
                                STAT_UAlsAnimationInstance_RefreshDynamicTransitions, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (DynamicTransitionsState.bUpdatedThisFrame || !IsValid(Settings))
    {
        return;
    }

    DynamicTransitionsState.bUpdatedThisFrame = true;

    if (DynamicTransitionsState.FrameDelay > 0)
    {
        DynamicTransitionsState.FrameDelay -= 1;
        return;
    }

    if (!TransitionsState.bTransitionsAllowed)
    {
        return;
    }

    // Check each foot to see if the location difference between the foot look and its desired / target location
    // exceeds a threshold. If it does, play an additive transition animation on that foot. The currently set
    // transition plays the second half of a 2 foot transition animation, so that only a single foot moves.

    const auto FootLockDistanceThresholdSquared{
        FMath::Square(Settings->DynamicTransitions.FootLockDistanceThreshold * LocomotionState.Scale)
    };

    const auto FootLockLeftDistanceSquared{
        FVector::DistSquared(FeetState.Left.TargetLocation, FeetState.Left.LockLocation)};
    const auto FootLockRightDistanceSquared{
        FVector::DistSquared(FeetState.Right.TargetLocation, FeetState.Right.LockLocation)};

    const auto bTransitionLeftAllowed{
        FAnimWeight::IsRelevant(FeetState.Left.LockAmount) && FootLockLeftDistanceSquared >
        FootLockDistanceThresholdSquared
    };

    const auto bTransitionRightAllowed{
        FAnimWeight::IsRelevant(FeetState.Right.LockAmount) && FootLockRightDistanceSquared >
        FootLockDistanceThresholdSquared
    };

    if (!bTransitionLeftAllowed && !bTransitionRightAllowed)
    {
        return;
    }

    TObjectPtr<UAnimSequenceBase> DynamicTransitionSequence;

    // If both transitions are allowed, choose the one with a greater lock distance.

    if (!bTransitionLeftAllowed)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingRightSequence
                                        : Settings->DynamicTransitions.StandingRightSequence;
    }
    else if (!bTransitionRightAllowed)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingLeftSequence
                                        : Settings->DynamicTransitions.StandingLeftSequence;
    }
    else if (FootLockLeftDistanceSquared >= FootLockRightDistanceSquared)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingLeftSequence
                                        : Settings->DynamicTransitions.StandingLeftSequence;
    }
    else
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingRightSequence
                                        : Settings->DynamicTransitions.StandingRightSequence;
    }

    if (IsValid(DynamicTransitionSequence))
    {
        // Block next dynamic transitions for about 2 frames to give the animation blueprint some time to properly react to the animation.

        DynamicTransitionsState.FrameDelay = 2;

        // Animation montages can't be played in the worker thread, so queue them up to play later in the game thread.

        TransitionsState.QueuedTransitionSequence = DynamicTransitionSequence;
        TransitionsState.QueuedTransitionBlendInDuration = Settings->DynamicTransitions.BlendDuration;
        TransitionsState.QueuedTransitionBlendOutDuration = Settings->DynamicTransitions.BlendDuration;
        TransitionsState.QueuedTransitionPlayRate = Settings->DynamicTransitions.PlayRate;
        TransitionsState.QueuedTransitionStartTime = 0.0f;

        if (IsInGameThread())
        {
            PlayQueuedTransitionAnimation();
        }
    }
}

void UAlsMoverAnimationInstance::RefreshDynamicTransitions()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshDynamicTransitions"),
                                STAT_UAlsAnimationInstance_RefreshDynamicTransitions, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (DynamicTransitionsState.bUpdatedThisFrame || !IsValid(Settings))
    {
        return;
    }

    DynamicTransitionsState.bUpdatedThisFrame = true;

    if (DynamicTransitionsState.FrameDelay > 0)
    {
        DynamicTransitionsState.FrameDelay -= 1;
        return;
    }

    if (!TransitionsState.bTransitionsAllowed)
    {
        return;
    }

    // Check each foot to see if the location difference between the foot look and its desired / target location
    // exceeds a threshold. If it does, play an additive transition animation on that foot. The currently set
    // transition plays the second half of a 2 foot transition animation, so that only a single foot moves.

    const auto FootLockDistanceThresholdSquared{
        FMath::Square(Settings->DynamicTransitions.FootLockDistanceThreshold * LocomotionState.Scale)
    };

    const auto FootLockLeftDistanceSquared{
        FVector::DistSquared(FeetState.Left.TargetLocation, FeetState.Left.LockLocation)};
    const auto FootLockRightDistanceSquared{
        FVector::DistSquared(FeetState.Right.TargetLocation, FeetState.Right.LockLocation)};

    const auto bTransitionLeftAllowed{
        FAnimWeight::IsRelevant(FeetState.Left.LockAmount) && FootLockLeftDistanceSquared >
        FootLockDistanceThresholdSquared
    };

    const auto bTransitionRightAllowed{
        FAnimWeight::IsRelevant(FeetState.Right.LockAmount) && FootLockRightDistanceSquared >
        FootLockDistanceThresholdSquared
    };

    if (!bTransitionLeftAllowed && !bTransitionRightAllowed)
    {
        return;
    }

    TObjectPtr<UAnimSequenceBase> DynamicTransitionSequence;

    // If both transitions are allowed, choose the one with a greater lock distance.

    if (!bTransitionLeftAllowed)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingRightSequence
                                        : Settings->DynamicTransitions.StandingRightSequence;
    }
    else if (!bTransitionRightAllowed)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingLeftSequence
                                        : Settings->DynamicTransitions.StandingLeftSequence;
    }
    else if (FootLockLeftDistanceSquared >= FootLockRightDistanceSquared)
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingLeftSequence
                                        : Settings->DynamicTransitions.StandingLeftSequence;
    }
    else
    {
        DynamicTransitionSequence = Stance == AlsStanceTags::Crouching
                                        ? Settings->DynamicTransitions.CrouchingRightSequence
                                        : Settings->DynamicTransitions.StandingRightSequence;
    }

    if (IsValid(DynamicTransitionSequence))
    {
        // Block next dynamic transitions for about 2 frames to give the animation blueprint some time to properly react to the animation.

        DynamicTransitionsState.FrameDelay = 2;

        // Animation montages can't be played in the worker thread, so queue them up to play later in the game thread.

        TransitionsState.QueuedTransitionSequence = DynamicTransitionSequence;
        TransitionsState.QueuedTransitionBlendInDuration = Settings->DynamicTransitions.BlendDuration;
        TransitionsState.QueuedTransitionBlendOutDuration = Settings->DynamicTransitions.BlendDuration;
        TransitionsState.QueuedTransitionPlayRate = Settings->DynamicTransitions.PlayRate;
        TransitionsState.QueuedTransitionStartTime = 0.0f;

        if (IsInGameThread())
        {
            PlayQueuedTransitionAnimation();
        }
    }
}

bool UAlsMoverAnimationInstance::IsRotateInPlaceAllowed()
{
    return RotationMode == AlsRotationModeTags::Aiming || ViewMode == AlsViewModeTags::FirstPerson;
}

void UAlsMoverAnimationInstance::RefreshRotateInPlace()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsAnimationInstance::RefreshRotateInPlace"),
                                STAT_UAlsAnimationInstance_RefreshRotateInPlace, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (RotateInPlaceState.bUpdatedThisFrame || !IsValid(Settings))
    {
        return;
    }

    RotateInPlaceState.bUpdatedThisFrame = true;

    if (LocomotionState.bMoving || !IsRotateInPlaceAllowed())
    {
        RotateInPlaceState.bRotatingLeft = false;
        RotateInPlaceState.bRotatingRight = false;
    }
    else
    {
        // Check if the character should rotate left or right by checking if the view yaw angle exceeds the threshold.

        RotateInPlaceState.bRotatingLeft = ViewState.YawAngle < -Settings->RotateInPlace.ViewYawAngleThreshold;
        RotateInPlaceState.bRotatingRight = ViewState.YawAngle > Settings->RotateInPlace.ViewYawAngleThreshold;
    }

    static constexpr auto PlayRateInterpolationSpeed{5.0f};

    if (!RotateInPlaceState.bRotatingLeft && !RotateInPlaceState.bRotatingRight)
    {
        RotateInPlaceState.PlayRate = bPendingUpdate
                                          ? Settings->RotateInPlace.PlayRate.X
                                          : FMath::FInterpTo(RotateInPlaceState.PlayRate,
                                                             Settings->RotateInPlace.PlayRate.X,
                                                             GetDeltaSeconds(), PlayRateInterpolationSpeed);
        return;
    }

    // If the character should rotate, set the play rate to scale with the view yaw
    // speed. This makes the character rotate faster when moving the camera faster.

    const auto PlayRate{
        FMath::GetMappedRangeValueClamped(Settings->RotateInPlace.ReferenceViewYawSpeed,
                                          Settings->RotateInPlace.PlayRate, ViewState.YawSpeed)
    };

    RotateInPlaceState.PlayRate = bPendingUpdate
                                      ? PlayRate
                                      : FMath::FInterpTo(RotateInPlaceState.PlayRate, PlayRate,
                                                         GetDeltaSeconds(), PlayRateInterpolationSpeed);
}

bool UAlsMoverAnimationInstance::IsTurnInPlaceAllowed()
{
    return RotationMode == AlsRotationModeTags::ViewDirection && ViewMode != AlsViewModeTags::FirstPerson;
}

void UAlsMoverAnimationInstance::InitializeTurnInPlace()
{
    TurnInPlaceState.ActivationDelay = 0.0f;
}

void UAlsMoverAnimationInstance::RefreshTurnInPlace()
{
#if WITH_EDITOR
    if (!IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
    {
        return;
    }
#endif

    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsMoverAnimationInstance::RefreshTurnInPlace"),
                                STAT_UAlsAnimationInstance_RefreshTurnInPlace, STATGROUP_Als)
    TRACE_CPUPROFILER_EVENT_SCOPE(__FUNCTION__);

    if (TurnInPlaceState.bUpdatedThisFrame || !IsValid(Settings))
    {
        return;
    }

    TurnInPlaceState.bUpdatedThisFrame = true;

    if (!TransitionsState.bTransitionsAllowed || !IsTurnInPlaceAllowed())
    {
        TurnInPlaceState.ActivationDelay = 0.0f;
        return;
    }

    if (ViewState.YawSpeed >= Settings->TurnInPlace.ViewYawSpeedThreshold ||
        FMath::Abs(ViewState.YawAngle) <= Settings->TurnInPlace.ViewYawAngleThreshold)
    {
        TurnInPlaceState.ActivationDelay = 0.0f;
        return;
    }

    TurnInPlaceState.ActivationDelay = TurnInPlaceState.ActivationDelay + GetDeltaSeconds();

    const auto ActivationDelay{
        FMath::GetMappedRangeValueClamped({Settings->TurnInPlace.ViewYawAngleThreshold, 180.0f},
                                          Settings->TurnInPlace.ViewYawAngleToActivationDelay,
                                          FMath::Abs(ViewState.YawAngle))
    };

    if (TurnInPlaceState.ActivationDelay <= ActivationDelay)
    {
        return;
    }

    const auto bTurnLeft{UAlsRotation::RemapAngleForCounterClockwiseRotation(ViewState.YawAngle) <= 0.0f};

    UAlsTurnInPlaceSettings *TurnInPlaceSettings{nullptr};
    FName TurnInPlaceSlotName;

    if (Stance == AlsStanceTags::Standing)
    {
        TurnInPlaceSlotName = UAlsConstants::TurnInPlaceStandingSlotName();

        if (FMath::Abs(ViewState.YawAngle) < Settings->TurnInPlace.Turn180AngleThreshold)
        {
            TurnInPlaceSettings = bTurnLeft
                                      ? Settings->TurnInPlace.StandingTurn90Left
                                      : Settings->TurnInPlace.StandingTurn90Right;
        }
        else
        {
            TurnInPlaceSettings = bTurnLeft
                                      ? Settings->TurnInPlace.StandingTurn180Left
                                      : Settings->TurnInPlace.StandingTurn180Right;
        }
    }
    else if (Stance == AlsStanceTags::Crouching)
    {
        TurnInPlaceSlotName = UAlsConstants::TurnInPlaceCrouchingSlotName();

        if (FMath::Abs(ViewState.YawAngle) < Settings->TurnInPlace.Turn180AngleThreshold)
        {
            TurnInPlaceSettings = bTurnLeft
                                      ? Settings->TurnInPlace.CrouchingTurn90Left
                                      : Settings->TurnInPlace.CrouchingTurn90Right;
        }
        else
        {
            TurnInPlaceSettings = bTurnLeft
                                      ? Settings->TurnInPlace.CrouchingTurn180Left
                                      : Settings->TurnInPlace.CrouchingTurn180Right;
        }
    }

    if (IsValid(TurnInPlaceSettings) && ALS_ENSURE(IsValid(TurnInPlaceSettings->Sequence)))
    {
        TurnInPlaceState.QueuedSettings = TurnInPlaceSettings;
        TurnInPlaceState.QueuedSlotName = TurnInPlaceSlotName;
        TurnInPlaceState.QueuedTurnYawAngle = ViewState.YawAngle;

        if (IsInGameThread())
        {
            PlayQueuedTurnInPlaceAnimation();
        }
    }
}

FPoseSnapshot &UAlsMoverAnimationInstance::SnapshotFinalRagdollPose()
{
    check(IsInGameThread());

    SnapshotPose(RagdollingState.FinalRagdollPose);
    return RagdollingState.FinalRagdollPose;
}

float UAlsMoverAnimationInstance::GetCurveValueClamped01(const FName &CurveName) const
{
    return UAlsMath::Clamp01(GetCurveValue(CurveName));
}

FAlsControlRigInput UAlsMoverAnimationInstance::GetControlRigInput() const
{
    return {
        .bUseHandIkBones = !IsValid(Settings) || Settings->General.bUseHandIkBones,
        .bUseFootIkBones = !IsValid(Settings) || Settings->General.bUseFootIkBones,
        .bFootOffsetAllowed = LocomotionMode != AlsLocomotionModeTags::InAir,
        .VelocityBlendForwardAmount = GroundedState.VelocityBlend.ForwardAmount,
        .VelocityBlendBackwardAmount = GroundedState.VelocityBlend.BackwardAmount,
        .FootLeftLocation{FVector{FeetState.Left.FinalLocation}},
        .FootLeftRotation{FQuat{FeetState.Left.FinalRotation}},
        .FootRightLocation{FVector{FeetState.Right.FinalLocation}},
        .FootRightRotation{FQuat{FeetState.Right.FinalRotation}},
        .SpineYawAngle = SpineState.YawAngle
    };
}