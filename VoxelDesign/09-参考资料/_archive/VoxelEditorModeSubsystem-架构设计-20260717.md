# VoxelEditorModeSubsystem-架构设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 组件架构

### 1.1 整体架构层次

```
┌─────────────────────────────────────────────────────────────┐
│ 接口层（Interface Layer）                                │
│ - VoxelEditorModeSubsystem 公开接口                       │
│   - 工具注册接口（RegisterToolClass）                      │
│   - 工具查询接口（GetAvailableTools/GetToolClassByName）   │
│   - 模式控制接口（EnterEditorMode/ExitEditorMode）         │
│   - 子系统访问接口（GetBlockWorld等）                     │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 控制层（Control Layer）                                    │
│ - 模式控制层                                              │
│   - 编辑器模式状态管理（bEditorModeActive）                 │
│   - 幂等操作实现                                          │
│   - 事件委托触发（OnEditorModeEntered/Exited）           │
│ - 工具管理层                                              │
│   - 工具类注册逻辑                                        │
│   - 工具类去重管理                                        │
│   - 工具查询实现                                        │
└────────────────────┬────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────┐
│ 服务层（Service Layer）                                    │
│ - 无状态服务层                                            │
│   - 统一子系统访问接口                                    │
│   - 蓝图库路径管理                                        │
│ - 工具实例化层                                            │
│   - 工具实例创建                                          │
│   - 工具生命周期管理                                      │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 工具注册系统架构

```
┌─────────────────────────────────────────────────────────────┐
│ 工具注册层（Tool Registration）                            │
│  • UVoxelToolBase（工具基类）                             │
│  • 注册接口：RegisterToolClass(UClass* ToolClass)         │
│  • 验证逻辑：检查继承关系                                 │
│  • 存储结构：TArray<TSubclassOf<UVoxelToolBase>>           │
│                                                             │
│  职责：管理所有编辑器工具类的注册和查询                       │
└──────────────────────────┬──────────────────────────────────┘
                           ↑ 注册工具类
                           │
┌──────────────────────────┴──────────────────────────────────┐
│ 工具分类层（Tool Classification）                        │
│  ┌──────────┬──────────┬──────────┬────────────────────┐   │
│  │ 批量工具 │ 选择工具 │ 操作工具 │ 直接操作工具         │   │
│  │ Batch    │ Select   │ Operate  │ Direct              │   │
│  └──────────┴──────────┴──────────┴────────────────────┘   │
│                                                             │
│  职─────────────────────────────────────────────────────┐   │
│  │ UVoxelToolBase 统一接口                             │   │
│  │  • ExecuteTool() 执行工具                           │   │
│  │  • GetToolName() 获取工具名称                       │   │
│  │  • SupportsPreview() 支持预览                       │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 子系统访问架构

