// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/UI/Item/ItemLibraryWidgetBase.h"

#include "Components/PanelWidget.h"
#include "Components/WrapBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "Core/UI/ItemLibrary/ItemLibraryTabButtonBase.h"
#include "Core/UI/ItemLibrary/ItemLibrarySlotBase.h"
#include "Voxel/VoxelPlayerController.h"

void UItemLibraryWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SearchTextBox)
	{
		SearchTextBox->OnTextChanged.AddDynamic(this, &UItemLibraryWidgetBase::HandleSearchChanged);
	}
	if (PrevPageButton)
	{
		PrevPageButton->OnClicked.AddDynamic(this, &UItemLibraryWidgetBase::HandlePrevPageClicked);
	}
	if (NextPageButton)
	{
		NextPageButton->OnClicked.AddDynamic(this, &UItemLibraryWidgetBase::HandleNextPageClicked);
	}
}

void UItemLibraryWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
}

void UItemLibraryWidgetBase::NativeDestruct()
{
	if (DataSource && bDataSourceChangedBound)
	{
		DataSource->OnDataSourceChanged.Remove(DataSourceChangedHandle);
		bDataSourceChangedBound = false;
	}
	Super::NativeDestruct();
}

void UItemLibraryWidgetBase::SetDataSource(UItemLibraryDataSource* InSource)
{
	if (DataSource && bDataSourceChangedBound)
	{
		DataSource->OnDataSourceChanged.Remove(DataSourceChangedHandle);
		bDataSourceChangedBound = false;
	}
	DataSource = InSource;
	if (DataSource)
	{
		DataSourceChangedHandle = DataSource->OnDataSourceChanged.AddUObject(this, &UItemLibraryWidgetBase::OnDataSourceChanged);
		bDataSourceChangedBound = true;
	}
}

void UItemLibraryWidgetBase::SetCategoryProvider(UItemLibraryCategoryProvider* InProvider)
{
	CategoryProvider = InProvider;
}

void UItemLibraryWidgetBase::InitializeLibrary()
{
	BindSubWidgetDelegates();
	ReloadFromDataSource();
}

void UItemLibraryWidgetBase::RefreshList()
{
	// ReloadFromDataSource 内部会 RebuildTabs → SwitchToCategory → ApplyFiltersAndRender → RenderCurrentPage，
	// 不要再额外调用 ApplyFiltersAndRender，否则会重复渲染当前页、重复 Add slot。
	ReloadFromDataSource();
}

void UItemLibraryWidgetBase::SwitchToCategory(const FString& CategoryId)
{
	SelectedCategoryId = CategoryId;
	for (UItemLibraryTabButtonBase* Btn : TabButtons)
	{
		if (Btn)
		{
			Btn->SetSelected(Btn->GetCategoryId() == CategoryId);
		}
	}
	CurrentPage = 0;
	ApplyFiltersAndRender();
}

void UItemLibraryWidgetBase::BindSubWidgetDelegates()
{
	if (SearchTextBox)
	{
		SearchTextBox->OnTextChanged.RemoveDynamic(this, &UItemLibraryWidgetBase::HandleSearchChanged);
	}
	if (PrevPageButton)
	{
		PrevPageButton->OnClicked.RemoveDynamic(this, &UItemLibraryWidgetBase::HandlePrevPageClicked);
	}
	if (NextPageButton)
	{
		NextPageButton->OnClicked.RemoveDynamic(this, &UItemLibraryWidgetBase::HandleNextPageClicked);
	}
}

