#include "AlsMoverLinkedAnimationInstance.h"

#include "AlsMoverAnimationInstance.h"
#include "AlsAnimationInstanceProxy.h"
#include "AlsMoverCharacter.h"
#include "Utility/AlsMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverLinkedAnimationInstance)

UAlsMoverLinkedAnimationInstance::UAlsMoverLinkedAnimationInstance()
{
	bUseMainInstanceMontageEvaluationData = true;
}

void UAlsMoverLinkedAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Parent = Cast<UAlsMoverAnimationInstance>(GetSkelMeshComponent()->GetAnimInstance());
	Character = Cast<AAlsMoverCharacter>(GetOwningActor());

#if WITH_EDITOR
	const auto* World{GetWorld()};

	if (IsValid(World) && !World->IsGameWorld())
	{
		// Use default objects for editor preview.

		if (!Parent.IsValid())
		{
			Parent = GetMutableDefault<UAlsMoverAnimationInstance>();
		}

		if (!IsValid(Character))
		{
			Character = GetMutableDefault<AAlsMoverCharacter>();
		}
	}
#endif
}

void UAlsMoverLinkedAnimationInstance::NativeBeginPlay()
{
	ALS_ENSURE_MESSAGE(Parent.IsValid(),
	                   TEXT("%s (%s) should only be used as a linked animation instance within the %s animation blueprint!"),
	                   ALS_GET_TYPE_STRING(UAlsMoverLinkedAnimationInstance).GetData(), *GetClass()->GetName(),
	                   ALS_GET_TYPE_STRING(UAlsMoverAnimationInstance).GetData());

	Super::NativeBeginPlay();
}

FAnimInstanceProxy* UAlsMoverLinkedAnimationInstance::CreateAnimInstanceProxy()
{
	return new FAlsAnimationInstanceProxy{this};
}

void UAlsMoverLinkedAnimationInstance::InitializeLook()
{
	if (Parent.IsValid())
	{
		Parent->InitializeLook();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshLook()
{
	if (Parent.IsValid())
	{
		Parent->RefreshLook();
	}
}

void UAlsMoverLinkedAnimationInstance::InitializeLean()
{
	if (Parent.IsValid())
	{
		Parent->InitializeLean();
	}
}

void UAlsMoverLinkedAnimationInstance::InitializeGrounded()
{
	if (Parent.IsValid())
	{
		Parent->InitializeGrounded();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshGrounded()
{
	if (Parent.IsValid())
	{
		Parent->RefreshGrounded();
	}
}

void UAlsMoverLinkedAnimationInstance::ResetGroundedEntryMode()
{
	if (Parent.IsValid())
	{
		Parent->ResetGroundedEntryMode();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshGroundedMovement()
{
	if (Parent.IsValid())
	{
		Parent->RefreshGroundedMovement();
	}
}

void UAlsMoverLinkedAnimationInstance::SetHipsDirection(const EAlsHipsDirection NewHipsDirection)
{
	if (Parent.IsValid())
	{
		Parent->SetHipsDirection(NewHipsDirection);
	}
}

void UAlsMoverLinkedAnimationInstance::InitializeStandingMovement()
{
	if (Parent.IsValid())
	{
		Parent->InitializeStandingMovement();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshStandingMovement()
{
	if (Parent.IsValid())
	{
		Parent->RefreshStandingMovement();
	}
}

void UAlsMoverLinkedAnimationInstance::ResetPivot()
{
	if (Parent.IsValid())
	{
		Parent->ResetPivot();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshCrouchingMovement()
{
	if (Parent.IsValid())
	{
		Parent->RefreshCrouchingMovement();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshInAir()
{
	if (Parent.IsValid())
	{
		Parent->RefreshInAir();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshDynamicTransitions()
{
	if (Parent.IsValid())
	{
		Parent->RefreshDynamicTransitions();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshRotateInPlace()
{
	if (Parent.IsValid())
	{
		Parent->RefreshRotateInPlace();
	}
}

void UAlsMoverLinkedAnimationInstance::InitializeTurnInPlace()
{
	if (Parent.IsValid())
	{
		Parent->InitializeTurnInPlace();
	}
}

void UAlsMoverLinkedAnimationInstance::RefreshTurnInPlace()
{
	if (Parent.IsValid())
	{
		Parent->RefreshTurnInPlace();
	}
}