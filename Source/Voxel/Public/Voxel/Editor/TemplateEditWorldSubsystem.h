#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/Item/VoxelItemBase.h"
#include "TemplateEditWorldSubsystem.generated.h"

class UVoxelBlockWorldSubsystem;
class UVoxelItemWorldSubsystem;
class UConfigDataSubsystem;

/**
 * 模板编辑场景专用子系统
 * 只在 TemplateEditLevel 激活
 * 职责：工作区管理 + 模板注入 + 捕获
 */
UCLASS()
class VOXEL_API UTemplateEditWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** 把模板注入工作区 */
	void ApplyTemplateToWorkArea(int32 TemplateId);

	/** 从工作区捕获模板 */
	FVoxelTemplate CaptureFromWorkArea(const FTemplateMeta& Meta);

	/** 保存当前工作区为模板 */
	bool SaveCurrentWorkArea(const FTemplateMeta& Meta);

	/** 获取工作区范围 */
	FBox GetWorkAreaBox() const { return WorkAreaBox; }

protected:
	/** 工作区范围 */
	FBox WorkAreaBox;

	/** 工作区中心 */
	FIntVector WorkAreaCenter = FIntVector(0, 0, 0);

	/** 工作区大小 */
	int32 WorkAreaSize = 32;

	UPROPERTY()
	UVoxelBlockWorldSubsystem* BlockSys;

	UPROPERTY()
	UVoxelItemWorldSubsystem* ItemWorld;

	UPROPERTY()
	UConfigDataSubsystem* ConfigSys;

	/** 初始化工作区 */
	void SetupWorkArea();
};
