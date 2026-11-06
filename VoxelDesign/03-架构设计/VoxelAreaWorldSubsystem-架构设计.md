# VoxelAreaWorldSubsystem-架构设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-11-04

---

## 1. 组件架构

### 1.0 经营实体、空间组件与存档编排

经营实体是资产与业务数据的唯一所有者。商圈、店铺、图外生意共用基础模型，按类型组合模块；实体可以没有真实空间。下图为逻辑归属，不硬定实现 class：

```text
FSaveData.Global.BusinessEntities
 └─ 经营实体（BusinessEntityId：FGuid）
     ├─ 身份/分类/名称/LOGO/ParentBusinessEntityId
     ├─ 经营关系、有效授权与必要 Lease/任命条款
     ├─ 按类型组合：bOpen、目录、预留/订单、声誉、营业时段等模块
     └─ 可选空间关联组件 → AreaRef={SceneId,AreaId}

FSceneSaveData（场景空间数据）
 └─ AVoxelAreaBaseInstance / FAreaSaveRecord
     ├─ AreaId、父子、类型/功能标记、覆盖、定位、准入约束
     └─ 空间表现/导航组件（不持经营数据或 Owner/Manager 权属真值）

家具/结构产权与 leaseID 日志 → 独立资产来源，不由空间包含转移
```

规则：

1. `BusinessEntityId` 是全局稳定经营标识，`ShopId` 只是经营 ID 别名，不能当作 `AreaId`；不自选 GUID 或 int32。`AreaRef={SceneId,AreaId}` 仅定位空间，子区域有 `AreaId` 但不是经营实体。
2. `Area.Type.Shop.*` 等既有配置 Tag 可继续用于实体分类；名称含 Area 不代表区域身份，具体 Tag 迁移待定。经营模块按实体类型组合，不按区域 Actor 挂载，也不以区域切换类型迁移经营数据。
3. 经营注册/持久化与空间注册/持久化分别编排。经营实体存入 `Global.BusinessEntities`；区域收集/回填 `FAreaSaveRecord` 仅含空间与空间组件载荷，不镜像关系、租约、bOpen、目录、订单或声誉。
4. `bOperating = bOpen && 当前有效营业时段` 纯派生、不入档；只手动写入并存档 `bOpen`，创建默认 false，时间变化不触发自动资格复检。
5. UI/Controller → 业务门面 → 经营实体存储与区域空间存储 → 事件 → UI。`UVoxelAreaWorldSubsystem` 只做空间 CRUD、定位与准入校验；产权、经营权限由实体经营关系/有效授权查询，区域不转发或存储经营业务。
6. 建造侧栏复用范围/模板预览，采集名称、类型/业态、经营意图与空间草稿后，单次 Confirm 概念请求 `CreateBusinessWithArea`（OperatorId、BusinessKind、Name、BusinessIntent、ParentBusinessEntityId、空间草稿等；具体请求 class 待定）。业务门面创建实体、空间、经营关系及必要 Lease/首期；有效 Lease 必须先于租户建筑/家具真写入。不要求手动绑定或创建后补建生意。
7. 子区域创建只写空间并关联现有主体。NPC 招商在同一连续面板预览空店/模板、确认候选与条款/产权授权，先 Lease 再真落位；V1 只预置商圈玩家标准租杂货店，NPC 模板招商及完整客房留后续。
8. 初始两钱包/业务账户由人物持有。住客只是限时房间/设施使用者，不是客房 Manager；区域门查询有效授权。
9. 换租户沿用同一经营实体、删区域不注销实体（已定）；招图外换工待定。空间卸载/删除不自动注销经营实体，也不转移租户资产。

