# VoxelEditorModeSubsystem-系统设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 系统上下文

### 1.1 系统边界

**VoxelEditorModeSubsystem负责**：
- 编辑器工具类的注册和管理
- 编辑器模式的全局状态控制
- 子系统访问接口（BlockWorld等）的统一提供
- 编辑器事件委托系统的管理
- 工具类的查询和实例化

**VoxelEditorModeSubsystem不负责**：
- 玩家编辑器状态的存储（由ControllerComponent负责）
- 具体的编辑操作实现（由各类工具负责）
- 工具的具体功能实现（由工具类自行负责）
- 玩家输入处理（由PlayerController负责）
- UI显示逻辑（由UI系统负责）
- 历史记录管理（由HistoryComponent负责）

### 1.2 外部依赖

**依赖的系统**：
- **UVoxelBlockWorldSubsystem**：提供体素世界访问接口
- **UVoxelAreaWorldSubsystem**：提供区域管理访问接口
- **UVoxelCreatureWorldSubsystem**：提供生物管理访问接口
- **UVoxelItemWorldSubsystem**：提供物品管理访问接口
- **UWorldObjectPoolSubsystem**：提供对象池访问接口

**被依赖的系统**：
- **编辑器工具**：通过子系统获取所需接口
- **UI系统**：查询可用工具列表
- **蓝图系统**：通过蓝图库路径动态加载工具
- **控制器组件**：查询编辑器模式状态

### 1.3 输入输出

**输入**：
- 工具类注册请求（TSubclassOf<UVoxelToolBase>）
- 编辑器模式切换请求（Enter/Exit）
- 工具查询请求（按名称/类型）
- 子系统访问请求

**输出**：
- 注册的工具类列表（TArray<TSubclassOf<UVoxelToolBase>>）
- 编辑器模式状态（bool）
- 子系统访问接口（UVoxelBlockWorldSubsystem*等）
- 工具实例（UVoxelToolBase*）
- 事件委托（OnEditorModeEntered/Exited）

---

## 2. 核心概念

### 2.1 基础概念

**编辑器工具类（ToolClass）**
- 定义：继承自UVoxelToolBase的编辑器工具抽象类
- 注册：通过RegisterToolClass()注册到子系统
- 管理存储：TArray<TSubclassOf<UVoxelToolBase>> RegisteredToolClasses
- 实例化：根据注册的工具类创建工具实例

**编辑器模式状态（EditorMode）**
- 定义：全局编辑器模式的激活状态
- 存储：bool bEditorModeActive
- 作用：控制编辑器相关功能是否可用
- 特性：全局状态，与具体区域无关

**子系统访问接口（Subsystem Access）**
- 定义：统一提供各类子系统的访问接口
- 类型：
  - UVoxelBlockWorldSubsystem* GetBlockWorld()
  - UVoxelAreaWorldSubsystem* GetAreaWorld()
  - UVoxelCreatureWorldSubsystem* GetCreatureWorld()
  - UVoxelItemWorldSubsystem* GetItemWorld()
  - UWorldObjectPoolSubsystem* GetObjectPool()
- 作用：避免各工具重复获取子系统

**事件委托（Delegate）**
- 定义：编辑器模式变更的事件通知机制
- 类型：
  - FSimpleMulticastDelegate OnEditorModeEntered
  - FSimpleMulticastDelegate OnEditorModeExited
- 作用：通知其他系统编辑器模式状态变更

### 2.2 组合概念

**工具注册系统**
- 注册机制：RegisterToolClass(UClass* ToolClass)
- 验证逻辑：检查工具类是否继承自UVoxelToolBase
- 存储结构：TArray<TSubclassOf<UVoxelToolBase>>
- 查询接口：
  - TArray<TSubclassOf<UVoxelToolBase>> GetAvailableTools()
  - TSubclassOf<UVoxelToolBase> GetToolClassByName(FString ToolName)

**模式控制系统**
- 激活机制：EnterEditorMode()
- 停用机制：ExitEditorMode()
- 状态查询：IsEditorModeActive()
- 幂等特性：重复调用无副作用
- 事件通知：触发对应的委托事件

