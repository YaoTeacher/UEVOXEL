#pragma once

#include "CoreMinimal.h"
#include "VoxelUISubsystemTypes.generated.h"

// Toast 提示类型
UENUM(BlueprintType)
enum class EToastType : uint8
{
	Info        UMETA(DisplayName = "信息"),
	Warning     UMETA(DisplayName = "警告"),
	Error       UMETA(DisplayName = "错误"),
	Success     UMETA(DisplayName = "成功")
};
