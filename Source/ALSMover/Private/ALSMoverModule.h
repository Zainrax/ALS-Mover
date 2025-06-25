#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FAutoCompleteCommand;

class FALSMoverModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if ALLOW_CONSOLE
	void OnRegisterConsoleAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteCommands);
#endif
};