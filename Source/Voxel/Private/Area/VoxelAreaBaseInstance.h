
#pragma once

#include "CoreMinimal.h"
#include "VoxelNavArea.h"
#include "VoxelSpawnAreaComponent.h"
#include "GameplayTagContainer.h" 
#include "Data/Area/VoxelAreaBase.h"
#include "GameFramework/Actor.h"
#include "VoxelAreaBaseInstance.generated.h"

// 前置声明
class UVoxelFunctionAreaComponent;
class AVoxelGameState;
class UNavModifierComponent;
class UStaticMeshComponent;
class AUVoxelItemBaseInstance; // [新增] 引用 Item 类
struct FNavigationRelevantData;

/**
 * 区域实例 Actor
 * 代表场景中的一个逻辑区域（如房间、店铺、功能区）。
 * 职责：
 * 1. 维护区域的物理范围 (FBox)。
 * 2. 管理区域内的组件 (Spawn/Function)。
 * 3. 记录区域内的家具列表 (Furniture Items)。
 * 4. 处理权限与层级关系 (Parent/Sub Areas)。
 */
UCLASS(BlueprintType, Blueprintable)
class AVoxelAreaBaseInstance : public AActor
{
    GENERATED_BODY()
    
public:
    AVoxelAreaBaseInstance();
    
    // =========================================================
    // [Core Data] 核心数据
    // =========================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    FAreaInstance AreaBase;
    
    // 父区域引用（层级关系）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    AVoxelAreaBaseInstance* ParentArea = nullptr;

    // 子区域索引 Key: AreaId
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area")
    TMap<int, AVoxelAreaBaseInstance*> SubAreasIndex;
    
    // 相邻区域索引 (预留)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area")
    TMap<int, AVoxelAreaBaseInstance*> NeighborAreasIndex;

    // =========================================================
    // [Furniture Management] 家具管理 (新增)
    // =========================================================
    
    // 区域内的家具列表 (弱引用，生命周期由 ItemSubsystem 管理)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Area|Items")
    TArray<AUVoxelItemBaseInstance*> FurnitureItems;

    /** 注册家具到本区域 */
    UFUNCTION(BlueprintCallable, Category = "Area|Items")
    void RegisterFurnitureItem(AUVoxelItemBaseInstance* Item);

    /** 从本区域注销家具 */
    UFUNCTION(BlueprintCallable, Category = "Area|Items")
    void UnregisterFurnitureItem(AUVoxelItemBaseInstance* Item);

    // =========================================================
    // [Spatial Query] 空间查询
    // =========================================================

    /** * 判断一个网格点是否在区域内 
     * 目前基于 AABB 包围盒实现，未来可扩展为多形状检测
     */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    bool IsPointInArea(const FIntVector& GridPos) const;

    /** 获取区域的世界包围盒 */
    UFUNCTION(BlueprintCallable, Category = "Area|Query")
    FBox GetAreaBounds() const;

    // =========================================================
    // [Lifecycle] 生命周期
    // =========================================================

    /** 初始化区域 */
    void InitArea(const FAreaInstance& InitialData, int CreatorId);

    /** 销毁区域 (清理子区域、组件、通知家具解绑) */
    UFUNCTION(BlueprintCallable, Category = "Area")
    void DeAreaInstance();

    // =========================================================
    // [Components] 组件管理
    // =========================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Components")
    TObjectPtr<UNavModifierComponent> NavModifierComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Components")
    TObjectPtr<UVoxelSpawnAreaComponent> SpawnAreaComponent;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Components")
    TObjectPtr<UVoxelFunctionAreaComponent> FunctionAreaComponent;

    /** 根据 Tag 动态挂载/卸载功能组件 */
    UFUNCTION(BlueprintCallable, Category = "Area|Components")
    void UpdateAreaTypeFunc();

    /** 清理所有功能组件 */
    UFUNCTION(BlueprintCallable, Category = "Area|Components")
    void CleanFunctionComponent() const;

