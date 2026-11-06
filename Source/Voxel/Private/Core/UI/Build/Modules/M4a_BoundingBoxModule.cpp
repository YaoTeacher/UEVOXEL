#include "Core/UI/Build/Modules/M4a_BoundingBoxModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Voxel/Editor/VoxelEditorData.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"

#define BIND_BUTTON(btn, func) \
	if (btn) \
	{ \
		btn->OnClicked.RemoveDynamic(this, &UM4a_BoundingBoxModule::func); \
		btn->OnClicked.AddDynamic(this, &UM4a_BoundingBoxModule::func); \
	}

#define BIND_TEXT(box, func) \
	if (box) \
	{ \
		box->OnTextCommitted.RemoveDynamic(this, &UM4a_BoundingBoxModule::func); \
		box->OnTextCommitted.AddDynamic(this, &UM4a_BoundingBoxModule::func); \
	}

bool UM4a_BoundingBoxModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	return ToolTag == FCoreTags::Tool_Selection_SimpleGeometry ||
	       ToolTag == FCoreTags::Tool_Edit_SimpleGeometry ||
	       ToolTag.MatchesTag(FCoreTags::Tool_AreaEdit) ||
	       ToolTag == FCoreTags::Tool_Cutout;
}

// ==================== 显示层 ====================

void UM4a_BoundingBoxModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC) return;

	StateComponent = SC;

	// 订阅状态广播：拖拽改变包围盒时实时刷新 UI
	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM4a_BoundingBoxModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	const FVoxelGeometryDesc& Geometry = SC->GetTmpGeometry();
	const FIntVector& MinIndex = Geometry.Box.MinIndex;
	const FIntVector& MaxIndex = Geometry.Box.MaxIndex;
	const FIntVector& Offset = Geometry.Box.CoreOffsetIndex;

	// 更新坐标输入框（显示 = 索引 + 偏移）
	if (Input_MinX) Input_MinX->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.X + Offset.X)));
	if (Input_MinY) Input_MinY->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.Y + Offset.Y)));
	if (Input_MinZ) Input_MinZ->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.Z + Offset.Z)));
	if (Input_MaxX) Input_MaxX->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.X + Offset.X)));
	if (Input_MaxY) Input_MaxY->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.Y + Offset.Y)));
	if (Input_MaxZ) Input_MaxZ->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.Z + Offset.Z)));

	// 更新偏移输入框（显示 = 几何中心 + 偏移）
	const FIntVector Center = (MinIndex + MaxIndex) / 2;
	if (OffsetX_Value) OffsetX_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.X + Offset.X)));
	if (OffsetY_Value) OffsetY_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.Y + Offset.Y)));
	if (OffsetZ_Value) OffsetZ_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.Z + Offset.Z)));

	// 更新体积
	if (VolumeText)
	{
		const FIntVector Size = MaxIndex - MinIndex;
		const int64 Volume = static_cast<int64>(Size.X) * Size.Y * Size.Z;
		VolumeText->SetText(FText::FromString(FString::Printf(TEXT("%lld 体素"), Volume)));
	}
}

// ==================== 生命周期 ====================

