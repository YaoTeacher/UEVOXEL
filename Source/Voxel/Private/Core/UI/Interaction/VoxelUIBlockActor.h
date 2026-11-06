// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Enum/GameStateEnum.h"
#include "GameFramework/Actor.h"
#include "VoxelUIBlockActor.generated.h"

UCLASS()
class AVoxelUIBlockActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AVoxelUIBlockActor();

	// 【优化】添加根组件，解耦实体和线框的变换
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot; 

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	UStaticMeshComponent* WireFrameMeshComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Components")
	EUIStateEnum UIState = EUIStateEnum::Normal;
	
	UPROPERTY(EditDefaultsOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> NorMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> SelecMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> ErrorMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Material")
	UStaticMesh* WireFrameCube;
	
	UPROPERTY(EditDefaultsOnly, Category = "Material")
	UStaticMesh* Cube;
	UPROPERTY(EditDefaultsOnly, Category = "Material")
	UStaticMesh* Tube;
	UPROPERTY(EditDefaultsOnly, Category = "Material")
	UStaticMesh* Sphere;
	
	UFUNCTION(BlueprintCallable, Category = "Components")
	void UpdateAreaByBox(EBatchShapeType InputShape, FBox Bound, float BlockLength);

	UFUNCTION(BlueprintCallable, Category = "Components")
	void UpdateState(EUIStateEnum Input);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	const float BaseMeshSize = 100.0f;

public:	

};
