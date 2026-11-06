#pragma once

#include "CoreMinimal.h"
#include "Data/VoxelCutoutData.h"
#include "CutoutBoxListItemObject.generated.h"

/** ListView 条目数据包装（UObject，供 UListView 使用） */
UCLASS(BlueprintType)
class UCutoutBoxListItemObject : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = "Cutout")
	FCutoutBoxData Data;
};
