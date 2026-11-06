// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voxel/Editor/VoxelToolContext.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelItemWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"

UWorld* FVoxelToolContext::GetWorld() const
{
	return PlayerController ? PlayerController->GetWorld() : nullptr;
}

UVoxelBlockWorldSubsystem* FVoxelToolContext::GetBlockWorld() const
{
	return PlayerController ? PlayerController->GetBlockWorldSubsystem() : nullptr;
}

UVoxelItemWorldSubsystem* FVoxelToolContext::GetItemWorld() const
{
	UWorld* W = GetWorld();
	return W ? W->GetSubsystem<UVoxelItemWorldSubsystem>() : nullptr;
}

UVoxelAreaWorldSubsystem* FVoxelToolContext::GetAreaWorld() const
{
	return PlayerController ? PlayerController->GetAreaWorldSubsystem() : nullptr;
}

UConfigDataSubsystem* FVoxelToolContext::GetConfigData() const
{
	UWorld* W = GetWorld();
	return W ? W->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>() : nullptr;
}

UVoxelGameInstance* FVoxelToolContext::GetGameInstance() const
{
	UWorld* W = GetWorld();
	return W ? Cast<UVoxelGameInstance>(W->GetGameInstance()) : nullptr;
}

float FVoxelToolContext::GetBlockLength() const
{
	if (UVoxelGameInstance* GI = GetGameInstance())
	{
		return static_cast<float>(GI->WorldConfig.BlockLength);
	}
	return 100.0f;
}
