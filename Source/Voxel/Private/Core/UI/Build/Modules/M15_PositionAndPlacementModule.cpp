#include "Core/UI/Build/Modules/M15_PositionAndPlacementModule.h"
#include "Core/UI/Build/Modules/MaterialRemapRow.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Voxel/Editor/UVoxelTemplateUtils.h"
#include "Core/VoxelGameInstance.h"
#include "Core/CoreTags.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Core/UI/Item/ItemLibraryWidgetBase.h"
#include "Core/UI/ItemLibrary/VoxelItemDataSource.h"
#include "Core/UI/ItemLibrary/ItemLibraryCategoryProvider.h"
#include "Core/UI/ItemLibrary/ItemLibrarySlotBase.h"
#include "Core/UI/ItemLibrary/ItemLibraryTabButtonBase.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"

// 方向名称定义（前、右、后、左，顺时针）
const TArray<FString> UM15_PositionAndPlacementModule::DirectionNames = {
	TEXT("前"),
	TEXT("右"),
	TEXT("后"),
	TEXT("左")
};

bool UM15_PositionAndPlacementModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag == FCoreTags::Tool_Edit_Template || ToolTag == FCoreTags::Tool_Edit_Furniture;
}

// ==================== 显示层 ====================

void UM15_PositionAndPlacementModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC) return;

	StateComponent = SC;

	UE_LOG(LogTemp, Warning, TEXT("[M15] UpdateFromToolState activated. Class=%s Tool=%s, Phase=%d, MaterialListHost=%s, MaterialPicker=%s, TreeMaterialListHost=%s, TreeMaterialPicker=%s"),
		*GetClass()->GetName(),
		*SC->GetCurrentToolTag().ToString(), (int32)SC->GetBuildPhase(),
		MaterialListHost ? TEXT("VALID") : TEXT("NULL"),
		MaterialPicker ? TEXT("VALID") : TEXT("NULL"),
		GetWidgetFromName(TEXT("MaterialListHost")) ? TEXT("VALID") : TEXT("NULL"),
		GetWidgetFromName(TEXT("MaterialPicker")) ? TEXT("VALID") : TEXT("NULL"));

	// 订阅状态广播：BASE 跟随鼠标、EDIT Gizmo 拖动时位置/旋转会持续变化，
	// 需要实时刷新 UI（否则控件停留在工具切换时的初始值）
	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM15_PositionAndPlacementModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	RefreshFromGeometry();
}

void UM15_PositionAndPlacementModule::HandleBuildModeParamsUpdated()
{
	RefreshFromGeometry();
}

void UM15_PositionAndPlacementModule::RefreshFromGeometry()
{
	if (!StateComponent) return;

	const FVoxelGeometryDesc& Geo = StateComponent->GetTmpGeometry();

	// 更新位置输入框（从SC的Box.MinIndex读取）
	const FIntVector& Position = Geo.Box.isValid() ? Geo.Box.MinIndex : FIntVector::ZeroValue;
	if (PositionXInput) PositionXInput->SetText(FText::AsNumber(Position.X));
	if (PositionYInput) PositionYInput->SetText(FText::AsNumber(Position.Y));
	if (PositionZInput) PositionZInput->SetText(FText::AsNumber(Position.Z));

	// 更新方向显示（从SC的Rotation.Yaw计算）
	if (DirectionText)
	{
		const int32 DirIdx = YawToDirectionIndex(Geo.Rotation.Yaw);
		const int32 ClampedIdx = FMath::Clamp(DirIdx, 0, DirectionNames.Num() - 1);
		DirectionText->SetText(FText::FromString(DirectionNames[ClampedIdx]));
	}

	// 重建素材替换列表（仅模板工具且蓝图配了容器时）
	RebuildMaterialList();
}

// ==================== 素材替换 ====================