void UM4a_BoundingBoxModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 文本提交（OnTextCommitted）
	BIND_TEXT(Input_MinX, OnMinXTextCommitted);
	BIND_TEXT(Input_MinY, OnMinYTextCommitted);
	BIND_TEXT(Input_MinZ, OnMinZTextCommitted);
	BIND_TEXT(Input_MaxX, OnMaxXTextCommitted);
	BIND_TEXT(Input_MaxY, OnMaxYTextCommitted);
	BIND_TEXT(Input_MaxZ, OnMaxZTextCommitted);
	BIND_TEXT(OffsetX_Value, OnCoreXTextCommitted);
	BIND_TEXT(OffsetY_Value, OnCoreYTextCommitted);
	BIND_TEXT(OffsetZ_Value, OnCoreZTextCommitted);

	// 增减按钮（OnClicked）
	BIND_BUTTON(Btn_MinX_Dec, OnBtn_MinX_Dec);
	BIND_BUTTON(Btn_MinX_Inc, OnBtn_MinX_Inc);
	BIND_BUTTON(Btn_MinY_Dec, OnBtn_MinY_Dec);
	BIND_BUTTON(Btn_MinY_Inc, OnBtn_MinY_Inc);
	BIND_BUTTON(Btn_MinZ_Dec, OnBtn_MinZ_Dec);
	BIND_BUTTON(Btn_MinZ_Inc, OnBtn_MinZ_Inc);
	BIND_BUTTON(Btn_MaxX_Dec, OnBtn_MaxX_Dec);
	BIND_BUTTON(Btn_MaxX_Inc, OnBtn_MaxX_Inc);
	BIND_BUTTON(Btn_MaxY_Dec, OnBtn_MaxY_Dec);
	BIND_BUTTON(Btn_MaxY_Inc, OnBtn_MaxY_Inc);
	BIND_BUTTON(Btn_MaxZ_Dec, OnBtn_MaxZ_Dec);
	BIND_BUTTON(Btn_MaxZ_Inc, OnBtn_MaxZ_Inc);
	BIND_BUTTON(Btn_OffsetX_Dec, OnBtn_OffsetX_Dec);
	BIND_BUTTON(Btn_OffsetX_Inc, OnBtn_OffsetX_Inc);
	BIND_BUTTON(Btn_OffsetY_Dec, OnBtn_OffsetY_Dec);
	BIND_BUTTON(Btn_OffsetY_Inc, OnBtn_OffsetY_Inc);
	BIND_BUTTON(Btn_OffsetZ_Dec, OnBtn_OffsetZ_Dec);
	BIND_BUTTON(Btn_OffsetZ_Inc, OnBtn_OffsetZ_Inc);

	// 步长下拉框
	if (StepSizeComboBox)
	{
		StepSizeComboBox->OnSelectionChanged.RemoveDynamic(this, &UM4a_BoundingBoxModule::OnStepSizeChanged);
		StepSizeComboBox->OnSelectionChanged.AddDynamic(this, &UM4a_BoundingBoxModule::OnStepSizeChanged);
	}
}

void UM4a_BoundingBoxModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM4a_BoundingBoxModule::NativeDestruct()
{
	// 解绑广播，防止悬空回调
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM4a_BoundingBoxModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM4a_BoundingBoxModule::HandleBuildModeParamsUpdated()
{
	// TmpGeometry 变化时重新读取并刷新 UI
	if (StateComponent)
	{
		const FVoxelGeometryDesc& Geometry = StateComponent->GetTmpGeometry();
		const FIntVector& MinIndex = Geometry.Box.MinIndex;
		const FIntVector& MaxIndex = Geometry.Box.MaxIndex;
		const FIntVector& Offset = Geometry.Box.CoreOffsetIndex;

		if (Input_MinX) Input_MinX->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.X + Offset.X)));
		if (Input_MinY) Input_MinY->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.Y + Offset.Y)));
		if (Input_MinZ) Input_MinZ->SetText(FText::FromString(FString::Printf(TEXT("%d"), MinIndex.Z + Offset.Z)));
		if (Input_MaxX) Input_MaxX->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.X + Offset.X)));
		if (Input_MaxY) Input_MaxY->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.Y + Offset.Y)));
		if (Input_MaxZ) Input_MaxZ->SetText(FText::FromString(FString::Printf(TEXT("%d"), MaxIndex.Z + Offset.Z)));

		const FIntVector Center = (MinIndex + MaxIndex) / 2;
		if (OffsetX_Value) OffsetX_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.X + Offset.X)));
		if (OffsetY_Value) OffsetY_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.Y + Offset.Y)));
		if (OffsetZ_Value) OffsetZ_Value->SetText(FText::FromString(FString::Printf(TEXT("%d"), Center.Z + Offset.Z)));

		if (VolumeText)
		{
			const FIntVector Size = MaxIndex - MinIndex;
			const int64 Volume = static_cast<int64>(Size.X) * Size.Y * Size.Z;
			VolumeText->SetText(FText::FromString(FString::Printf(TEXT("%lld 体素"), Volume)));
		}
	}
}