**蓝图库系统**
- 路径管理：FString BlueprintLibraryPath
- 用途：动态加载蓝图编辑器工具
- 更新机制：SetBlueprintLibraryPath(FString Path)
- 访问接口：GetBlueprintLibraryPath()

**工具查询系统**
- 按名称查询：GetToolClassByName(FString ToolName)
- 按索引查询：GetToolClassByIndex(int32 Index)
- 列表查询：GetAvailableTools() / GetToolClasses()
- 工具实例化：CreateToolInstance(TSubclassOf<UVoxelToolBase> ToolClass)

### 2.3 行为概念

**工具注册流程**
```
编辑器工具注册请求
  ↓
RegisterToolClass(UClass* ToolClass)
  ↓
验证工具类继承关系
  ↓
添加到 RegisteredToolClasses 数组
  ↓
返回注册成功状态
```

**编辑器模式切换流程**
```
EnterEditorMode() / ExitEditorMode()
  ↓
检查当前状态（幂等操作）
  ↓
更新 bEditorModeActive 状态
  ↓
触发对应的事件委托
  ↓
通知其他系统状态变更
```

**工具获取流程**
```
工具查询请求
  ↓
GetToolClassByName() / GetToolClassByIndex()
  ↓
搜索 RegisteredToolClasses 数组
  ↓
返回匹配的工具类
  ↓
（可选）CreateToolInstance() 创建实例
```

**子系统访问流程**
```
子系统访问请求
  ↓
GetBlockWorld() / GetAreaWorld() 等
  ↓
通过 GetWorld()->GetSubsystem<T>() 获取
  ↓
返回子系统指针（可能为nullptr）
```

### 2.4 概念关系

```
UVoxelEditorModeSubsystem
├─ 核心状态层
│  ├─ bEditorModeActive（编辑器模式状态）
│  └─ BlueprintLibraryPath（蓝图库路径）
│
├─ 工具管理层
│  ├─ RegisteredToolClasses（注册工具类列表）
│  ├─ RegisterToolClass（工具注册）
│  └─ GetAvailableTools（工具查询）
│
├─ 模式控制层
│  ├─ EnterEditorMode（进入模式）
│  ├─ ExitEditorMode（退出模式）
│  └─ OnEditorModeEntered/Exited（事件委托）
│
├─ 子系统访问层
│  ├─ GetBlockWorld（体素世界）
│  ├─ GetAreaWorld（区域世界）
│  ├─ GetCreatureWorld（生物世界）
│  ├─ GetItemWorld（物品世界）
│  └─ GetObjectPool（对象池）
│
└─ 工具实例化层
   ├─ CreateToolInstance（工具实例创建）
   └─ 工具实例管理
```

---

## 3. 设计原则

### 3.1 架构原则

**无状态服务层**
- 只提供工具注册和查询功能
- 不存储玩家相关数据
- 不负责具体工具逻辑

**全局模式管理**
- 编辑器模式为全局状态
- 与具体区域无关
- 控制编辑器相关功能可用性

**解耦设计**
- 与ControllerComponent解耦
- 玩家状态由Component管理
- 子系统访问通过统一接口

### 3.2 接口设计原则

**幂等操作**
- EnterEditorMode() / ExitEditorMode() 重复调用无副作用
- RegisterToolClass() 重复注册同一类多次只保留一次
- 确保接口调用安全性

**统一查询接口**
- 提供多种工具查询方式（按名称、按索引、列表）
- 子系统访问统一通过GetXxxWorld()接口
- 避免直接获取子系统导致耦合

**事件驱动设计**
- 使用事件委托通知状态变更
- 其他系统可订阅状态变化
- 解耦状态变更通知逻辑

### 3.3 数据管理原则

**工具类管理**
- 使用TArray存储工具类
- 运行时动态注册
- 支持热重载（编辑器模式）

**子系统缓存**
- 每次调用时获取最新子系统
- 避免缓存子系统指针
- 确保子系统有效性

