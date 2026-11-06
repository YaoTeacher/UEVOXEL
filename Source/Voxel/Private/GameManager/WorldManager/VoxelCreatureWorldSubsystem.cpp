// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager/WorldManager/VoxelCreatureWorldSubsystem.h"

#include "AIController.h"
#include "VoxelAreaWorldSubsystem.h"
#include "WorldObjectPoolSubsystem.h"
#include "AI/NPC/VoxelIntelligentCreatureInstance.h"
#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "Core/VoxelGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/CreatureGeneratorUtils.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "GameFramework/PlayerController.h"

// 静态成员初始化
TArray<EDemandEnum> UVoxelCreatureWorldSubsystem::DemandEnumlist;

void UVoxelCreatureWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// 初始化枚举缓存
	if (DemandEnumlist.Num() == 0)
	{
		InitializeDemandEnum();
	}

	// 注册对象池
	RegisterCreaturePool();
	UE_LOG(LogTemp, Log, TEXT("VoxelCreatureSubsystem Initialized with Object Pool"));
}

void UVoxelCreatureWorldSubsystem::AddCreature(FName CreatureId)
{
	
}

void UVoxelCreatureWorldSubsystem::Deinitialize()
{
	StopSpawnTimer();
	RemoveAllCreature();
	InScenceCreatureList.Empty();
	Super::Deinitialize();
}

void UVoxelCreatureWorldSubsystem::RegisterCreaturePool()
{
    UObjectPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
    if (!PoolSys) return;

    // ========================================================================
    // 1. 创建函数 (当池为空，或初始化预生成时调用)
    // ========================================================================
    auto CreateFunc = [this]() -> AVoxelNpcBaseInstance*
    {
       if (!CreatureClass) return nullptr;

       FActorSpawnParameters SpawnParams;
       SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
       
       // 生成在极远的位置
       FVector HiddenLoc(0, 0, -100000); 
       AVoxelNpcBaseInstance* NewNpc = GetWorld()->SpawnActor<AVoxelNpcBaseInstance>(CreatureClass, HiddenLoc, FRotator::ZeroRotator, SpawnParams);

       // [关键修复] 生成后必须立即"打晕"，防止它受重力影响下坠或跑AI
       if (NewNpc)
       {
           // A. 停止 AI 控制器
           if (AAIController* AIC = Cast<AAIController>(NewNpc->GetController()))
           {
               AIC->StopMovement();
           }

       		// [核心修复] 彻底冻结移动组件
       		if (UCharacterMovementComponent* MoveComp = NewNpc->GetCharacterMovement())
       		{
       			MoveComp->StopMovementImmediately();
       			MoveComp->SetMovementMode(MOVE_None); // 1. 模式设为 None
       			MoveComp->GravityScale = 0.0f;        // 2. [新增] 物理重力设为 0，防止漏网
       			MoveComp->Deactivate();               // 3. [新增] 彻底停用组件，防止它自己醒来
       			MoveComp->SetComponentTickEnabled(false);
       		}

       		// [新增] 确保根组件（胶囊体）不模拟物理
       		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(NewNpc->GetRootComponent()))
       		{
       			RootPrim->SetSimulatePhysics(false); // 确保物理引擎不接管
       			RootPrim->SetPhysicsLinearVelocity(FVector::ZeroVector);
       		}

       		NewNpc->SetActorHiddenInGame(true);
       		NewNpc->SetActorEnableCollision(false);
       		NewNpc->SetActorTickEnabled(false);
       }

       return NewNpc;
    };

    // ========================================================================
    // 2. 重置函数 (当回收对象 Release 时调用)
    // ========================================================================
    auto ResetFunc = [](AVoxelNpcBaseInstance* Npc)
    {
       if (!Npc) return;
       
       // A. 停止 AI
       if (AAIController* AIC = Cast<AAIController>(Npc->GetController()))
       {
          AIC->StopMovement();
       }

       // B. 关闭移动与物理
    	if (UCharacterMovementComponent* MoveComp = Npc->GetCharacterMovement())
    	{
    		MoveComp->StopMovementImmediately();
    		MoveComp->SetMovementMode(MOVE_None);
    		MoveComp->GravityScale = 0.0f;  // [新增]
    		MoveComp->Deactivate();         // [新增]
    		MoveComp->SetComponentTickEnabled(false);
    	}

       // C. 隐藏与归位
       Npc->SetActorHiddenInGame(true);
       Npc->SetActorEnableCollision(false);
       Npc->SetActorTickEnabled(false);
       Npc->SetActorLocation(FVector(0, 0, -100000));
    };

    // 注册名为 "NpcPool" 的池
    // CreatureNumLimit+2 是预留一点缓冲空间
    PoolSys->RegisterPool<AVoxelNpcBaseInstance>(FName("NpcPool"), CreateFunc, ResetFunc, CreatureNumLimit + 2);
}

