# 编辑系统 Controller 组件重组架构设计

> **文档目的**：重构 VoxelPlayerController 组件职责划分
> **创建日期**：2026-08-23
> **版本**：v2.0（重写，放弃 v1.x 按名字分堆的方案）
> **状态**：待评审
> **原型参考**：`.superpowers/brainstorm/vs-20260807/content/voxel-ui-v6.html`

---

## 一、v1.x 为什么错了

v1.x 把状态按"名字像不像"分堆：InHand 叫"选中"就塞 Selection，CurArea 叫"区域"就塞 Selection，BuildPhase 叫"阶段"就塞 Interaction。结果是四个"选中作用域"互相重叠、EditingArea 与 CurArea 同义反复。

**先回答"这个游戏里到底存在几类状态"这个本质问题，再谈放哪。**

## 二、状态分类（按语义，不按名字）

| # | 状态 | 本质 | 谁关心 | 举例 |
|---|------|------|--------|------|
| 1 | **模式** | 玩家现在处于什么游戏状态，决定输入语义 | 输入分发器 | 冒险/管理/电影/场景建造 |
| 2 | **输入意图** | 本次鼠标/键盘操作想干什么 | 当前模式的处理器 | 左键=查看/框选/放置/攻击；右键=取消/清除/返回 |
| 3 | **世界实体选中** | 玩家查看了场景中哪个已存在的实体 | 详情面板、高亮、编辑工具 | 区域A、生物B、家具C |
| 4 | **手持物** | 玩家从物品库选了什么，准备放置/使用 | 手持槽UI、预览渲染、放置工具 | 拿了一块石砖，准备铺地 |
| 5 | **编辑参数** | 当前工具的工艺参数（用户调的数值） | 工具执行、参数UI、预览 | 厚度=3、方向=前后、笔刷=圆形 |
| 6 | **编辑目标** | 体素选区/几何范围（要改哪里） | 工具执行、选区渲染 | 框选了 12×18×8 的区域 |
| 7 | **工具激活** | 当前激活哪个工具 | 工具路由、工具面板高亮 | 区域编辑工具 |
| 8 | **编辑流程状态** | 两点编辑走到哪一步 | 工具状态机、预览 | IDLE/PLACING_FIRST/EDITING |

## 三、组件划分（2 个，不是 5 个）

### 3.1 UVoxelInteractionComponent（交互逻辑组件）

**职责**：接收输入 → 解释意图 → 分发到正确的处理器。所有"输入怎么翻译成游戏行为"的逻辑都在这里。

**持有**：模式（Adventure/Manage/Film/SceneBuild）、编辑流程状态（IDLE/PLACING/EDITING）、当前工具 Tag、家具拖拽状态机。

**不持有**：任何"选中"状态、任何编辑参数。

**核心接口**：

```cpp
// 模式（决定输入语义）
void SetControllerMode(EVoxelControllerMode Mode);
EVoxelControllerMode GetControllerMode() const;

// 输入入口（Controller 蓝图转发）
void HandleMouseInput();    // 左键
void HandleMouseDrag();
void HandleMouseRelease();
void HandleMouseCancel();   // 右键
void HandleMouseHover();

// 工具激活
void SetCurrentTool(FGameplayTag ToolTag);

// 编辑流程状态（两点编辑）
void SetBuildPhase(EPreviewInteractionState Phase);
EPreviewInteractionState GetBuildPhase() const;
```

**分发逻辑**（HandleMouseInput 伪码）：

