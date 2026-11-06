#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "VoxelPrimaryGameLayout.generated.h"

class UVoxelGameMainPanel;
class UBuildPanelWidget;
class UVoxelToastWidget;
class UVoxelDragProgressWidget;
class UVoxelInfoTooltip;
class UVoxelDialogBase;
class UVoxelScreenBase;
class UVoxelEditConfirmWidget;
class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UCommonActivatableWidgetQueue;
class UCanvasPanel;

UCLASS()
class VOXEL_API UVoxelPrimaryGameLayout : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

public:
	static UVoxelPrimaryGameLayout* GetFromPC(APlayerController* PC);

	UCommonActivatableWidget* PushGameMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass, UObject* Context = nullptr);
	UCommonActivatableWidget* PushModal(TSubclassOf<UCommonActivatableWidget> WidgetClass, UObject* Context = nullptr);
	void ShowToast(const FText& Message, float Duration = 3.0f);
	void ShowDragProgress();
	void HideDragProgress();
	void UpdateDragProgress(float Progress);
	void ShowInfoTooltip(TSubclassOf<UVoxelInfoTooltip> TooltipClass, UObject* Context = nullptr);
	void HideInfoTooltip();
	bool HasInfoTooltip() const { return ActiveTooltip != nullptr; }
	UObject* GetCurrentScreen() const;

	/**
	 * 关闭 GameMenu 栈上的当前激活面板。
	 * 无激活面板但栈内有内容时（激活流程中断/未完成）强制清空——兜底避免「退出建造」无响应。
	 */
	void CloseGameMenu();

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelGameMainPanel> MainPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UBuildPanelWidget> BuildPanelClass;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelToastWidget> ToastClass;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelDragProgressWidget> DragProgressClass;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelInfoTooltip> FurnitureTooltipClass;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelDialogBase> TemplateSaveDialogClass;

	/** 编辑确认/取消场景弹窗（常驻，显隐由控件自身订阅广播驱动） */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel|UI|Config")
	TSubclassOf<UVoxelEditConfirmWidget> EditConfirmWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuStack;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetStack> ModalStack;

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidgetQueue> ToastQueue;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> DragProgressWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveTooltip = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UVoxelEditConfirmWidget> EditConfirmWidget = nullptr;

	float CurrentDragProgress = 0.0f;
};