void UItemLibraryWidgetBase::RebuildTabs()
{
	if (!TabHost || !TabButtonClass)
	{
		return;
	}

	for (UItemLibraryTabButtonBase* Btn : TabButtons)
	{
		if (Btn)
		{
			Btn->OnTabClicked.RemoveAll(this);
		}
	}
	TabButtons.Reset();
	TabHost->ClearChildren();

	// 库组件不应当自己切换类型：类型由外部工具通过 DataSource 的 QueryTag 决定。
	if (!bShowCategoryTabs)
	{
		// 无分类 Tab，SelectedCategoryId 置空使 ApplyFiltersAndRender 显示全部条目
		SelectedCategoryId.Empty();
		return;
	}

	for (const FItemLibraryCategory& Category : Categories)
	{
		UItemLibraryTabButtonBase* Btn = CreateWidget<UItemLibraryTabButtonBase>(this, TabButtonClass);
		if (!Btn)
		{
			continue;
		}
		Btn->SetCategory(Category);
		Btn->SetSelected(Category.bIsDefault);
		Btn->OnTabClicked.AddDynamic(this, &UItemLibraryWidgetBase::HandleTabButtonClicked);
		TabHost->AddChild(Btn);
		TabButtons.Add(Btn);
	}
}

void UItemLibraryWidgetBase::ReloadFromDataSource()
{
	AllEntries.Reset();
	Categories.Reset();

	if (DataSource)
	{
		AllEntries = DataSource->GetAllEntries();
	}
	if (CategoryProvider)
	{
		Categories = CategoryProvider->GenerateCategories(AllEntries);
	}

	UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] ReloadFromDataSource: DataSource=%s, CategoryProvider=%s, Entries=%d, Categories=%d, ShowTabs=%d"),
		DataSource ? *DataSource->GetClass()->GetName() : TEXT("NULL"),
		CategoryProvider ? *CategoryProvider->GetClass()->GetName() : TEXT("NULL"),
		AllEntries.Num(), Categories.Num(), bShowCategoryTabs ? 1 : 0);

	RebuildTabs();

	// 不显示分类 Tab 时直接渲染全部条目，不经过 SwitchToCategory 的分类选择逻辑
	if (!bShowCategoryTabs)
	{
		CurrentPage = 0;
		ApplyFiltersAndRender();
		return;
	}

	FString DefaultId;
	for (const FItemLibraryCategory& C : Categories)
	{
		if (C.bIsDefault)
		{
			DefaultId = C.CategoryId;
			break;
		}
	}
	if (DefaultId.IsEmpty() && Categories.Num() > 0)
	{
		DefaultId = Categories[0].CategoryId;
	}
	SwitchToCategory(DefaultId);
}

void UItemLibraryWidgetBase::ApplyFiltersAndRender()
{
	FilteredEntries.Reset();

	const FItemLibraryCategory* CurrentCategory = Categories.FindByPredicate(
		[this](const FItemLibraryCategory& C) { return C.CategoryId == SelectedCategoryId; });

	for (const FItemLibraryEntry& Entry : AllEntries)
	{
		if (CurrentCategory && CategoryProvider)
		{
			if (!CategoryProvider->BelongsToCategory(Entry, *CurrentCategory))
			{
				continue;
			}
		}
		if (DataSource && !DataSource->MatchesSearch(Entry, CurrentSearch))
		{
			continue;
		}
		FilteredEntries.Add(Entry);
	}

	TotalPages = GetTotalPages(FilteredEntries.Num());
	if (CurrentPage >= TotalPages)
	{
		CurrentPage = FMath::Max(0, TotalPages - 1);
	}
	RenderCurrentPage();
}

