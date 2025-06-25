#include "ALSMoverModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Console.h"
#include "ConsoleSettings.h"

#define LOCTEXT_NAMESPACE "ALSMover"

void FALSMoverModule::StartupModule()
{
	// Module startup logic
	
#if ALLOW_CONSOLE
	UConsole::RegisterConsoleAutoCompleteEntries.AddRaw(this, &FALSMoverModule::OnRegisterConsoleAutoCompleteEntries);
#endif
}

void FALSMoverModule::ShutdownModule()
{
	// Module shutdown logic
	
#if ALLOW_CONSOLE
	UConsole::RegisterConsoleAutoCompleteEntries.RemoveAll(this);
#endif
}

#if ALLOW_CONSOLE
void FALSMoverModule::OnRegisterConsoleAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteCommands)
{
	const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();
	const FColor CommandColor = ConsoleSettings ? ConsoleSettings->AutoCompleteCommandColor : FColor::White;
	
	// Register main category
	{
		FAutoCompleteCommand& Command = AutoCompleteCommands.AddDefaulted_GetRef();
		Command.Command = TEXT("ShowDebug AlsMover");
		Command.Desc = TEXT("Shows all ALS Mover debug information");
		Command.Color = CommandColor;
	}
	
	// Register sub-categories
	{
		FAutoCompleteCommand& Command = AutoCompleteCommands.AddDefaulted_GetRef();
		Command.Command = TEXT("ShowDebug AlsMover.States");
		Command.Desc = TEXT("Shows ALS Mover state information (Stance, Gait, Rotation Mode)");
		Command.Color = CommandColor;
	}
	
	{
		FAutoCompleteCommand& Command = AutoCompleteCommands.AddDefaulted_GetRef();
		Command.Command = TEXT("ShowDebug AlsMover.Movement");
		Command.Desc = TEXT("Shows ALS Mover movement information (Velocity, Speed, Movement Mode)");
		Command.Color = CommandColor;
	}
	
	{
		FAutoCompleteCommand& Command = AutoCompleteCommands.AddDefaulted_GetRef();
		Command.Command = TEXT("ShowDebug AlsMover.Input");
		Command.Desc = TEXT("Shows ALS Mover input information (Move/Look input, button states)");
		Command.Color = CommandColor;
	}
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FALSMoverModule, ALSMover)