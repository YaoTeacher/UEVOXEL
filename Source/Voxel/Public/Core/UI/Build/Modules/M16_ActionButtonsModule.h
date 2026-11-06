#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M16_ActionButtonsModule.generated.h"

UCLASS()
class VOXEL_API UM16_ActionButtonsModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override { return true; }
	virtual void UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC) override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnUndoClicked();

	UFUNCTION()
	void OnRedoClicked();

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<class UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<class UButton> ResetButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<class UButton> UndoButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<class UButton> RedoButton;
};
