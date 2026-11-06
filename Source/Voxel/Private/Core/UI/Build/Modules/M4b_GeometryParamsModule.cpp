// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Build/Modules/M4b_GeometryParamsModule.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/CoreTags.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/VoxelGameInstance.h"
#include "Utils/ChunkUtilsFunction.h"

bool UM4b_GeometryParamsModule::ShouldShowForTool(FGameplayTag ToolTag) const
{
	// 显示条件: 简单几何选区工具或简单几何编辑工具
	return ToolTag == FCoreTags::Tool_Selection_SimpleGeometry ||
	       ToolTag == FCoreTags::Tool_Edit_SimpleGeometry;
}

void UM4b_GeometryParamsModule::UpdateFromToolState(UVoxelPlayerStateCacheComponent* SC)
{
	if (!SC)
	{
		return;
	}

	StateComponent = SC;

	if (!bDelegatesBound)
	{
		SC->BindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM4b_GeometryParamsModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = true;
	}

	RefreshFromGeometry();
}

void UM4b_GeometryParamsModule::RefreshFromGeometry()
{
	if (!StateComponent) return;
	FVoxelGeometryDesc Geometry = StateComponent->GetTmpGeometry();

	// 更新旋转滑块和文本显示
	if (RotationPitchSlider && RotationPitchText)
	{
		RotationPitchSlider->SetValue(Geometry.Rotation.Pitch);
		RotationPitchText->SetText(FText::AsNumber(Geometry.Rotation.Pitch));
	}

	if (RotationYawSlider && RotationYawText)
	{
		RotationYawSlider->SetValue(Geometry.Rotation.Yaw);
		RotationYawText->SetText(FText::AsNumber(Geometry.Rotation.Yaw));
	}

	if (RotationRollSlider && RotationRollText)
	{
		RotationRollSlider->SetValue(Geometry.Rotation.Roll);
		RotationRollText->SetText(FText::AsNumber(Geometry.Rotation.Roll));
	}

	// 更新厚度滑条和文本显示
	if (ThicknessInput)
	{
		ThicknessInput->SetValue(Geometry.Thickness);
	}
	if (ThicknessValueText)
	{
		ThicknessValueText->SetText(FText::AsNumber(Geometry.Thickness));
	}

	// 更新正方体模式复选框
	if (RegularModeCheckBox)
	{
		RegularModeCheckBox->SetCheckedState(Geometry.bIsRegular ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}

	// 同步变换原点 UI
	if (PivotModeComboBox)
	{
		FString ModeStr = (Geometry.PivotMode == EVoxelPivotMode::Cursor)
			? TEXT("游标") : TEXT("物体中心");
		PivotModeComboBox->SetSelectedOption(ModeStr);
	}

	if (TransformSpaceComboBox)
	{
		FString SpaceStr = (Geometry.TransformSpace == EVoxelTransformSpace::Local)
			? TEXT("局部轴") : TEXT("绝对坐标系");
		TransformSpaceComboBox->SetSelectedOption(SpaceStr);
	}

	if (PivotCoordText)
	{
		const FVector& PivotPos = Geometry.PivotWorld;
		PivotCoordText->SetText(FText::FromString(
			FString::Printf(TEXT("X:%.0f Y:%.0f Z:%.0f"),
				PivotPos.X, PivotPos.Y, PivotPos.Z)));
	}

	// 壳体面：回读勾选状态（守卫防回写触发推送）+ 仅壳模式（Thickness != 0）可用
	bIsUpdatingShellFaces = true;
	auto SetFaceChecked = [](UCheckBox* Box, const TSet<EActorDirection>& Faces, EActorDirection Dir)
	{
		if (Box)
		{
			Box->SetCheckedState(Faces.Contains(Dir) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		}
	};
	SetFaceChecked(ShellFaceUpCheckBox, Geometry.ShellFaces, EActorDirection::Up);
	SetFaceChecked(ShellFaceDownCheckBox, Geometry.ShellFaces, EActorDirection::Down);
	SetFaceChecked(ShellFaceLeftCheckBox, Geometry.ShellFaces, EActorDirection::Left);
	SetFaceChecked(ShellFaceRightCheckBox, Geometry.ShellFaces, EActorDirection::Right);
	SetFaceChecked(ShellFaceFrontCheckBox, Geometry.ShellFaces, EActorDirection::Forward);
	SetFaceChecked(ShellFaceBackCheckBox, Geometry.ShellFaces, EActorDirection::Backward);
	bIsUpdatingShellFaces = false;

	if (ShellFacePanel)
	{
		ShellFacePanel->SetIsEnabled(Geometry.Thickness != 0);
	}
}

void UM4b_GeometryParamsModule::NativeDestruct()
{
	if (StateComponent && bDelegatesBound)
	{
		StateComponent->UnbindAllBuildEvents(this, GET_FUNCTION_NAME_CHECKED(UM4b_GeometryParamsModule, HandleBuildModeParamsUpdated));
		bDelegatesBound = false;
	}
	Super::NativeDestruct();
}

void UM4b_GeometryParamsModule::HandleBuildModeParamsUpdated()
{
	RefreshFromGeometry();
}

void UM4b_GeometryParamsModule::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (RotationPitchSlider)
		RotationPitchSlider->OnValueChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnRotationPitchChanged);
	if (RotationYawSlider)
		RotationYawSlider->OnValueChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnRotationYawChanged);
	if (RotationRollSlider)
		RotationRollSlider->OnValueChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnRotationRollChanged);
	if (ThicknessInput)
		ThicknessInput->OnValueChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnThicknessSliderChanged);
	if (RegularModeCheckBox)
		RegularModeCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnRegularModeCheckChanged);
	if (PivotModeComboBox)
		PivotModeComboBox->OnSelectionChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnPivotModeChanged);
	if (TransformSpaceComboBox)
		TransformSpaceComboBox->OnSelectionChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnTransformSpaceChanged);
	if (CursorToCenterButton)
		CursorToCenterButton->OnClicked.AddDynamic(this, &UM4b_GeometryParamsModule::OnCursorToCenterClicked);
	if (CenterToCursorButton)
		CenterToCursorButton->OnClicked.AddDynamic(this, &UM4b_GeometryParamsModule::OnCenterToCursorClicked);

	// 壳体面勾选（6 面共用一个回调，从控件状态全量收集）
	if (ShellFaceUpCheckBox)
		ShellFaceUpCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
	if (ShellFaceDownCheckBox)
		ShellFaceDownCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
	if (ShellFaceLeftCheckBox)
		ShellFaceLeftCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
	if (ShellFaceRightCheckBox)
		ShellFaceRightCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
	if (ShellFaceFrontCheckBox)
		ShellFaceFrontCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
	if (ShellFaceBackCheckBox)
		ShellFaceBackCheckBox->OnCheckStateChanged.AddDynamic(this, &UM4b_GeometryParamsModule::OnShellFaceChanged);
}

