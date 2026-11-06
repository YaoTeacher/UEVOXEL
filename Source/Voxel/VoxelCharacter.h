// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Creature/CreatureBase.h"
#include "Private/Item/Interfaces/InteractionItemInterface.h"
#include "GameFramework/Character.h"
#include "VoxelCharacter.generated.h"


class AVoxelGameMode;

USTRUCT(Blueprintable)
struct FInteractionData 
{
	GENERATED_USTRUCT_BODY();

	

	FInteractionData() :
		CurrentInteractable(nullptr),
		LastInteractionCheckTime(0.0f)
	{
	};

	UPROPERTY()
	AActor* CurrentInteractable;

	UPROPERTY()
	float LastInteractionCheckTime;
};

UCLASS(Blueprintable)
class AVoxelCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVoxelCharacter();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	FTrueCreatureData InformBase;
	// Called every frame.
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	/** Returns TopDownCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	FTimerHandle TimeHandle_Interaction;
	
	FInteractionData InteractionData;

	
	

protected:
	
private:
	/** Top down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	 UCameraComponent* TopDownCameraComponent;

	/** Camera boom positioning the camera above the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	 USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "ControlledPawn | Interaction")
	TScriptInterface<IInteractionItemInterface> TargetInteractable;
};

