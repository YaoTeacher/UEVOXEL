// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/BuildSlotWidget.h"

#include "Components/Button.h"

void UBuildSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 绑定自检：BindWidget 失败必须显式报错，禁止静默失败（同 ToolIconButtonWidget 规范）
	UE_LOG(LogTemp, Log, TEXT("[BuildSlot] NativeOnInitialized %s：ClickBtn=%s IconImage=%s"),
		*GetName(), ClickBtn ? *ClickBtn->GetName() : TEXT("NULL"), IconImage ? *IconImage->GetName() : TEXT("NULL"));
	if (!ClickBtn)
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildSlot] %s ClickBtn 未绑定（蓝图缺少同名 Button 控件），点击回调不会触发"), *GetName());
		return;
	}

	ClickBtn->OnClicked.AddUniqueDynamic(this, &UBuildSlotWidget::OnClickButton);
	ClickBtn->OnPressed.AddUniqueDynamic(this, &UBuildSlotWidget::OnPressedDebug);
	ClickBtn->OnReleased.AddUniqueDynamic(this, &UBuildSlotWidget::OnReleasedDebug);
}

void UBuildSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UBuildSlotWidget::InitSlotData(FGameplayTag InTag, TSoftObjectPtr<UTexture2D> InIcon)
{
	FItemLibraryEntry Entry;
	Entry.EntryTag = InTag;
	Entry.Icon = InIcon.LoadSynchronous();
	InitFromEntry(Entry);
}

void UBuildSlotWidget::InitFromEntry_Implementation(const FItemLibraryEntry& Entry)
{
	Super::InitFromEntry_Implementation(Entry);
	ItemTag = Entry.EntryTag;

	if (IconImage)
	{
		if (UTexture2D* Tex = Entry.Icon)
		{
			IconImage->SetBrushFromTexture(Tex);
			IconImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UE_LOG(LogTemp, Log, TEXT("[BuildSlot] '%s': 图标已设置 Tex=%s Size=%dx%d"),
				*Entry.EntryTag.ToString(), *Tex->GetName(), Tex->GetSizeX(), Tex->GetSizeY());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildSlot] '%s': Entry.Icon is null，图标未显示"), *Entry.EntryTag.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[BuildSlot] '%s': IconImage 未绑定（BindWidget 失败），图标无法显示"), *Entry.EntryTag.ToString());
	}
}

void UBuildSlotWidget::OnClickButton()
{
	UE_LOG(LogTemp, Warning, TEXT("[BuildSlot] OnClickButton: HandleId=%d, Tag=%s"),
		CurrentEntry.HandleId, *CurrentEntry.EntryTag.ToString());
	OnSlotClicked.Broadcast(CurrentEntry);
}

void UBuildSlotWidget::OnPressedDebug()
{
	UE_LOG(LogTemp, Warning, TEXT("[BuildSlot] OnPressed: Tag=%s（Slate 已命中按钮）"),
		*CurrentEntry.EntryTag.ToString());
}

void UBuildSlotWidget::OnReleasedDebug()
{
	UE_LOG(LogTemp, Warning, TEXT("[BuildSlot] OnReleased: Tag=%s（释放已到达按钮）"),
		*CurrentEntry.EntryTag.ToString());
	// 修复：CaptureDuringMouseDown 模式下 OnClicked 可能不触发，在 OnReleased 中兜底广播
	OnClickButton();
}
