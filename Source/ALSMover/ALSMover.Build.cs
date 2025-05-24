using UnrealBuildTool;

public class ALSMover : ModuleRules
{
	public ALSMover(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		bEnableNonInlinedGenCppWarnings = true;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"GameplayTags", 
			"AnimGraphRuntime",
			"EnhancedInput",
			"Mover",
			"ALS", "ALSCamera", // Dependency on existing ALS module for shared types/settings
			"NetworkPrediction" // Required for some Mover classes in Blueprint
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"EngineSettings", 
			"NetCore", 
			"PhysicsCore",
			"DeveloperSettings"
		});

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"MessageLog"
			});
		}

		SetupIrisSupport(Target);
	}
}