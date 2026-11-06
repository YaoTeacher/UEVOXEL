// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelConfigStruct.h"
#include "VoxelSceneSaveGame.h"
#include "Data/Scene/SceneData.h"
#include "Engine/GameInstance.h"
#include "VoxelGameInstance.generated.h"

class UConfigDataSubsystem;
class UDataRegistry;
/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class VOXEL_API UVoxelGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(visibleAnywhere, Blueprintable,category="SaveGame/Save")
	FSaveBaseData CurSaveData;
	UPROPERTY(EditAnywhere, Blueprintable,category="SaveGame/Scene")
	FSceneData SceneData;

	UPROPERTY(EditDefaultsOnly, Category = "System Preload")
	TSubclassOf<class UVoxelBlockWorldSubsystem> VoxelBlockSubsystemClass;
	UPROPERTY(EditDefaultsOnly, Category = "System Preload")
	TSubclassOf<class UVoxelAreaWorldSubsystem> VoxelAreaSubsystemClass;
	UPROPERTY(EditDefaultsOnly, Category = "System Preload")
	TSubclassOf<class UVoxelCreatureWorldSubsystem> VoxelCreatureSubsystemClass;
	UPROPERTY(EditDefaultsOnly, Category = "System Preload")
	TSubclassOf<class UConfigDataSubsystem> VoxelConfigDataSubsystemClass;

	UFUNCTION(BlueprintCallable, Category = "Scene|Save")
	void SaveCurScene();
	UFUNCTION(BlueprintCallable, Category = "Scene|Load")
	void LoadSaveData(FString SaveSlotName);
	UFUNCTION(BlueprintCallable, Category = "Scene|Load")
	void OnWorldGenerationComplete();
	// UFUNCTION(Blueprintable, Category = "Scene|Load")
	// void GenerateScene();
	UFUNCTION(BlueprintCallable, Category = "Scene|Load")
	void LoadSceneData(int InputSceneId);
	UFUNCTION(BlueprintCallable, Category = "Scene|Save")
	void ChangeScene(int InputSceneId);
	// 初始化游戏数据
	virtual void Init() override;
	
	virtual void Shutdown() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldSetting")
	FWorldConfig WorldConfig;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldSetting")
	int CurPlayerId = 0; // 角色系统未上线：测试用 0（系统角色身份）；上线后单机固定 1（联机 1-99）

	/** 玩家主角的业务 CreatureId。CreatureId 分区约定：
	 *    负值      = 未指派（IsNullOwner 判 -1）
	 *    0         = 系统角色（系统区域/地图归属，玩家无权编辑）
	 *    1-99      = 玩家（单机固定 1；联机时主角 ID = 玩家 ID，无需映射层）
	 *    100-9999  = 固定角色（剧情/系统预设，待角色系统）
	 *    10000-99999 = 随机角色（CreatureGeneratorUtils 现行生成区间）
	 *  玩家与主角不拆分两个 ID（单主角设计）；若未来支持多主角再引入映射。
	 *  当前阶段（角色系统未上线）测试主角用 0，即以系统角色身份编辑。 */
	UFUNCTION(BlueprintPure, Category = "WorldSetting")
	int32 GetMainCharacterId() const { return CurPlayerId; }

	/** 上者的 WorldContext 静态便捷版：取不到 GameInstance 时返回 -1（未指派语义 = 无归属，任何权限判定都失败）。
	 *  UI 各处 CharId 取值样板（GI 判空 + 三元）统一收敛到此入口 */
	UFUNCTION(BlueprintPure, Category = "WorldSetting", meta = (WorldContext = "WorldContextObject"))
	static int32 GetMainCharacterIdSafe(const UObject* WorldContextObject);

	/** 场景类型配置表（按 EScenceTypeEnum 查维度/种子策略）。可在 BP_GameInstance 中指定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene|Config")
	TObjectPtr<UDataTable> WorldSceneDataTable = nullptr;

	/** 按场景类型查配置；表未指定或无匹配行时返回 nullptr（调用方用默认值兜底）。 */
	const FSceneConfig* FindSceneConfig(EScenceTypeEnum SceneType) const;

	/** 统一的 BlockLength 获取，避免各处硬编码 100/20 兜底。 */
	float GetBlockLength() const { return static_cast<float>(WorldConfig.BlockLength); }


	UFUNCTION()
	FSceneBaseData GetCurScene()
	{
		return SceneData.BaseData;
	}

	// ===== 模板编辑场景切换 =====
	
	/** 正在编辑的模板 ID（-2=不在模板编辑模式，-1=新建，>=0=编辑已有） */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Template|Edit")
	int32 PendingEditHandleId = -2;

	/** 暂存的主 Level 名称 */
	UPROPERTY(Transient)
	FName StashedMainLevelName;

	/** 进入模板编辑模式 */
	UFUNCTION(BlueprintCallable, Category = "Template|Edit")
	void EnterTemplateEditMode(int32 HandleId);

	/** 退出模板编辑模式 */
	UFUNCTION(BlueprintCallable, Category = "Template|Edit")
	void ExitTemplateEditMode(bool bSave);

	/** 获取是否在模板编辑模式 */
	UFUNCTION(BlueprintPure, Category = "Template|Edit")
	bool IsInTemplateEditMode() const { return PendingEditHandleId >= -1 && GetWorld() && GetWorld()->GetName().Contains(TEXT("TemplateEdit")); }

private:
	// 新游戏（无存档场景）生成完成后需把 Pawn 贴地；读档则用存档位置。由 LoadSceneData 设置。
	bool bPendingNewScenePawnSnap = false;
};
