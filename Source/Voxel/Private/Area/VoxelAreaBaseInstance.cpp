#include "Area/VoxelAreaBaseInstance.h"
#include "NavModifierComponent.h"
#include "VoxelFunctionAreaComponent.h"
#include "VoxelSpawnAreaComponent.h"
#include "Item/VoxelItemBaseInstance.h" // [新增] 引入 Item
#include "Core/VoxelGameInstance.h"
#include "Data/Creature/CreatureBase.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsDeclares.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsShared/ImmediatePhysicsCore.h"
#include "Core/CoreTags.h"

// =========================================================
// [Constructor] 构造函数
// =========================================================

AVoxelAreaBaseInstance::AVoxelAreaBaseInstance()
{
    PrimaryActorTick.bCanEverTick = false;
    
    NavModifierComponent = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifierComponent"));
    BoxVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UStaticMesh"));
    
    SetRootComponent(BoxVisual);
    // 默认隐藏可视化
    BoxVisual->SetHiddenInGame(true);
}

void AVoxelAreaBaseInstance::BeginPlay()
{
    Super::BeginPlay();
}

// =========================================================
// [Furniture Management] 家具管理
// =========================================================

void AVoxelAreaBaseInstance::RegisterFurnitureItem(AUVoxelItemBaseInstance* Item)
{
    if (Item && !FurnitureItems.Contains(Item))
    {
        FurnitureItems.Add(Item);
        // 这里可以触发一些事件，比如 "工作台已就绪"
        UE_LOG(LogTemp, Verbose, TEXT("Area %s registered furniture: %s"), *AreaBase.AreaName.ToString(), *Item->GetName());
    }
}

void AVoxelAreaBaseInstance::UnregisterFurnitureItem(AUVoxelItemBaseInstance* Item)
{
    if (Item)
    {
        FurnitureItems.Remove(Item);
    }
}

// =========================================================
// [Spatial Query] 空间查询
// =========================================================

bool AVoxelAreaBaseInstance::IsPointInArea(const FIntVector& GridPos) const
{
    // 目前基于简单的 Box 判定
    // 逻辑：X >= Min.X && X < Max.X (注意 Max 通常是边界外侧，是否包含取决于定义)
    // 根据你的 ChunkUtilsFunction::ChangeArea 逻辑，通常是闭区间或半开区间
    // 假设这里的 Min/Max 是网格坐标的闭区间：
    
    if (GridPos.X < AreaBase.AreaCoverage.MinIndex.X || GridPos.X > AreaBase.AreaCoverage.MaxIndex.X) return false;
    if (GridPos.Y < AreaBase.AreaCoverage.MinIndex.Y || GridPos.Y > AreaBase.AreaCoverage.MaxIndex.Y) return false;
    if (GridPos.Z < AreaBase.AreaCoverage.MinIndex.Z || GridPos.Z > AreaBase.AreaCoverage.MaxIndex.Z) return false;
    
    return true;
}

FBox AVoxelAreaBaseInstance::GetAreaBounds() const
{
    return FBox(FVector(AreaBase.AreaCoverage.MinIndex), FVector(AreaBase.AreaCoverage.MaxIndex));
}

// =========================================================
// [Lifecycle] 生命周期与销毁
// =========================================================

void AVoxelAreaBaseInstance::InitArea(const FAreaInstance& InitialData, int CreatorId)
{
    UpdateAreaBorder();
    UpdateAreaVisual(true);
    UE_LOG(LogTemp, Log, TEXT("Area %d initialized successfully"), AreaBase.AreaId);
}