**路径管理**
- BlueprintLibraryPath可动态更新
- 支持运行时修改
- 用于蓝图工具动态加载

### 3.4 性能原则

**轻量级实现**
- 避免不必要的存储
- 使用原生容器（TArray）
- 最小化内存占用

**高效查询**
- 工具查询使用数组线性搜索
- 适合少量工具场景
- 可优化为TMap以提升性能

**按需获取**
- 子系统访问时才获取
- 避免预先获取所有子系统
- 减少无效调用

---

## 4. 系统约束

### 4.1 技术约束

**工具类约束**
- 必须继承自UVoxelToolBase
- 使用UCLASS()宏
- 支持蓝图可实现性

**子系统约束**
- 子系统必须为UWorld的子系统
- 通过GetWorld()->GetSubsystem<T>()获取
- 可能为nullptr（需检查）

**事件委托约束**
- 使用FSimpleMulticastDelegate
- 支持多订阅者
- 不带参数通知

**蓝图约束**
- BlueprintLibraryPath必须为有效路径
- 支持动态加载蓝图工具
- 需要处理加载失败情况

### 4.2 性能约束

**工具数量限制**
- 注册工具类数量 < 100个
- 查询时间 < 1ms（线性搜索）
- 内存占用 < 1MB

**子系统访问开销**
- 每次获取子系统 < 1ms
- 缓存子系统指针（不推荐）
- 避免频繁调用

**事件触发开销**
- 事件委托触发 < 0.1ms
- 支持大量订阅者
- 不影响主线程性能

### 4.3 业务约束

**编辑器模式约束**
- 全局状态，不区分玩家
- 支持多玩家同时激活
- 退出模式时不影响其他系统

**工具注册约束**
- 运行时注册
- 支持编辑器模式下注册
- 不支持动态移除工具类

**子系统访问约束**
- 只能在运行时调用
- 需要检查子系统有效性
- 不负责子系统生命周期

---

## 5. 设计目标

### 5.1 功能目标

**核心功能**
1. ✅ 工具类注册和管理
2. ✅ 编辑器模式状态控制
3. ✅ 子系统访问接口提供
4. ✅ 工具查询和实例化
5. ✅ 事件委托系统
6. ✅ 蓝图库路径管理

**扩展功能**
1. ✅ 幂等操作支持
2. ✅ 多种工具查询方式
3. ✅ 运行时工具注册
4. ✅ 动态蓝图加载支持
5. ✅ 子系统统一访问接口

### 5.2 质量目标

**可维护性**
- 清晰的职责划分
- 与其他系统解耦
- 易于扩展新工具类型

**可测试性**
- 工具注册功能可测试
- 模式切换功能可测试
- 事件委托可测试

### 5.3 性能目标

**注册性能**
- 工具注册时间 < 1ms
- 工具查询时间 < 1ms
- 内存占用 < 1MB

**模式切换性能**
- 模式切换 < 0.1ms
- 事件触发 < 0.1ms
- 不影响主线程性能

---

## 6. 与其他子系统的关系

### 6.1 与VoxelBlockWorldSubsystem的关系

```
VoxelEditorModeSubsystem
  ↓ 提供
GetBlockWorld() 接口
  ↓
VoxelBlockWorldSubsystem
  ↓ 返回
体素世界访问接口
```

**职责划分**：
- VoxelEditorModeSubsystem：提供统一访问接口
- VoxelBlockWorldSubsystem：实现具体的体素世界功能

### 6.2 与VoxelAreaWorldSubsystem的关系

```
VoxelEditorModeSubsystem
  ↓ 提供
GetAreaWorld() 接口
  ↓
VoxelAreaWorldSubsystem
  ↓ 返回
区域管理访问接口
```

**职责划分**：
- VoxelEditorModeSubsystem：提供统一访问接口
- VoxelAreaWorldSubsystem：实现具体的区域管理功能

### 6.3 与VoxelCreatureWorldSubsystem的关系

