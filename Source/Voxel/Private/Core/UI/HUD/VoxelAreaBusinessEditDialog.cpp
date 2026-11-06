#include "Core/UI/HUD/VoxelAreaBusinessEditDialog.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"

#include "Area/VoxelAreaBaseInstance.h"
#include "Data/Area/AreaEnums.h"
#include "Core/UI/Base/VoxelCreaturePickerWidget.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/VoxelPlayerController.h"

namespace
{
	/**
	 * 取 Tag 的直接父 Tag（FGameplayTag 无原生接口，按字符串截取最后一段）
	 * 顶层 Tag（如 Area.Type）无父，返回 EmptyTag
	 */
	FGameplayTag GetDirectParentTag(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return FGameplayTag::EmptyTag;
		}
		const FString TagString = Tag.ToString();
		int32 DotIndex = INDEX_NONE;
		if (!TagString.FindLastChar(TEXT('.'), DotIndex) || DotIndex <= 0)
		{
			return FGameplayTag::EmptyTag;
		}
		return FGameplayTag::RequestGameplayTag(FName(TagString.Left(DotIndex)), false);
	}
}

void UVoxelAreaBusinessEditDialog::NativeConstruct()
{
	Super::NativeConstruct();

	// AddUniqueDynamic：弹窗在模态栈中可能经历 反激活→再激活 的二次 Construct，
	// AddDynamic 会重复绑定导致一次点击触发两次处理（曾引发「保存成功后又报类型冲突」）
	if (ConfirmButton) ConfirmButton->OnClicked.AddUniqueDynamic(this, &UVoxelAreaBusinessEditDialog::OnConfirmClicked);
	if (CancelButton) CancelButton->OnClicked.AddUniqueDynamic(this, &UVoxelAreaBusinessEditDialog::OnCancelClicked);
}

UWidget* UVoxelAreaBusinessEditDialog::NativeGetDesiredFocusTarget() const
{
	return CancelButton;
}

void UVoxelAreaBusinessEditDialog::OnDialogOpened(UObject* Context)
{
	TargetArea = Cast<AVoxelAreaBaseInstance>(Context);
	if (!TargetArea)
	{
		UE_LOG(LogTemp, Error, TEXT("[AreaEditDialog] OnDialogOpened Context 不是 AVoxelAreaBaseInstance，表单保持空"));
		return;
	}
	RefreshFromArea();
}

void UVoxelAreaBusinessEditDialog::RefreshFromArea()
{
	if (!TargetArea) return;

	// --- 权限判定（统一入口：后端 IsAreaEditable；Owner 单独判定用于「仅所有人可转让所有权」） ---
	const int32 CurPlayerId = UVoxelGameInstance::GetMainCharacterIdSafe(this);
	const bool bIsOwner = TargetArea->AreaBase.IsOwner(CurPlayerId);
	const bool bCanEdit = TargetArea->IsAreaEditable(CurPlayerId);

	// --- 字段值填充 ---
	if (NameInput)
	{
		NameInput->SetText(FText::FromName(TargetArea->AreaBase.AreaName));
		NameInput->SetIsEnabled(bCanEdit);
	}

	FillTypeOptions();
	if (TypeCombo) TypeCombo->SetIsEnabled(bCanEdit);

	FillStatusOptions();
	if (StatusCombo) StatusCombo->SetIsEnabled(bCanEdit);

	if (OwnerPicker)
	{
		OwnerPicker->InitPicker(TargetArea->AreaBase.OwnerCreatureId, true);
		OwnerPicker->SetPickerEnabled(bIsOwner); // 经营人不可改所有人
	}

	if (ManagerPicker)
	{
		ManagerPicker->InitPicker(TargetArea->AreaBase.ManagerCreatureId, true);
		ManagerPicker->SetPickerEnabled(bCanEdit);
	}
}

