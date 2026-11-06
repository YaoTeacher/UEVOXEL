#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelInfoTooltip.h"
#include "VoxelFurnitureDetailDialog.generated.h"

class AUVoxelItemBaseInstance;

/**
 * 家具信息浮窗（L2.5 InfoTooltip 层）
 *
 * 职责：展示选中家具的实例信息（名称/描述/HandleId/朝向/占用格数/数量）
 * - 非模态、跟随鼠标位置、不暂停游戏
 * - 只展示信息，不提供操作按钮（旋转/删除通过 R/Delete 快捷键处理）
 *
 * 通过 AVoxelHUD::ShowInfoTooltip 打开，Context 传入 AUVoxelItemBaseInstance*
 *
 * UI 控件（蓝图子类 BindWidget 绑定，OptionalWidget = true 允许缺失）：
 * - TitleText: 家具名称
 * - ContentText: 多行信息（"标签: 值\n" 格式）
 */
UCLASS()
class VOXEL_API UVoxelFurnitureDetailDialog : public UVoxelInfoTooltip
{
	GENERATED_BODY()

public:
	// 浮窗打开时调用：从 Context 获取家具 Actor 并刷新显示
	virtual void OnTooltipShown(UObject* Context) override;

	// 浮窗关闭时清理
	virtual void OnTooltipHidden() override;

private:
	/** 刷新家具信息到 TitleText + ContentText */
	void RefreshFurnitureInfo();

	/** 缓存的家具 Actor */
	UPROPERTY(Transient)
	TWeakObjectPtr<AUVoxelItemBaseInstance> FurnitureActor;
};