void UM4b_GeometryParamsModule::NativeConstruct()
{
	Super::NativeConstruct();
}

void UM4b_GeometryParamsModule::OnRotationPitchChanged(float Value)
{
	if (!StateComponent)
	{
		return;
	}

	// 从SC读当前旋转，修改后写回（显示由UpdateFromToolState驱动）
	FRotator NewRotation = StateComponent->GetTmpGeometry().Rotation;
	NewRotation.Pitch = Value;

	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetGeometryRotation(NewRotation);
		}
	}
}

void UM4b_GeometryParamsModule::OnRotationYawChanged(float Value)
{
	if (!StateComponent)
	{
		return;
	}

	FRotator NewRotation = StateComponent->GetTmpGeometry().Rotation;
	NewRotation.Yaw = Value;

	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetGeometryRotation(NewRotation);
		}
	}
}

void UM4b_GeometryParamsModule::OnRotationRollChanged(float Value)
{
	if (!StateComponent)
	{
		return;
	}

	FRotator NewRotation = StateComponent->GetTmpGeometry().Rotation;
	NewRotation.Roll = Value;

	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetGeometryRotation(NewRotation);
		}
	}
}

void UM4b_GeometryParamsModule::OnThicknessSliderChanged(float Value)
{
	const int32 NewThickness = FMath::RoundToInt(Value);

	// 通过 InteractComponent 设置厚度
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetGeometryThickness(NewThickness);
		}
	}
}