### 1.1 整体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ 应用接口层（Application Interface）                           │
│ - VoxelPlayerController（玩家控制器）                        │
│ - AI系统（AI System）                                        │
│ - 物品系统（Item System）                                     │
│   ├── 区域查询接口                                            │
│   ├── 区域修改请求                                            │
│   └── 区域创建管理                                            │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 核心查询层（Core Query Layer）                               │
│ - GetAreaIndexByPosition（位置查询接口）                      │
│ - GetAreaActor（安全获取接口）                                │
│ - FindAreasByType（类型查询接口）                            │
│ - IsAreaConflicting（冲突检测接口）                           │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 生命周期层（Lifecycle Layer）                                │
│ - GenerateAreas（批量生成）                                  │
│ - GenerateAreaInstance（单区域生成）                         │
│ - UpdateAreaByPtr（区域修改）                                │
│ - CleanAreaFromIndexById（区域删除）                         │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 数据管理层（Data Management Layer）                          │
│ ┌──────────────────────────────────────────────┐          │
│ │ 区域索引管理（Area Index）                    │          │
│ │ ├── AreaIndex：ID→Actor映射                   │          │
│ │ ├── SpawnArea：生成区域映射                   │          │
│ │ ├── ConnectAreaIndex：连接区域映射             │          │
│ │ └── ChunkToAreaMap：空间索引                  │          │
│ └──────────────────────────────────────────────┘          │
│ ┌──────────────────────────────────────────────┐          │
│ │ 空间哈希系统（Spatial Hash）                 │          │
│ │ ├── 分块管理（16×16×16）                    │          │
│ │ ├── 索引更新                              │          │
│ │ └── 候选区域查找                          │          │
│ └──────────────────────────────────────────────┘          │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 辅助服务层（Helper Services）                               │
│ ├── 权限验证（Permission Check）                             │
│ ├── 数据验证（Input Validation）                             │
│ ├── 导航更新（Navigation Update）                            │
│ └── 可视化控制（Visual Control）                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 空间哈希架构

```
┌─────────────────────────────────────────────────────────────┐
│ 空间索引结构（Spatial Index Structure）                      │
│                                                             │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 空间分块管理（Chunk Management）                        │ │
│ │                                                         │ │
│ │ ┌─────────────────┐ ┌─────────────────┐  ...            │ │
│ │ │ [0,0,0] Chunk   │ │ [1,0,0] Chunk   │                │ │
│ │ │ ├─ Area ID: 5   │ │ ├─ Area ID: 12  │                │ │
│ │ │ ├─ Area ID: 8   │ │ ├─ Area ID: 15  │                │ │
│ │ │ └─ Area ID: 20  │ │ └─ Area ID: 18  │                │ │
│ │ └─────────────────┴─────────────────┘  ...            │ │
│ │                                                         │ │
│ │ ┌─────────────────┐ ┌─────────────────┐  ...            │ │
│ │ │ [0,1,0] Chunk   │ │ [1,1,0] Chunk   │                │ │
│ │ │ ├─ Area ID: 3   │ │ ├─ Area ID: 7   │                │ │
│ │ │ └─ Area ID: 11  │ │ └─ Area ID: 22  │                │ │
│ │ └─────────────────┴─────────────────┘  ...            │ │
│ │                                                         │ │
│ │  ... (无限扩展的3D网格)                                 │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ └────────────────────────┬─────────────────────────────────┘ │
│                          ↓ 区域查询                          │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 候选区域选择（Candidate Selection）                     │ │
│ │                                                         │ │
│ │ 输入：世界坐标 (X, Y, Z)                                │ │
│ │ 1. 计算：ChunkCoord = (X/16, Y/16, Z/16)               │ │
│ │ 2. 查询：ChunkToAreaMap[ChunkCoord] → CandidateIDs     │ │
│ │ 3. 精确检测：遍历候选区域，找出包含该点的区域             │ │
│ │ 4. 优先级：选择体积最小的区域（子区域优先）              │ │
│ │                                                         │ │
│ │ 输出：唯一的 Area ID                                    │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 核心组件职责

#### **应用接口层**
- **VoxelPlayerController**：接收玩家编辑操作，验证权限
- **AI系统**：查询AI所在区域，获取区域信息用于行为决策
- **物品系统**：查询物品所在区域，管理区域内的物品列表

#### **核心查询层**
- **GetAreaIndexByPosition**：两阶段查询，空间哈希快速定位
- **GetAreaActor**：安全获取区域Actor指针，带有效性检查
- **FindAreasByType**：根据GameplayTag查询特定类型区域
- **IsAreaConflicting**：检测新区域与现有区域的冲突

#### **生命周期层**
- **GenerateAreas**：从存档数据批量重建所有区域
- **GenerateAreaInstance**：运行时动态创建新区域
- **UpdateAreaByPtr**：修改现有区域的属性
- **CleanAreaFromIndexById**：删除空间并清理空间索引/表现；不自动注销实体、合同或产权日志，关联注销规则待定

#### **数据管理层**
- **AreaIndex**：当前场景 AreaId 到 Actor 的空间映射；跨场景用 AreaRef，不是全局经营登记
- **SpawnArea**：专门用于生成区域的索引
- **ConnectAreaIndex**：连接区域的索引
- **ChunkToAreaMap**：空间索引，加速位置查询

#### **辅助服务层**
- **权限验证**：检查玩家是否有权限创建/修改区域
- **数据验证**：验证区域数据的完整性和合法性
- **导航更新**：通知导航系统更新区域边界
- **可视化控制**：控制区域边界框的显示/隐藏

---

## 2. 接口定义

### 2.1 生成管理接口

#### **GenerateAreas - 批量生成**

```cpp
/**
 * 从存档数据生成所有区域
 * @param SceneData 包含所有区域数据的场景存档
 */
