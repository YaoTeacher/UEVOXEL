#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "CutoutBoxListEntry.generated.h"

class UCheckBox;
class UTextBlock;
class UButton;

/** 透明框列表行控件（UListView 条目） */
UCLASS(Abstract)
class UCutoutBoxListEntry : public UCommonUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UCheckBox* EnabledCheckBox;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* LabelText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* SizeText;

	/** 位置信息文本（MinIndex 坐标，INDEX 空间） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UTextBlock* PositionText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* LocateButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	UButton* DeleteButton;

private:
	UPROPERTY()
	UObject* ListItem;

	UFUNCTION()
	void OnEnabledCheckStateChanged(bool bChecked);

	UFUNCTION()
	void OnDeleteClicked();

	UFUNCTION()
	void OnLocateClicked();
};
