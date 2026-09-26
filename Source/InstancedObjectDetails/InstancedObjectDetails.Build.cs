using UnrealBuildTool;

public class InstancedObjectDetails : ModuleRules
{
	public InstancedObjectDetails(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"PropertyEditor",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"ContentBrowser",
			"GameplayTags",
		});
	}
}