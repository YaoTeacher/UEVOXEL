# Voxel UI 系统 - 完整改造计划

> **For Claude:** 必需子技能：使用 superpowers:executing-plans 逐步执行此计划。

**目标：** 为 UE5 体素沙盒游戏构建统一的 UI 管理系统，支持 5 层 UI 分层（HUD/Screen/Dialog/Toast/3D Widget）、弹窗栈管理、输入模式自动切换、事件驱动更新，解决当前 UI 分散、无统一基类、弹窗系统缺失、输入模式粗糙等问题。

**架构：**

- 中央管理器：`UVoxelUISubsystem` (UGameInstanceSubsystem) 统管所有 2D UI
- 5 层分离：L1 HUD（常驻）/ L2 Screen（全屏页面）/ L3 Dialog（模态弹窗栈）/ L4 Toast（自动消失）/ L5 3D Widget（世界内 UI）
- 基类体系：`UVoxelWidgetBase`（根） → `UVoxelHUDWidget` / `UVoxelScreenBase` / `UVoxelDialogBase` / `UVoxelToastWidget`
- 事件驱动：`OnDataChanged` 数据事件总线 + 现有 `OnBuildModeParamsUpdated` 建造事件
- 输入模式自动管理：`RefreshInputMode` 根据栈状态切换 GameAndUI/UIOnly
- 暂停管理：Dialog 打开时可选暂停

**技术栈：**

- UE5.8 C++ + UMG
- UGameInstanceSubsystem 作为 UI 管理器基类
- FTickableGameObject 实现 Toast 队列定时
- FInputModeGameAndUI / FInputModeUIOnly 输入模式
- Delegate 多播事件总线

***

## 现状诊断

### 核心问题

1. **没有 UI 管理器**：UI 分散在各处，生命周期失控
2. **UUI\_Base 空类**：无项目级基类，无统一主题/动画/输入
3. **弹窗系统缺失**：硬编码路径，无遮罩/输入模式/栈管理
4. **输入模式粗糙**：始终 GameAndUI，光标常显，ChangeInputModeGameOnly 是 Bug
5. **NPC UI 错位**：由生物 ConsumComponent 持有，不在 PlayerController/HUD
6. **命名误导**：VoxelControllerVisualUIComponent 实际是 3D 视觉，不碰 2D UI
7. **旧代码残留**：BuildAreaModifyWidget 废弃未删，VoxelItemListPanelWidget 异常用法
8. **HUD 范围太小**：AVoxelHUD 只管 3 个 Widget，其余全在 HUD 之外

### 保留不动的部分

- 物品库体系（DataSource/Provider/Entry/SlotBase）—— 设计良好
- 建造模块化（M1-M16 + ToolDetailModuleBase）—— 架构清晰
- 3D 视觉体系（Voxel3DUIActor/VoxelUIBlockActor/PreviewActor）—— 独立完整
- StateComponent 事件广播（OnBuildModeParamsUpdated）—— 建造专用保留

***

## 5 层 UI 架构

```
┌──────────────────────────────────────────────────────────────────┐
│                      GameInstance                                │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  UVoxelUISubsystem (UI 管理子系统)                          │  │
│  │  ├─ HUD 层管理 (ShowHUD/HideHUD)                            │  │
│  │  ├─ Screen 层管理 (OpenScreen/CloseScreen)                  │  │
│  │  ├─ Dialog 栈管理 (PushDialog/PopDialog/CloseAllDialogs)    │  │
│  │  ├─ Toast 队列管理 (ShowToast)                              │  │
│  │  ├─ 输入模式自动管理 (RefreshInputMode)                     │  │
│  │  ├─ 暂停管理 (SetGamePaused)                                │  │
│  │  └─ 数据事件总线 (OnDataChanged/NotifyDataChanged)          │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
                              ↓ 管理 5 层
┌──────────────┬──────────────┬──────────────┬─────────────┬──────────────┐
│  L1: HUD     │  L2: Screen  │  L3: Dialog  │  L4: Toast  │  L5: 3D Widget│
│  (常驻)      │  (全屏页面)  │  (模态弹窗)  │  (自动消失) │  (世界内 UI)  │
│  ZOrder=10   │  ZOrder=50   │  ZOrder=100  │  ZOrder=200 │  (3D 场景)   │
├──────────────┼──────────────┼──────────────┼─────────────┼──────────────┤
│ InteractionW │ BuildPanelW  │ TemplateSave │ ToastWidget │ Voxel3DUIActor│
│ BuildInterW  │ MainMenu     │ ConfirmDialog│             │ VoxelUIBlock │
│ VoxelHUDW    │ Settings     │ ShopDialog   │             │ PreviewActor │
│ (新建)       │ NPCDetail    │              │             │ WidgetComp   │
└──────────────┴──────────────┴──────────────┴─────────────┴──────────────┘
```

