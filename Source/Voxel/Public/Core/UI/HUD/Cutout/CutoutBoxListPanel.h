#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "CutoutBoxListPanel.generated.h"

class UListView;
class UButton;
class UTextBlock;
class UVoxelPlayerStateCacheComponent;

/**
 * 透明框常驻列表面板（VoxelGameMainPanel 子控件，跨工具可达）
 * - ListView 列出所有框，逐行生效开关/定位/删除
 * - NewButton 切到 Tool.Cutout 新建框
 * - 订阅 CutoutSys->OnBoxListChanged 刷新
 */
UCLASS(Abstract)
class UCutoutBoxListPanel : public UVoxelWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UListView* BoxListView;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* AllShowButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* AllHideButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* NewButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* CountText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* EmptyHint;

private:
	UFUNCTION()
	void OnNewButtonClicked();

	UFUNCTION()
	void OnAllShowClicked();

	UFUNCTION()
	void OnAllHideClicked();

	UFUNCTION()
	void OnBoxListChanged();

	void RefreshList();

	UPROPERTY()
	UVoxelPlayerStateCacheComponent* CachedSC;
};
