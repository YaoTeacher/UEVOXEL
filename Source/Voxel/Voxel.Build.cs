// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Voxel : ModuleRules
{
	public Voxel(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayStateTreeModule" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "GameplayTags", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "ProceduralMeshComponent", "DataRegistry", "SlateCore", "Slate", "UMG", "CommonUI", "CommonInput", "DeveloperSettings" });

        // 编辑器专用 API（GEditor / SetActorLabel 等，已用 WITH_EDITOR 守卫）
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