```
┌─────────────────────────────────────────────────────────────┐
│ 统一接口层（Unified Interface）                            │
│  • GetBlockWorld()：体素世界访问                         │
│  • GetAreaWorld()：区域管理访问                           │
│  • GetCreatureWorld()：生物管理访问                     │
│  • GetItemWorld()：物品管理访问                         │
│  • GetObjectPool()：对象池访问                           │
│                                                             │
│  职责：提供统一的子系统访问接口，避免直接获取                 │
└──────────────────────────┬──────────────────────────────────┘
                           ↓ 按需获取
                           │
┌──────────────────────────┴──────────────────────────────────┐
│ 子系统层（Subsystem Layer）                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ UVoxelBlockWorldSubsystem                          │   │
│  │  • 体素数据管理                                     │   │
│  │  • 区块生成                                         │   │
│  │  • 异步处理                                         │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ UVoxelAreaWorldSubsystem                           │   │
│  │  • 区域/领地管理                                   │   │
│  │  • 碰撞检测                                         │   │
│  │  • 区域数据更新                                   │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ UVoxelCreatureWorldSubsystem                       │   │
│  │  • NPC和生物管理                                   │   │
│  │  • 生命周期管理                                     │   │
│  │  • AI行为树                                        │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ UVoxelItemWorldSubsystem                          │   │
│  │  • 物品管理                                         │   │
│  │  • 物品栏系统                                     │   │
│  │  • 物品交互                                       │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ UWorldObjectPoolSubsystem                         │   │
│  │  • 对象池管理                                     │   │
│  │  • 模板实例化                                     │   │
│  │  • 实例复用                                       │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 1.4 组件职责概述

#### **接口层**

**VoxelEditorModeSubsystem 公开接口**
- 提供所有外部调用需要的接口
- 确保接口的幂等性和安全性
- 支持蓝图可调用性

#### **控制层**

**模式控制组件**
- 管理编辑器模式的全局状态
- 实现幂等的进入/退出操作
- 触发对应的事件委托

**工具管理组件**
- 处理工具类的注册和去重
- 实现工具查询逻辑
- 管理工具类列表

#### **服务层**

**无状态服务组件**
- 提供子系统访问的统一接口
- 管理蓝图库路径配置
- 不存储任何玩家状态

**工具实例化组件**
- 根据工具类创建实例
- 管理工具实例的生命周期
- 提供实例化的错误处理

---

## 2. 接口定义

### 2.1 工具管理接口

#### **工具注册接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 注册工具类
     * @param ToolClass 工具类（必须继承自 UVoxelToolBase）
     * @return true 成功注册，false 失败
     * @note 重复注册同一类只会保留一次
     * @note 必须在游戏运行时调用
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Tools")
    bool RegisterToolClass(TSubclassOf<UVoxelToolBase> ToolClass);
};
```