void UVoxelCreatureWorldSubsystem::StartSpawnTimer()
{
	if (UWorld* World = GetWorld())
	{
		// 每 0.2 秒尝试生成一个，避免帧尖峰
		World->GetTimerManager().SetTimer(SpawnTimerHandle, this, &UVoxelCreatureWorldSubsystem::OnSpawnTimerTick, 0.2f, true);
	}
}

void UVoxelCreatureWorldSubsystem::StopSpawnTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

void UVoxelCreatureWorldSubsystem::OnSpawnTimerTick()
{
	// 1. 检查类是否有效
	if (!CreatureClass) return;

	// 2. 清理无效指针 (懒惰清理)
	for (auto It = InScenceCreatureList.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// 3. 检查数量限制
	if (InScenceCreatureList.Num() >= CreatureNumLimit)
	{
		return; // 数量已满，跳过
	}

	// 4. 生成一个
	SpawnCreature();
}

void UVoxelCreatureWorldSubsystem::SpawnCreature()
{
	UObjectPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
    if (!PoolSys) { StopSpawnTimer(); return; } 

    // 1. 从对象池获取 Actor
    AVoxelNpcBaseInstance* NpcBase = PoolSys->Acquire<AVoxelNpcBaseInstance>(FName("NpcPool"));
    AVoxelIntelligentCreatureInstance* Creature = Cast<AVoxelIntelligentCreatureInstance>(NpcBase);

    if (Creature)
    {
        // =============================================================
        // [核心修改] 调用生成器产生 RPG 数据
        // =============================================================
        
        // A. 准备配置 (这里是全随机，您也可以根据区域不同设置强制种族)
        FCreatureGenerationSettings GenSettings;
        GenSettings.RandomSeed = FMath::Rand(); 
        
        // B. 执行生成
        FCreatureGenerationResult GenResult = UCreatureGeneratorUtils::GenerateCreatureData(this, GenSettings);

        if (GenResult.bIsValid)
        {
            // C. 将数据注入 Actor
            Creature->InitializeFromGenerationResult(GenResult);
        	#if WITH_EDITOR
        	Creature->SetActorLabel(GenResult.TrueData.RealName);
        	#endif

        	Creature->RebuildBodyVisuals();
            UE_LOG(LogTemp, Log, TEXT("Spawned NPC: %s (Race: %s, Job: %s)"), 
                *GenResult.TrueData.RealName, 
                *GenResult.TrueData.RaceTag.ToString(),
                *GenResult.TrueData.ProfessionTag.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Creature Generation Failed!"));
            // 失败处理：回收对象并返回
            PoolSys->Release(FName("NpcPool"), Creature);
            return;
        }

        // =============================================================
        // 下面是位置和激活逻辑 (保持不变，但去掉了手动赋值 ID 的部分)
        // =============================================================
    	FTransform FinalTransform;
        // 设置生成区域
        if (!SetSpawnId(Creature))
        {
            // ... 失败处理 ...
        	UE_LOG(LogTemp, Error, TEXT("Null Creature Generation Position!"));
        	FinalTransform=FTransform(FRotator::ZeroRotator,
        	FVector(
				FMath::RandRange(-1000, 1000),
				FMath::RandRange(-1000, 1000),
				200
				),
        		FVector::OneVector);
            //PoolSys->Release(FName("NpcPool"), Creature);
            //return;
        }
        else
        {
        	FinalTransform = SetCreatureSpawnTransform(Creature->CreatureData.SpawnAreaID);
        }
    	
        // 设置位置
    	UE_LOG(LogTemp, Error, TEXT("Final Position %s!"),
				*FinalTransform.ToString());
        Creature->SetActorTransform(FinalTransform);
    	UE_LOG(LogTemp, Error, TEXT("Creature Position %s!"),
				*Creature->GetTransform().ToString());
    	
    	// [Activation] 先激活组件，确保 CharacterMovement 准备好接收指令
    	Creature->ForceWakeUp(FinalTransform);

        // [Lifecycle]
        Creature->ResetCreature();

        // [切面] 应用当前切面参数到新生物（spawn hook）
        // 确保新生物立即响应切面虚化（无需等待下次 ApplySliceVisuals）
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (UVoxelPreviewComponent* VisualUI = PC->FindComponentByClass<UVoxelPreviewComponent>())
            {
                VisualUI->ApplySliceToNewActor(Creature);
            }
        }

        // 加入管理列表
        InScenceCreatureList.Add(Creature->CreatureData.CreatureID, Creature);
    }
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to acquire creature from pool!"));
		StopSpawnTimer(); return;
	}
}

