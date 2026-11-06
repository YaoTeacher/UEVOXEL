#include "Core/UI/Base/VoxelCreaturePickerWidget.h"

#include "Components/ComboBoxString.h"

#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "Core/VoxelGameInstance.h"
#include "GameManager/WorldManager/VoxelCreatureWorldSubsystem.h"

void UVoxelCreaturePickerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// AddUniqueDynamic：容器栈可能经历 反激活→再激活 的二次 Construct
	if (CreatureCombo)
	{
		CreatureCombo->OnSelectionChanged.AddUniqueDynamic(this, &UVoxelCreaturePickerWidget::HandleSelectionChanged);
	}
}

void UVoxelCreaturePickerWidget::InitPicker(int32 CurrentId, bool bIncludeSelf)
{
	if (!CreatureCombo) return;

	CreatureCombo->ClearOptions();
	OptionIds.Reset();

	UVoxelCreatureWorldSubsystem* CreatureSys = GetWorld() ? GetWorld()->GetSubsystem<UVoxelCreatureWorldSubsystem>() : nullptr;
	if (!CreatureSys) return;

	int32 SelectedIndex = INDEX_NONE;

	// 当前主角快捷项（显示名走统一解析：0=系统角色 / 玩家主角=「玩家」）
	const int32 MainCharId = UVoxelGameInstance::GetMainCharacterIdSafe(this);
	if (bIncludeSelf && MainCharId >= 0)
	{
		CreatureCombo->AddOption(MakeCreatureOptionText(CreatureSys->ResolveCreatureName(MainCharId), MainCharId));
		OptionIds.Add(MainCharId);
		if (CurrentId == MainCharId)
		{
			SelectedIndex = 0;
		}
	}

	// 可管理在场生物（与主角快捷项重复的跳过）
	for (AVoxelNpcBaseInstance* Creature : CreatureSys->GetSelectableCreaturesForUser(MainCharId))
	{
		if (!Creature) continue;
		const int32 CreatureId = Creature->CreatureData.CreatureID;
		if (bIncludeSelf && CreatureId == MainCharId) continue;

		CreatureCombo->AddOption(MakeCreatureOptionText(CreatureSys->ResolveCreatureName(CreatureId), CreatureId));
		OptionIds.Add(CreatureId);
		if (CreatureId == CurrentId)
		{
			SelectedIndex = OptionIds.Num() - 1;
		}
	}

	// 当前值不在可选列表（离场生物/系统角色 0 等）：补显示项避免空白
	if (SelectedIndex == INDEX_NONE && CurrentId >= 0)
	{
		const FString Option = MakeCreatureOptionText(CreatureSys->ResolveCreatureName(CurrentId), CurrentId) + TEXT("（当前）");
		CreatureCombo->AddOption(Option);
		OptionIds.Add(CurrentId);
		SelectedIndex = OptionIds.Num() - 1;
	}

	if (SelectedIndex != INDEX_NONE)
	{
		CreatureCombo->SetSelectedIndex(SelectedIndex);
	}
}

int32 UVoxelCreaturePickerWidget::GetSelectedCreatureId() const
{
	// 未选中/未命中返回 -1（调用方按「保持原值」处理）
	if (!CreatureCombo) return -1;
	const int32 Index = CreatureCombo->GetSelectedIndex();
	return OptionIds.IsValidIndex(Index) ? OptionIds[Index] : -1;
}

void UVoxelCreaturePickerWidget::SetPickerEnabled(bool bEnabled)
{
	if (CreatureCombo)
	{
		CreatureCombo->SetIsEnabled(bEnabled);
	}
}

void UVoxelCreaturePickerWidget::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// 程序化填充期间的 SetSelectedIndex 不对外广播（SelectionType 反映来源）
	if (SelectionType == ESelectInfo::Direct) return;

	const int32 CreatureId = GetSelectedCreatureId();
	if (CreatureId >= 0)
	{
		OnCreaturePicked.Broadcast(CreatureId);
	}
}

FString UVoxelCreaturePickerWidget::MakeCreatureOptionText(const FText& DisplayName, int32 CreatureId)
{
	return FString::Printf(TEXT("%s #%d"), *DisplayName.ToString(), CreatureId);
}