```
模式 = GetControllerMode()

左键点击：
  ├─ 冒险/管理模式：射线命中实体？
  │    ├─ 是 → 写"世界实体选中" → 广播
  │    └─ 否 → 清除"世界实体选中"（点空地=取消查看）
  │
  ├─ 场景建造模式：当前工具需要什么？
  │    ├─ 两点编辑工具（区域/几何/剖切框）：
  │    │    IDLE → 记第一个点，BuildPhase=PLACING_FIRST
  │    │    PLACING_FIRST → 记第二个点，BuildPhase=EDITING，进入编辑
  │    ├─ 放置工具（单格/家具/模板）：
  │    │    读"手持物" + 读"编辑参数" → 执行放置
  │    └─ 选择工具：
  │         框选体素 → 写"编辑目标"
  │
  └─ 电影模式：忽略（或只响应相机控制）

右键：
  ├─ 编辑中（BuildPhase != IDLE）→ 取消编辑，BuildPhase=IDLE
  ├─ 有"世界实体选中" → 清除选中
  ├─ 有父级区域 → 返回父级
  └─ 无操作
```

### 3.2 UVoxelPlayerStateCacheComponent（玩家状态缓存组件）

**职责**：玩家侧所有"我选了什么/我拿了什么/我调了什么参数"的缓存。纯数据，无逻辑。

**持有**：

```cpp
// === 世界实体选中（我查看了场景中的哪个实体） ===
struct FSelectedEntity {
    EVoxelSelectedEntityType Type;   // Area / Creature / Furniture / None
    int32 Id;                         // AreaId / CreatureId / HandleId
};
FSelectedEntity SelectedEntity;
// 派生：选中区域时，建造工具的编辑对象自动跟随（IDLE 守卫）

// === 手持物（我从物品库选了什么准备放置） ===
FInteractableData HandItem;  // HandleId + Quantity + InteractableType + Direction

// === 编辑参数（当前工具的工艺参数） ===
FVoxelGeometryDesc EditGeometry;      // 两点/厚度/旋转/规则化
FAreaModificationData EditAreaInfo;   // 区域业务数据草稿
// 笔刷、切面等

// === 编辑目标（体素选区/几何范围） ===
TArray<FIntVector> VoxelSelection;    // 体素选区
FPlacementSelection PlacementSelection; // 放置选区
// 剪贴板
```

**事件**（细粒度）：

```cpp
OnSelectedEntityChanged    // 实体选中变化 → 详情面板刷新 + 高亮更新
OnHandItemChanged          // 手持物变化 → 手持槽UI + 预览渲染
OnEditGeometryChanged      // 编辑参数变化 → 参数UI + 预览重建
OnSelectionChanged         // 体素选区变化 → 选区渲染
```

**关键设计**：这个组件只做"存"和"广播"，不做任何逻辑判断。所有"该不该更新"的逻辑在 InteractionComponent 里决定。

## 四、为什么这样分（对比 v1.x）

| v1.x 错误 | v2.0 修正 | 依据 |
|-----------|-----------|------|
| InHand 归"选中" | InHand 归"状态缓存"，与实体选中平级但不同字段 | InHand 是"手持物品实例"（继承 FVoxelItemInstanceBase），不是"选中实体"。它俩唯一的共同点是都是玩家侧状态 |
| EditingArea vs CurArea | 删除 EditingArea 概念，直接用 SelectedEntity.Type==Area 时的 Id | 不需要两个名字指同一个东西。建造工具需要"当前编辑哪个区域"时，直接问 `SelectedEntity` |
| BuildPhase/ToolTag 归 Interaction | BuildPhase/ToolTag 归 Interaction（输入解释需要），但**存储**在 StateCache | Interaction 是逻辑组件，StateCache 是数据组件。Interaction 读写 StateCache，不自己持有 |
| 5 个组件 | 2 个组件 | 减少边界，降低"放哪"的决策成本 |

## 五、ToolRegistry 全局化

**现状**：`UVoxelToolComponent` 持有 `TMap<FGameplayTag, UVoxelToolBase*> ToolRegistry`，每个 Controller 各建 12 个工具实例。

**问题**：工具无状态（状态全在 StateCache/世界子系统，经 `FVoxelToolContext` 传入），注册表是"世界有哪些工具"的静态能力，不是玩家私产。

