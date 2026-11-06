#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelWidgetBase.generated.h"


class UVoxelPrimaryGameLayout;

UCLASS(Abstract)
class VOXEL_API UVoxelWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient) UObject* DataContext;

	UFUNCTION(BlueprintPure, Category = "Voxel|UI")
	UVoxelPrimaryGameLayout* GetLayout() const;
};