```
VoxelEditorModeSubsystem
  ↓ 提供
GetCreatureWorld() 接口
  ↓
VoxelCreatureWorldSubsystem
  ↓ 返回
生物管理访问接口
```

**职责划分**：
- VoxelEditorModeSubsystem：提供统一访问接口
- VoxelCreatureWorldSubsystem：实现具体的生物管理功能

### 6.4 与VoxelItemWorldSubsystem的关系

```
VoxelEditorModeSubsystem
  ↓ 提供
GetItemWorld() 接口
  ↓
VoxelItemWorldSubsystem
  ↓ 返回
物品管理访问接口
```

**职责划分**：
- VoxelEditorModeSubsystem：提供统一访问接口
- VoxelItemWorldSubsystem：实现具体的物品管理功能

### 6.5 与WorldObjectPoolSubsystem的关系

```
VoxelEditorModeSubsystem
  ↓ 提供
GetObjectPool() 接口
  ↓
WorldObjectPoolSubsystem
  ↓ 返回
对象池访问接口
```

**职责划分**：
- VoxelEditorModeSubsystem：提供统一访问接口
- WorldObjectPoolSubsystem：实现具体对象池功能

### 6.6 与ControllerComponent的关系

```
ControllerComponent
  ↓ 查询
IsEditorModeActive()
  ↓
VoxelEditorModeSubsystem
  ↓ 返回
编辑器模式状态
```

**职责划分**：
- ControllerComponent：管理玩家编辑器状态
- VoxelEditorModeSubsystem：提供全局编辑器模式状态

---

## 7. 使用示例

### 7.1 工具注册示例

```cpp
// 编辑器工具类实现
UCLASS()
class UVoxelPaintTool : public UVoxelToolBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Voxel|Editor")
    virtual void ExecutePaint(const FIntVector& Position, const FGameplayTag& BlockType) override;
};

// 在编辑器模块中注册工具
VoxelEditorModeSubsystem->RegisterToolClass(UVoxelPaintTool::StaticClass());
```

### 7.2 模式切换示例

```cpp
// 进入编辑器模式
VoxelEditorModeSubsystem->EnterEditorMode();

// 退出编辑器模式
VoxelEditorModeSubsystem->ExitEditorMode();

// 检查编辑器模式状态
if (VoxelEditorModeSubsystem->IsEditorModeActive())
{
    // 执行编辑器相关操作
}
```

### 7.3 工具查询示例

```cpp
// 获取所有可用工具
TArray<TSubclassOf<UVoxelToolBase>> AvailableTools =
    VoxelEditorModeSubsystem->GetAvailableTools();

// 按名称获取工具类
TSubclassOf<UVoxelToolBase> ToolClass =
    VoxelEditorModeSubsystem->GetToolClassByName("PaintTool");

// 获取工具实例
UVoxelToolBase* ToolInstance = VoxelEditorModeSubsystem->CreateToolInstance(ToolClass);
if (ToolInstance)
{
    ToolInstance->ExecuteTool(/* 参数 */);
}
```

### 7.4 子系统访问示例

```cpp
// 获取体素世界子系统
UVoxelBlockWorldSubsystem* BlockWorld =
    VoxelEditorModeSubsystem->GetBlockWorld();
if (BlockWorld)
{
    // 执行体素世界操作
    BlockWorld->ModifyVoxelData(/* 参数 */);
}

// 获取区域世界子系统
UVoxelAreaWorldSubsystem* AreaWorld =
    VoxelEditorModeSubsystem->GetAreaWorld();
if (AreaWorld)
{
    // 执行区域管理操作
}
```

---

## 8. 相关文档

- [VoxelEditorModeSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelEditorModeSubsystem-架构设计.md)（待创建）
- [VoxelToolBase-工具基类设计](/VoxelDesign/04-类设计/VoxelToolBase-类设计.md)（待创建）
- [编辑工具系统-系统设计](/VoxelDesign/02-系统设计/编辑工具系统-系统设计.md)（待创建）
- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [VoxelAreaWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelAreaWorldSubsystem-系统设计.md)（待创建）

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 系统设计已完成