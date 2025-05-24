#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AlsMoverTestGameMode.generated.h"

/**
 * Simple game mode for testing ALS Mover implementation
 */
UCLASS(BlueprintType, Blueprintable)
class ALSMOVER_API AAlsMoverTestGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AAlsMoverTestGameMode();

protected:
    virtual void BeginPlay() override;
};