void GenerateAreas(FSceneData& SceneData)
{
    // 遍历存档中的区域数据
    for (const auto& AreaData : SceneData.Areas)
    {
        // 1. 创建区域Actor
        AVoxelAreaBaseInstance* AreaInstance = CreateAreaInstance(AreaData.Transform);

        // 2. 设置区域数据
        SetupAreaInstance(AreaInstance, AreaData, GetParentArea(AreaData.ParentId));

        // 3. 注册到空间索引
        RegisterAreaToSpatialMap(AreaInstance);

        // 4. 最终化实例
        FinalizeAreaInstance(AreaInstance);
    }

    // 重建空间索引（确保一致性）
    RebuildSpatialIndex();
}
```

#### **GenerateAreaInstance - 单区域生成**

```cpp
/**
 * 运行时动态生成新区域
 * @param ModificationData 区域修改数据
 * @param CurSelectedArea 当前选中的区域（作为父区域）
 * @param Controller 玩家控制器（用于权限验证）
 * @return 新创建的区域Actor，失败返回nullptr
 */
AVoxelAreaBaseInstance* GenerateAreaInstance(
    const FAreaModificationData& ModificationData,
    AVoxelAreaBaseInstance* CurSelectedArea,
    AVoxelPlayerController* Controller)
{
    // 1. 权限检查
    if (!CheckGenerateAreaPermission(Controller, ModificationData))
    {
        return nullptr;
    }

    // 2. 输入验证
    FAreaInstance AreaInfo = GenerateAreaInfo(ModificationData);
    if (!ValidateAreaInput(AreaInfo))
    {
        return nullptr;
    }

    // 3. 冲突检测
    if (IsAreaConflicting(AreaInfo))
    {
        return nullptr;
    }

    // 4. 创建Actor
    AVoxelAreaBaseInstance* AreaInstance = CreateAreaInstance(AreaInfo.Transform);
    if (!AreaInstance)
    {
        return nullptr;
    }

    // 5. 设置实例数据
    SetupAreaInstance(AreaInstance, AreaInfo, CurSelectedArea);

    // 6. 注册到索引
    RegisterAreaToSpatialMap(AreaInstance);

    // 7. 最终化
    FinalizeAreaInstance(AreaInstance);

    return AreaInstance;
}
```

### 2.2 查询接口

#### **GetAreaIndexByPosition - 位置查询**

```cpp
/**
 * 根据世界网格坐标获取区域ID
 * 实现子区域优先查询策略
 * @param BlockPos 世界网格坐标
 * @return 区域ID，未找到返回-1
 */
