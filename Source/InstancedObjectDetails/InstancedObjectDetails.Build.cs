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
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",
			"PropertyEditor",
			"ContentBrowser",
		});
	}
}