void UVoxelAreaBusinessEditDialog::FillTypeOptions()
{
	if (!TypeCombo || !TargetArea) return;

	TypeCombo->ClearOptions();
	TypeTags.Reset();

	UConfigDataSubsystem* ConfigSys = GetGameInstance() ? GetGameInstance()->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!ConfigSys)
	{
		UE_LOG(LogTemp, Error, TEXT("[AreaEditDialog] ConfigDataSubsystem 不可用，类型下拉为空"));
		return;
	}

	const TArray<const FAreaTypeBase*> AllTypes = ConfigSys->GetAllAreaTypes();

	// 收集全部出现过的「直接父 Tag」→ 拥有子类型的层级节点集合
	// 注意：bAbstract 是重叠许可标志，与类型层级无关，禁止用于此过滤
	TSet<FGameplayTag> ParentTags;
	ParentTags.Reserve(AllTypes.Num());
	for (const FAreaTypeBase* Type : AllTypes)
	{
		if (Type)
		{
			ParentTags.Add(GetDirectParentTag(Type->AreaTypeTag));
		}
	}

	// 候选：同一直接父 Tag 且自身无子类型（叶子类型才可直接选）
	const FGameplayTag CurrentTag = TargetArea->AreaBase.Function.AreaType;
	const FGameplayTag CurrentParent = GetDirectParentTag(CurrentTag);
	const FAreaTypeBase* CurrentType = ConfigSys->GetAreaTypeByTag(CurrentTag);

	int32 SelectedIndex = INDEX_NONE;
	for (const FAreaTypeBase* Type : AllTypes)
	{
		if (!Type || !Type->AreaTypeTag.IsValid()) continue;
		if (GetDirectParentTag(Type->AreaTypeTag) != CurrentParent) continue;
		if (ParentTags.Contains(Type->AreaTypeTag)) continue; // 层级节点不可直接选

		TypeCombo->AddOption(Type->DisplayName.ToString());
		TypeTags.Add(Type->AreaTypeTag);
		if (Type->AreaTypeTag == CurrentTag)
		{
			SelectedIndex = TypeTags.Num() - 1;
		}
	}

	// 当前类型是层级节点（异常数据/未分类）：附加保底项，保证表单显示当前状态
	if (CurrentType && SelectedIndex == INDEX_NONE)
	{
		const FString FallbackOption = FString::Printf(TEXT("%s（当前）"), *CurrentType->DisplayName.ToString());
		TypeCombo->AddOption(FallbackOption);
		TypeTags.Add(CurrentTag);
		SelectedIndex = TypeTags.Num() - 1;
		UE_LOG(LogTemp, Warning, TEXT("[AreaEditDialog] 当前类型 %s 为层级节点，仅作保底显示"), *CurrentTag.ToString());
	}

	if (SelectedIndex != INDEX_NONE)
	{
		TypeCombo->SetSelectedIndex(SelectedIndex);
	}
}

void UVoxelAreaBusinessEditDialog::FillStatusOptions()
{
	if (!StatusCombo || !TargetArea) return;

	StatusCombo->ClearOptions();
	StatusValues.Reset();

	const UEnum* StatusEnum = StaticEnum<EAreaStatus>();
	const int32 CurrentStatus = static_cast<int32>(TargetArea->AreaBase.Function.Status);
	int32 SelectedIndex = INDEX_NONE;

	const int32 NumStatus = StatusEnum ? StatusEnum->NumEnums() : 0;
	for (int32 i = 0; i < NumStatus; ++i)
	{
		// 跳过 UENUM 内部生成的 MAX 项
		if (StatusEnum->GetAuthoredNameStringByIndex(i) == TEXT("MAX"))
		{
			continue;
		}
		const int32 Value = StatusEnum->GetValueByIndex(i);
		StatusCombo->AddOption(StatusEnum->GetDisplayNameTextByIndex(i).ToString());
		StatusValues.Add(Value);
		if (Value == CurrentStatus)
		{
			SelectedIndex = StatusValues.Num() - 1;
		}
	}

	if (SelectedIndex != INDEX_NONE)
	{
		StatusCombo->SetSelectedIndex(SelectedIndex);
	}
}