void UItemLibraryWidgetBase::RenderCurrentPage()
{
	UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] RenderCurrentPage: ListHost=%s, SlotClass=%s, Filtered=%d, Page=%d/%d, ItemsPerPage=%d"),
		ListHost ? TEXT("VALID") : TEXT("NULL"),
		SlotClass ? *SlotClass->GetName() : TEXT("NULL"),
		FilteredEntries.Num(), CurrentPage, TotalPages, ItemsPerPage);

	if (!ListHost || !SlotClass)
	{
		return;
	}
	ListHost->ClearChildren();

	// 设置 WrapBox 的行宽阈值（与旧 VoxelItemListPanelWidget 保持一致的行为）
	if (WrapSize > 0.0f)
	{
		ListHost->SetWrapSize(WrapSize);
	}

	const int32 Start = CurrentPage * ItemsPerPage;
	const int32 End = FMath::Min(Start + ItemsPerPage, FilteredEntries.Num());
	for (int32 i = Start; i < End; ++i)
	{
		const FItemLibraryEntry& Entry = FilteredEntries[i];
		UItemLibrarySlotBase* SlotWidget = CreateWidget<UItemLibrarySlotBase>(this, SlotClass);
		if (!SlotWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] CreateWidget failed: SlotClass=%s HandleId=%d Name=%s"),
				SlotClass ? *SlotClass->GetName() : TEXT("NULL"), Entry.HandleId, *Entry.DisplayName.ToString());
			continue;
		}
		UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] Add slot: HandleId=%d Name=%s Tag=%s"),
			Entry.HandleId, *Entry.DisplayName.ToString(), *Entry.EntryTag.ToString());
		SlotWidget->InitFromEntry(Entry);
		SlotWidget->OnSlotClicked.AddDynamic(this, &UItemLibraryWidgetBase::HandleSlotClicked);
		ListHost->AddChildToWrapBox(SlotWidget);
	}

	if (PageNumberText)
	{
		const int32 Display = FMath::Max(0, CurrentPage) + 1;
		const int32 Total = FMath::Max(1, TotalPages);
		PageNumberText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Display, Total)));
	}
	if (PrevPageButton)
	{
		PrevPageButton->SetIsEnabled(CurrentPage > 0);
	}
	if (NextPageButton)
	{
		NextPageButton->SetIsEnabled(CurrentPage < TotalPages - 1);
	}
}

int32 UItemLibraryWidgetBase::GetTotalPages(int32 Count) const
{
	if (ItemsPerPage <= 0 || Count <= 0)
	{
		return 1;
	}
	return FMath::Max(1, FMath::DivideAndRoundUp(Count, ItemsPerPage));
}

void UItemLibraryWidgetBase::HandleSearchChanged(const FText& Text)
{
	CurrentSearch = Text.ToString();
	CurrentPage = 0;
	ApplyFiltersAndRender();
}

void UItemLibraryWidgetBase::HandlePrevPageClicked()
{
	const int32 NewPage = FMath::Clamp(CurrentPage - 1, 0, FMath::Max(0, TotalPages - 1));
	if (NewPage != CurrentPage)
	{
		CurrentPage = NewPage;
		RenderCurrentPage();
	}
}

void UItemLibraryWidgetBase::HandleNextPageClicked()
{
	const int32 NewPage = FMath::Clamp(CurrentPage + 1, 0, FMath::Max(0, TotalPages - 1));
	if (NewPage != CurrentPage)
	{
		CurrentPage = NewPage;
		RenderCurrentPage();
	}
}

void UItemLibraryWidgetBase::HandleTabButtonClicked(const FString& CategoryId)
{
	SwitchToCategory(CategoryId);
}

void UItemLibraryWidgetBase::HandleSlotClicked(const FItemLibraryEntry& Entry)
{
	UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] HandleSlotClicked: HandleId=%d, bActivate=%d, DataSource=%s"),
		Entry.HandleId, bActivateDataSourceOnClick ? 1 : 0, DataSource ? TEXT("VALID") : TEXT("NULL"));

	if (bActivateDataSourceOnClick && DataSource)
	{
		AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer());
		DataSource->OnEntryActivated(Entry, PC);
	}
	OnEntryClicked.Broadcast(Entry);
	UE_LOG(LogTemp, Warning, TEXT("[ItemLibrary] OnEntryClicked broadcast done"));
}

void UItemLibraryWidgetBase::OnDataSourceChanged()
{
	ReloadFromDataSource();
}
