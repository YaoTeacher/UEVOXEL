#include "Core/UI/HUD/VoxelEntityDetailPanel.h"
#include "Core/UI/HUD/VoxelAreaBusinessEditDialog.h"
#include "Core/UI/HUD/VoxelEntitySubAreaRowWidget.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Data/Area/AreaEnums.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameManager/WorldManager/VoxelCreatureWorldSubsystem.h"
#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/VoxelGameInstance.h"
#include "Voxel/VoxelPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UVoxelEntityDetailPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UVoxelEntityDetailPanel::OnCloseClicked);
	if (ParentAreaButton) ParentAreaButton->OnClicked.AddDynamic(this, &UVoxelEntityDetailPanel::OnParentAreaClicked);
	if (EditBusinessButton) EditBusinessButton->OnClicked.AddDynamic(this, &UVoxelEntityDetailPanel::OnEditBusinessClicked);
	if (DecorateButton) DecorateButton->OnClicked.AddDynamic(this, &UVoxelEntityDetailPanel::OnDecorateClicked);

	// 只订阅广播刷新，不持有选中状态
	if (UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp())
	{
		SelComp->OnSelectedEntityChanged.AddDynamic(this, &UVoxelEntityDetailPanel::HandleSelectedEntityChanged);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityDetail] EntitySelectionComponent 未找到（需挂在 PlayerController 上）"));
	}

	RefreshFromSelection();
}

void UVoxelEntityDetailPanel::NativeDestruct()
{
	if (UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp())
	{
		SelComp->OnSelectedEntityChanged.RemoveDynamic(this, &UVoxelEntityDetailPanel::HandleSelectedEntityChanged);
	}
	if (BoundArea)
	{
		BoundArea->OnAreaBaseInfoChanged.RemoveDynamic(this, &UVoxelEntityDetailPanel::HandleAreaBaseInfoChanged);
		BoundArea = nullptr;
	}
	Super::NativeDestruct();
}

void UVoxelEntityDetailPanel::NavigateToArea(int32 AreaId)
{
	// 导航 = 切换选中，走业务方法
	if (UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp())
	{
		SelComp->SelectAreaById(AreaId);
	}
}

void UVoxelEntityDetailPanel::HandleSelectedEntityChanged(EVoxelSelectedEntityType /*NewType*/, int32 /*NewEntityId*/)
{
	RefreshFromSelection();
}

void UVoxelEntityDetailPanel::OnCloseClicked()
{
	// 关闭详情 = 取消选中：清实体选中 + 清空建造态选中区域（均走业务方法）
	if (UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp())
	{
		SelComp->ClearSelectedEntity();
	}
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->DeselectArea();
		}
	}
}

void UVoxelEntityDetailPanel::OnParentAreaClicked()
{
	if (CurrentParentAreaId >= 0)
	{
		NavigateToArea(CurrentParentAreaId);
	}
}

void UVoxelEntityDetailPanel::OnEditBusinessClicked()
{
	UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp();
	AVoxelAreaBaseInstance* Area = SelComp ? SelComp->GetSelectedArea() : nullptr;
	if (!Area) return;

	// 打开区域业务数据编辑弹窗（Context = 目标区域，表单内走 InteractionComp->ModifyArea）
	if (!AreaEditDialogClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityDetail] AreaEditDialogClass 未配置（WBP_EntityDetailPanel 默认值需指向 WBP_AreaBusinessEditDialog）"));
		if (UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer()))
		{
			Layout->ShowToast(FText::FromString(TEXT("编辑弹窗未配置")), 2.0f);
		}
		return;
	}

	UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer());
	if (!Layout)
	{
		UE_LOG(LogTemp, Error, TEXT("[EntityDetail] PrimaryGameLayout 不可用，无法打开编辑弹窗"));
		return;
	}
	Layout->PushModal(AreaEditDialogClass, Area);
}

void UVoxelEntityDetailPanel::OnDecorateClicked()
{
	AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
	UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp();
	AVoxelAreaBaseInstance* Area = SelComp ? SelComp->GetSelectedArea() : nullptr;
	if (!PC || !PC->InteractionComp || !Area) return;

	// 进入建造模式，并把建造上下文选中到该区域（读取已解析的 Area，写操作走 InteractionComp）
	PC->InteractionComp->SetPlayerMode(EVoxelControllerMode::SceneBuildMode);
	PC->InteractionComp->SelectArea(Area);
}