void UVoxelAreaBusinessEditDialog::OnConfirmClicked()
{
	if (!TargetArea)
	{
		ShowToast(FText::FromString(TEXT("编辑目标无效")), true);
		return;
	}

	const int32 CurPlayerId = UVoxelGameInstance::GetMainCharacterIdSafe(this);
	const bool bIsOwner = TargetArea->AreaBase.IsOwner(CurPlayerId);

	// --- 校验：名称非空 ---
	if (!NameInput || NameInput->GetText().ToString().TrimStartAndEnd().IsEmpty())
	{
		ShowToast(FText::FromString(TEXT("区域名称不能为空")), true);
		return;
	}

	// --- 收集表单值 ---
	const int32 TypeIndex = TypeCombo ? TypeCombo->GetSelectedIndex() : INDEX_NONE;
	const FGameplayTag SelectedTypeTag = TypeTags.IsValidIndex(TypeIndex) ? TypeTags[TypeIndex] : FGameplayTag::EmptyTag;
	if (!SelectedTypeTag.IsValid())
	{
		ShowToast(FText::FromString(TEXT("请选择区域类型")), true);
		return;
	}

	const int32 StatusIndex = StatusCombo ? StatusCombo->GetSelectedIndex() : INDEX_NONE;
	if (!StatusValues.IsValidIndex(StatusIndex))
	{
		ShowToast(FText::FromString(TEXT("请选择营业状态")), true);
		return;
	}

	// 选择器未选中（-1）时保持原值，避免误清空权属（选中值可为 0 = 系统角色）
	const int32 PickedOwnerId = OwnerPicker ? OwnerPicker->GetSelectedCreatureId() : -1;
	const int32 PickedManagerId = ManagerPicker ? ManagerPicker->GetSelectedCreatureId() : -1;
	const int32 SelectedOwnerId = (PickedOwnerId >= 0) ? PickedOwnerId : TargetArea->AreaBase.OwnerCreatureId;
	const int32 SelectedManagerId = (PickedManagerId >= 0) ? PickedManagerId : TargetArea->AreaBase.ManagerCreatureId;

	// --- 字段级权限：仅所有人可转让所有权 ---
	if (!bIsOwner && SelectedOwnerId != TargetArea->AreaBase.OwnerCreatureId)
	{
		ShowToast(FText::FromString(TEXT("仅所有人可修改所有人")), true);
		return;
	}

	// --- 类型冲突预检（后端 ModifyArea 内仍会兜底校验） ---
	if (!TargetArea->CheckTypeConflict(SelectedTypeTag))
	{
		ShowToast(FText::FromString(TEXT("区域类型冲突（与相邻/包含区域不兼容）")), true);
		return;
	}

	// --- 组装修改数据（未在表单中编辑的字段保持原值） ---
	FAreaModificationData NewData;
	NewData.AreaName = FName(*NameInput->GetText().ToString().TrimStartAndEnd());
	NewData.AreaType = SelectedTypeTag;
	NewData.Status = static_cast<EAreaStatus>(StatusValues[StatusIndex]);
	NewData.OwnerCreatureId = SelectedOwnerId;
	NewData.ManagerCreatureId = SelectedManagerId;
	NewData.ParentScenceId = TargetArea->AreaBase.ParentScenceId;
	NewData.ParentAreaId = TargetArea->AreaBase.ParentAreaId;
	NewData.AreaCoverage = TargetArea->AreaBase.AreaCoverage;
	NewData.bIsValid = true;

	// --- 提交业务方法（不直改数据） ---
	AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>();
	if (!PC || !PC->InteractionComp)
	{
		ShowToast(FText::FromString(TEXT("玩家控制器不可用")), true);
		return;
	}
	PC->InteractionComp->ModifyArea(NewData);

	ShowToast(FText::FromString(TEXT("区域信息已保存")), false);
	Close();
}

void UVoxelAreaBusinessEditDialog::OnCancelClicked()
{
	Close();
}

void UVoxelAreaBusinessEditDialog::ShowToast(const FText& Message, bool bIsError) const
{
	if (UVoxelPrimaryGameLayout* Layout = UVoxelPrimaryGameLayout::GetFromPC(GetOwningPlayer()))
	{
		Layout->ShowToast(Message, 2.0f);
	}
	if (bIsError)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaEditDialog] %s"), *Message.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[AreaEditDialog] %s"), *Message.ToString());
	}
}