void AVoxelAreaBaseInstance::DeAreaInstance()
{
    // 1. 递归销毁子区域
    for (auto& SubAreaPair : SubAreasIndex)
    {
        if (SubAreaPair.Value && SubAreaPair.Value->IsValidLowLevel())
        {
            SubAreaPair.Value->DeAreaInstance();
        }
    }
    SubAreasIndex.Empty();

    // 2. 通知家具解绑 (防止家具持有悬空指针)
    for (AUVoxelItemBaseInstance* Item : FurnitureItems)
    {
        if (Item && Item->IsValidLowLevel())
        {
            Item->ItemInstance.ParentArea = nullptr; // 断开引用
        }
    }
    FurnitureItems.Empty();

    // 3. 清理组件与自身
    CleanFunctionComponent();
    Destroy();
}

// =========================================================
// [Components] 组件挂载逻辑
// =========================================================

void AVoxelAreaBaseInstance::UpdateAreaTypeFunc()
{
    // 获取当前所有 Actor 组件
    TArray<UActorComponent*> Components;
    GetComponents(Components);

    // 清理旧的功能组件 (保留 Mesh 和 Nav)
    for (UActorComponent* Component : Components)
    {
        if (Component && (!Component->IsA(UStaticMeshComponent::StaticClass()) && !Component->IsA(UNavModifierComponent::StaticClass())))
        {
            Component->DestroyComponent();
        }
    }
    
    // 重置指针
    SpawnAreaComponent = nullptr;
    FunctionAreaComponent = nullptr;

    // 1. 如果是出生点
    if (AreaBase.Function.AreaType.MatchesTag(FCoreTags::Area_Type_Spawn))
    {
        FString ComponentName = FString::Printf(TEXT("SpawnAreaComponent_%s"), *AreaBase.AreaName.ToString());
        SpawnAreaComponent = NewObject<UVoxelSpawnAreaComponent>(this, FName(ComponentName));
        SpawnAreaComponent->RegisterComponent();
        SpawnAreaComponent->SpawnDirection = EActorDirection::Forward;
    }
    // 2. 如果是商店
    else if (AreaBase.Function.AreaType.MatchesTag(FCoreTags::Area_Type_Shop))
    {
        FString ComponentName = FString::Printf(TEXT("FunctionAreaComponent_%s"), *AreaBase.AreaName.ToString());
        FunctionAreaComponent = NewObject<UVoxelFunctionAreaComponent>(this, FName(ComponentName));
        FunctionAreaComponent->RegisterComponent();
    }
    // 3. 如果是子功能区
    else if (AreaBase.Function.AreaType.MatchesTag(FCoreTags::Area_Type_SubFunction))
    {
        FString ComponentName = FString::Printf(TEXT("SubFunctionComponent_%s"), *AreaBase.AreaName.ToString());
        FunctionAreaComponent = NewObject<UVoxelFunctionAreaComponent>(this, FName(ComponentName));
        FunctionAreaComponent->RegisterComponent();
    }
}

void AVoxelAreaBaseInstance::UpdateCurAreaType(FGameplayTag InputType)
{
    if(const FAreaTypeBase* TypeDef = GetGameInstance()->GetSubsystem<UConfigDataSubsystem>()->GetAreaTypeByTag(InputType))
    {
        AreaBase.UpdateAreaType(*TypeDef);
        UpdateAreaTypeFunc();
    }
}

void AVoxelAreaBaseInstance::CleanFunctionComponent() const
{
    if (SpawnAreaComponent) SpawnAreaComponent->DestroyComponent();
    if (FunctionAreaComponent) FunctionAreaComponent->DestroyComponent();
}

// =========================================================
// [Border & Visual] 边界更新与可视化
// =========================================================