void UVoxelEntityDetailPanel::RefreshFromSelection()
{
	UVoxelPlayerStateCacheComponent* SelComp = GetSelectionComp();
	AVoxelAreaBaseInstance* Area = (SelComp && SelComp->GetSelectedEntityType() == EVoxelSelectedEntityType::Area)
		? SelComp->GetSelectedArea() : nullptr;

	if (!Area)
	{
		// 无选中实体（None / 未支持的类型 / 区域已删除）：清空并整体隐藏面板
		if (AreaCard) AreaCard->SetVisibility(ESlateVisibility::Collapsed);
		if (ActionGroupBox) ActionGroupBox->SetVisibility(ESlateVisibility::Collapsed);
		if (EmptyStateText) EmptyStateText->SetVisibility(ESlateVisibility::Collapsed);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	if (EmptyStateText) EmptyStateText->SetVisibility(ESlateVisibility::Collapsed);
	if (AreaCard) AreaCard->SetVisibility(ESlateVisibility::Visible);
	if (ActionGroupBox) ActionGroupBox->SetVisibility(ESlateVisibility::Visible);
	FillAreaCard(Area);
}

void UVoxelEntityDetailPanel::FillAreaCard(AVoxelAreaBaseInstance* Area)
{
	const FAreaInstance& Info = Area->AreaBase;

	// 区域类型配置（图标仍需原始配置；显示名走统一入口，内含 TagName 兜底）
	UConfigDataSubsystem* ConfigSys = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		ConfigSys = GI->GetSubsystem<UConfigDataSubsystem>();
	}
	const FAreaTypeBase* TypeCfg = ConfigSys ? ConfigSys->GetAreaTypeByTag(Info.Function.AreaType) : nullptr;
	const FText TypeName = ConfigSys ? ConfigSys->GetAreaTypeDisplayName(Info.Function.AreaType) : FText::FromString(TEXT("未分类"));

	// 状态文本（EAreaStatus 自带 DisplayName）
	const FText StatusText = StaticEnum<EAreaStatus>()->GetDisplayValueAsText(Info.Function.Status);

	if (AreaNameText) AreaNameText->SetText(FText::FromName(Info.AreaName));
	if (AreaSubTitleText) AreaSubTitleText->SetText(FText::Format(FText::FromString(TEXT("{0} · {1}")), TypeName, StatusText));

	if (AreaIconImage)
	{
		if (TypeCfg && TypeCfg->AssetFragment.Icon)
		{
			AreaIconImage->SetBrushFromTexture(TypeCfg->AssetFragment.Icon);
			AreaIconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			AreaIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 父链面包屑
	CurrentParentAreaId = -1;
	if (Area->ParentArea)
	{
		CurrentParentAreaId = Area->ParentArea->AreaBase.AreaId;
		if (ParentAreaRow) ParentAreaRow->SetVisibility(ESlateVisibility::Visible);
		if (ParentAreaText) ParentAreaText->SetText(FText::Format(
			FText::FromString(TEXT("‹ {0}")), FText::FromName(Area->ParentArea->AreaBase.AreaName)));
	}
	else
	{
		if (ParentAreaRow) ParentAreaRow->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 属性区
	if (StatusValueText) StatusValueText->SetText(StatusText);

	const FIntVector Min = Info.AreaCoverage.MinIndex;
	const FIntVector Max = Info.AreaCoverage.MaxIndex;
	if (RangeValueText)
	{
		RangeValueText->SetText(FText::FromString(FString::Printf(
			TEXT("(%d,%d,%d) ~ (%d,%d,%d)"), Min.X, Min.Y, Min.Z, Max.X, Max.Y, Max.Z)));
	}
	if (SizeValueText)
	{
		SizeValueText->SetText(FText::FromString(FString::Printf(
			TEXT("%d×%d×%d"), Max.X - Min.X + 1, Max.Y - Min.Y + 1, Max.Z - Min.Z + 1)));
	}
	if (ManagerValueText) ManagerValueText->SetText(ResolveCreatureName(Info.ManagerCreatureId));
	if (OwnerValueText) OwnerValueText->SetText(ResolveCreatureName(Info.OwnerCreatureId));

	if (AuthorityValueText)
	{
		const FString PublicText = Info.Authority.bIsPublic ? TEXT("公开") : TEXT("私有");
		const FString BattleText = Info.Authority.bIsAllowBattle ? TEXT("允许战斗") : TEXT("禁止战斗");
		AuthorityValueText->SetText(FText::FromString(PublicText + TEXT(" · ") + BattleText));
	}

	RebuildSubAreaList(Area);

	// 「编辑业务」按钮仅所有人/经营人可见（权限判定统一走后端 IsAreaEditable，规则扩展只改后端）
	if (EditBusinessButton)
	{
		const bool bCanEdit = Area->IsAreaEditable(UVoxelGameInstance::GetMainCharacterIdSafe(this));
		EditBusinessButton->SetVisibility(bCanEdit ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 订阅区域基础数据变更（编辑弹窗保存后自动刷新详情卡）
	if (BoundArea != Area)
	{
		if (BoundArea)
		{
			BoundArea->OnAreaBaseInfoChanged.RemoveDynamic(this, &UVoxelEntityDetailPanel::HandleAreaBaseInfoChanged);
		}
		BoundArea = Area;
		BoundArea->OnAreaBaseInfoChanged.AddDynamic(this, &UVoxelEntityDetailPanel::HandleAreaBaseInfoChanged);
	}
}

void UVoxelEntityDetailPanel::HandleAreaBaseInfoChanged(AVoxelAreaBaseInstance* Area)
{
	// 仅当前选中区域的数据变更才刷新（避免其他区域保存触发无关刷新）
	if (Area && Area == BoundArea)
	{
		RefreshFromSelection();
	}
}

void UVoxelEntityDetailPanel::RebuildSubAreaList(AVoxelAreaBaseInstance* Area)
{
	if (!SubAreaListBox) return;

	SubAreaListBox->ClearChildren();

	const int32 SubCount = Area->SubAreasIndex.Num();
	if (SubAreaSection)
	{
		SubAreaSection->SetVisibility(SubCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (SubCount > 0 && !SubAreaRowClass)
	{
		// 行控件类未配置 = 全部行不可用，显式报错不做静默降级
		UE_LOG(LogTemp, Error, TEXT("[EntityDetail] SubAreaRowClass 未配置（WBP_EntityDetailPanel 默认值需指向 WBP_EntitySubAreaRow）"));
		return;
	}

	TArray<AVoxelAreaBaseInstance*> SubAreas;
	Area->SubAreasIndex.GenerateValueArray(SubAreas);
	SubAreas.Sort([](const AVoxelAreaBaseInstance& A, const AVoxelAreaBaseInstance& B)
	{
		return A.AreaBase.AreaId < B.AreaBase.AreaId;
	});

	for (AVoxelAreaBaseInstance* Sub : SubAreas)
	{
		if (!Sub) continue;

		UVoxelEntitySubAreaRowWidget* Row = CreateWidget<UVoxelEntitySubAreaRowWidget>(this, SubAreaRowClass);
		if (!Row) continue;

		Row->OnRowClicked.AddDynamic(this, &UVoxelEntityDetailPanel::HandleSubAreaRowClicked);
		Row->InitializeRow(
			FText::FromName(Sub->AreaBase.AreaName),
			StaticEnum<EAreaStatus>()->GetDisplayValueAsText(Sub->AreaBase.Function.Status),
			Sub->AreaBase.AreaId);

		SubAreaListBox->AddChildToVerticalBox(Row);
	}
}

void UVoxelEntityDetailPanel::HandleSubAreaRowClicked(int32 AreaId)
{
	NavigateToArea(AreaId);
}

FText UVoxelEntityDetailPanel::ResolveCreatureName(int32 CreatureId) const
{
	// 统一解析入口：CreatureWorldSubsystem（分区语义唯一实现处）
	if (UWorld* World = GetWorld())
	{
		if (UVoxelCreatureWorldSubsystem* CreatureSys = World->GetSubsystem<UVoxelCreatureWorldSubsystem>())
		{
			return CreatureSys->ResolveCreatureName(CreatureId);
		}
	}
	return FText::FromString(FString::Printf(TEXT("#%d"), CreatureId));
}

UVoxelPlayerStateCacheComponent* UVoxelEntityDetailPanel::GetSelectionComp() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		return PC->FindComponentByClass<UVoxelPlayerStateCacheComponent>();
	}
	return nullptr;
}
