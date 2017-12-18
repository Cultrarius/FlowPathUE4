// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FlowPathPlugin : ModuleRules
{
	public FlowPathPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new string[] {"FlowPathPlugin/Public"});
		PrivateIncludePaths.AddRange(new string[] {"FlowPathPlugin/Private"});
			
		PublicDependencyModuleNames.AddRange(new string[] {"Core"});
        PrivateDependencyModuleNames.AddRange(new string[] {"CoreUObject", "Engine"});
	}
}