void UM15_PositionAndPlacementModule::RebuildMaterialList()
{
	if (!StateComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: StateComponent=NULL"));
		return;
	}
	if (!MaterialListHost)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: MaterialListHost=NULL (WBP_M15 bind missing)"));
		return;
	}

	// 仅模板工具显示素材替换（家具是单实例，无需替换列表）
	if (StateComponent->GetCurrentToolTag() != FCoreTags::Tool_Edit_Template)
	{
		if (LastBuiltHandleId != -1)
		{
			MaterialListHost->ClearChildren();
			LastBuiltHandleId = -1;
			LastBuiltRemapCount = -1;
		}
		return;
	}

	UVoxelGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UVoxelGameInstance>() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!GI)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: GameInstance=NULL"));
		return;
	}
	if (!CDS)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: ConfigDataSubsystem=NULL"));
		return;
	}

	const FInteractableData HandData = StateComponent->GetInteractableDataInHand();
	const int32 TemplateId = HandData.HandleId;
	const int32 RemapCount = StateComponent->GetTemplateRemap().Num();

	// 缓存守卫：模板未变且替换表未变则跳过重建（避免每帧 hover 广播时重建）
	if (TemplateId == LastBuiltHandleId && RemapCount == LastBuiltRemapCount)
	{
		return;
	}
	LastBuiltHandleId = TemplateId;
	LastBuiltRemapCount = RemapCount;

	MaterialListHost->ClearChildren();

	const FVoxelTemplate* Tmpl = CDS->GetTemplateById(TemplateId);
	if (!Tmpl)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M15] RebuildMaterialList: no template in hand. HandItemId=%d"), TemplateId);
		return;
	}

	if (!RowClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: RowClass=NULL (WBP_M15 RowClass not configured)"));
		return;
	}

	const TArray<FString> Mats = UVoxelTemplateUtils::GetDistinctMaterials(*Tmpl, CDS);
	const TMap<FString, FString>& Remap = StateComponent->GetTemplateRemap();
	if (Mats.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[M15] RebuildMaterialList: template has no replaceable materials. TemplateId=%d Entries=%d"),
			TemplateId, Tmpl->Entries.Num());
		return;
	}

	int32 CreatedRows = 0;
	for (const FString& OriginFullId : Mats)
	{
		const FString* NewFullIdPtr = Remap.Find(OriginFullId);
		const FString CurrentFullId = NewFullIdPtr ? *NewFullIdPtr : OriginFullId;

		UMaterialRemapRow* Row = CreateWidget<UMaterialRemapRow>(this, RowClass);
		if (!Row)
		{
			UE_LOG(LogTemp, Error, TEXT("[M15] RebuildMaterialList FAILED: CreateWidget Row failed. RowClass=%s OriginFullId='%s'"),
				*RowClass->GetName(), *OriginFullId);
			continue;
		}

		Row->InitRow(OriginFullId, CurrentFullId, CDS);
		Row->OnPickRequested.AddDynamic(this, &UM15_PositionAndPlacementModule::HandleRowPickRequested);
		MaterialListHost->AddChild(Row);
		++CreatedRows;
	}

	UE_LOG(LogTemp, Warning, TEXT("[M15] RebuildMaterialList OK: TemplateId=%d Mats=%d Rows=%d HostChildren=%d Remap=%d"),
		TemplateId, Mats.Num(), CreatedRows, MaterialListHost->GetChildrenCount(), Remap.Num());
}