int32 GetAreaIndexByPosition(FIntVector BlockPos) const
{
    // 1. 空间哈希定位候选区域
    FIntVector ChunkCoord = GetSpatialChunkCoord(BlockPos);

    if (!ChunkToAreaMap.Contains(ChunkCoord))
    {
        return INVALID_AREA_ID;
    }

    const TArray<int32>& CandidateAreas = ChunkToAreaMap[ChunkCoord];

    // 2. 精确检测并返回体积最小的包含区域
    AVoxelAreaBaseInstance* SmallestArea = nullptr;
    int32 SmallestVolume = INT_MAX;

    for (int32 AreaId : CandidateAreas)
    {
        AVoxelAreaBaseInstance* Area = GetAreaActor(AreaId);
        if (!Area) continue;

        // 检查是否包含该点
        if (Area->AreaBase.Coverage.Contains(FVector(BlockPos)))
        {
            // 子区域优先：选择体积最小的区域
            FBox Bounds = Area->AreaBase.Coverage;
            int32 Volume = Bounds.Volume();

            if (Volume < SmallestVolume)
            {
                SmallestVolume = Volume;
                SmallestArea = Area;
            }
        }
    }

    return SmallestArea ? SmallestArea->AreaBase.Id : INVALID_AREA_ID;
}
```

#### **GetAreaActor - 安全获取**

```cpp
/**
 * 安全获取区域Actor指针
 * @param AreaId 区域ID
 * @return 区域Actor指针，无效ID返回nullptr
 */
AVoxelAreaBaseInstance* GetAreaActor(int32 AreaId) const
{
    if (AreaId == INVALID_AREA_ID)
    {
        return nullptr;
    }

    AVoxelAreaBaseInstance* Area = AreaIndex.FindRef(AreaId);
    return Area ? Area : nullptr;
}
```

#### **FindAreasByType - 类型查询**

```cpp
/**
 * 根据类型查找区域
 * @param InputType 区域类型（GameplayTag）
 * @return 匹配的区域列表
 */
TArray<AVoxelAreaBaseInstance*> FindAreasByType(FGameplayTag InputType) const
{
    TArray<AVoxelAreaBaseInstance*> Result;

    for (const auto& Pair : AreaIndex)
    {
        AVoxelAreaBaseInstance* Area = Pair.Value;
        if (Area && Area->AreaBase.AreaType.MatchesTagExact(InputType))
        {
            Result.Add(Area);
        }
    }

    return Result;
}
```

### 2.3 更新接口

#### **UpdateAreaByPtr - 指针修改**

```cpp
/**
 * 通过指针修改区域属性
 * @param Controller 操作玩家控制器
 * @param Area 要修改的区域Actor
 * @param ModificationData 修改数据
 */
void UpdateAreaByPtr(
    AVoxelPlayerController* Controller,
    AVoxelAreaBaseInstance* Area,
    const FAreaModificationData& ModificationData)
{
    if (!Area || !Controller)
    {
        return;
    }

    // 1. 权限检查
    if (!CheckModificationPermission(Controller, Area))
    {
        return;
    }

    // 2. 验证修改数据
    if (!ValidateAreaInput(ModificationData))
    {
        return;
    }

    // 3. 从空间索引移除
    UnregisterAreaFromSpatialMap(Area);

    // 4. 更新数据
    Area->AreaBase.Coverage = ModificationData.Coverage;
    Area->AreaBase.Name = ModificationData.Name;
    // 仅空间字段；不映射 Owner/Manager、合同或营业数据

    // 5. 重新注册到空间索引
    RegisterAreaToSpatialMap(Area);

    // 6. 更新可视化
    UpdateAreaVisual(Area, true);

    // 7. 更新导航系统
    UpdateNavigationSystem(Area);
}
```

#### **IsAreaConflicting - 冲突检测**

```cpp
/**
 * 检测区域冲突
 * @param Input 要检测的区域数据
 * @return true表示有冲突，false表示无冲突
 */
bool IsAreaConflicting(FAreaInstance& Input) const
{
    for (const auto& Pair : AreaIndex)
    {
        AVoxelAreaBaseInstance* ExistingArea = Pair.Value;
        if (!ExistingArea) continue;

        // 跳过自身
        if (ExistingArea->AreaBase.Id == Input.Id)
        {
            continue;
        }

        // 检查空间重叠
        if (ExistingArea->AreaBase.Coverage.Overlap(Input.Coverage))
        {
            // 检查父子关系（父子区域可以重叠）
            if (ExistingArea->AreaBase.Id != Input.ParentAreaId &&
                Input.ParentAreaId != ExistingArea->AreaBase.Id)
            {
                return true;
            }
        }
    }

    return false;
}
```

---

## 3. 数据流

### 3.1 区域生成流程

```
区域生成请求（FSceneData 或 FAreaModificationData）
    ↓
