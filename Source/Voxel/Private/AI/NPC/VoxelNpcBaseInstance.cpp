// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/UI/NPCUI/DemandChartWidget.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utils/CreatureGeneratorUtils.h"

AVoxelNpcBaseInstance::AVoxelNpcBaseInstance()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. 初始化胶囊体
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// 2. 初始化核心躯干 (MainBodyMesh)
	// 在 ACharacter 中，GetMesh() 默认存在。我们将它作为核心部件(Chest)。
	MainBodyMesh = GetMesh();
	if (MainBodyMesh)
	{
		// 优化设置：作为 Master Pose 的 Leader，它必须时刻更新，否则从属部件会卡住
		MainBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		MainBodyMesh->bEnableUpdateRateOptimizations = true; // 允许 URO 优化
		
		// 默认位置调整 (根据你的美术资源调整，通常 Blender 导出不需要旋转)
		MainBodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
		MainBodyMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	// 3. 移动组件配置
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
}

void AVoxelNpcBaseInstance::BeginPlay()
{
	Super::BeginPlay();
}

void AVoxelNpcBaseInstance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 确保销毁时清理动态组件
	ClearBodyVisuals();
	Super::EndPlay(EndPlayReason);
}

void AVoxelNpcBaseInstance::InitializeFromGenerationResult(const FCreatureGenerationResult& GenResult)
{
	// 1. 同步核心数据
	this->CreatureData = GenResult.TrueData;     // 身份、外观 DNA
	this->RuntimeState = GenResult.InitialState; // 初始属性、状态

	// 2. 组装身体模型
	RebuildBodyVisuals();

	// 3. (可选) 应用属性到移动组件
	// if (float* Speed = RuntimeState.FinalAttributes.Find(Tag_MoveSpeed))
	//     GetCharacterMovement()->MaxWalkSpeed = *Speed;
}

void AVoxelNpcBaseInstance::ClearBodyVisuals()
{
	// 遍历当前的 BodyState 清理动态生成的组件
	for (auto& Pair : CreatureData.BodyVisual.BodyState)
	{
		FRuntimeBodyPartInstance& Instance = Pair.Value;

		// 如果组件存在，且不是 MainBodyMesh (核心躯干由 Character 管理，只需置空网格)
		if (Instance.BodyPartComponent)
		{
			if (Instance.BodyPartComponent == MainBodyMesh)
			{
				MainBodyMesh->SetSkeletalMesh(nullptr);
				// 不要 Destroy MainBodyMesh!
			}
			else
			{
				Instance.BodyPartComponent->DestroyComponent();
			}
			
			// 指针置空
			Instance.BodyPartComponent = nullptr;
		}
	}
}

