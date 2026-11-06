#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "VoxelGameUserSettings.generated.h"

class UVoxelGameInstance;

/**
 * 项目设置单例，基于引擎 UGameUserSettings。
 * - 继承白送：分辨率 / 全屏模式 / Scalability 画质档位（自动持久化到 GameUserSettings.ini）
 * - 新增：可见范围 LoadDistance（玩家性能开关，Config 持久化）
 * 权威分离：BP CDO 的 WorldConfig.LoadDistance = 设计默认兜底；
 *           ini 用户偏好 = ApplySettings 后覆盖运行时值，永远赢。
 */
UCLASS()
class VOXEL_API UVoxelGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/** 可见范围（chunk 为单位），玩家性能开关。范围 1~8。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Voxel|Settings", meta = (ClampMin = 1, ClampMax = 8))
	int32 LoadDistance = 3;

	/** 获取本项目的设置单例 */
	UFUNCTION(BlueprintPure, Category = "Voxel|Settings")
	static UVoxelGameUserSettings* Get() { return Cast<UVoxelGameUserSettings>(UGameUserSettings::GetGameUserSettings()); }

	/**
	 * 设置画质预设（0=低 1=中 2=高 3=极致），统一设各 Scalability 级别（不立即生效）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Settings")
	void SetQualityPreset(int32 Level);

	/**
	 * 应用画质预设：设级别 + 生效 + 保存。
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Settings")
	void ApplyQualityPreset(int32 Level);

	/** 重写：先应用引擎画质，再把 LoadDistance 推入当前 GameInstance 的 WorldConfig（运行时 UI 改动即时生效） */
	virtual void ApplySettings(bool bCheckForCommandLineOverrides = false) override;

	/** 重写：钳制 LoadDistance 范围 */
	virtual void ValidateSettings() override;
};