void AVoxelAreaBaseInstance::UpdateAreaBorder()
{
    // 计算中心点和缩放 (假设 BlockLength)
    const int BlockLength = GetGameInstance<UVoxelGameInstance>()->WorldConfig.BlockLength;
    const float ScaleFactor = 0.01f; // 缩放因子，用于适配 Mesh 大小
    
    FVector Min = FVector(AreaBase.AreaCoverage.MinIndex);
    FVector Max = FVector(AreaBase.AreaCoverage.MaxIndex);

    // 计算中心点 (世界坐标)
    // 注意：根据你的坐标系定义，这里可能需要精细调整 +0.5 或 +1
    FVector Center = (Min + Max + FVector::OneVector) * 0.5f * BlockLength;
                     
    // 计算缩放 (尺寸)
    FVector Dimensions = (Max - Min + FVector(1)); // +1 是为了包含边界方块
    FVector Scale = Dimensions * BlockLength * ScaleFactor;
    
    FTransform Transform(FRotator::ZeroRotator, Center, Scale);
    SetActorTransform(Transform);
    
    if (NavModifierComponent)
    {
        NavModifierComponent->CalculateBounds();
    }
}

void AVoxelAreaBaseInstance::UpdateAreaBorderByPoints(FIntVector min, FIntVector max)
{
    // 如果是子功能区，必须限制在父区域内
    if(AreaBase.IsSubFunction())
    {
        AreaBase.AreaCoverage.SetBoundingBox(CheckParentBorder(min, true), CheckParentBorder(max, false));
    }
    else
    {
        AreaBase.AreaCoverage.SetBoundingBox(min, max);
    }
    
    UpdateAreaBorder();
}

void AVoxelAreaBaseInstance::UpdateAreaBorderByBox(FBox Bound)
{
    UpdateAreaBorderByPoints(FIntVector(Bound.Min), FIntVector(Bound.Max));
}

void AVoxelAreaBaseInstance::UpdateAreaVisual(bool isVisible, bool isSelected, bool bReadOnly)
{
    if (!BoxVisual) return;

    BoxVisual->SetHiddenInGame(!isVisible);
    SetActorEnableCollision(isVisible);

    if (isVisible)
    {
        if (bReadOnly)
        {
            UpdateVisualColor(FColor(128, 128, 128));
        }
        else if (isSelected)
        {
            UpdateVisualColor(FColor::Red);
        }
        else
        {
            UpdateVisualColor(AreaColor);
        }
    }
}

void AVoxelAreaBaseInstance::UpdateAreaColor(FColor input)
{
    AreaColor = input;

}

void AVoxelAreaBaseInstance::UpdateVisualColor(FColor input)
{
    UMaterialInstanceDynamic* DynamicMat = GetOrCreateDynamicMaterial();
    if (DynamicMat)
    {
        DynamicMat->SetVectorParameterValue(TEXT("AreaColor"), input);
    }
}

UMaterialInstanceDynamic* AVoxelAreaBaseInstance::GetOrCreateDynamicMaterial()
{
    if (!BoxVisual || !AreaVisualMaterial) return nullptr;

    if (AreaDynamicMaterial && AreaDynamicMaterial->IsValidLowLevel())
    {
        return AreaDynamicMaterial;
    }

    AreaDynamicMaterial = UMaterialInstanceDynamic::Create(AreaVisualMaterial, this);
    if (AreaDynamicMaterial)
    {
        AreaDynamicMaterial->SetVectorParameterValue(TEXT("AreaColor"), FLinearColor(AreaColor));
        BoxVisual->SetMaterial(0, AreaDynamicMaterial);
    }
    return AreaDynamicMaterial;
}

FIntVector AVoxelAreaBaseInstance::CheckParentBorder(FIntVector InputPosition, bool IsMin)
{
    if(!ParentArea) return InputPosition;

    FIntVector TmpPosition = FIntVector(0);
    const FIntVector ParentMin = ParentArea->AreaBase.AreaCoverage.MinIndex;
    const FIntVector ParentMax = ParentArea->AreaBase.AreaCoverage.MaxIndex;

    for(int i=0; i < 3; i++)
    {
        if(IsMin)
        {
            // 如果比父区域最小边界还小，强制拉回父区域边界
            TmpPosition[i] = (InputPosition[i] < ParentMin[i]) ? ParentMin[i] : InputPosition[i];
        }
        else
        {
            // 如果比父区域最大边界还大，强制拉回
            TmpPosition[i] = (InputPosition[i] > ParentMax[i]) ? ParentMax[i] : InputPosition[i];
        }
    }
    return TmpPosition;
}