#### **工具查询接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 获取所有已注册的工具类
     * @return 已注册的工具类列表
     * @note 返回的是类定义，不是实例
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Tools")
    TArray<TSubclassOf<UVoxelToolBase>> GetAvailableTools() const;

    /**
     * 根据名称获取工具类
     * @param ToolName 工具类名称
     * @return 工具类，未找到则返回 nullptr
     * @note 名称匹配使用精确匹配
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Tools")
    TSubclassOf<UVoxelToolBase> GetToolClassByName(FName ToolName) const;

    /**
     * 根据索引获取工具类
     * @param Index 工具类索引
     * @return 工具类，索引无效则返回 nullptr
     * @note 索引从0开始，越界返回nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Tools")
    TSubclassOf<UVoxelToolBase> GetToolClassByIndex(int32 Index) const;
};
```

#### **工具实例化接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 创建工具实例
     * @param ToolClass 工具类
     * @return 工具实例，失败则返回 nullptr
     * @note 调用者负责管理实例生命周期
     * @note 实例不会自动添加到游戏世界
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Tools")
    UVoxelToolBase* CreateToolInstance(TSubclassOf<UVoxelToolBase> ToolClass);
};
```

### 2.2 模式控制接口

#### **编辑器模式控制接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 进入编辑器模式
     * @return true 成功进入，false 失败
     * @note 幂等操作，重复调用无副作用
     * @note 全局状态，与具体区域无关
     * @note 触发 OnEditorModeEntered 事件
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Mode")
    bool EnterEditorMode();

    /**
     * 退出编辑器模式
     * @return true 成功退出，false 失败
     * @note 幂等操作，重复调用无副作用
     * @note 触发 OnEditorModeExited 事件
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Mode")
    bool ExitEditorMode();

    /**
     * 检查编辑器模式是否激活
     * @return true 编辑器模式已激活，false 未激活
     * @note 全局状态查询
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Mode")
    bool IsEditorModeActive() const;
};
```

### 2.3 子系统访问接口

#### **核心子系统访问接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 获取方块世界子系统
     * @return BlockWorld 子系统，如果未找到则返回 nullptr
     * @note 每次调用都会重新获取子系统
     * @note 使用前需要检查有效性
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UVoxelBlockWorldSubsystem* GetBlockWorld() const;

    /**
     * 获取区域世界子系统
     * @return AreaWorld 子系统，如果未找到则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UVoxelAreaWorldSubsystem* GetAreaWorld() const;

    /**
     * 获取生物世界子系统
     * @return CreatureWorld 子系统，如果未找到则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UVoxelCreatureWorldSubsystem* GetCreatureWorld() const;

    /**
     * 获取物品世界子系统
     * @return ItemWorld 子系统，如果未找到则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UVoxelItemWorldSubsystem* GetItemWorld() const;

    /**
     * 获取对象池子系统
     * @return ObjectPool 子系统，如果未找到则返回 nullptr
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UWorldObjectPoolSubsystem* GetObjectPool() const;
};
```

### 2.4 配置管理接口

#### **蓝图库路径管理接口**

```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 设置蓝图库保存路径
     * @param Path 蓝图库路径
     * @note 运行时可修改
     * @note 用于动态加载蓝图工具
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Config")
    void SetBlueprintLibraryPath(FString Path);

    /**
     * 获取蓝图库保存路径
     * @return 蓝图库的保存路径
     * @note 默认路径：/Game/Voxel/Blueprints
     */
    UFUNCTION(BlueprintPure, Category = "Editor|Config")
    FString GetBlueprintLibraryPath() const;
};
```

### 2.5 事件委托接口

#### **编辑器模式事件委托**

```cpp
class UVoxelEditorModeSubsystem
{
    /** 进入编辑器模式时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnEditorModeEntered OnEditorModeEntered;

    /** 退出编辑器模式时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnEditorModeExited OnEditorModeExited;
};
```

---

## 3. 数据流

### 3.1 工具注册流程

```
工具类注册请求
    ↓
RegisterToolClass(UClass* ToolClass)
    ↓
验证工具类继承关系
    ├─ 检查是否继承自 UVoxelToolBase
    ├─ 检查是否为有效的UClass
    └─ 检查是否已经注册
    ↓
添加到 RegisteredToolClasses 数组
    ├─ 去重处理（避免重复注册）
    └─ 维护注册顺序
    ↓
返回注册成功状态
```

### 3.2 模式切换流程

```
EnterEditorMode() / ExitEditorMode()
    ↓
检查当前状态（幂等操作）
    ├─ 如果已经是目标状态，直接返回true
    └─ 如果状态不同，继续执行
    ↓
更新 bEditorModeActive 状态
    ├─ Enter：true
    └─ Exit：false
    ↓
触发对应的事件委托
    ├─ OnEditorModeEntered / OnEditorModeExited
    └→ 通知所有订阅者
    ↓
通知其他系统状态变更
    ↓
返回操作成功状态
```

### 3.3 工具获取流程

```
工具查询请求
    ↓
GetToolClassByName() / GetToolClassByIndex()
    ↓
搜索 RegisteredToolClasses 数组
    ├─ 按名称：线性搜索匹配 FName
    ├─ 按索引：直接访问数组元素
    └─ 未找到：返回 nullptr
    ↓
返回匹配的工具类
    └─ TSubclassOf<UVoxelToolBase>
    ↓
（可选）CreateToolInstance() 创建实例
    ├→ NewObject<UVoxelToolBase>(ToolClass)
    └→ 处理创建失败情况
```

### 3.4 子系统访问流程

```
子系统访问请求
    ↓
GetBlockWorld() / GetAreaWorld() 等
    ↓
通过 GetWorld()->GetSubsystem<T>() 获取
    ├─ 检查 World 是否有效
    └→ GetWorld()->GetSubsystem<TSubsystemClass>()
    ↓
返回子系统指针
    ├→ 成功：返回子系统指针
    └→ 失败：返回 nullptr
    ↓
调用者需要检查指针有效性
```

### 3.5 事件广播流程

```
编辑器模式状态变更
    ↓
检查状态是否真的改变
    ├─ EnterEditorMode：从false变为true
    └─ ExitEditorMode：从true变为false
    ↓
触发对应的事件委托
    ├─ OnEditorModeEntered.Broadcast()
    └─ OnEditorModeExited.Broadcast()
    ↓
所有订阅者收到通知
    ├→ ControllerComponent 更新状态
    ├→ UI系统更新显示
    └→ 其他系统响应变化
```

---

## 4. 生命周期管理

### 4.1 Subsystem生命周期

```
游戏启动
  ↓
UWorld 创建
  ↓
TickableWorldSubsystem 创建
  ↓
Initialize() 调用
  ├─ 初始化成员变量
  ├─ 设置默认配置
  └─ 准备接收注册请求
  ↓
运行中（游戏进行中）
  ├→ Tick() 更新
  ├→ 接收各种接口调用
  └→ 管理工具类和状态
  ↓
Deinitialize() 调用
  ├→ 清理注册的工具类
  ├→ 重置状态变量
  └→ 准备销毁
  ↓
Subsystem 销毁
```

### 4.2 工具类生命周期

```
工具类定义
  ↓
编译加载到内存
  ↓
运行时注册
    ↓
RegisterToolClass() 调用
    ↓
验证和存储
    ↓
在 RegisteredToolClasses 中存在
    ↓
游戏运行期间一直存在
    ↓
Subsystem 销毁时清理
```

### 4.3 编辑器模式生命周期

```
初始状态：false
  ↓
EnterEditorMode() 调用
  ↓
状态变为：true
  ↓
OnEditorModeEntered 触发
  ↓
编辑器模式激活
  ↓
ExitEditorMode() 调用
  ↓
状态变为：false
  ↓
OnEditorModeExited 触发
```

### 4.4 事件订阅生命周期

```
订阅者创建
  ↓
绑定事件委托
    ↓
OnEditorModeEntered/Exited.AddDynamic(this, &Handler)
  ↓
接收事件通知
    ↓
处理事件
    ↓
订阅者销毁
  ↓
解除事件绑定
    ↓
委托自动清理
```

---

## 5. 类关系图

### 5.1 VoxelEditorModeSubsystem核心关系

```
UVoxelEditorModeSubsystem（核心子系统）
    ├─ 拥有 → RegisteredToolClasses（工具类列表）
    ├─ 管理 → bEditorModeActive（模式状态）
    ├─ 管理 → BlueprintLibraryPath（蓝图库路径）
    ├─ 提供 → OnEditorModeEntered（事件委托）
    └─ 提供 → OnEditorModeExited（事件委托）

UVoxelEditorModeSubsystem
    ├─ 使用 → UVoxelBlockWorldSubsystem（子系统访问）
    ├─ 使用 → UVoxelAreaWorldSubsystem（子系统访问）
    ├─ 使用 → UVoxelCreatureWorldSubsystem（子系统访问）
    ├─ 使用 → UVoxelItemWorldSubsystem（子系统访问）
    └─ 使用 → UWorldObjectPoolSubsystem（子系统访问）
```

### 5.2 工具系统关系

```
UVoxelToolBase（工具基类）
    ↑ 继承
    ├── UVoxelBatchEditTool（批量编辑工具）
    ├── UVoxelSelectionTool（选择工具）
    ├── UVoxelOperationTool（操作工具）
    └── UVoxelDirectTool（直接操作工具）

使用关系：
UVoxelEditorModeSubsystem
    ├── 管理 → UVoxelToolBase 及其子类
    ├── 提供 → 工具实例化接口
    └── 提供 → 工具查询接口
```

### 5.3 控制器组件关系

```
AVoxelPlayerController（玩家控制器）
    └─ 拥有 → UVoxelControllerStateComponent（状态管理组件）

UVoxelControllerStateComponent
    ↓ 查询
    UVoxelEditorModeSubsystem::IsEditorModeActive()
    ↓ 返回
    编辑器模式状态
    ↓ 使用
    管理玩家编辑器状态
```

### 5.4 事件系统关系

```
UVoxelEditorModeSubsystem
    ├── 广播 → OnEditorModeEntered
    └── 广播 → OnEditorModeExited

订阅者：
UVoxelControllerStateComponent
    ├── 订阅 → OnEditorModeEntered
    └── 订阅 → OnEditorModeExited
    ↓ 处理
    更新玩家编辑器状态

UI系统（蓝图）
    ├── 订阅 → OnEditorModeEntered
    └── 订阅 → OnEditorModeExited
    ↓ 处理
    更新UI显示
```

---

## 6. 扩展点

### 6.1 新增工具类型

**继承工具基类**：
```cpp
class UVoxelCustomTool : public UVoxelToolBase
{
    GENERATED_BODY()

public:
    virtual void ExecuteTool(const FToolExecutionParams& Params) override;
    virtual bool CanExecuteTool(const FToolExecutionParams& Params) const override;
    virtual FToolPreviewData GetPreviewData(const FToolExecutionParams& Params) const override;
};
```

**注册工具**：
```cpp
// 在适当的地方注册工具
VoxelEditorModeSubsystem->RegisterToolClass(UVoxelCustomTool::StaticClass());
```

### 6.2 新增子系统访问

**扩展子系统接口**：
```cpp
class UVoxelEditorModeSubsystem
{
    // 新增子系统访问接口
    UFUNCTION(BlueprintPure, Category = "Editor|Subsystems")
    UVoxelCustomSubsystem* GetCustomSubsystem() const;
};
```

### 6.3 扩展事件系统

**新增事件委托**：
```cpp
class UVoxelEditorModeSubsystem
{
    /** 工具注册完成时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnToolRegistered OnToolRegistered;

    /** 工具被移除时广播 */
    UPROPERTY(BlueprintAssignable, Category = "Editor|Events")
    FOnToolRemoved OnToolRemoved;
};
```

### 6.4 扩展配置选项

**新增配置项**：
```cpp
class UVoxelEditorModeSubsystem
{
    /** 编辑器模式快捷键 */
    UPROPERTY(EditAnywhere, Category = "Editor|Config")
    TMap<FName, FInputActionKeyMapping> EditorModeKeyMappings;

