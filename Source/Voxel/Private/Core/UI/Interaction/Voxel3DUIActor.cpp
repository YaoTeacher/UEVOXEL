// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/Interaction/Voxel3DUIActor.h"
#include "Voxel/VoxelPlayerController.h"
#include "Utils/ChunkUtilsFunction.h"
#include "Components/TextRenderComponent.h"

AVoxel3DUIActor::AVoxel3DUIActor()
{
	// 创建箭头Mesh
	UIMesh = CreateDefaultSubobject<UStaticMeshComponent>("Axis");
	UIMesh->SetCollisionProfileName("UI");
	UIMesh->SetMaterial(0,DefaultMetarial);
	RootComponent =UIMesh;

	// 文字标签（默认隐藏，业务按需 SetLabel）
	LabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	LabelComponent->SetupAttachment(RootComponent);
	LabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	LabelComponent->SetHorizontalAlignment(EHTA_Center);
	LabelComponent->SetTextRenderColor(FColor::White);
	LabelComponent->SetWorldSize(24.0f);
	LabelComponent->SetVisibility(false);

	// 启用点击事件
	UIMesh->SetGenerateOverlapEvents(false);
}

void AVoxel3DUIActor::BeginPlay()
{
	Super::BeginPlay();

	// 设置为可点击
	UIMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AVoxel3DUIActor::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	UE_LOG(LogTemp, VeryVerbose, TEXT("AVoxel3DUIActor::NotifyActorOnClicked: %s"), *GetName());
}

void AVoxel3DUIActor::NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	Super::NotifyActorOnInputTouchBegin(FingerIndex);

	UE_LOG(LogTemp, VeryVerbose, TEXT("AVoxel3DUIActor::NotifyActorOnInputTouchBegin: %s"), *GetName());
}

void AVoxel3DUIActor::SetOwningController(AVoxelPlayerController* Controller)
{
	OwningController = Controller;
}

void AVoxel3DUIActor::IsSelected(bool input)
{
	if(input)
	{
		UIMesh->SetMaterial(0,SelectedMetarial);
	}
	else
	{
		UIMesh->SetMaterial(0,DefaultMetarial);
	}
}

void AVoxel3DUIActor::SetDir(EActorDirection input)
{
	Direction = input;
	UIMesh->SetRelativeRotation(UChunkUtilsFunction::GetSpawnDirection(Direction));
}

void AVoxel3DUIActor::SetLabel(const FText& Text)
{
	if (!LabelComponent)
	{
		return;
	}

	const bool bHasText = !Text.IsEmpty();
	LabelComponent->SetText(Text);
	LabelComponent->SetVisibility(bHasText);
}

void AVoxel3DUIActor::SetLabelColor(const FLinearColor& Color)
{
	if (LabelComponent)
	{
		LabelComponent->SetTextRenderColor(Color.ToFColor(true));
	}
}
