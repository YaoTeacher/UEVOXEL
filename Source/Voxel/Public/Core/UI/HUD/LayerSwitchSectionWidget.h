#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "GameplayTagContainer.h"
#include "LayerSwitchSectionWidget.generated.h"

class UHorizontalBox;
class UVerticalBox;
class UButton;
class UToolIconButtonWidget;
class UVoxelPlayerStateCacheComponent;

/**
 * 渲染图层切换区（TopLeftBar 子组件）
 *
 * 交互（混合式）：
 * - 图标列常驻：点图标 = 开关图层（多选，激活高亮）
 * - 激活图层自动在下方展开详情区（可多个叠加）
 * - 整块可折叠为单个"▣ 渲染图层"按钮
 *
 * 注册机制（GameplayTag 驱动，与工具列同构）：
 * - InitializeLayers(RootTag) 遍历 Tag 子级动态创建 UToolIconButtonWidget
 * - 图标/命名来自 ConfigDataSubsystem（DT_TagUIDefConfig）
 * - LayerDetailClasses: Tag -> 详情 Widget 类（蓝图配置）
 * - 激活状态存 SC（ActiveLayers + OnLayersChanged）
 */
UCLASS(Abstract)
class VOXEL_API ULayerSwitchSectionWidget : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	/** 初始化图层（遍历 RootTag 的直接子标签创建图标按钮） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|Layer")
	void InitializeLayers(FGameplayTag RootTag);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// === BindWidget 子控件 ===

	/** 图标行（图标即开关） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UHorizontalBox* LayerIconRow;

	/** 激活详情容器（垂直堆叠展开的详情面板） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UVerticalBox* DetailContainer;

	/** 折叠按钮（收起整个区为单按钮） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* CollapseButton;

	/** 折叠态单按钮容器（WBP 内与 LayerIconRow 互斥显示） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* FoldedButton;

	/** 图标按钮类（复用工具按钮组件） */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Layer|Config")
	TSubclassOf<UToolIconButtonWidget> IconButtonClass;

	/** 图层 Tag -> 详情 Widget 类（蓝图配置，为热力图/风向图预留） */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Layer|Config")
	TMap<FGameplayTag, TSubclassOf<UUserWidget>> LayerDetailClasses;

private:
	// === 事件回调 ===

	UFUNCTION()
	void OnLayerIconClicked(UToolIconButtonWidget* Button);

	UFUNCTION()
	void OnCollapseClicked();

	UFUNCTION()
	void OnFoldedClicked();

	UFUNCTION()
	void OnLayersChanged();

	// === 内部 ===

	/** 获取 SC（缓存） */
	UVoxelPlayerStateCacheComponent* GetStateComponent() const;

	/** 获取 InteractionComp（业务写入统一走此入口） */
	class UVoxelInteractionComponent* GetToolComponent() const;

	/** 刷新所有图标的激活态与详情区 */
	void RefreshSection();

	/** 重建详情区（当前激活的图层按序实例化详情类） */
	void RebuildDetails();

	UPROPERTY()
	TObjectPtr<UVoxelPlayerStateCacheComponent> CachedSC;

	/** 根标签（InitializeLayers 传入） */
	FGameplayTag RootTag;

	/** Tag -> 图标按钮映射 */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UToolIconButtonWidget>> LayerButtons;

	/** 是否折叠 */
	bool bCollapsed = false;
};
