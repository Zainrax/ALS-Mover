#include "AlsMoverTestGameMode.h"
#include "AlsMoverCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsMoverTestGameMode)

AAlsMoverTestGameMode::AAlsMoverTestGameMode()
{
    // Set default pawn class to our ALS Mover character
    DefaultPawnClass = AAlsMoverCharacter::StaticClass();
    
    // Use default player controller for now
    PlayerControllerClass = APlayerController::StaticClass();
}

void AAlsMoverTestGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Warning, TEXT("ALS Mover Test Game Mode Started"));
}