***

## 阶段 1：基础设施搭建

**目标：** 创建 UI 管理子系统和基类体系，不破坏现有功能。

### 任务 1.1：创建 UI 系统类型定义

- 创建：`Source/Voxel/Public/Core/UI/System/VoxelUISubsystemTypes.h`
- 内容：`EToastType` 枚举（Info/Warning/Error/Success）、`FDataChangedEvent` 委托声明

### 任务 1.2：创建项目 UI 根基类

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelWidgetBase.h`
- 创建：`Source/Voxel/Private/Core/UI/Base/VoxelWidgetBase.cpp`
- 内容：`UObject* DataContext`、`HandleDataChanged` 虚函数、`GetUISubsystem()` 辅助、NativeConstruct/Destruct 订阅事件

### 任务 1.3：创建 L1 HUD 基类

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelHUDWidget.h`
- 内容：`OnHUDShown/OnHUDHidden` 虚函数

### 任务 1.4：创建 L2 Screen 基类

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelScreenBase.h`
- 内容：`OnEnterScreen/OnExitScreen` 虚函数、`HandleEscape` 虚函数

### 任务 1.5：创建 L3 Dialog 基类

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelDialogBase.h`
- 内容：`OnDialogOpened/OnDialogClosed` 虚函数、`Close()` 方法、`HandleEscape` 默认关闭自己