权限验证（CheckGenerateAreaPermission）
    ├─ 验证玩家权限
    ├─ 验证区域类型合法性
    └─ 验证特殊区域要求
    ↓
输入验证（ValidateAreaInput）
    ├─ 验证区域边界有效性
    ├─ 验证区域类型配置
    └─ 验证父子关系约束
    ↓
冲突检测（IsAreaConflicting）
    ├─ 检查与现有区域重叠
    ├─ 检查父子关系约束
    └─ 检查特殊规则（如建筑类型）
    ↓
创建区域实例
    ├─ 创建Actor（CreateAreaInstance）
    ├─ 设置组件（SetupAreaInstance）
    ├─ 设置父子关系
    └─ 初始化功能组件
    ↓
注册到空间索引（RegisterAreaToSpatialMap）
    ├─ 计算覆盖的空间分块
    ├─ 添加到ChunkToAreaMap
    └─ 更新候选区域列表
    ↓
最终化（FinalizeAreaInstance）
    ├─ 更新可视化
    ├─ 更新导航系统
    └─ 触发事件通知
```

### 3.2 空间查询流程

```
位置查询请求（FIntVector BlockPos）
    ↓
空间哈希定位
    ├─ 计算ChunkCoord = BlockPos / 16
    ├─ 查询ChunkToAreaMap
    └─ 获取候选区域ID列表
    ↓
精确碰撞检测
    ├─ 遍历候选区域
    ├─ 检查BlockPos是否在区域内
    └─ 计算区域体积
    ↓
子区域优先选择
    ├─ 选择体积最小的包含区域
    └─ 处理边界特殊情况
    ↓
返回区域ID
    ├─ 转换为整数返回
    └─ 处理未找到情况
```

### 3.3 区域更新流程

```
区域更新请求（UpdateAreaByPtr）
    ↓
权限检查（CheckModificationPermission）
    ├─ 验证操作者权限
    ├─ 查询关联实体的有效经营关系/授权（区域无所有权真值）
    └─ 验证修改范围
    ↓
数据验证（ValidateAreaInput）
    ├─ 验证新边界有效性
    ├─ 验证类型一致性
    └─ 验证数据完整性
    ↓
空间索引更新
    ├─ 从索引中移除（Unregister）
    ├─ 应用修改（更新属性）
    ├─ 重新注册（Register）
    └─ 批量重建支持
    ↓
可视化更新
    ├─ 更新边界框显示
    ├─ 更新组件状态
    └─ 触发UI刷新
    ↓
导航系统更新
    ├─ 更新NavModifier
    ├─ 重新计算导航
    └─ 同步AI系统
```

### 3.4 空间索引维护流程

```
区域位置变化（创建/更新/删除）
    ↓
索引操作准备
    ├─ 计算影响的空间分块
    ├─ 确定更新策略（增量/重建）
    └─ 锁定相关数据
    ↓
空间索引更新
    ├─ 添加：Region → ChunkToAreaMap
    ├─ 删除：Region ← ChunkToAreaMap
    └─ 修改：先删除后添加
    ↓
索引一致性验证
    ├─ 检查双向引用完整性
    ├─ 验证区域数量一致性
    └─ 检测孤立区域
    ↓
性能优化处理
    ├─ 延迟索引重建
    ├─ 批量操作合并
    └─ 内存使用优化
```

---

## 4. 生命周期管理

### 4.1 区域生命周期状态

```
未创建（NotCreated）
  ↓ 玩家创建请求
创建中（Creating）
  ├─ 权限验证
  ├─ 数据准备
  └─ Actor创建
  ↓ 完成创建
已激活（Active）
  ├─ 接收查询请求
  ├─ 维护空间索引
  └── 更新可视化
  ↓ 修改请求
更新中（Updating）
  ├─ 权限检查
  ├─ 数据更新
  └─ 索引重建
  ↓ 完成更新
已激活（Active）
  ↓ 删除请求
