// Fill out your copyright notice in the Description page of Project Settings.

#include "Area/VoxelNavArea.h"

UVoxelNavArea::UVoxelNavArea(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DrawColor = FColor(17, 2, 0);	// brownish
	DefaultCost = 1.0f;

	bIsPrivate=false;
	AreaType =FGameplayTag::EmptyTag;
}