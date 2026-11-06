// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelGameState.h"

#include "GeometryTypes.h"
#include "Net/UnrealNetwork.h"
#include "Utils/ChunkUtilsFunction.h"

AVoxelGameState::AVoxelGameState()
{
    
}

// EVoxelControllerBuildState AVoxelGameState::GetBuildState()
// {
// 	return  BuildState;
// }
//
// EVoxelControllerMode AVoxelGameState::GetControllerMode()
// {
// 	return PlayerMode;
// }

// void AVoxelGameState::ChangeBuildState(EVoxelControllerBuildState Input)
// {
//     
//     if (PlayerMode==EVoxelControllerMode::SceneBuildMode)
//     {
//         if (Input==EVoxelControllerBuildState::OutBuildMode)
//         {
//             return;
//         }
//         BuildState = Input;
//         return;
//     }
//     
//     if (PlayerMode!=EVoxelControllerMode::SceneBuildMode)
//     {
//         BuildState = EVoxelControllerBuildState::OutBuildMode;
//     }
// }


// void AVoxelGameState::ChangeControllerMode(EVoxelControllerMode Input)
// {
//
//     
// }
//
// void AVoxelGameState::ChangeEditState(EVoxelControllerEditState Input)
// {
//     
// }

void AVoxelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AVoxelGameState, PlayerScore);
    // DOREPLIFETIME(AVoxelGameState, PlayerMode);
    // DOREPLIFETIME(AVoxelGameState, BuildState );
    DOREPLIFETIME(AVoxelGameState, PlayerGameState);
    DOREPLIFETIME(AVoxelGameState, CurrentMap);
}