    // =========================================================
    // [Info Update] 信息更新
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Area|Update")
    bool GenerateAreaInfo(const FAreaInstance& Input);
    
    UFUNCTION(BlueprintCallable, Category = "Area|Update")
    bool UpdateAllAreaInfo(FAreaInstance& Input, int UpdatePlayerId);
    
    bool UpdateBaseAreaInfo(const FAreaModificationData& Input, int UpdatePlayerId);
    
    UFUNCTION(BlueprintCallable, Category = "Area|Update")
    void UpdateParentArea(FAreaInstance& Parent, int OperatorId);
    
    UFUNCTION(BlueprintCallable, Category = "Area|Update")
    void ChangeParentArea(AVoxelAreaBaseInstance* InputArea);

    // 更新类型并刷新组件
    UFUNCTION(BlueprintCallable, Category = "Area|Update")
    void UpdateCurAreaType(FGameplayTag InputType);
    
    // 检查类型冲突
    bool CheckTypeConflict(FGameplayTag InputTag);

    // =========================================================
    // [Border & Visual] 边界与可视化
    // =========================================================

    UFUNCTION(BlueprintCallable, Category = "Area|Visual")
    void UpdateAreaBorderByPoints(FIntVector min, FIntVector max);
    
    UFUNCTION(BlueprintCallable, Category = "Area|Visual")
    void UpdateAreaBorderByBox(FBox Bound);

    UFUNCTION(BlueprintCallable, Category = "Area|Visual")
    void UpdateAreaVisual(bool isVisible, bool isSelected = false, bool bReadOnly = false);

    UFUNCTION(BlueprintCallable, Category = "Area|Visual")
    void UpdateAreaColor(FColor input);
    void UpdateVisualColor(FColor input);

    UFUNCTION(BlueprintCallable, Category = "Area|Visual")
    void UpdateAreaBorder();

    // =========================================================
    // [Permissions] 权限管理
    // =========================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCreatureEnteredArea, int, CreatureId, AVoxelAreaBaseInstance*, Area);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCreatureLeftArea, int, CreatureId, AVoxelAreaBaseInstance*, Area);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAreaPermissionsChanged, AVoxelAreaBaseInstance*, Area);
    // 基础业务数据（名称/类型/状态/权属/覆盖）更新成功后广播，UI 订阅刷新
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAreaBaseInfoChanged, AVoxelAreaBaseInstance*, Area);
    
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    void SetAreaOwner(int NewOwnerId);
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    void SetAreaManager(int NewManagerId);
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    void AddAllowedUser(int CreatureId);
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    void RemoveAllowedUser(int CreatureId);
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    bool CanCreatureEnter(int CreatureId) const;
    UFUNCTION(BlueprintCallable, Category = "Area|Permissions")
    bool IsAreaEditable(int CharCheckId) const;

    UPROPERTY(BlueprintAssignable, Category = "Area|Events")
    FOnCreatureEnteredArea OnCreatureEnteredArea;
    UPROPERTY(BlueprintAssignable, Category = "Area|Events")
    FOnCreatureLeftArea OnCreatureLeftArea;
    UPROPERTY(BlueprintAssignable, Category = "Area|Events")
    FOnAreaPermissionsChanged OnAreaPermissionsChanged;

    UPROPERTY(BlueprintAssignable, Category = "Area|Update")
    FOnAreaBaseInfoChanged OnAreaBaseInfoChanged;

protected:
    // 可视化组件
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Visual")
    UStaticMeshComponent* BoxVisual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area|Visual")
    TObjectPtr<UMaterialInterface> AreaVisualMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> AreaDynamicMaterial;

    FColor AreaColor = FColor::Green;

    virtual void BeginPlay() override;
    
    UFUNCTION(BlueprintCallable, Category = "Area|Internal")
    UMaterialInstanceDynamic* GetOrCreateDynamicMaterial();

    FIntVector CheckParentBorder(FIntVector InputPosition, bool IsMin);
};