销毁中（Destroying）
  ├─ 从索引移除
  ├─ 组件清理
  └─ Actor销毁
  ↓ 完全销毁
已清理（Cleaned）
```

### 4.2 生命周期管理接口

```cpp
// 创建阶段
void GenerateAreaInstance(...);     // 外部接口
void CreateAreaInstance(...);        // 内部创建
void SetupAreaInstance(...);        // 设置数据

// 运行时阶段
void UpdateAreaByPtr(...);           // 修改接口
void UpdateAreaVisual(...);         // 可视化更新

// 销毁阶段
void CleanAreaFromIndexById(...);   // 删除接口
void UnregisterAreaFromSpatialMap(...); // 索引清理

// 批量管理
void GenerateAreas(...);            // 批量创建
void RebuildSpatialIndex();         // 索引重建
```

### 4.3 资源管理策略

```cpp
// Actor生命周期
- 使用UObject管理内存
- 支持对象池复用
- 及时清理无效引用

// 索引管理
- 延迟重建策略
- 批量操作优化
- 内存使用监控

// 数据持久化
- 区域数据序列化
- 索引重建信息
- 状态保存恢复
```

---

## 5. 类关系图

### 5.1 核心系统关系

```
UVoxelAreaWorldSubsystem（核心子系统）
    ├─ 持有 → TMap<int, AVoxelAreaBaseInstance*> AreaIndex
    ├─ 持有 → TMap<FIntVector, TArray<int32>> ChunkToAreaMap
    ├─ 管理 → AVoxelAreaBaseInstance（区域Actor）
    └─ 依赖 → UVoxelGameInstance
```

### 5.2 与其他子系统关系

```
与ItemWorld的关系：
ItemWorldSubsystem
    ↓ 查询
GetAreaIndexByPosition / GetAreaActor
    ↓
UVoxelAreaWorldSubsystem
    ↓ 返回
区域信息（ID、类型、范围）
```

```
与BlockWorld的关系：
VoxelBlockWorldSubsystem
    ↓ 通知
区域创建/删除/修改
    ↓
UVoxelAreaWorldSubsystem
    ↓ 更新
空间索引、可视化、导航
```

```
与AI系统关系：
AI系统
    ↓ 查询
AI当前位置区域
    ↓
UVoxelAreaWorldSubsystem
    ↓ 返回
区域类型、权限、边界
```

### 5.3 数据结构关系

```
FAreaInstance（区域数据结构）
    ├─ Id：区域唯一ID
    ├─ Name：区域名称
    ├─ AreaType：区域类型
    ├─ Coverage：空间范围
    ├─ ParentAreaId：父区域ID
    ├─ CreatorId：创建来源记录，不赋予产权/经营权
    └─ Components：空间/导航/表现组件，不含经营模块

AVoxelAreaBaseInstance（区域Actor）
    ├─ AreaBase：FAreaInstance数据
    ├─ ParentArea：父区域Actor引用
    ├─ SubAreasIndex：子区域映射
    └─ Components：功能组件集合

空间索引结构
    ├─ Key：FIntVector（空间分块坐标）
    └─ Value：TArray<int32>（区域ID列表）
```

---

## 6. 扩展点

### 6.1 新的区域类型支持

#### **步骤1：定义区域类型**
```cpp
// 在配置表中定义新的区域类型
FAreaTypeBase NewAreaType;
NewAreaType.AreaType = FGameplayTag::RequestGameplayTag("Area.Custom");
NewAreaType.DisplayName = "自定义区域";
NewAreaType.DefaultSize = FVector(100, 100, 300);
NewAreaType.AllowedParentTypes.Add(FGameplayTag::RequestGameplayTag("Area.Building"));
NewAreaType.RequiredComponents.Add("NavModifier");
```

#### **步骤2：扩展Actor类**
```cpp
class AVoxelCustomAreaInstance : public AVoxelAreaBaseInstance
{
    GENERATED_BODY()

public:
    // 自定义功能组件
    UPROPERTY(VisibleAnywhere)
    UCustomFunctionComponent* CustomComponent;