void UVoxelCreatureWorldSubsystem::RemoveCreature(int CreatureID)
{
	if (TWeakObjectPtr<AVoxelNpcBaseInstance>* FoundPtr = InScenceCreatureList.Find(CreatureID))
	{
		if (FoundPtr->IsValid())
		{
			AVoxelNpcBaseInstance* Npc = FoundPtr->Get();
			
			// [Object Pool] 归还给池子
			if (UObjectPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<UObjectPoolSubsystem>())
			{
				PoolSys->Release(FName("NpcPool"), Npc);
			}
			else
			{
				// 兜底销毁
				Npc->Destroy();
			}
		}
		InScenceCreatureList.Remove(CreatureID);
	}
}

AVoxelNpcBaseInstance* UVoxelCreatureWorldSubsystem::GetCreatureById(int32 CreatureId) const
{
	if (const TWeakObjectPtr<AVoxelNpcBaseInstance>* FoundPtr = InScenceCreatureList.Find(CreatureId))
	{
		if (FoundPtr->IsValid())
		{
			return FoundPtr->Get();
		}
	}
	return nullptr;
}

TArray<int32> UVoxelCreatureWorldSubsystem::GetAllCreatureIds() const
{
	TArray<int32> Result;
	Result.Reserve(InScenceCreatureList.Num());
	for (const auto& Elem : InScenceCreatureList)
	{
		if (Elem.Value.IsValid())
		{
			Result.Add(Elem.Key);
		}
	}
	return Result;
}

TArray<AVoxelNpcBaseInstance*> UVoxelCreatureWorldSubsystem::GetSelectableCreaturesForUser(int32 UserId) const
{
	// 预留筛选入口：当前管理业务规则未实现，直接返回全部在场生物；
	// 后续按 UserId 过滤（如仅本人员工/店主）
	TArray<AVoxelNpcBaseInstance*> Result;
	Result.Reserve(InScenceCreatureList.Num());
	for (const auto& Elem : InScenceCreatureList)
	{
		if (Elem.Value.IsValid())
		{
			Result.Add(Elem.Value.Get());
		}
	}
	Result.Sort([](const AVoxelNpcBaseInstance& A, const AVoxelNpcBaseInstance& B)
	{
		return A.CreatureData.CreatureID < B.CreatureData.CreatureID;
	});
	return Result;
}