// =========================================================
// [Info Update] 信息更新逻辑
// =========================================================

bool AVoxelAreaBaseInstance::GenerateAreaInfo(const FAreaInstance& Input)
{
    UConfigDataSubsystem* ConfigData = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
    if (!ConfigData) return false;

    // 验证类型是否存在
    if (!ConfigData->GetAreaTypeByTag(Input.Function.AreaType)) return false;

    FAreaInstance CopiedData = Input;
    AreaBase.InitializeAreaInform(CopiedData);
    UpdateAreaBorder();
    return true;
}

bool AVoxelAreaBaseInstance::UpdateAllAreaInfo(FAreaInstance& Input, int UpdatePlayerId)
{
    if(!IsAreaEditable(UpdatePlayerId)) return false;

    UConfigDataSubsystem* ConfigData = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
    if (!ConfigData) return false;

    const FAreaTypeBase* AreaType = ConfigData->GetAreaTypeByTag(Input.Function.AreaType);
    if (!AreaType) return false;

    if (!CheckTypeConflict(AreaType->AreaTypeTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("Area Update Failed: Type Conflict %s"), *AreaType->AreaTypeTag.ToString());
        return false;
    }
    return AreaBase.UpdateAreaInform(Input);
}

bool AVoxelAreaBaseInstance::UpdateBaseAreaInfo(const FAreaModificationData& Input, int UpdatePlayerId)
{
	if(!IsAreaEditable(UpdatePlayerId)) return false;

	UConfigDataSubsystem* ConfigData = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
	if (!ConfigData) return false;

	const FAreaTypeBase* AreaType = ConfigData->GetAreaTypeByTag(Input.AreaType);
	if (!AreaType) return false;

	if (!CheckTypeConflict(AreaType->AreaTypeTag)) return false;

	AreaBase.ParentScenceId = Input.ParentScenceId;
	AreaBase.AreaName = Input.AreaName;
	AreaBase.Function.AreaType = Input.AreaType;
	AreaBase.Function.Status = Input.Status;
	AreaBase.OwnerCreatureId = Input.OwnerCreatureId;
	AreaBase.ManagerCreatureId = Input.ManagerCreatureId;
	AreaBase.AreaCoverage = Input.AreaCoverage;
	UpdateAreaBorder();
	OnAreaBaseInfoChanged.Broadcast(this);
	return true;
}

void AVoxelAreaBaseInstance::UpdateParentArea(FAreaInstance& Parent, int OperatorId)
{
    if(!IsAreaEditable(OperatorId)) return;
    if (AreaBase.ParentScenceId != Parent.ParentScenceId) return;

    UConfigDataSubsystem* ConfigSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UConfigDataSubsystem>();
    if (!ConfigSubsystem) return;

    const FAreaTypeBase* CurrentAreaType = ConfigSubsystem->GetAreaTypeByTag(AreaBase.Function.AreaType);
    if (!CurrentAreaType) return;

    if (!CurrentAreaType->CheckAllowParentType(Parent.Function.AreaType)) return;

    AreaBase.ParentAreaId = Parent.AreaId;
}

void AVoxelAreaBaseInstance::ChangeParentArea(AVoxelAreaBaseInstance* InputArea)
{
    // 简单的父级切换逻辑：只要输入区域是容器类型即可
    if(ParentArea == nullptr && InputArea != this && InputArea != nullptr)
    {
        if (InputArea->AreaBase.Function.AreaType.MatchesTag(FCoreTags::Area_Type_BusinessCluster) ||
            InputArea->AreaBase.Function.AreaType.MatchesTag(FCoreTags::Area_Type_Default))
        {
            ParentArea = InputArea;
            AreaBase.ParentAreaId = InputArea->AreaBase.AreaId;
        }
    }
    else
    {
        ParentArea = nullptr;
        AreaBase.ParentAreaId = -1;
    }
}