// ==================== 交互层：按钮回调（只改SC，不改UI） ====================

void UM4a_BoundingBoxModule::OnBtn_MinX_Dec() { ApplyCoordinateDelta(true, 0, false); }
void UM4a_BoundingBoxModule::OnBtn_MinX_Inc() { ApplyCoordinateDelta(true, 0, true); }
void UM4a_BoundingBoxModule::OnBtn_MinY_Dec() { ApplyCoordinateDelta(true, 1, false); }
void UM4a_BoundingBoxModule::OnBtn_MinY_Inc() { ApplyCoordinateDelta(true, 1, true); }
void UM4a_BoundingBoxModule::OnBtn_MinZ_Dec() { ApplyCoordinateDelta(true, 2, false); }
void UM4a_BoundingBoxModule::OnBtn_MinZ_Inc() { ApplyCoordinateDelta(true, 2, true); }
void UM4a_BoundingBoxModule::OnBtn_MaxX_Dec() { ApplyCoordinateDelta(false, 0, false); }
void UM4a_BoundingBoxModule::OnBtn_MaxX_Inc() { ApplyCoordinateDelta(false, 0, true); }
void UM4a_BoundingBoxModule::OnBtn_MaxY_Dec() { ApplyCoordinateDelta(false, 1, false); }
void UM4a_BoundingBoxModule::OnBtn_MaxY_Inc() { ApplyCoordinateDelta(false, 1, true); }
void UM4a_BoundingBoxModule::OnBtn_MaxZ_Dec() { ApplyCoordinateDelta(false, 2, false); }
void UM4a_BoundingBoxModule::OnBtn_MaxZ_Inc() { ApplyCoordinateDelta(false, 2, true); }

void UM4a_BoundingBoxModule::OnBtn_OffsetX_Dec() { ApplyOffsetDelta(0, false); }
void UM4a_BoundingBoxModule::OnBtn_OffsetX_Inc() { ApplyOffsetDelta(0, true); }
void UM4a_BoundingBoxModule::OnBtn_OffsetY_Dec() { ApplyOffsetDelta(1, false); }
void UM4a_BoundingBoxModule::OnBtn_OffsetY_Inc() { ApplyOffsetDelta(1, true); }
void UM4a_BoundingBoxModule::OnBtn_OffsetZ_Dec() { ApplyOffsetDelta(2, false); }
void UM4a_BoundingBoxModule::OnBtn_OffsetZ_Inc() { ApplyOffsetDelta(2, true); }

// ==================== 交互层：文本提交（只改SC，不改UI） ====================

void UM4a_BoundingBoxModule::OnMinXTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(true, 0, Text); }
void UM4a_BoundingBoxModule::OnMinYTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(true, 1, Text); }
void UM4a_BoundingBoxModule::OnMinZTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(true, 2, Text); }
void UM4a_BoundingBoxModule::OnMaxXTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(false, 0, Text); }
void UM4a_BoundingBoxModule::OnMaxYTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(false, 1, Text); }
void UM4a_BoundingBoxModule::OnMaxZTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToCoord(false, 2, Text); }
void UM4a_BoundingBoxModule::OnCoreXTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToOffset(0, Text); }
void UM4a_BoundingBoxModule::OnCoreYTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToOffset(1, Text); }
void UM4a_BoundingBoxModule::OnCoreZTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{ ApplyTextToOffset(2, Text); }

// ==================== 交互层：步长切换（纯UI状态） ====================