FText UVoxelCreatureWorldSubsystem::ResolveCreatureName(int32 CreatureId) const
{
	if (CreatureId == 0)
	{
		// 0 = 系统角色（系统区域/地图归属）
		return FText::FromString(TEXT("系统角色"));
	}
	if (CreatureId < 0)
	{
		// 负值 = 未指派
		return FText::FromString(TEXT("未指派"));
	}
	// 玩家主角自身
	if (const UWorld* World = GetWorld())
	{
		if (const UVoxelGameInstance* GI = World->GetGameInstance<UVoxelGameInstance>())
		{
			if (CreatureId == GI->GetMainCharacterId())
			{
				return FText::FromString(TEXT("玩家"));
			}
		}
	}
	// 在场生物
	if (const AVoxelNpcBaseInstance* Creature = GetCreatureById(CreatureId))
	{
		if (!Creature->CreatureData.RealName.IsEmpty())
		{
			return FText::FromString(Creature->CreatureData.RealName);
		}
	}
	return FText::FromString(FString::Printf(TEXT("#%d"), CreatureId));
}

void UVoxelCreatureWorldSubsystem::RemoveAllCreature()
{
	UObjectPoolSubsystem* PoolSys = GetWorld()->GetSubsystem<UObjectPoolSubsystem>();
	
	for (auto& Elem : InScenceCreatureList)
	{
		if (Elem.Value.IsValid())
		{
			if (PoolSys)
			{
				PoolSys->Release(FName("NpcPool"), Elem.Value.Get());
			}
			else
			{
				Elem.Value->Destroy();
			}
		}
	}
	InScenceCreatureList.Empty();
}

FTransform UVoxelCreatureWorldSubsystem::SetCreatureSpawnTransform(int SpawnAreaId) const
{
	FVector Pos = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;

	if (UVoxelAreaWorldSubsystem* AWS = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>())
	{
		if (auto* AreaInstancePtr = AWS->SpawnArea.Find(SpawnAreaId))
		{
			AVoxelAreaBaseInstance* Area = *AreaInstancePtr;
			if (Area)
			{
				Pos = GetRandomPositionInArea(
					Area->AreaBase.AreaCoverage.MinIndex,
					Area->AreaBase.AreaCoverage.MaxIndex
				);

				if (Area->SpawnAreaComponent)
				{
					Rot = FRotator(Area->SpawnAreaComponent->GetSpawnDirection());
				}
			}
		}
	}

	float BlockLen = 100.0f; 
	if (UVoxelGameInstance* GI = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		BlockLen = GI->WorldConfig.BlockLength;
	}

	// [Safety] 增加 Z 轴偏移防止卡在地里
	return FTransform(Rot, Pos * BlockLen + FVector(0, 0, 150.0), FVector::OneVector);
}

FVector UVoxelCreatureWorldSubsystem::GetRandomPositionInArea(FIntVector Min, FIntVector Max)
{
	return FVector(
		FMath::RandRange(Min.X, Max.X),
		FMath::RandRange(Min.Y, Max.Y),
		Min.Z // 始终在地面层生成
	);
}

void UVoxelCreatureWorldSubsystem::InitializeDemandEnum()
{
	UEnum* EnumPtr = StaticEnum<EDemandEnum>();
	if (!EnumPtr) return;

	DemandEnumlist.Empty();
	for (int32 i = 0; i < EnumPtr->NumEnums() - 1; i++)
	{
		if (EnumPtr->GetDisplayNameTextByIndex(i).IsEmpty()) continue;
		DemandEnumlist.AddUnique(static_cast<EDemandEnum>(EnumPtr->GetValueByIndex(i)));
	}
}

bool UVoxelCreatureWorldSubsystem::SetSpawnId(AVoxelIntelligentCreatureInstance* Input)
{
	if (!Input) return false;
	
	UVoxelAreaWorldSubsystem* AWS = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>();
	if (!AWS) false;
	
	
	if (bool bInvalid = !AWS->SpawnArea.Contains(Input->CreatureData.SpawnAreaID))
	{
		TArray<int> Keys;
		AWS->SpawnArea.GetKeys(Keys);
		if (Keys.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, Keys.Num() - 1);
			Input->CreatureData.SpawnAreaID = Keys[RandomIndex];
		}
		else
		{
			return false;
		}
	}
	
	return true;
}