void UM4b_GeometryParamsModule::OnRegularModeCheckChanged(bool bChecked)
{
	// 通过 InteractComponent 设置正方体模式
	if (AVoxelPlayerController* PC = GetOwningPlayer<AVoxelPlayerController>())
	{
		if (PC->InteractionComp)
		{
			PC->InteractionComp->SetRegularGeometry(bChecked);
		}
	}
}

void UM4b_GeometryParamsModule::OnPivotModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!StateComponent) return;

	EVoxelPivotMode NewMode = (SelectedItem == TEXT("游标"))
		? EVoxelPivotMode::Cursor
		: EVoxelPivotMode::ObjectCenter;

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();
	Geo.PivotMode = NewMode;
	ApplyGeometryToState(Geo);
}

void UM4b_GeometryParamsModule::OnTransformSpaceChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!StateComponent) return;

	EVoxelTransformSpace NewSpace = (SelectedItem == TEXT("局部轴"))
		? EVoxelTransformSpace::Local
		: EVoxelTransformSpace::Global;

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();
	Geo.TransformSpace = NewSpace;
	ApplyGeometryToState(Geo);
}

void UM4b_GeometryParamsModule::OnCursorToCenterClicked()
{
	if (!StateComponent) return;

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	float BlockLength = 20.0f;
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		BlockLength = GI->GetBlockLength();
	}

	Geo.PivotWorld = Geo.GetGeometricCenter(BlockLength);
	ApplyGeometryToState(Geo);
}

void UM4b_GeometryParamsModule::OnCenterToCursorClicked()
{
	if (!StateComponent) return;

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();

	float BlockLength = 20.0f;
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		BlockLength = GI->GetBlockLength();
	}

	// 移动物体使几何中心 = 游标位置
	FVector CurrentCenter = Geo.GetGeometricCenter(BlockLength);
	FVector Delta = Geo.PivotWorld - CurrentCenter;
	FIntVector DeltaIndex = UChunkUtilsFunction::WorldDeltaToIndexDelta(Delta, BlockLength);
	Geo.Box.SetBoundingBox(Geo.Box.MinIndex + DeltaIndex, Geo.Box.MaxIndex + DeltaIndex);

	ApplyGeometryToState(Geo);
}

// ==================== 交互层统一出口 ====================

void UM4b_GeometryParamsModule::ApplyGeometryToState(const FVoxelGeometryDesc& Geo)
{
	if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
	{
		if (PC->InteractionComp) PC->InteractionComp->SetTmpGeometry(Geo);
	}
}

// ==================== 壳体面选择 ====================

void UM4b_GeometryParamsModule::OnShellFaceChanged(bool bChecked)
{
	if (bIsUpdatingShellFaces)
	{
		return;
	}
	PushShellFaces();
}

void UM4b_GeometryParamsModule::PushShellFaces()
{
	if (!StateComponent) return;

	auto IsChecked = [](const UCheckBox* Box)
	{
		return Box && Box->GetCheckedState() == ECheckBoxState::Checked;
	};

	FVoxelGeometryDesc Geo = StateComponent->GetTmpGeometry();
	Geo.ShellFaces.Reset();
	if (IsChecked(ShellFaceUpCheckBox))    Geo.ShellFaces.Add(EActorDirection::Up);
	if (IsChecked(ShellFaceDownCheckBox))  Geo.ShellFaces.Add(EActorDirection::Down);
	if (IsChecked(ShellFaceLeftCheckBox))  Geo.ShellFaces.Add(EActorDirection::Left);
	if (IsChecked(ShellFaceRightCheckBox)) Geo.ShellFaces.Add(EActorDirection::Right);
	if (IsChecked(ShellFaceFrontCheckBox)) Geo.ShellFaces.Add(EActorDirection::Forward);
	if (IsChecked(ShellFaceBackCheckBox))  Geo.ShellFaces.Add(EActorDirection::Backward);

	// 空集合 = 全部面（语义见 FVoxelGeometryDesc::ShellFaces 注释）
	ApplyGeometryToState(Geo);
}
