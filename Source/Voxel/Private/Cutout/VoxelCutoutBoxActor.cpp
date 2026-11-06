#include "Cutout/VoxelCutoutBoxActor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Object.h"

AVoxelCutoutBoxActor::AVoxelCutoutBoxActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 默认盒体网格（引擎 Cube），可在 BP 覆盖
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeFinder.Succeeded())
	{
		BoxMesh = CubeFinder.Object;
	}

	// 线框材质：复用项目现有 Mt_WireFrame（BLEND_Translucent + Color 参数）
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireframeMatFinder(TEXT("/Game/Resource/UI/UITexture/Mt_WireFrame.Mt_WireFrame"));
	if (WireframeMatFinder.Succeeded())
	{
		WireframeMaterial = WireframeMatFinder.Object;
	}

	// 填充材质：复用项目现有区域透明填充 WorldTempTextureArray_AreaVisual（AreaColor 参数）
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AreaMatFinder(TEXT("/Game/Resource/Materials/WorldTempTextureArray_AreaVisual.WorldTempTextureArray_AreaVisual"));
	if (AreaMatFinder.Succeeded())
	{
		FillMaterial = AreaMatFinder.Object;
	}

	WireframeBox = CreateDefaultSubobject<UStaticMeshComponent>("WireframeBox");
	// 必须显式设根组件，否则变换不传播（LogActor 警告 + 大小位置错乱）
	SetRootComponent(WireframeBox);
	WireframeBox->SetCastShadow(false);
	WireframeBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WireframeBox->SetCanEverAffectNavigation(false);
	WireframeBox->SetHiddenInGame(false);
	if (BoxMesh) WireframeBox->SetStaticMesh(BoxMesh);

	FillBox = CreateDefaultSubobject<UStaticMeshComponent>("FillBox");
	FillBox->SetupAttachment(WireframeBox);  // 挂到根，随根变换
	FillBox->SetCastShadow(false);
	FillBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 默认非编辑模式无碰撞
	FillBox->SetCanEverAffectNavigation(false);
	FillBox->SetHiddenInGame(true);  // 默认非编辑模式隐藏
	if (BoxMesh) FillBox->SetStaticMesh(BoxMesh);

	// FillBox 碰撞：仅查询，挡 Cutout 通道(ECC_GameTraceChannel8)，忽略其余全部
	FillBox->SetCollisionObjectType(ECC_WorldStatic);
	FillBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	FillBox->SetCollisionResponseToChannel(ECC_GameTraceChannel8, ECR_Block);
}

void AVoxelCutoutBoxActor::UpdateBounds(const FIntVector& MinIndex, const FIntVector& MaxIndex, float BlockLength)
{
	const FVector Min(MinIndex);
	const FVector Max(MaxIndex);
	const FVector Center = (Min + Max + FVector::OneVector) * 0.5f * BlockLength;
	const FVector Dimensions = (Max - Min + FVector::OneVector) * BlockLength;

	SetActorLocation(Center);
	SetActorRotation(FRotator::ZeroRotator);

	// 静态网格 Cube 默认 100x100x100，缩放到目标尺寸
	const FVector Scale = Dimensions / 100.0f;
	if (WireframeBox) WireframeBox->SetWorldScale3D(Scale);
	if (FillBox) FillBox->SetWorldScale3D(Scale);
}

void AVoxelCutoutBoxActor::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	// 生效=白/亮（低不透明度）；失效=灰/暗
	// 线框材质 Mt_WireFrame 参数：Color (VectorParameter) + Brightness (ScalarParameter)
	const FLinearColor WireColor = bEnabled
		? FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)
		: FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
	const float WireBrightness = bEnabled ? 3.0f : 1.0f;

	// 填充材质 AreaVisual 参数：AreaColor (VectorParameter, alpha=不透明度)
	const FLinearColor FillColor = bEnabled
		? FLinearColor(1.0f, 1.0f, 1.0f, 0.05f)
		: FLinearColor(0.5f, 0.5f, 0.5f, 0.03f);

	if (UMaterialInstanceDynamic* MID = GetOrCreateWireframeMID())
	{
		MID->SetVectorParameterValue(TEXT("Color"), WireColor);
		MID->SetScalarParameterValue(TEXT("Brightness"), WireBrightness);
	}
	if (UMaterialInstanceDynamic* MID = GetOrCreateFillMID())
	{
		MID->SetVectorParameterValue(TEXT("AreaColor"), FillColor);
	}
}

void AVoxelCutoutBoxActor::SetEditMode(bool bInEditMode)
{
	bEditMode = bInEditMode;
	if (FillBox)
	{
		FillBox->SetHiddenInGame(!bEditMode);
		FillBox->SetCollisionEnabled(bEditMode ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	// WireframeBox 始终显示
}

UMaterialInstanceDynamic* AVoxelCutoutBoxActor::GetOrCreateWireframeMID()
{
	if (WireframeMID) return WireframeMID;
	if (!WireframeBox) return nullptr;
	// 惰性加载兜底：Mt_WireFrame
	if (!WireframeMaterial)
	{
		WireframeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/UI/UITexture/Mt_WireFrame.Mt_WireFrame"));
	}
	if (!WireframeMaterial) return nullptr;
	WireframeMID = UMaterialInstanceDynamic::Create(WireframeMaterial, this);
	if (WireframeMID) WireframeBox->SetMaterial(0, WireframeMID);
	return WireframeMID;
}

UMaterialInstanceDynamic* AVoxelCutoutBoxActor::GetOrCreateFillMID()
{
	if (FillMID) return FillMID;
	if (!FillBox) return nullptr;
	// 惰性加载兜底：WorldTempTextureArray_AreaVisual
	if (!FillMaterial)
	{
		FillMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Materials/WorldTempTextureArray_AreaVisual.WorldTempTextureArray_AreaVisual"));
	}
	if (!FillMaterial) return nullptr;
	FillMID = UMaterialInstanceDynamic::Create(FillMaterial, this);
	if (FillMID) FillBox->SetMaterial(0, FillMID);
	return FillMID;
}
