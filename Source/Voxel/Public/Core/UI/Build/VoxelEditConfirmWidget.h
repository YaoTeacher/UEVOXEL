#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelEditConfirmWidget.generated.h"

class UButton;
class UVoxelPlayerStateCacheComponent;
class UVoxelInteractionComponent;

/**
 * 编辑确认/取消场景弹窗（非模态，屏幕空间，每帧跟随世界锚点）
 *
 * 用途：建造模式进入待确认编辑（BuildPhase == EDITING）时，
 * 在编辑区域（TmpGeometry 包围盒顶部中心）附近弹出确认/取消按钮
 * - 非模态、无遮罩、不暂停游戏、不进 CommonUI 栈
 * - 每帧将锚点世界坐标投影到屏幕并跟随（投影失败时隐藏，恢复后自动出现）
 * - 显隐完全由订阅 StateComponent 广播驱动（OnBuildPhaseChanged/OnGeometryChanged/OnToolChanged）
 * - 按钮只调业务门面：确认 = InteractionComp->RequestConfirmVoxelEdit()，
 *   取消 = InteractionComp->CancelCurrentEdit()（与右键取消同语义）
 *
 * 蓝图子类 BindWidget 控件（OptionalWidget）：
 * - ConfirmButton: 确认按钮
 * - CancelButton: 取消按钮
 *
 * 由 UVoxelPrimaryGameLayout 常驻创建（RootCanvas 子控件，默认 Collapsed）
 */
UCLASS()
class VOXEL_API UVoxelEditConfirmWidget : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	/** 将弹窗定位到当前编辑锚点附近（投影 + 偏移 + 视口钳制）；无锚点或投影失败时返回 false */
	bool UpdateAnchorPosition();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

	/** 按 BuildPhase 刷新可见性（EDITING = 显示，其余 = 隐藏）；广播回调入口 */
	UFUNCTION()
	void RefreshVisibility();

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnCancelClicked();

	/** 计算当前编辑锚点世界坐标（TmpGeometry 包围盒顶部中心）；无有效几何时返回 false */
	bool ComputeAnchorWorldLocation(FVector& OutLocation) const;

	/** 确认按钮（应用编辑） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> ConfirmButton;

	/** 取消按钮（完整取消，与右键同语义） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> CancelButton;

	/** 相对锚点投影位置的屏幕偏移（默认置于锚点上方，避免遮挡编辑区域） */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Confirm")
	FVector2D TargetOffset = FVector2D(16.0f, -70.0f);

	/** 缓存的 StateComponent（NativeConstruct 解析并绑定事件） */
	UPROPERTY(Transient)
	TObjectPtr<UVoxelPlayerStateCacheComponent> StateComponent;

	/** 缓存的 InteractionComponent（OnToolChanged/OnBuildPhaseChanged 持有者） */
	UPROPERTY(Transient)
	TObjectPtr<UVoxelInteractionComponent> InteractionComponent;
};
