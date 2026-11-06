#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "Core/UI/ItemLibrary/ItemLibraryEntry.h"
#include "M15_PositionAndPlacementModule.generated.h"

struct FVoxelGeometryDesc;

class UEditableTextBox;
class UTextBlock;
class UButton;
class UPanelWidget;
class UMaterialRemapRow;
class UItemLibraryWidgetBase;
class UConfigDataSubsystem;

/**
 * M15: 位置和放置控制模块
 *
 * 数据流（遵循框架）：
 * - 交互层：位置输入/旋转按钮 → 从SC读Geometry → 改字段 → SC->SetTmpGeometry（触发广播）
 * - 显示层：UpdateFromToolState(SC) → 从SC读Geometry → 更新所有UI控件
 *
 * 显示条件: Tool.Edit.Template (模板放置), Tool.Edit.Furniture (家具放置)
 */
UCLASS()
class VOXEL_API UM15_PositionAndPlacementModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaceRequested);

	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

	/** 放置请求委托 */
	UPROPERTY(BlueprintAssignable, Category = "M15|Events")
	FOnPlaceRequested OnPlaceRequested;

	/** 方向索引转 Yaw 角度 */
	UFUNCTION(BlueprintPure, Category = "M15|Direction")
	static float DirectionIndexToYaw(int32 Index);

	/** Yaw 角度转方向索引 */
	UFUNCTION(BlueprintPure, Category = "M15|Direction")
	static int32 YawToDirectionIndex(float Yaw);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
	virtual void NativeDestruct() override;

	/** 响应状态广播：位置/旋转变化时实时刷新 UI（BASE 跟随鼠标、EDIT Gizmo 拖动） */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

	/** 从 SC 的 TmpGeometry 读取位置/旋转并刷新所有 UI 控件 */
	void RefreshFromGeometry();

	/** 重建素材替换列表行（根据当前手持模板 + 替换表） */
	void RebuildMaterialList();

	/** 行点击回调：弹出同类选择器 */
	UFUNCTION()
	void HandleRowPickRequested(FString OriginFullId);

	/** 选择器条目点击回调：写入替换表 */
	UFUNCTION()
	void HandlePickerEntryClicked(const FItemLibraryEntry& Entry);

	// === UI 控件绑定 ===

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PositionXInput;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PositionYInput;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PositionZInput;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DirectionText;

	UPROPERTY(meta = (BindWidget))
	UButton* TurnLeftButton;

	UPROPERTY(meta = (BindWidget))
	UButton* TurnRightButton;

	UPROPERTY(meta = (BindWidget))
	UButton* PlaceButton;

	// === 素材替换列表（模板放置时按需使用） ===

	/** 素材替换行的容器（VerticalBox / ScrollBox 等），运行时动态填充行 */
	UPROPERTY(meta = (BindWidget))
	UPanelWidget* MaterialListHost;

	/** 素材替换选择器（复用 ItemLibraryWidgetBase），点击行时按同类 Tag 过滤显示 */
	UPROPERTY(meta = (BindWidget))
	UItemLibraryWidgetBase* MaterialPicker;

	/** 行控件类（蓝图配置为 WBP_MaterialRemapRow） */
	UPROPERTY(EditAnywhere, Category = "M15|MaterialRemap")
	TSubclassOf<UMaterialRemapRow> RowClass;

	// === 事件处理（交互层：只改SC，不改UI） ===

	UFUNCTION()
	void OnPositionXTextChanged(const FText& Text);

	UFUNCTION()
	void OnPositionYTextChanged(const FText& Text);

	UFUNCTION()
	void OnPositionZTextChanged(const FText& Text);

	UFUNCTION()
	void OnTurnLeftClicked();

	UFUNCTION()
	void OnTurnRightClicked();

	UFUNCTION()
	void OnPlaceClicked();

private:
	/** 委托绑定标记，避免重复绑定 */
	bool bDelegatesBound = false;

	/** 选择器委托绑定标记 */
	bool bPickerBound = false;

	/** 当前正在替换的原素材 FullId（选择器打开期间记录） */
	FString PendingRemapOriginFullId;

	/** 上次构建列表时的模板 Id（避免每帧重建） */
	int32 LastBuiltHandleId = -1;

	/** 上次构建列表时的替换表条目数（替换变化时重建） */
	int32 LastBuiltRemapCount = -1;

	/** 方向名称数组 */
	static const TArray<FString> DirectionNames;

	/** 交互层：从SC读位置→改→写回SC */
	void ApplyPositionDelta(int32 Axis, int32 NewValue);

	/** 交互层：旋转→写回SC */
	void ApplyRotation(int32 DirectionIndex);

	/** 交互层统一出口：经 InteractionComp 写回 StateComponent（触发广播），UI 不直改 SC */
	void ApplyGeometryToState(const FVoxelGeometryDesc& Geo);

	/** 执行放置操作 */
	void ExecutePlace();
};