void UM15_PositionAndPlacementModule::HandleRowPickRequested(FString OriginFullId)
{
	if (!MaterialPicker)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandleRowPickRequested FAILED: MaterialPicker=NULL"));
		return;
	}

	UVoxelGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UVoxelGameInstance>() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!CDS)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandleRowPickRequested FAILED: ConfigDataSubsystem=NULL"));
		return;
	}

	// 记录当前正在替换的原素材 FullId
	PendingRemapOriginFullId = OriginFullId;

	// 计算同类过滤 Tag（方块→全方块，家具→同子分类）
	const FGameplayTag FilterTag = UVoxelTemplateUtils::GetRemapFilterTag(OriginFullId, CDS);
	if (!FilterTag.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandleRowPickRequested FAILED: invalid FilterTag. OriginFullId='%s'"), *OriginFullId);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[M15] HandleRowPickRequested: OriginFullId='%s' FilterTag=%s"),
		*OriginFullId, *FilterTag.ToString());

	// 注入数据源（按同类 Tag 过滤）+ 分类提供器
	UVoxelItemDataSource* DS = NewObject<UVoxelItemDataSource>(MaterialPicker, UVoxelItemDataSource::StaticClass());
	DS->QueryTag = FilterTag;
	MaterialPicker->bActivateDataSourceOnClick = false; // 材质替换是纯选择器，不能触发 SetItemInHand
	MaterialPicker->SetDataSource(DS);

	UTagHierarchyCategoryProvider* CP = NewObject<UTagHierarchyCategoryProvider>(MaterialPicker, UTagHierarchyCategoryProvider::StaticClass());
	CP->RootTag = FilterTag;
	MaterialPicker->SetCategoryProvider(CP);

	// 绑定条目点击（一次即可）
	if (!bPickerBound)
	{
		MaterialPicker->OnEntryClicked.AddDynamic(this, &UM15_PositionAndPlacementModule::HandlePickerEntryClicked);
		bPickerBound = true;
	}

	MaterialPicker->InitializeLibrary();
	MaterialPicker->SetVisibility(ESlateVisibility::Visible);
}

void UM15_PositionAndPlacementModule::HandlePickerEntryClicked(const FItemLibraryEntry& Entry)
{
	if (!StateComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandlePickerEntryClicked FAILED: StateComponent=NULL"));
		return;
	}
	if (PendingRemapOriginFullId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandlePickerEntryClicked FAILED: PendingRemapOriginFullId empty. HandleId=%d"), Entry.HandleId);
		return;
	}

	// 由 Entry.HandleId (Handle) 反查 FullId
	UVoxelGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UVoxelGameInstance>() : nullptr;
	UConfigDataSubsystem* CDS = GI ? GI->GetSubsystem<UConfigDataSubsystem>() : nullptr;
	if (!CDS)
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandlePickerEntryClicked FAILED: ConfigDataSubsystem=NULL"));
		return;
	}
	const FString NewFullId = CDS->GetFullIdByHandle(Entry.HandleId);
	if (NewFullId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[M15] HandlePickerEntryClicked FAILED: HandleId=%d not in ItemMap"), Entry.HandleId);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[M15] HandlePickerEntryClicked: Remap '%s' -> '%s' (%s)"),
		*PendingRemapOriginFullId, *NewFullId, *Entry.DisplayName.ToString());

	// 写入替换表（经 InteractionComp 触发广播 → RefreshFromGeometry → 预览+列表刷新）
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp) PC->InteractionComp->SetTemplateRemapEntry(PendingRemapOriginFullId, NewFullId);
	}

	// 关闭选择器
	if (MaterialPicker)
	{
		MaterialPicker->SetVisibility(ESlateVisibility::Collapsed);
	}
	PendingRemapOriginFullId.Reset();
}

void UM15_PositionAndPlacementModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM15_PositionAndPlacementModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

// ==================== 生命周期 ====================

void UM15_PositionAndPlacementModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (PositionXInput)
		PositionXInput->OnTextChanged.AddDynamic(this, &UM15_PositionAndPlacementModule::OnPositionXTextChanged);
	if (PositionYInput)
		PositionYInput->OnTextChanged.AddDynamic(this, &UM15_PositionAndPlacementModule::OnPositionYTextChanged);
	if (PositionZInput)
		PositionZInput->OnTextChanged.AddDynamic(this, &UM15_PositionAndPlacementModule::OnPositionZTextChanged);
	if (TurnLeftButton)
		TurnLeftButton->OnClicked.AddDynamic(this, &UM15_PositionAndPlacementModule::OnTurnLeftClicked);
	if (TurnRightButton)
		TurnRightButton->OnClicked.AddDynamic(this, &UM15_PositionAndPlacementModule::OnTurnRightClicked);
	if (PlaceButton)
		PlaceButton->OnClicked.AddDynamic(this, &UM15_PositionAndPlacementModule::OnPlaceClicked);
}

