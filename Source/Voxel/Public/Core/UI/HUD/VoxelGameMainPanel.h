#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Core/Enum/GameStateEnum.h"
#include "VoxelGameMainPanel.generated.h"

class UInteractionWidget;
class UBuildInteractionWidget;
class UVoxelAreaNavigatorWidget;
class UVoxelTopLeftBarWidget;
class UCutoutBoxListPanel;
class UVoxelAreaItemWidget;
class UVoxelPlayerStateCacheComponent;
class UVoxelInteractionComponent;
class UCommonActivatableWidget;
class UButton;
class UTextBlock;
class AVoxelAreaBaseInstance;
class UVoxelEnvPanelWidget;
class UVoxelEntityDetailPanel;
class UCanvasPanel;

UCLASS()
class VOXEL_API UVoxelGameMainPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void ShowInteraction() const;
	void HideInteraction() const;
	void ShowBuildInteraction() const;
	void HideBuildInteraction() const;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UInteractionWidget> InteractionWidget;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UBuildInteractionWidget> BuildInteractionWidget;

	/** 透明框常驻列表面板（跨工具可达） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UCutoutBoxListPanel> CutoutBoxListPanel;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelAreaNavigatorWidget> AreaNavigator;

	/** 统一左上管理栏（面包屑/高度/图层区聚合） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVoxelTopLeftBarWidget> TopLeftBar;

	/** 根画布（EnvPanel 锚点放置用，蓝图内控件名 CanvasPanel_0） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UCanvasPanel> CanvasPanel_0;

	/** 环境域面板类（蓝图配置，右上角常驻） */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelEnvPanelWidget> EnvPanelClass;

	/** 已生成的环境域面板实例 */
	UPROPERTY(Transient)
	TObjectPtr<UVoxelEnvPanelWidget> SpawnedEnvPanel;

	/** 实体详情面板类（蓝图配置，右列环境组下方；区域/生物/家居共槽互斥） */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelEntityDetailPanel> EntityDetailPanelClass;

	/** 已生成的实体详情面板实例 */
	UPROPERTY(Transient)
	TObjectPtr<UVoxelEntityDetailPanel> SpawnedEntityDetailPanel;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> ModeToggleButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> ModeToggleText;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UCommonActivatableWidget> BuildPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelAreaItemWidget> BreadcrumbItemClass;

	UFUNCTION()
	void OnModeToggleClicked();

	UFUNCTION()
	void HandleControllerModeChanged(UVoxelInteractionComponent* Interaction);

	/** 区域切换时刷新导航（替代原先 8 事件全量刷新） */
	UFUNCTION()
	void HandleCurrentAreaChanged(AVoxelAreaBaseInstance* OldArea, AVoxelAreaBaseInstance* NewArea);

	void RefreshModeDisplay();
	void RefreshNavigator(UVoxelPlayerStateCacheComponent* SC);
	void BindDelegate();
	void UnbindDelegate();
	void ApplyModeToScreenPanel(EVoxelControllerMode Mode);

	/** NativeConstruct 已执行标记：区分构造期 Layout 未就绪（预期）与构造后失败（真实故障） */
	bool bConstructed = false;
};