**方案**：`UVoxelToolWorldSubsystem`（World 子系统）

```cpp
UCLASS()
class VOXEL_API UVoxelToolWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UVoxelToolBase* FindTool(FGameplayTag ToolTag) const;
private:
    void RegisterTools();  // 12 工具 NewObject，Outer=this，生命周期=World
    TMap<FGameplayTag, TObjectPtr<UVoxelToolBase>> ToolRegistry;
};
```

**InteractionComponent 使用**：

```cpp
void UVoxelInteractionComponent::HandleMouseInput()
{
    if (GetControllerMode() == EVoxelControllerMode::SceneBuildMode)
    {
        UVoxelToolBase* Tool = GetWorld()->GetSubsystem<UVoxelToolWorldSubsystem>()->FindTool(CurrentToolTag);
        if (Tool)
        {
            FVoxelToolContext Context = BuildContext();  // 组装 StateCache/Controller/World 引用
            Tool->OnMouseInput(Context, SharedMouseHit);
        }
    }
}
```

## 六、迁移后的引用关系

```
蓝图 IA → VoxelPlayerController::OnXXXInput
              ↓
    InteractionComponent::HandleMouseXXX
              ↓
    ├─ 读模式 → 决定输入语义
    ├─ 读 BuildPhase → 决定两点编辑走到哪步
    ├─ 读 CurrentToolTag → 查 ToolSubsystem 取工具实例
    ├─ 读写 StateCache（SelectedEntity/HandItem/EditGeometry/VoxelSelection）
    └─ 调工具方法（传入 Context）
              ↓
    工具读写 StateCache / 世界子系统
              ↓
    StateCache 广播 → UI/Preview 订阅刷新
```

## 七、与现有代码的映射

| 现有 | 迁移到 | 说明 |
|------|--------|------|
| `UVoxelToolComponent::ToolRegistry` | `UVoxelToolWorldSubsystem::ToolRegistry` | 全局化 |
| `UVoxelToolComponent::HandleMouseInput` 等 | `UVoxelInteractionComponent::HandleMouseXXX` | 输入逻辑 |
| `UVoxelBuildStateComponent::PlayerMode` | `UVoxelInteractionComponent::PlayerMode`（逻辑）+ `UVoxelPlayerStateCacheComponent`（如需缓存） | 模式是输入语义，归交互逻辑 |
| `UVoxelBuildStateComponent::BuildPhase` | `UVoxelInteractionComponent`（逻辑）+ `StateCache`（存储） | 两点编辑状态机 |
| `UVoxelBuildStateComponent::CurrentToolTag` | `UVoxelInteractionComponent` | 工具激活是交互状态 |
| `UVoxelBuildStateComponent::CurArea` | `StateCache::SelectedEntity`（Type==Area 时的 Id） | 删除 EditingArea 概念 |
| `UVoxelBuildStateComponent::InteractableDataInHand` | `StateCache::HandItem` | 手持物，不是选中实体 |
| `UVoxelBuildStateComponent::TmpGeometry/TmpAreaInfo` | `StateCache::EditGeometry/EditAreaInfo` | 编辑参数 |
| `UVoxelSelectionComponent`（全量） | `StateCache::VoxelSelection/PlacementSelection/Clipboard` | 体素选区 |
| `UVoxelEntitySelectionComponent`（全量） | `StateCache::SelectedEntity` + 高亮逻辑 | 实体选中 |
| `UVoxelToolComponent::FurnitureDragState` | `UVoxelInteractionComponent` | 拖拽状态机是输入逻辑 |
| `UVoxelToolComponent::BuildToolContext` | `UVoxelInteractionComponent` | 组装 Context 需要 Controller 引用 |
| `UVoxelPreviewComponent` | 不动 | 呈现层已正交 |
| `UVoxelHistoryComponent` | 不动 | 历史层已正交 |