### 任务 1.6：创建 L4 Toast 基类

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelToastWidget.h`
- 创建：`Source/Voxel/Private/Core/UI/Base/VoxelToastWidget.cpp`
- 内容：`InitializeToast(Message, Duration, Type)`、`OnToastShown/OnToastExpired` 虚函数、`RemainingTime` 定时

### 任务 1.7：创建弹窗遮罩

- 创建：`Source/Voxel/Public/Core/UI/Base/VoxelBackdropWidget.h`
- 内容：半透明黑色背景，拦截点击事件

### 任务 1.8：创建 UI 管理子系统

- 创建：`Source/Voxel/Public/Core/UI/System/VoxelUISubsystem.h`
- 创建：`Source/Voxel/Private/Core/UI/System/VoxelUISubsystem.cpp`
- 内容：5 层管理 API、`RefreshInputMode`、`SetGamePaused`、`HandleEscape`、`OnDataChanged` 事件总线、`FTickableGameObject` 实现 Toast 定时

### 任务 1.9：删除空类 UUI\_Base

- 删除：`Source/Voxel/Public/Core/UI/UI_Base.h`
- 删除：`Source/Voxel/Private/Core/UI/UI_Base.cpp`
- 理由：被 `UVoxelWidgetBase` 替代，无任何子类继承

### 任务 1.10：编译验证

- 成功标准：编译通过，0 错误 0 警告，现有功能不受影响

**阶段 1 成功标准：**

- UVoxelUISubsystem 可通过 `GetGameInstance()->GetSubsystem<UVoxelUISubsystem>()` 获取
- 5 个基类编译通过
- 现有 UI 功能完全不受影响

***

## 阶段 2：迁移 Dialog 层（弹窗系统）

**目标：** VoxelTemplateSaveDialog 迁移到新弹窗系统，验证栈管理、遮罩、输入模式。

### 任务 2.1：VoxelTemplateSaveDialog 改为继承 UVoxelDialogBase

- 修改：`Source/Voxel/Public/Core/UI/Build/VoxelTemplateSaveDialog.h`
- 修改：`Source/Voxel/Private/Core/UI/Build/VoxelTemplateSaveDialog.cpp`
- 改动：
  - 父类从 `UUserWidget` 改为 `UVoxelDialogBase`
  - 移除 `static Open()` 方法（改为由 UISubsystem.PushDialog 创建）
  - 移除硬编码蓝图路径 `LoadClass`
  - 移除手动 `AddToViewport/RemoveFromParent`
  - 实现 `OnDialogOpened(Context)` 初始化选区统计
  - `OnConfirmClicked/OnCancelClicked` 改为调用 `Close()`

### 任务 2.2：M5 调用方式迁移

- 修改：`Source/Voxel/Private/Core/UI/Build/Modules/M5_SelectionInfoModule.cpp`
- 改动：`UVoxelTemplateSaveDialog::Open(this, SC)` 改为 `UISys->PushDialog(VoxelTemplateSaveDialogClass, SC)`
- M5 需要配置 `TSubclassOf<UVoxelDialogBase> TemplateSaveDialogClass` UPROPERTY

### 任务 2.3：WBP\_VoxelTemplateSaveDialog 蓝图重父类

- 蓝图父类从 `UUserWidget` 改为 `UVoxelDialogBase`
- 通过 UE MCP UMGToolSet 操作

### 任务 2.4：编译与验证

- 成功标准：
  - 弹窗通过 `PushDialog` 打开
  - 显示遮罩
  - 输入模式切换为 UIOnly
  - 游戏暂停
  - ESC/取消按钮关闭弹窗，恢复输入模式
  - 确认按钮正常保存模板

**阶段 2 成功标准：** 弹窗完整流程正常工作，遮罩/输入模式/暂停/ESC 全部正确。

***

## 阶段 3：迁移 HUD 层与 Screen 层

**目标：** AVoxelHUD 瘦身为壳，BuildPanel 通过 OpenScreen 管理。

### 任务 3.1：创建游戏 HUD Widget

- 创建：`Source/Voxel/Public/Core/UI/HUD/VoxelGameHUDWidget.h`
- 创建：`Source/Voxel/Private/Core/UI/HUD/VoxelGameHUDWidget.cpp`
- 内容：继承 `UVoxelHUDWidget`，整合 InteractionWidget + BuildInteractionWidget 作为子组件

### 任务 3.2：AVoxelHUD 改造为壳

- 修改：`Source/Voxel/Public/VoxelHUD.h`
- 修改：`Source/Voxel/Private/VoxelHUD.cpp`
- 改动：
  - 移除 3 个 Widget UPROPERTY（BuildWidget/InteractionWidget/BuildInteractionWidget）
  - BeginPlay 改为调用 `UISys->ShowHUD()`
  - `ShowBuildWidget/HideBuildWidget` 改为转发到 UISubsystem

### 任务 3.3：BuildPanelWidget 改为继承 UVoxelScreenBase

- 修改：`Source/Voxel/Public/Core/UI/Build/BuildPanelWidget.h`
- 修改：`Source/Voxel/Private/Core/UI/Build/BuildPanelWidget.cpp`
- 改动：
  - 父类从 `UUserWidget` 改为 `UVoxelScreenBase`
  - 实现 `OnEnterScreen/OnExitScreen`
  - NativeConstruct 中的初始化逻辑移到 `OnEnterScreen`

### 任务 3.4：通过 OpenScreen 打开 BuildPanel

- 修改：`Source/Voxel/Private/VoxelHUD.cpp` 或 `VoxelPlayerController.cpp`
- 改动：原 `CreateWidget + AddToViewport` 改为 `UISys->OpenScreen(BuildPanelClass)`

### 任务 3.5：WBP\_BuildPanelWidget 蓝图重父类

- 蓝图父类从 `UUserWidget` 改为 `UVoxelScreenBase`

### 任务 3.6：WBP\_VoxelGameHUD 蓝图创建

- 创建：`Content/Resource/UI/CoreUI/WBP_VoxelGameHUD`
- 父类：`UVoxelGameHUDWidget`
- 包含 InteractionWidget + BuildInteractionWidget 子组件

### 任务 3.7：编译与验证

- 成功标准：
  - 游戏启动显示 HUD（Interaction + BuildInteraction）
  - BuildPanel 通过 OpenScreen 打开/关闭
  - 输入模式正确切换（HUD: GameAndUI, Screen: GameAndUI, Dialog: UIOnly）
  - 现有建造功能完全正常

**阶段 3 成功标准：** HUD 和 Screen 层迁移完成，现有功能不受影响。

***

## 阶段 4：Toast 系统与事件总线

**目标：** Toast 提示可用，数据变化事件总线集成。

### 任务 4.1：创建 Toast Widget 蓝图

- 创建：`Content/Resource/UI/CoreUI/WBP_VoxelToast`
- 父类：`UVoxelToastWidget`
- 内容：消息文本 + 类型图标 + 自动消失动画

### 任务 4.2：UISubsystem 配置 Toast 类

- 修改：`Source/Voxel/Private/Core/UI/System/VoxelUISubsystem.cpp`
- 改动：Initialize 中加载 `WBP_VoxelToast` 类

### 任务 4.3：替换硬编码提示

- 搜索项目中 `UE_LOG(LogTemp, Warning` 的用户提示性日志
- 改为 `UISys->ShowToast(Message, Duration, Type)`
- 优先替换：金币不足、保存成功、操作失败等用户可见提示

### 任务 4.4：游戏系统接入事件总线

- 修改：`VoxelBlockWorldSubsystem`、`VoxelItemWorldSubsystem` 等子系统
- 改动：在数据变化时调用 `UISys->NotifyDataChanged("BlockData"/"Inventory"/"Template")`

### 任务 4.5：UI Widget 订阅事件

- 修改：`UVoxelWidgetBase::NativeConstruct` 订阅 `OnDataChanged`
- 修改：`UVoxelWidgetBase::NativeDestruct` 取消订阅
- 子类重写 `HandleDataChanged` 实现自动刷新

### 任务 4.6：编译与验证

- 成功标准：
  - `ShowToast("测试", 3.0f)` 显示 Toast，3 秒后自动消失
  - 多个 Toast 排队显示
  - 数据变化时打开的 UI 自动刷新

**阶段 4 成功标准：** Toast 系统可用，事件总线集成完成。

***

## 阶段 5：NPC UI 重构

**目标：** NPC UI 从生物 ConsumComponent 迁移到 UISubsystem 集中管理。

### 任务 5.1：NPC UI 类父类迁移

- 修改：`Source/Voxel/Private/Core/UI/NPCUI/CreatureInfromWidget.h`
- 修改：`Source/Voxel/Private/Core/UI/NPCUI/NpcActionWidget.h`
- 改动：父类改为 `UVoxelScreenBase`（NPC 详情页作为 Screen）

### 任务 5.2：从 ConsumComponent 移除 UI 持有

- 修改：`Source/Voxel/Private/AI/NPC/ConsumComponent.h`
- 修改：`Source/Voxel/Private/AI/NPC/ConsumComponent.cpp`
- 改动：移除 `NpcActionWidget` UPROPERTY 和创建逻辑

### 任务 5.3：PlayerController 集中管理 NPC UI

- 修改：`Source/Voxel/VoxelPlayerController.h`
- 修改：`Source/Voxel/VoxelPlayerController.cpp`
- 改动：
  - 点击生物时调用 `UISys->OpenScreen(NPCDetailClass, Creature)`
  - `NPCDetailScreen->OnEnterScreen(Creature)` 初始化数据

### 任务 5.4：蓝图重父类

- `WBP_CreatureGen` 父类改为 `UVoxelScreenBase`
- `WBP_NpcActionWidget` 父类改为 `UVoxelScreenBase`

### 任务 5.5：编译与验证

- 成功标准：
  - 点击生物打开 NPC 详情页（Screen）
  - 关闭详情页恢复游戏
  - NPC UI 数据正确显示

**阶段 5 成功标准：** NPC UI 完全由 UISubsystem 管理，ConsumComponent 不再持有 UI。

***

## 阶段 6：清理与修复

**目标：** 删除废弃代码，修复 Bug，整理蓝图。

### 任务 6.1：删除废弃代码

- 删除：`Source/Voxel/Public/Core/UI/Build/BuildAreaModifyWidget.h`
- 删除：`Source/Voxel/Private/Core/UI/Build/BuildAreaModifyWidget.cpp`
- 删除：`Source/Voxel/Public/Inventory/InventoryPanel.h`（空类）
- 删除：`Source/Voxel/Public/Core/UI/ItemIconWidget.h`（空类）
- 删除：`Source/Voxel/Private/Core/UI/NPCUI/DynamicRowWidget.h`（空类）
- 核对引用，确保无残留

### 任务 6.2：修复 VoxelItemListPanelWidget 异常用法

- 修改：`Source/Voxel/Private/Core/UI/VoxelItemListPanelWidget.cpp:141`
- 改动：子 Widget 不应 `AddToViewport(2)`，改为 `AddChildToPanel` 或直接作为子组件

### 任务 6.3：修复 ChangeInputModeGameOnly Bug

- 修改：`Source/Voxel/VoxelPlayerController.cpp:176`
- 改动：实现真正的 `FInputModeGameOnly`，而非仍是 `FInputModeGameAndUI`

### 任务 6.4：重命名 VoxelControllerVisualUIComponent

- 重命名：`VoxelControllerVisualUIComponent` → `VoxelControllerVisualComponent`
- 修改所有引用文件
- 更新蓝图中的组件名

### 任务 6.5：删除废弃蓝图

- 删除：`Content/Resource/UI/BuildModeUI/WBP_AreaModifyWidget.uasset`
- 删除：`Content/Resource/UI/BuildModeUI/WBP_TestImport.uasset`
- 核对其他废弃蓝图

### 任务 6.6：编译与全面验证

- 成功标准：
  - 编译通过，0 错误 0 警告
  - 所有 UI 功能正常
  - 无废弃代码残留

**阶段 6 成功标准：** 代码整洁，Bug 修复，命名规范。

***

## 阶段 7：ESC 处理与最终验证

**目标：** ESC 处理链完善，全面回归测试。

### 任务 7.1：实现 ESC 处理链

- 修改：`Source/Voxel/VoxelPlayerController.cpp`
- 改动：绑定 ESC 键 → `UISys->HandleEscape()`
- UISubsystem.HandleEscape 逻辑：Dialog 栈优先 → Screen → 默认行为

### 任务 7.2：全面回归测试

- 测试清单：
  - [ ] 游戏启动 HUD 正常显示
  - [ ] BuildPanel 打开/关闭正常
  - [ ] 模板保存弹窗完整流程
  - [ ] ESC 关闭弹窗/Screen
  - [ ] 输入模式切换正确
  - [ ] Toast 显示与消失
  - [ ] NPC UI 打开/关闭
  - [ ] 建造模式所有工具正常
  - [ ] 物品库筛选/搜索正常
  - [ ] 区域编辑功能正常

**阶段 7 成功标准：** 所有功能正常，UI 系统改造完成。

***

## 关键设计决策

| 决策点              | 选择                             | 理由                  |
| ---------------- | ------------------------------ | ------------------- |
| UI 管理器类型         | `UGameInstanceSubsystem`       | 跨场景持久化，全局唯一         |
| 弹窗栈 vs Common UI | 自实现栈                           | 纯 PC 游戏，无需手柄导航，可控性强 |
| 输入模式默认           | `GameAndUI`                    | 建造模式需要鼠标+键盘         |
| 暂停策略             | 仅 Dialog 暂停，Screen 不暂停         | 模拟经营游戏特点            |
| NPC UI 归属        | PlayerController → UISubsystem | 集中管理，避免分散           |
| AVoxelHUD 去留     | 保留为壳，委托 UISubsystem            | 兼容 UE HUD 体系        |
| 基类层级             | 3 层（WidgetBase → 层级基类 → 具体类）   | 平衡复用与复杂度            |
| 事件总线             | `OnDataChanged` 字符串参数          | 简单灵活，未来可扩展为 Tag     |

***

## 风险与对策

| 风险                | 对策                                         |
| ----------------- | ------------------------------------------ |
| 迁移期间功能损坏          | 每个阶段后回归测试，保留旧代码路径直到新路径验证                   |
| 蓝图重父类导致属性丢失       | 迁移前导出蓝图默认值，迁移后重新配置                         |
| NPC UI 迁移影响 AI 逻辑 | ConsumComponent 保留数据，仅 UI 创建移到 UISubsystem |
| 输入模式切换导致操作异常      | RefreshInputMode 集中管理，避免散落调用               |
| 性能：Toast 频繁创建     | 对象池复用 ToastWidget                          |

***

## 文件结构规划

```
Source/Voxel/
├─ Public/Core/UI/
│  ├─ System/                          【新增】UI 系统层
│  │  ├─ VoxelUISubsystem.h            UI 管理子系统
│  │  └─ VoxelUISubsystemTypes.h       枚举/委托
│  ├─ Base/                            【新增】基类
│  │  ├─ VoxelWidgetBase.h             项目 UI 根基类
│  │  ├─ VoxelHUDWidget.h              L1 HUD 基类
│  │  ├─ VoxelScreenBase.h             L2 Screen 基类
│  │  ├─ VoxelDialogBase.h             L3 Dialog 基类
│  │  ├─ VoxelToastWidget.h            L4 Toast 基类
│  │  └─ VoxelBackdropWidget.h         遮罩
│  ├─ HUD/                             【新增】HUD 实现
│  │  └─ VoxelGameHUDWidget.h          游戏 HUD
│  ├─ Build/                           【保留】建造 UI（父类迁移）
│  ├─ Item/                            【保留】不变
│  ├─ ItemLibrary/                     【保留】不变
│  ├─ NPCUI/                           【保留】父类迁移
│  ├─ Interaction/                     【保留】迁移到 HUD 子组件
│  └─ Tool/                            【保留】不变
```

