// Fill out your copyright notice in the Description page of Project Settings.


#include "Area/VoxelAreaFunctionLibrary.h"

#include "VoxelGameState.h"

TArray<FIntVector> UVoxelAreaFunctionLibrary::GetNeighbourGround(FIntVector CurGround)
{
	  TArray<FIntVector> neibor;

        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                if (x == 0 && y == 0)
                {
                    continue;
                }
                for (int z = -1; z <= 1; z++)
                {
                    FIntVector curCheck = FIntVector(x, y, z);
                    

                }
            }
        }

        return neibor;
}