    // 自定义行为
    void OnCustomAreaActivated();
    void OnCustomAreaDeactivated();
};
```

#### **步骤3：注册到系统**
```cpp
// 在ConfigDataSubsystem中注册
ConfigSubsystem->RegisterAreaType(NewAreaType);
```

### 6.2 空间哈希粒度调整

```cpp
// 修改空间分块大小
class UVoxelAreaWorldSubsystem
{
private:
    // 可配置的空间分块大小
    UPROPERTY(Config)
    int32 SpatialChunkSize = 16;

    // 根据地图大小自动调整
    void OptimizeSpatialChunkSize()
    {
        if (WorldBounds > 10000)
        {
            SpatialChunkSize = 32; // 大地图使用更大的分块
        }
    }
};
```

### 6.3 新的查询策略

#### **添加查询模式枚举**
```cpp
enum class EAreaQueryMode
{
    Smallest,       // 默认：子区域优先
    Largest,        // 选择最大的包含区域
    First,          // 选择第一个包含的区域
    Nearest,        // 选择最近的区域
    Custom          // 自定义策略
};

// 扩展查询接口
int32 GetAreaIndexByPosition(FIntVector BlockPos, EAreaQueryMode Mode);
```

#### **实现自定义策略**
```cpp
int32 GetAreaIndexByPosition_Custom(FIntVector BlockPos)
{
    // 实现自定义选择逻辑
    // 例如：选择创建时间最久远的区域
    // 或选择具有特定属性的区域
}
```

### 6.4 区域功能扩展

#### **添加新的事件系统**
```cpp
// 区域事件基类
class FAreaEventBase
{
public:
    virtual void OnAreaCreated(AVoxelAreaBaseInstance* Area) {}
    virtual void OnAreaModified(AVoxelAreaBaseInstance* Area) {}
    virtual void OnAreaDestroyed(AVoxelAreaBaseInstance* Area) {}
};

// 注册事件处理器
void RegisterEventHandler(TSharedPtr<FAreaEventBase> Handler);
```

#### **空间状态与经营状态分离**

区域只维护空间注册、更新、卸载状态，不能新增经营 Planning/Closed 或 bOpen 载荷。经营生命周期及营业状态由全局实体业务维护；bOperating 只按 bOpen&&有效时段 派生。空间关闭/卸载与经营注销不绑定，删除区域的具体关联处理待定。

### 6.5 性能优化扩展

#### **查询缓存系统**
```cpp
// 查询结果缓存
struct FAreaQueryCache
{
    FIntVector Position;
    int32 AreaId;
    FDateTime Timestamp;
};

class UVoxelAreaWorldSubsystem
{
private:
    TMap<FIntVector, FAreaQueryCache> QueryCache;
    float CacheLifetime = 5.0f; // 5秒缓存
};
```

#### **异步区域加载**
```cpp
// 异步加载区域
void AsyncLoadArea(int32 AreaId, TFunction<void(AVoxelAreaBaseInstance*)> CompletionCallback);
```

---

## 7. 相关文档

### 7.1 子系统详细文档

- [VoxelAreaWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelAreaWorldSubsystem-系统设计.md)
- [编辑系统-系统设计](/VoxelDesign/02-系统设计/编辑系统-系统设计.md)

### 7.2 核心架构文档

- [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md)
- [ConfigDataSubsystem-架构设计](/VoxelDesign/03-架构设计/ConfigDataSubsystem-架构设计.md)

### 7.3 其他相关文档

**数据结构**：
- [FVoxelGeometryDesc-系统设计](/VoxelDesign/02-系统设计/编辑系统-FVoxelGeometryDesc-系统设计.md)
- [区域系统-完整设计](/VoxelDesign/02-系统设计/区域系统-完整设计.md)（空间数据与业务交界）

**集成接口**：
- 导航架构：暂无独立文档，多盒导航方案待定。
- [AI决策系统-架构设计](/VoxelDesign/03-架构设计/AI决策系统-架构设计.md)

**性能优化**：
- 空间索引/查询性能独立技术方案暂无文件，现有设计见本文空间哈希章节，不保留失效链接。

---

**文档版本**: 1.0
**最后更新**: 2026-11-04
**状态**: 目标架构；经营/空间分离与组合创建待迁移，本次未实现代码