void AVoxelNpcBaseInstance::RebuildBodyVisuals()
{
	// 0. 前置检查与清理
	ClearBodyVisuals(); // 先清理旧的，防止重叠

	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UConfigDataSubsystem* Config = GI->GetSubsystem<UConfigDataSubsystem>();
	
	// 加载体型定义
	if (CreatureData.BodyVisual.BodyType.IsNull()) return;
	const UBodyTypeDefinition* BodyType = CreatureData.BodyVisual.BodyType.LoadSynchronous();
	if (!BodyType) return;

	// 1. 获取安装顺序 (拓扑排序)
	// 确保父节点 (Chest) 先创建，子节点 (Head/Arm) 后创建
	TArray<FGameplayTag> OrderedSlots;
	BodyType->GetOrderedSlots(OrderedSlots);

	// 2. 遍历顺序列表进行组装
	for (const FGameplayTag& SlotTag : OrderedSlots)
	{
		// A. 从存档数据(BodyState)里找这个部位的实例
		FRuntimeBodyPartInstance* PartInstance = CreatureData.BodyVisual.BodyState.Find(SlotTag);
		
		// 如果找不到(没生成) 或者 状态是已断裂(Severed)，则跳过不渲染
		if (!PartInstance || PartInstance->bIsSevered) continue;

		// B. 加载 Definition 资产 (用于查找变体)
		UBodyPartDefinition* PartDef = PartInstance->Definition.LoadSynchronous();
		if (!PartDef) continue;

		// C. [核心改造] 通过 FName ID 查找具体的变体配置
		// 这里的 SelectedVariantId 是生成阶段存入的 (例如 "Leather_Tunic_01")
		const FBodyPartVariant* VariantData = PartDef->Variants.Find(PartInstance->SelectedVariantId);
		if (!VariantData) 
		{
			// 兜底：如果 ID 找不到(可能资产改了)，尝试用第一个
			// if (PartDef->Variants.Num() > 0) VariantData = &PartDef->Variants.Array()[0].Value;
			continue; 
		}

		// D. 加载模型资源 (Mesh)
		USkeletalMesh* MeshToLoad = VariantData->MeshAsset.LoadSynchronous();
		if (!MeshToLoad) continue;

		// E. 创建或复用组件
		USkeletalMeshComponent* CurrentComp = nullptr;
		bool bIsRoot = (SlotTag == BodyType->RootSlot); // 检查是否是根部位(Chest)

		if (bIsRoot)
		{
			// 根部位：复用 ACharacter 自带的 Mesh
			CurrentComp = MainBodyMesh;
			CurrentComp->SetSkeletalMesh(MeshToLoad);
			
			// 设置动画蓝图 (如果有配置种族动画)
			// if (Config) CurrentComp->SetAnimInstanceClass(Config->GetRaceAnimBP(...));
		}
		else
		{
			// 子部位：动态创建组件
			FName CompName = FName(*SlotTag.ToString());
			CurrentComp = NewObject<USkeletalMeshComponent>(this, CompName);
			CurrentComp->RegisterComponent(); // 必须注册
			CurrentComp->SetSkeletalMesh(MeshToLoad);
			
			// 优化：关闭子部件的独立 Tick，完全依赖 MasterPose
			// CurrentComp->SetComponentTickEnabled(false); 

			// F. 挂载逻辑 (Attachment)
			// 查找结构定义中的父节点 Tag
			if (const FBodyStructureEntry* StructEntry = BodyType->BodyStructure.Find(SlotTag))
			{
				// 去 BodyState 里找父节点的 RuntimeInstance，进而拿到父组件指针
				if (FRuntimeBodyPartInstance* ParentInst = CreatureData.BodyVisual.BodyState.Find(StructEntry->ParentSlot))
				{
					// 只有父节点组件存在时才能挂载 (父节点没断)
					if (ParentInst->BodyPartComponent) 
					{
						if (PartDef->MountType == EBodyPartMountType::MasterPose)
						{
							// [Master Pose 模式] (衣服/肉体)
							// 物理挂载到父节点 (保持层级整洁)
							CurrentComp->AttachToComponent(ParentInst->BodyPartComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
							
							// 动画驱动来自 MainBodyMesh (核心)
							CurrentComp->SetLeaderPoseComponent(MainBodyMesh); 
						}
						// else 
						// {
						// 	// [Socket 模式] (挂件/武器/义肢)
						// 	// 挂在父节点的指定 Socket 上
						// 	// 注意：这里用 PartTag 作为 SocketName，或者需要在 Definition 里加 SocketName 字段
						// 	CurrentComp->AttachToComponent(ParentInst->BodyPartComponent, FAttachmentTransformRules::SnapToTargetIncludingScale, PartDef->PartTag);
						// }
					}
					else
					{
						// 如果父节点组件为空(可能父节点断了但数据还在)，销毁当前组件并跳过
						CurrentComp->DestroyComponent();
						continue;
					}
				}
			}
		}
		
		// G. 应用材质覆盖
		for(int i=0; i < VariantData->MaterialOverrides.Num(); ++i)
		{
			 if(!VariantData->MaterialOverrides[i].IsNull())
			 {
				 CurrentComp->SetMaterial(i, VariantData->MaterialOverrides[i].LoadSynchronous());
			 }
		}

		// H. 回填组件指针 (非常重要！供子节点查找)
		PartInstance->BodyPartComponent = CurrentComp;
		// 暂存 Mesh 引用以防 GC (虽然 Component 引用了 Mesh 应该没问题)
		PartInstance->LoadedMesh = MeshToLoad; 
	}
}

void AVoxelNpcBaseInstance::ForceWakeUp(const FTransform& SpawnTransform)
{
	// 1. 位置归位
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);

	// 2. 激活 Actor
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);

	// 3. 强制刷新核心 Mesh
	if (MainBodyMesh)
	{
		MainBodyMesh->SetHiddenInGame(false);
		MainBodyMesh->SetVisibility(true);
		
		// 强制刷新动画与骨骼
		if (MainBodyMesh->GetAnimInstance())
		{
			MainBodyMesh->TickAnimation(0.0f, false);
		}
		MainBodyMesh->RefreshBoneTransforms(nullptr);
		MainBodyMesh->UpdateComponentToWorld();
	}

	// 4. 重置移动组件
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetComponentTickEnabled(true);
		MoveComp->Activate(true);
		MoveComp->bConstrainToPlane = false; 
		MoveComp->GravityScale = 1.0f;
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_Falling);
	}
    
	// 5. 胶囊体
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->UpdateBodySetup();
		Capsule->SetVisibility(true); // 调试用，实际游戏可能隐藏
	}

	// 6. 注册组件
	this->ReregisterAllComponents();
}

void AVoxelNpcBaseInstance::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVoxelNpcBaseInstance::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

