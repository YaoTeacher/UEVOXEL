// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/VoxelControllerPawn.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Voxel/VoxelPlayerController.h"
#include "VoxelGameInstance.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Sets default values
AVoxelControllerPawn::AVoxelControllerPawn()
{
	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when character does
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level
	CameraBoom->TargetArmLength = 1500.f; // 默认臂长
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 默认俯视角

	// Create a camera...
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// 挂载选中描边后处理材质（CustomDepth + Stencil → 白色描边）
	// 材质采样 CustomStencil 缓冲做 Roberts cross 边缘检测，选中 Actor 边缘描白色
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OutlineMaterialFinder(
		TEXT("/Game/Resource/Materials/M_PostProcess_Outline.M_PostProcess_Outline"));
	if (OutlineMaterialFinder.Succeeded())
	{
		TopDownCameraComponent->PostProcessSettings.WeightedBlendables.Array.Add(
			FWeightedBlendable(1.0f, OutlineMaterialFinder.Object));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VoxelControllerPawn] M_PostProcess_Outline not found at /Game/Resource/Materials/M_PostProcess_Outline"));
	}

 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AVoxelControllerPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

void AVoxelControllerPawn::MoveForward(float Value)
{
	if ( Value != 0.0f)
	{
		// 基于相机方向计算前进向量
		const FRotator YawRotation(0, CameraBoom->GetComponentRotation().Yaw, 0);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AVoxelControllerPawn::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// 基于相机方向计算右方向量
		const FRotator YawRotation(0, CameraBoom->GetComponentRotation().Yaw, 0);
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void AVoxelControllerPawn::MoveUp_World(float Val)
{
	if (Val != 0.f)
	{
		// 基于体素高度上下移动
		float BlockLength = 20.0f;
		if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
		{
			BlockLength = GI->GetBlockLength();
		}

		FVector NewLocation = GetActorLocation() + FVector(0.f, 0.f, Val * BlockLength);
		SetActorLocation(NewLocation);
	}
}

void AVoxelControllerPawn::CameraTurnLeft(bool IsTurnLeft)
{
	if (!IsValid(CameraBoom))
	{
		UE_LOG(LogTemp, Error, TEXT("CameraBoom component not found on pawn!"));
		return;
	}

	// 3. 配置旋转参数
	const float RotationAmount = 90.0f; // 可考虑设为UPROPERTY可配置变量
	const float YawRotation = IsTurnLeft ? RotationAmount : -RotationAmount;
    
	// 4. 应用平滑旋转
	const FRotator NewRotation = FRotator(0, YawRotation, 0);
	CameraBoom->AddRelativeRotation(NewRotation);
	AVoxelPlayerController* a =Cast<AVoxelPlayerController>(GetController());
	// 5. 更新碰撞可视化（如果需要）
		//a->InteractionComp->();
	
}

void AVoxelControllerPawn::Move(FVector Val)
{

}

void AVoxelControllerPawn::RotateCameraYaw(float Value)
{
	if (!CameraBoom || Value == 0.f) return;

	// Q/E 步幅 90 度旋转
	FRotator CurrentRot = CameraBoom->GetRelativeRotation();
	CurrentRot.Yaw += (Value > 0.f) ? 90.f : -90.f;
	CameraBoom->SetRelativeRotation(CurrentRot);
}

void AVoxelControllerPawn::RotateCameraPitch(float Value)
{
	if (!CameraBoom || Value == 0.f) return;

	FRotator CurrentRot = CameraBoom->GetRelativeRotation();
	// 步幅 15 度，限制 Pitch 在 -85 到 -10 之间（俯视角范围）
	float Step = (Value > 0.f) ? 15.f : -15.f;
	CurrentRot.Pitch = FMath::Clamp(CurrentRot.Pitch + Step, -85.f, -10.f);
	CameraBoom->SetRelativeRotation(CurrentRot);
}

void AVoxelControllerPawn::AdjustCameraDistance(float Value)
{
	if (!CameraBoom || Value == 0.f) return;

	// 滚轮方向反转，步幅 ×20，限制在 300 到 5000 之间
	float NewLength = CameraBoom->TargetArmLength - Value * 20.f;
	CameraBoom->TargetArmLength = FMath::Clamp(NewLength, 300.f, 5000.f);
}

bool AVoxelControllerPawn::SnapToGround()
{
	if (!GetWorld()) return false;

	// 从 Pawn 上方一段向下射线，找正下方最近地面（体素地形，ECC_GameTraceChannel1）
	const FVector Origin = GetActorLocation();
	const FVector Start = Origin + FVector(0.f, 0.f, 5000.f);
	const FVector End = Origin - FVector(0.f, 0.f, 5000.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_GameTraceChannel1, Params))
	{
		// 贴到地面表面（保留 X/Y，取命中点 Z）
		SetActorLocation(FVector(Origin.X, Origin.Y, Hit.Location.Z));
		return true;
	}
	return false;
}

void AVoxelControllerPawn::RequestSnapToGround()
{
	// 立即试一次；失败则启动重试定时器（地形流式加载未就绪）
	if (SnapToGround())
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		SnapToGroundTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (SnapToGround())
			{
				GetWorldTimerManager().ClearTimer(SnapToGroundTimerHandle);
				UE_LOG(LogTemp, Log, TEXT("[ControllerPawn] SnapToGround 成功贴地"));
			}
		}),
		0.2f, true);
}

void AVoxelControllerPawn::OrbitCamera(float YawDelta, float PitchDelta, const FVector& OrbitCenter)
{
	if (!CameraBoom) return;
	if (YawDelta == 0.f && PitchDelta == 0.f) return;

	// 鼠标中键拖拽：绕 Pawn 自身位置（=相机注视点/视觉中心，WASD/ZX 移动的那个点）旋转。
	// 相机挂在弹簧臂上且始终回望 Pawn，转 Boom 即绕 Pawn 公转，Pawn 不动、钉在屏幕中心。
	// 与 Q/E（RotateCameraYaw）、R/F（RotateCameraPitch）的 SetRelativeRotation 机制一致。
	// 注意：绝不平移 Pawn--平移 Pawn 会同时移动视觉中心与 chunk 流式圆心，导致乱动+世界刷新。
	constexpr float Sensitivity = 0.3f;

	FRotator NewRelRot = CameraBoom->GetRelativeRotation();
	NewRelRot.Yaw += YawDelta * Sensitivity;
	NewRelRot.Pitch = FMath::Clamp(NewRelRot.Pitch + PitchDelta * Sensitivity, -85.f, -10.f);
	CameraBoom->SetRelativeRotation(NewRelRot);
}

// Called every frame
void AVoxelControllerPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AVoxelControllerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{

	// if (const ULocalPlayer* LocalPlayer = Cast<AVoxelPlayerController>(GetController())->GetLocalPlayer())
	// {
	// 	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	// 	{
	// 		// 移除可能存在的旧上下文
	// 		Subsystem->RemoveMappingContext(DefaultMappingContext);
 //            
	// 		// 添加新上下文
	// 		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	// 	}
	// }
}