void UM4a_BoundingBoxModule::OnStepSizeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectedItem == TEXT("1x"))      MultiNum = 1;
	else if (SelectedItem == TEXT("2x")) MultiNum = 2;
	else if (SelectedItem == TEXT("5x")) MultiNum = 5;
	else if (SelectedItem == TEXT("10x"))MultiNum = 10;
}

// ==================== 交互层：核心逻辑（从SC读→改→经ToolComp写回，触发广播） ====================

void UM4a_BoundingBoxModule::ApplyGeometryToState(const FVoxelGeometryDesc& Geo)
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp) PC->InteractionComp->SetTmpGeometry(Geo);
	}
}

void UM4a_BoundingBoxModule::ApplyCoordinateDelta(bool bIsMin, int32 Axis, bool bIncrease)
{
	if (!StateComponent) return;

	const int32 Delta = bIncrease ? MultiNum : -MultiNum;
	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	UE_LOG(LogTemp, Warning, TEXT("[M4a] BEFORE: Min=%s Max=%s Offset=%s | bIsMin=%d Axis=%d Delta=%d"),
		*Geo.Box.MinIndex.ToString(), *Geo.Box.MaxIndex.ToString(), *Geo.Box.CoreOffsetIndex.ToString(),
		bIsMin ? 1 : 0, Axis, Delta);

	if (bIsMin)
	{
		Geo.Box.MinIndex[Axis] += Delta;
	}
	else
	{
		Geo.Box.MaxIndex[Axis] += Delta;
	}

	UE_LOG(LogTemp, Warning, TEXT("[M4a] AFTER MOD: Min=%s Max=%s Offset=%s"),
		*Geo.Box.MinIndex.ToString(), *Geo.Box.MaxIndex.ToString(), *Geo.Box.CoreOffsetIndex.ToString());

	ApplyGeometryToState(Geo);

	// 广播后重新读，确认 CoreOffsetIndex 是否被改
	FVoxelGeometryDesc GeoAfter = StateComponent->GetTmpGeometry();
	UE_LOG(LogTemp, Warning, TEXT("[M4a] AFTER BROADCAST: Min=%s Max=%s Offset=%s"),
		*GeoAfter.Box.MinIndex.ToString(), *GeoAfter.Box.MaxIndex.ToString(), *GeoAfter.Box.CoreOffsetIndex.ToString());
}

void UM4a_BoundingBoxModule::ApplyOffsetDelta(int32 Axis, bool bIncrease)
{
	if (!StateComponent) return;

	const int32 Delta = bIncrease ? MultiNum : -MultiNum;
	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	Geo.Box.CoreOffsetIndex[Axis] += Delta;

	ApplyGeometryToState(Geo);
}

void UM4a_BoundingBoxModule::ApplyTextToCoord(bool bIsMin, int32 Axis, const FText& Text)
{
	if (!StateComponent) return;

	const FString Str = Text.ToString();
	if (Str.IsEmpty()) return;

	const int32 NewDisplayValue = FCString::Atoi(*Str);
	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	// 显示值 = 索引 + 偏移 → 索引 = 显示值 - 偏移
	const int32 NewIndex = NewDisplayValue - Geo.Box.CoreOffsetIndex[Axis];

	if (bIsMin)
	{
		Geo.Box.MinIndex[Axis] = NewIndex;
	}
	else
	{
		Geo.Box.MaxIndex[Axis] = NewIndex;
	}

	ApplyGeometryToState(Geo);
}

void UM4a_BoundingBoxModule::ApplyTextToOffset(int32 Axis, const FText& Text)
{
	if (!StateComponent) return;

	const FString Str = Text.ToString();
	if (Str.IsEmpty()) return;

	const int32 NewDisplayValue = FCString::Atoi(*Str);
	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	// 显示值 = 几何中心 + 偏移 → 偏移 = 显示值 - 几何中心
	const FIntVector Center = (Geo.Box.MinIndex + Geo.Box.MaxIndex) / 2;
	Geo.Box.CoreOffsetIndex[Axis] = NewDisplayValue - Center[Axis];

	ApplyGeometryToState(Geo);
}