## 八、命名规范（避免再混）

| 概念 | 变量名 | 说明 |
|------|--------|------|
| 模式 | `PlayerMode` / `ControllerMode` | 冒险/管理/电影/建造 |
| 编辑流程状态 | `BuildPhase` | IDLE/PLACING_FIRST/EDITING |
| 当前工具 | `CurrentToolTag` | GameplayTag |
| 世界实体选中 | `SelectedEntity` | {Type, Id}，Type∈{Area,Creature,Furniture,None} |
| 手持物 | `HandItem` | FInteractableData |
| 编辑参数 | `EditGeometry` / `EditAreaInfo` | 厚度/方向/区域数据草稿 |
| 体素选区 | `VoxelSelection` | TArray<FIntVector> |

**禁用词**：`InHand`（太模糊）、`EditingArea`（与 CurArea 同义反复）、`CurArea`（改用 SelectedEntity.Id when Type==Area）。

## 九、实施顺序（已确认）

**排序原则**：风险递增；每阶段独立编译验证；引用内聚度高的先做。

### 阶段 1：ToolSubsystem（全局工具注册表）——最孤立，先行

- 新建 `VoxelToolWorldSubsystem.h/cpp`（GameManager/WorldManager/）
- 迁移 `RegisterTools()`（12 工具，Outer=子系统）/ `FindTool` / `GetAllTools`
- ToolComponent 内部改薄转发（FindTool→Subsystem；BeginPlay 的 RegisterTools 调用删除，子系统 Initialize 更早执行，顺带消除"启动期注册表为空"边界）
- 依据：注册表引用 100% 内聚在 ToolComponent 内部（30 处全在此文件），工具类/UI/Context/蓝图零改动
- 验证门：全量编译 + PIE 工具切换/执行正常

### 阶段 2：StateCache（玩家状态缓存）

- 新建 `UVoxelPlayerStateCacheComponent`
- 吸收：EntitySelectionComponent 全量（消亡）+ SelectionComponent 全量（消亡）+ BuildState 数据段（CurArea/CurParentArea/InHand/TmpGeometry/TmpAreaInfo/笔刷/切面）
- BuildState 暂留 PlayerMode/BuildPhase（Interaction 未建，阶段 3 随迁随删；避免二次迁移）
- 影响面：UI/工具/外围 ~65 文件机械替换（编译器逐一暴露）
- 验证门：全量编译 + 选中链路三同步（右键清空/高亮/详情面板）

### 阶段 3：Interaction（交互逻辑）+ 双组件消亡

- 新建 `UVoxelInteractionComponent`
- 吸收：ToolComponent 输入段（HandleMouse*/Trace*/家具拖拽状态机/执行门面）+ BuildState 残余（PlayerMode/BuildPhase）+ CurrentToolTag
- Controller 转发目标替换（入口签名不变，蓝图零改动）
- 删除：ToolComponent、BuildState（至此 4 旧组件全部消亡）
- 验证门：全量编译 + PIE 全流程回归（漫游点选/框选/家具拖拽/区域编辑/模板放置/切工具）

### 阶段 4：文档同步

Controller 架构 v3.0 + 工具系统/选区系统文档补注。

## 十、评审要点

1. **2 个组件是否够**？Interaction（逻辑）+ StateCache（数据），还是仍需要拆出第三个？
2. **SelectedEntity 派生编辑对象**：建造工具需要"当前编辑哪个区域"时，直接读 `SelectedEntity.Type==Area ? SelectedEntity.Id : -1`，是否需要额外的守卫/缓存？
3. **ToolSubsystem 放 World 还是 GameInstance**？工具需要访问 World 子系统（区块/区域/生物），World 子系统作用域匹配；但 GameInstance 子系统可以跨 World 复用工具实例。
4. **右键语义**：编辑中→取消编辑 > 有选中→清选中 > 有父级→返层级，优先级是否符合预期？