// =========================================================
// [Permissions & Checks] 权限与检查
// =========================================================

bool AVoxelAreaBaseInstance::CheckTypeConflict(FGameplayTag InputTag)
{
    bool bIsValid = false;

    // 1. 公共设施 (无 Owner, 无 Parent)
    if (InputTag.MatchesTag(FCoreTags::Area_Type_Spawn) || InputTag.MatchesTag(FCoreTags::Area_Type_Default))
    {
        bIsValid = (AreaBase.OwnerCreatureId == 0 && AreaBase.ManagerCreatureId == 0 && AreaBase.ParentAreaId == -1);
    }
    // 2. 商业地皮 (有 Owner, 无 Parent)
    else if (InputTag.MatchesTag(FCoreTags::Area_Type_BusinessCluster))
    {
        bIsValid = (AreaBase.OwnerCreatureId != 0 && AreaBase.ManagerCreatureId != 0 && AreaBase.ParentAreaId == -1);
    }
    // 3. 具体店铺 (有 Owner, 有 Parent)
    else if (InputTag.MatchesTag(FCoreTags::Area_Type_Shop) || InputTag.MatchesTag(FCoreTags::Area_Type_SubFunction))
    {
        bIsValid = (AreaBase.OwnerCreatureId != 0 && AreaBase.ManagerCreatureId != 0 && AreaBase.ParentAreaId != -1);
    }
    
    return bIsValid;
}

bool AVoxelAreaBaseInstance::IsAreaEditable(int CharCheckId) const
{
    // 权限只认 Owner / Manager，无特殊 ID 分支
    return AreaBase.IsManager(CharCheckId) || AreaBase.IsOwner(CharCheckId);
}

void AVoxelAreaBaseInstance::SetAreaOwner(int NewOwnerId)
{
    if(IsAreaEditable(GetGameInstance<UVoxelGameInstance>()->GetMainCharacterId()))
    {
        if (NewOwnerId == AreaBase.OwnerCreatureId) return;
        AreaBase.OwnerCreatureId = NewOwnerId;
        OnAreaPermissionsChanged.Broadcast(this);
    }
}

void AVoxelAreaBaseInstance::SetAreaManager(int NewManagerId)
{
    if(IsAreaEditable(GetGameInstance<UVoxelGameInstance>()->GetMainCharacterId()))
    {
        if (NewManagerId == AreaBase.ManagerCreatureId) return;
        AreaBase.ManagerCreatureId = NewManagerId;
        OnAreaPermissionsChanged.Broadcast(this);
    }
}

void AVoxelAreaBaseInstance::AddAllowedUser(int CreatureId)
{
    if(IsAreaEditable(GetGameInstance<UVoxelGameInstance>()->GetMainCharacterId()))
    {
        if (AreaBase.UsableCreatureId.Contains(CreatureId)) return;
        AreaBase.UsableCreatureId.Add(CreatureId);
        OnAreaPermissionsChanged.Broadcast(this);
    }
}

void AVoxelAreaBaseInstance::RemoveAllowedUser(int CreatureId)
{
    if(IsAreaEditable(GetGameInstance<UVoxelGameInstance>()->GetMainCharacterId()))
    {
        if(AreaBase.UsableCreatureId.Find(CreatureId))
        {
            AreaBase.UsableCreatureId.Remove(CreatureId);
        }
    }
}

bool AVoxelAreaBaseInstance::CanCreatureEnter(int CreatureId) const
{
    if (AreaBase.Authority.bIsPublic) return true;
    
    if (AreaBase.IsOwner(CreatureId) || 
        AreaBase.IsManager(CreatureId) || 
        AreaBase.UsableCreatureId.Contains(CreatureId))
    {
        return true;
    }
    return false;
}