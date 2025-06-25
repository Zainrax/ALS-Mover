#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "AlsMoverAnimationInterface.generated.h"

class UAlsCharacterMoverComponent;

/**
 * Base animation instance class for ALS Mover characters
 * Provides a clean interface for reading state from the Mover system
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API UAlsMoverAnimationInterface : public UAnimInstance
{
    GENERATED_BODY()

public:
    UAlsMoverAnimationInterface();

protected:
    // ~Begin UAnimInstance Interface
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    // ~End UAnimInstance Interface

    // Get the mover component from the owning character
    UFUNCTION(BlueprintCallable, Category = "ALS Mover Animation")
    UAlsCharacterMoverComponent* GetAlsMoverComponent() const;

    // ---- State Variables (Updated from Mover) ----
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    FGameplayTag Gait;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    FGameplayTag Stance;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    FGameplayTag RotationMode;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    FGameplayTag LocomotionMode;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    FGameplayTag OverlayMode;
    
    // ---- Movement Variables ----
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    float Speed = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    FVector Velocity = FVector::ZeroVector;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    FVector Acceleration = FVector::ZeroVector;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    bool bIsMoving = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    bool bHasMovementInput = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|Movement", meta = (AllowPrivateAccess = "true"))
    bool bIsOnGround = false;
    
    // ---- Helper Variables ----
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    bool bIsWalking = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    bool bIsRunning = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    bool bIsSprinting = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    bool bIsCrouching = false;
    
    UPROPERTY(BlueprintReadOnly, Category = "ALS Animation|State", meta = (AllowPrivateAccess = "true"))
    bool bIsAiming = false;

    // ---- Settings ----
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Animation|Settings")
    float MovingThreshold = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Animation|Settings")
    float InputThreshold = 0.1f;

private:
    // Update all state variables from the mover component
    void UpdateStateFromMover();
    
    // Update helper boolean variables
    void UpdateHelperVariables();
    
    // Cached mover component reference
    UPROPERTY()
    TObjectPtr<UAlsCharacterMoverComponent> CachedMoverComponent = nullptr;
};