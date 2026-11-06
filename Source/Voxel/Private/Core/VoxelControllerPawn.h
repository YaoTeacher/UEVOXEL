// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/Pawn.h"
#include "VoxelControllerPawn.generated.h"

class UInputMappingContext;
class UInputAction;
class USpringArmComponent;

UCLASS()
class AVoxelControllerPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AVoxelControllerPawn();
	
	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* ClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* CameraControllAction;

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

 
	virtual void MoveForward(float Value) override;

	virtual void MoveRight(float Value) override;

	virtual void MoveUp_World(float Val) override;
	UFUNCTION(BlueprintCallable, Category="Pawn")
	void CameraTurnLeft(bool IsTurnLeft);
	UFUNCTION(BlueprintCallable, Category="Pawn")
	void Move(FVector Val);

	/** 直接旋转 CameraBoom 的 Yaw（Q/E） */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void RotateCameraYaw(float Value);

	/** 直接旋转 CameraBoom 的 Pitch（R/F） */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void RotateCameraPitch(float Value);

	/** 调整 CameraBoom 的臂长（滚轮缩放） */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void AdjustCameraDistance(float Value);

	/**
	 * 把 Pawn 的 Z 贴到正下方最近地面（体素地形，ECC_GameTraceChannel1）。
	 * X/Y 不变。地形未加载时会返回 false。
	 * @return true=成功贴地
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	bool SnapToGround();

	/**
	 * 请求贴地：以定时器重试 SnapToGround 直到成功或超时。
	 * 用于新游戏生成时地形尚未就绪的情况。读档不应调用（用存档位置）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void RequestSnapToGround();

	/**
	 * 按住鼠标中键拖拽旋转相机（绕视野中心=Pawn 当前位置公转）
	 * @param YawDelta 鼠标 X 增量
	 * @param PitchDelta 鼠标 Y 增量
	 * @param OrbitCenter 已废弃（绕 Pawn 自身转，忽略此参数）
	 * @note 只旋转弹簧臂（SetRelativeRotation），不平移 Pawn。
	 *       相机挂在臂上且始终回望 Pawn，转臂即绕 Pawn 公转，Pawn 钉在屏幕中心。
	 *       平移 Pawn 会同时移动视觉中心与 chunk 流式圆心，禁止。
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	void OrbitCamera(float YawDelta, float PitchDelta, const FVector& OrbitCenter = FVector::ZeroVector);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Camera, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** SnapToGround 重试定时器句柄（新游戏贴地用） */
	FTimerHandle SnapToGroundTimerHandle;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

};
