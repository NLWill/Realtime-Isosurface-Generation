// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TerrainManipulation : ModuleRules
{
	public TerrainManipulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "GeometryFramework", "GeometryCore", "DynamicMesh", "CallableComputeShaders", "SimpleComputeShaders" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CallableComputeShaders", "SimpleComputeShaders" });
	}
}
