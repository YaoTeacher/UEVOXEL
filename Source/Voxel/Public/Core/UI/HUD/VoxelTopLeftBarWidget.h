#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "VoxelTopLeftBarWidget.generated.h"

class UVoxelAreaNavigatorWidget;
class UVoxelAreaItemWidget;
class UVoxelPlayerStateCacheComponent;

/**
 * 左上区域导航栏（常驻 HUD）
 *
 * 环境域控件（高度滑杆/渲染切面/图层切换）已迁至 UVoxelEnvPanelWidget（右上环境卡）。
 * 本控件只保留区域面包屑导航聚合。
 *
 * 数据流：MainPanel 在区域变化时调用 RefreshNavigator（单向，无 Tick）
 */
UCLASS(Abstract)
class VOXEL_API UVoxelTopLeftBarWidget : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	UVoxelTopLeftBarWidget();

	/** 透传面包屑样式类到内部 AreaNavigator（MainPanel 调用） */
	UFUNCTION(BlueprintCallable, Category = "Voxel|UI|TopLeftBar")
	void SetBreadcrumbItemClass(TSubclassOf<UVoxelAreaItemWidget> InItemClass);

	/** 刷新内部导航器（MainPanel 调用） */
	void RefreshNavigator(UVoxelPlayerStateCacheComponent* SC);

protected:
	/** 区域面包屑导航 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UVoxelAreaNavigatorWidget* AreaNavigator;
};
