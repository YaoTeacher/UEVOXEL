// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/NPC/VoxelIntelligentCreatureInstance.h"

#include "AIController.h"
#include "ConsumComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/VoxelGameInstance.h"
#include "GameManager/WorldManager/VoxelCreatureWorldSubsystem.h"

// Sets default values
AVoxelIntelligentCreatureInstance::AVoxelIntelligentCreatureInstance()
{
	ConsumComponent = CreateDefaultSubobject<UConsumComponent>("consum");
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


}

void AVoxelIntelligentCreatureInstance::IntializeNpcSubData()
{
	if(ConsumComponent != nullptr)
	{
		ConsumComponent->ParentInstance = this;
		
		// // 确保 Widget 被初始化
		// if (StateInformWidget)
		// {
		// 	StateInformWidget->InitWidget();
		// }
		
		//ConsumComponent->InitializeCustomerTendency();
	}
}

void AVoxelIntelligentCreatureInstance::BeginPlay()
{
	Super::BeginPlay();
	// 注意：对象池复用时，BeginPlay 不会再次调用，所以逻辑要放在 ResetCreature 中
	ResetCreature();
}

void AVoxelIntelligentCreatureInstance::ResetCreature()
{
	// 1. 重置基础数据
	LiveTime = 0;
	
	// 2. 重置 AI 控制器状态
	AAIController* AIC = Cast<AAIController>(GetController());
	if (AIC)
	{
		AIC->StopMovement();
		// 如果有行为树，在这里重启
		// AIC->RunBehaviorTree(...);
	}

	// 3. 启动生命周期计时器
	GetWorldTimerManager().SetTimer(LiveTimeTimerHandle, this, &AVoxelIntelligentCreatureInstance::UpdateLiveTime, 1.0f, true);

	// 4. 初始化业务数据
	IntializeNpcSubData();
}

void AVoxelIntelligentCreatureInstance::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVoxelIntelligentCreatureInstance::UpdateLiveTime()
{
	LiveTime++;
	if (LiveTime > MaxLiveTime)
	{
		UE_LOG(LogTemp, Log, TEXT("Lifetime expired: %s"), *CreatureData.RealName);
		RequestDespawn();
	}
}

void AVoxelIntelligentCreatureInstance::RequestDespawn()
{
	// 清除计时器
	GetWorldTimerManager().ClearTimer(LiveTimeTimerHandle);

	// 通知 Subsystem 回收自己
	if (UWorld* World = GetWorld())
	{
		if (UVoxelCreatureWorldSubsystem* Subsystem = World->GetSubsystem<UVoxelCreatureWorldSubsystem>())
		{
			Subsystem->RemoveCreature(CreatureData.CreatureID);
			return; // 成功移交给 Subsystem 处理
		}
	}

	// 兜底：如果没有 Subsystem，则销毁
	Destroy(); 
}

// Called to bind functionality to input
void AVoxelIntelligentCreatureInstance::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