void UM15_PositionAndPlacementModule::NativeConstruct()
{
	Super::NativeConstruct();
}

// ==================== 交互层：位置输入（只改SC，不改UI） ====================

void UM15_PositionAndPlacementModule::OnPositionXTextChanged(const FText& Text)
{
	const FString Str = Text.ToString();
	if (Str.IsEmpty() || !FCString::IsNumeric(*Str)) return;
	ApplyPositionDelta(0, FCString::Atoi(*Str));
}

void UM15_PositionAndPlacementModule::OnPositionYTextChanged(const FText& Text)
{
	const FString Str = Text.ToString();
	if (Str.IsEmpty() || !FCString::IsNumeric(*Str)) return;
	ApplyPositionDelta(1, FCString::Atoi(*Str));
}

void UM15_PositionAndPlacementModule::OnPositionZTextChanged(const FText& Text)
{
	const FString Str = Text.ToString();
	if (Str.IsEmpty() || !FCString::IsNumeric(*Str)) return;
	ApplyPositionDelta(2, FCString::Atoi(*Str));
}

// ==================== 交互层：旋转（只改SC，不改UI） ====================

void UM15_PositionAndPlacementModule::OnTurnLeftClicked()
{
	if (!StateComponent) return;

	const FRotator CurrentRotation = StateComponent->GetTmpGeometry().Rotation;
	const int32 CurrentIdx = YawToDirectionIndex(CurrentRotation.Yaw);
	const int32 NewIdx = (CurrentIdx + DirectionNames.Num() - 1) % DirectionNames.Num();
	ApplyRotation(NewIdx);
}

void UM15_PositionAndPlacementModule::OnTurnRightClicked()
{
	if (!StateComponent) return;

	const FRotator CurrentRotation = StateComponent->GetTmpGeometry().Rotation;
	const int32 CurrentIdx = YawToDirectionIndex(CurrentRotation.Yaw);
	const int32 NewIdx = (CurrentIdx + 1) % DirectionNames.Num();
	ApplyRotation(NewIdx);
}

// ==================== 交互层：放置 ====================

void UM15_PositionAndPlacementModule::OnPlaceClicked()
{
	ExecutePlace();
}

// ==================== 交互层：核心逻辑 ====================

void UM15_PositionAndPlacementModule::ApplyPositionDelta(int32 Axis, int32 NewValue)
{
	if (!StateComponent) return;

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	// 移动 Box 使 MinIndex[Axis] = NewValue
	const int32 Delta = NewValue - Geo.Box.MinIndex[Axis];
	Geo.Box.MinIndex[Axis] = NewValue;
	Geo.Box.MaxIndex[Axis] += Delta;

	ApplyGeometryToState(Geo);
}

void UM15_PositionAndPlacementModule::ApplyGeometryToState(const FVoxelGeometryDesc& Geo)
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp) PC->InteractionComp->SetTmpGeometry(Geo);
	}
}

void UM15_PositionAndPlacementModule::ApplyRotation(int32 DirectionIndex)
{
	if (!StateComponent) return;

	const float NewYaw = DirectionIndexToYaw(DirectionIndex);
	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();
	Geo.Rotation.Yaw = NewYaw;

	ApplyGeometryToState(Geo);
}

void UM15_PositionAndPlacementModule::ExecutePlace()
{
	OnPlaceRequested.Broadcast();

	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (UVoxelInteractionComponent* InteractionComp = PC->FindComponentByClass<UVoxelInteractionComponent>())
		{
			InteractionComp->RequestConfirmVoxelEdit();
		}
	}
}

// ==================== 静态工具方法 ====================

float UM15_PositionAndPlacementModule::DirectionIndexToYaw(int32 Index)
{
	Index = FMath::Clamp(Index, 0, 3);
	static const float YawValues[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
	return YawValues[Index];
}

int32 UM15_PositionAndPlacementModule::YawToDirectionIndex(float Yaw)
{
	Yaw = FMath::Fmod(Yaw, 360.0f);
	if (Yaw < 0.0f) Yaw += 360.0f;
	return FMath::RoundToInt(Yaw / 90.0f) % 4;
}
