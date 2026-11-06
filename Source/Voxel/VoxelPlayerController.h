// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VoxelCharacter.h"
#include "Voxel/VoxelGameMode.h"
#include "Voxel/Private/Data/Area/VoxelAreaBase.h"
#include "Core/VoxelControllerPawn.h"
#include "VoxelPlayerController.generated.h"

class UVoxelItemWorldSubsystem;
class UVoxelAreaWorldSubsystem;
class UVoxelPreviewComponent;
class UVoxelInteractionComponent;
class UVoxelPrimaryGameLayout;
class UVoxelControllerHistoryComponent;
class UVoxelPlayerStateCacheComponent;
class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class AVoxelUIBlockActor;
class AVoxel3DUIActor;
class UVoxelGameInstance;
class UVoxelScreenBase;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * Voxel 玩家控制器
 *
 * 职责：
 * - 协调输入检测、业务处理和渲染更新
 * - 管理多个 ControllerComponent（State, Interact, VisualUI, History）
 * - 处理相机控制
 * - 管理子系统引用
 *
 * 架构：
 * - 蓝图检测输入 → InteractComponent 处理业务逻辑
 * - StateComponent 存储状态数据
 * - VisualUIComponent 处理渲染
 * - HistoryComponent 管理撤销/重做
 */
UCLASS(BlueprintType)
class VOXEL_API AVoxelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
    AVoxelPlayerController();

    // [诊断] 鼠标按键追踪
    void Diagnostic_OnLeftMousePressed();
    void Diagnostic_OnLeftMouseReleased();

    // ==================== 输入配置 ====================

	/** 玩家 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Input")
	int PlayerId;

	/** 默认输入映射上下文 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel|Input", meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	// ==================== 组件引用 ====================

	/** 交互逻辑组件 - 交互流程状态唯一持有者 + 业务门面（模式/工具 Tag/建造阶段/输入分发/工具路由） */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Voxel|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVoxelInteractionComponent> InteractionComp;

	/** 玩家状态缓存组件 - 「我选了什么」唯一数据源（实体选中 + 体素选区/剪切板 + 后续手持物/编辑参数） */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Voxel|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVoxelPlayerStateCacheComponent> StateCacheComp;

	/** 视觉UI组件 - 管理渲染和UI元素 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Voxel|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVoxelPreviewComponent> PreviewComp;

	/** 历史组件 - 管理撤销/重做栈 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Voxel|Components", meta = (PrivateAccess = "true"))
	TObjectPtr<UVoxelControllerHistoryComponent> HistoryComp;

	// ==================== 相机控制 ====================

	/** 当前控制的角色 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Voxel|Camera")
	AVoxelCharacter* CurControlledCharacter;

	/** 移动相机（WASD 或 方向键） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraMove(FVector2D Value);

	/** 旋转相机（左右旋转） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraYawRotate(float RotationAmount);

	/** 升降相机（上下移动） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraUp(float UpAmount);

	/** 俯仰相机（鼠标滚轮） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraPitchRotate(float WheelDelta);

	/** 调整相机距离（鼠标滚轮） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraDistance(float WheelDelta);

	/** 鼠标中键拖拽旋转相机（Yaw + Pitch） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void CameraOrbit(float YawDelta, float PitchDelta);

	/** 显示地面旋转环（IA_CameraOrbit Started 调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void ShowRotationRings();

	/** 隐藏地面旋转环（IA_CameraOrbit Completed 调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Camera")
	void HideRotationRings();

	// ==================== 子系统访问 ====================

	/** 获取游戏模式 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Subsystem")
	AVoxelGameMode* GetGameMode();

	/** 获取游戏实例 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Subsystem")
	UVoxelGameInstance* GetInstance();

	/** 获取游戏状态 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Subsystem")
	AVoxelGameState* GetSateReference();

	/** 获取区域世界子系统 */
	UVoxelAreaWorldSubsystem* GetAreaWorldSubsystem() { return AreaWorldSubsystem; }

	/** 获取方块世界子系统 */
	UVoxelBlockWorldSubsystem* GetBlockWorldSubsystem() { return BlockWorldSubsystem; }

	// ==================== HUD ====================

	/** PrimaryGameLayout 引用（替代旧 AVoxelHUD） */
	UPROPERTY(Transient)
	TObjectPtr<UVoxelPrimaryGameLayout> PrimaryLayout;

	/** PrimaryGameLayout 蓝图类（WBP_PrimaryGameLayout，配置各 Widget 类） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UVoxelPrimaryGameLayout> PrimaryLayoutClass;

	/** 暂停菜单面板类（ESC 无面板时 push） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voxel|UI", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UVoxelScreenBase> PauseMenuWidgetClass;

	/** ESC 处理：有面板则让其处理，无面板则打开暂停菜单 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI")
	void OnEscapePressed();

protected:
	/** 设置输入组件 */
	virtual void SetupInputComponent() override;

	/** 游戏开始时初始化 */
	virtual void BeginPlay() override;

	// ==================== 内部辅助方法 ====================


private:
	// ==================== 子系统缓存 ====================

	/** 游戏实例 */
	UPROPERTY()
	UVoxelGameInstance* GameInstance;

	/** 游戏模式 */
	UPROPERTY()
	AVoxelGameMode* GameMode;

	/** 游戏状态 */
	UPROPERTY()
	AVoxelGameState* GameState;

	/** 方块世界子系统 */
	UPROPERTY()
	UVoxelBlockWorldSubsystem* BlockWorldSubsystem;

	/** 物品世界子系统 */
	UPROPERTY()
	UVoxelItemWorldSubsystem* ItemWorldSubsystem;

	/** 区域世界子系统 */
	UPROPERTY()
	UVoxelAreaWorldSubsystem* AreaWorldSubsystem;

	// ==================== 控制对象缓存 ====================

	/** 控制的 Pawn */
	UPROPERTY()
	AVoxelControllerPawn* ControlledPawn;

	/** 控制的角色 */
	UPROPERTY()
	AVoxelCharacter* ControlledCharacter;

	// ==================== 输入状态缓存 ====================

	/** ESC 防抖时间戳（避免 C++ BindKey 与蓝图 IA_Escape 双触发） */
	double LastEscapePressedTime = -1.0;
};
