// Fill out your copyright notice in the Description page of Project Settings.


#include "BoxCoverageModifyWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Voxel/VoxelPlayerController.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"


FBox UBoxCoverageModifyWidget::GetCurBox()
{
	//UE_LOG(LogTemp, Warning, TEXT("CoreOffset = %s"), *CoreOffset.ToString())
	FBox box = FBox(BoxData.Min+FVector(CoreOffset),BoxData.Max+FVector(CoreOffset));
	return box;
}

void UBoxCoverageModifyWidget::UpdateBatchBlockInform()
{
	BoxData=UChunkUtilsFunction::CheckBox(BoxData);

	
	XMinText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Min.X+CoreOffset.X));
	YMinText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Min.Y+CoreOffset.Y));
	ZMinText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Min.Z+CoreOffset.Z));
	XMaxText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Max.X+CoreOffset.X));
	YMaxText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Max.Y+CoreOffset.Y));
	ZMaxText->SetText(FText::Format(FText::FromString("{0}"), BoxData.Max.Z+CoreOffset.Z));

	CoreOffsetXText->SetText(FText::Format(FText::FromString("{0}"), CoreOffset.X));
	CoreOffsetYText->SetText(FText::Format(FText::FromString("{0}"), CoreOffset.Y));
	CoreOffsetZText->SetText(FText::Format(FText::FromString("{0}"), CoreOffset.Z));
	
}

void UBoxCoverageModifyWidget::ResetCoreOffset()
{
	CoreOffset = FIntVector(0);

}

void UBoxCoverageModifyWidget::ChangeCurCoverageByClick(bool IsMin, FVector Input)
{
	if(IsMin==true)
	{
		BoxData.Min+=Input*FVector(MultiNum);
	}
	else
	{
		BoxData.Max+=Input*FVector(MultiNum);
	}
	//UE_LOG(LogTemp, Warning, TEXT("BoxData: %s"), *BoxData.ToString());
	UpdateBatchBlockInform();
}

void UBoxCoverageModifyWidget::ChangeCurCoreByClick(FIntVector Input)
{
	CoreOffset += Input;

	if (StateComponent)
	{
		FVoxelGeometryDesc Geometry = StateComponent->GetTmpGeometry();
		Geometry.Box.UpdateOffset(Input);
		// 经 InteractionComp 业务入口写回（触发广播），UI 不直改 SC
		if (AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetOwningPlayer()))
		{
			if (PC->InteractionComp) PC->InteractionComp->SetTmpGeometry(Geometry);
		}
	}

	UpdateBatchBlockInform();
}

void UBoxCoverageModifyWidget::ChangeCurCoverageByBox(FAreaCoverageFragment C)
{
	
		BoxData=C.BoundingBox();
		CoreOffset=C.CoreOffsetIndex;
		//UE_LOG(LogTemp, Warning, TEXT("AreaCoverage %s"),*C.BoundingBox().ToString());
		UpdateBatchBlockInform();
	
	
}

void UBoxCoverageModifyWidget::ChangeMulNum(int ChangeIndex)
{
	if(ChangeIndex<0||ChangeIndex>2)
	{
		UE_LOG(LogTemp, Error, TEXT("Error Index"));
		return;
	}
	int i=MultiNum[ChangeIndex];
	switch (i)
	{
		case 1:
			MultiNum[ChangeIndex]=2;
			break;
	case 2:
		MultiNum[ChangeIndex]=5;
		break;
	case 5:
		MultiNum[ChangeIndex]=10;
		break;
	case 10:
		MultiNum[ChangeIndex]=1;
		break;
	default:
			break;
	}

	XMultiText->SetText(FText::Format(FText::FromString("{0}"), MultiNum.X));
	YMultiText->SetText(FText::Format(FText::FromString("{0}"), MultiNum.Y));
	ZMultiText->SetText(FText::Format(FText::FromString("{0}"), MultiNum.Z));
}


void UBoxCoverageModifyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UBoxCoverageModifyWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AVoxelPlayerController* VoxelPC = Cast<AVoxelPlayerController>(PC))
		{
			StateComponent = VoxelPC->StateCacheComp;
		}
	}
}