    /** 默认工具类 */
    UPROPERTY(EditAnywhere, Category = "Editor|Config")
    TSubclassOf<UVoxelToolBase> DefaultToolClass;
};
```

### 6.5 扩展工具实例化选项

**自定义实例化逻辑**：
```cpp
class UVoxelEditorModeSubsystem
{
    /**
     * 创建工具实例（带参数）
     * @param ToolClass 工具类
     * @param InitializationParams 初始化参数
     * @return 工具实例
     */
    UFUNCTION(BlueprintCallable, Category = "Editor|Tools")
    UVoxelToolBase* CreateToolWithParams(
        TSubclassOf<UVoxelToolBase> ToolClass,
        const FToolInitializationParams& InitializationParams
    );
};
```

---

## 7. 相关文档

### 7.1 子系统详细文档

**核心子系统**：
- [VoxelEditorModeSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelEditorModeSubsystem-系统设计.md) - 完整系统设计文档
- [VoxelEditorModeSubsystem-架构设计](/VoxelDesign/03-架构设计/VoxelEditorModeSubsystem-架构设计.md) - 架构设计文档（当前文档）

**工具系统**：
- [VoxelToolBase-工具基类设计](/VoxelDesign/04-类设计/VoxelToolBase-类设计.md) - 工具基类设计
- [编辑工具系统-系统设计](/VoxelDesign/02-系统设计/编辑工具系统-系统设计.md) - 编辑工具系统设计

### 7.2 其他相关文档

**子系统关系**：
- [VoxelBlockWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelBlockWorldSubsystem-系统设计.md)
- [VoxelAreaWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelAreaWorldSubsystem-系统设计.md)
- [VoxelCreatureWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelCreatureWorldSubsystem-系统设计.md)
- [VoxelItemWorldSubsystem-系统设计](/VoxelDesign/02-系统设计/VoxelItemWorldSubsystem-系统设计.md)

**控制器系统**：
- [VoxelPlayerController-架构设计](/VoxelDesign/03-架构设计/VoxelPlayerController-架构设计.md)
- [VoxelControllerStateComponent-架构设计](/VoxelDesign/03-架构设计/VoxelControllerStateComponent-架构设计.md)

### 7.3 设计原则文档

**架构原则**：
- [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md) - 编辑系统整体架构
- [编辑系统-C++与蓝图职责划分](/VoxelDesign/03-架构设计/编辑系统-C++与蓝图职责划分-架构设计.md) - 职责划分

---

**文档版本**: 1.0
**最后更新**: 2026-01-24
**状态**: ✅ 架构设计已完成