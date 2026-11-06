#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M13a_AreaBasicInfoEditModule.generated.h"

class UButton;
class UVoxelAreaTreeWidget;

/**
 * M13a: 区域导航模块
 * 职责：纯导航——展示当前区域的子区域树 + 进入子区域层级
 *
 * 业务数据（父级/名称/所有人/经营人/类型/状态）不再在此编辑：
 * 区域确认生成后由实体详情面板负责修改。
 *
 * 显示条件: Tool.AreaEdit
 */
UCLASS()
class VOXEL_API UM13a_AreaBasicInfoEditModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	// === 模块接口 ===
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* StateComponent) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 响应 STATE 任意建造事件（含 OnCurrentAreaChanged），刷新导航树 */
	UFUNCTION()
	void HandleBuildModeParamsUpdated();

	/** 防重复绑定标志 */
	bool bDelegatesBound = false;

	// === UI 控件绑定 ===

	/** 子区域树容器（以当前区域为根展示子树） */
	UPROPERTY(meta = (BindWidget))
	UVoxelAreaTreeWidget* SubAreaTreeWidget;

	/** 进入当前选中区域按钮（将 CurArea 设为 CurParentArea） */
	UPROPERTY(meta = (BindWidget))
	UButton* EnterAreaButton;

	// === 事件处理 ===

	UFUNCTION()
	void OnEnterAreaClicked();

private:
	/** 更新子区域树（CurArea 未选中时隐藏树） */
	void UpdateSubAreaList();
};
