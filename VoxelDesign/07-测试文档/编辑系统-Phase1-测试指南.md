# 体素编辑系统 - 第一阶段集成测试指南（修订版）

> **修订说明**: 本文档修正了测试场景1的架构设计，采用正确的无状态服务层设计
>
> **修订日期**: 2026-01-08
> **修订原因**: 原测试文档要求 VoxelEditorModeSubsystem 管理状态，违反了子系统应该是无状态服务层的设计原则

---

## 架构澄清（重要）

### 设计原则

**VoxelEditorModeSubsystem** (UWorldSubsystem):
- **无状态服务层**
- 职责: 提供编辑能力接口，协调子系统
- **不持有编辑器状态** ← 关键

**VoxelControllerStateComponent** (每玩家一个):
- **状态管理层**
- 职责: 管理单个玩家的编辑器状态
- 每个玩家独立

### 为什么这样设计？

1. **多人游戏支持**: 每个玩家可以独立进入/退出编辑器模式
2. **职责分离**: 子系统提供服务，Controller 管理状态
3. **符合 UE5 最佳实践**: WorldSubsystem 应该是无状态的服务

---

## 场景 1: 编辑器模式进入/退出（修订版）

### 测试目标

验证编辑器模式的状态管理由玩家Controller管理，而非全局Subsystem。

### 测试步骤

#### 方法 1: 蓝图测试（推荐）

##### 测试蓝图设置

1. 打开 `BP_TestActor_EditSystem`
2. 添加以下测试逻辑：

```
Event BeginPlay
  ↓
Delay 2.0s
  ↓
【步骤 1】获取玩家控制器
  ↓
Get Player Controller → Cast to VoxelPlayerController
  ↓
Branch (IsValid?)
  ↓ True
【步骤 2】通过 StateComponent 进入编辑器模式
  ↓
Get StateComp (VoxelControllerStateComponent)
  ↓
Call EnterEditorMode(TargetArea = 测试区域实例)
  ↓
Print String "编辑器模式已进入（通过StateComp）"
  ↓
Delay 3.0s
  ↓
【步骤 3】验证状态
  ↓
Call StateComp.IsInEditorMode()
  ↓
Print String "是否在编辑器模式: " + (ReturnValue ? "是" : "否")
  ↓
Call StateComp.GetEditorModeArea()
  ↓
Print String "当前区域: " + (ReturnValue ? ReturnValue.GetName() : "None")
  ↓
Delay 2.0s
  ↓
【步骤 4】退出编辑器模式
  ↓
Call StateComp.ExitEditorMode()
  ↓
Print String "编辑器模式已退出"
  ↓
【步骤 5】验证退出
  ↓
Call StateComp.IsInEditorMode()
  ↓
Print String "是否在编辑器模式: " + (ReturnValue ? "是" : "否")
  ↓
【步骤 6】通知 Subsystem（可选，用于测试子系统通知）
  ↓
Get World → Get Subsystem (VoxelEditorModeSubsystem)
  ↓
Call NotifyPlayerEnteredEditorMode(EnteringPlayer, TargetArea)
  ↓
Print String "已通知 Subsystem 玩家进入编辑器模式"
  ↓ False
Print String "错误: PlayerController 未找到"
```

3. 编译蓝图
4. 拖入测试关卡
5. **运行**（Play In Editor）

#### 预期结果

**进入编辑器模式时**:
- `StateComp.IsInEditorMode()` 返回 `true`
- `StateComp.GetEditorModeArea()` 返回传入的区域
- `StateComp.OnEditorModeToggled` 事件触发，参数为 `true`
- （可选）`Subsystem.NotifyPlayerEnteredEditorMode()` 成功执行
- （可选）`Subsystem.OnPlayerEditorModeEntered` 事件触发

**退出编辑器模式时**:
- `StateComp.IsInEditorMode()` 返回 `false`
- `StateComp.GetEditorModeArea()` 返回 `nullptr`
- `StateComp.OnEditorModeToggled` 事件触发，参数为 `false`
- （可选）`Subsystem.NotifyPlayerExitedEditorMode()` 成功执行
- （可选）`Subsystem.OnPlayerEditorModeExited` 事件触发

#### 验证方法

**1. 屏幕输出验证**:

```
[预期输出]
编辑器模式已进入（通过StateComp）
是否在编辑器模式: 是
当前区域: TestArea_001
编辑器模式已退出
是否在编辑器模式: 否
已通知 Subsystem 玩家进入编辑器模式
```

**2. 蓝图调试**:

- 在蓝图中设置断点：
  - `EnterEditorMode()` 调用后
  - `ExitEditorMode()` 调用后
- 观察变量值：
  - `StateComp.bInEditorMode` (如果可见)
  - `StateComp.EditorModeArea`

**3. 事件监听验证**:

在蓝图中绑定事件：

```
Event BeginPlay
  ↓
Get StateComp
  ↓
Bind Event OnEditorModeToggled
  ↓
  Print String "编辑器模式状态改变: " + (bIsNowInEditorMode ? "进入" : "退出")
```

**4. 多玩家测试**（如果支持）:

```
// 创建第二个玩家控制器
AVoxelPlayerController* PC2 = GetWorld()->SpawnActor<AVoxelPlayerController>();

// 玩家1进入编辑器模式
PC1->StateComp->EnterEditorMode(Area1);

// 玩家2不进入编辑器模式（或进入不同区域）
PC2->StateComp->EnterEditorMode(Area2);

// 验证状态独立
bool bPC1InEditorMode = PC1->StateComp->IsInEditorMode();  // true
bool bPC2InEditorMode = PC2->StateComp->IsInEditorMode();  // true（或 false，取决于是否进入）
AVoxelAreaBaseInstance* Area1 = PC1->StateComp->GetEditorModeArea();  // Area1
AVoxelAreaBaseInstance* Area2 = PC2->StateComp->GetEditorModeArea();  // Area2

// 验证状态独立
check(Area1 != Area2);  // 两个玩家的编辑区域应该不同
```

---

### 与旧测试的对比

#### 旧测试（错误） ❌

```cpp
// ❌ 错误：测试全局状态
UVoxelEditorModeSubsystem* Subsystem = GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>();

// 进入全局编辑器模式（所有玩家共享）
Subsystem->EnterEditorMode(TargetArea);

// 检查全局状态（所有玩家共享）
bool bInEditorMode = Subsystem->IsInEditorMode();
EEditorModeState State = Subsystem->GetCurrentState();
AVoxelAreaBaseInstance* Area = Subsystem->GetCurrentArea();

// 退出全局编辑器模式
Subsystem->ExitEditorMode();
```

**问题**:
- 所有玩家共享同一个编辑器状态
- 无法支持多人独立编辑
- 违反了子系统应该是无状态服务层的设计

#### 新测试（正确） ✅

```cpp
// ✅ 正确：测试玩家独立状态
AVoxelPlayerController* PC = GetWorld()->GetFirstPlayerController<AVoxelPlayerController>();

// 通过 StateComponent 进入编辑器模式
PC->StateComp->EnterEditorMode(TargetArea);

// 通知 Subsystem（可选，用于测试子系统通知）
UVoxelEditorModeSubsystem* Subsystem = GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>();
Subsystem->NotifyPlayerEnteredEditorMode(PC, TargetArea);

// 检查玩家状态
bool bInEditorMode = PC->StateComp->IsInEditorMode();
AVoxelAreaBaseInstance* Area = PC->StateComp->GetEditorModeArea();

// 或通过 Subsystem 查询玩家状态
bInEditorMode = Subsystem->IsPlayerInEditorMode(PC);

// 退出编辑器模式
PC->StateComp->ExitEditorMode();
Subsystem->NotifyPlayerExitedEditorMode(PC);
```

**优势**:
- ✅ 每个玩家独立状态
- ✅ 支持多人游戏
- ✅ 符合 UE5 最佳实践
- ✅ 职责分离（子系统提供服务，Controller 管理状态）

---

### 测试用例矩阵

| 测试用例 | 测试方法 | 预期结果 | 旧测试 | 新测试 |
|---------|---------|---------|--------|--------|
| 玩家进入编辑器模式 | 蓝图 | StateComp.IsInEditorMode() = true | ❌ | ✅ |
| 玩家退出编辑器模式 | 蓝图 | StateComp.IsInEditorMode() = false | ❌ | ✅ |
| 获取当前编辑区域 | 蓝图 | StateComp.GetEditorModeArea() = 传入区域 | ❌ | ✅ |
| 多玩家独立状态 | C++ | 玩家1和玩家2状态互不影响 | ❌ | ✅ |
| Subsystem通知 | 蓝图 | 通知成功，事件触发 | ⚠️ | ✅ |
| 查询玩家状态 | C++ | Subsystem.IsPlayerInEditorMode(PC) = true | N/A | ✅ |

---

### 常见问题和解决方案

#### 问题 1: StateComp 未找到

**症状**:
```
蓝图执行失败: StateComp 为 nullptr
```

**解决方案**:
```cpp
// 确保使用 VoxelPlayerController
AVoxelPlayerController* PC = Cast<AVoxelPlayerController>(GetPlayerController());

if (!PC || !PC->StateComp)
{
    UE_LOG(LogTemp, Error, TEXT("PlayerController 或 StateComp 未找到"));
    return;
}
```

#### 问题 2: 事件未触发

**症状**:
- `OnEditorModeToggled` 事件未触发

**解决方案**:
```cpp
// 确保事件已绑定
PC->StateComp->OnEditorModeToggled.AddDynamic(this, &UMyClass::OnEditorModeChanged);

// 实现回调函数
void UMyClass::OnEditorModeChanged(bool bIsNowInEditorMode)
{
    UE_LOG(LogTemp, Log, TEXT("编辑器模式: %s"), bIsNowInEditorMode ? TEXT("进入") : TEXT("退出"));
}
```

#### 问题 3: 子系统通知失败

**症状**:
```
NotifyPlayerEnteredEditorMode 返回 false
```

**原因**: 关键子系统未准备好

**解决方案**:
```cpp
// 确保子系统已初始化
UVoxelEditorModeSubsystem* Subsystem = GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>();

if (!Subsystem->GetCameraManager() || !Subsystem->GetBatchEditManager())
{
    UE_LOG(LogTemp, Error, TEXT("关键子系统未准备好"));
    return;
}

// 现在可以安全通知
Subsystem->NotifyPlayerEnteredEditorMode(PC, TargetArea);
```

---

### 代码示例（C++）

```cpp
// 测试场景 1: 编辑器模式进入/退出
void TestEditorModeEntryExit()
{
    // 获取玩家控制器
    AVoxelPlayerController* PC = GetWorld()->GetFirstPlayerController<AVoxelPlayerController>();
    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("TestEditorModeEntryExit: PlayerController 未找到"));
        return;
    }

    // 获取 StateComponent
    UVoxelControllerStateComponent* StateComp = PC->StateComp;
    if (!StateComp)
    {
        UE_LOG(LogTemp, Error, TEXT("TestEditorModeEntryExit: StateComp 未找到"));
        return;
    }

    // 创建测试区域
    AVoxelAreaBaseInstance* TestArea = GetWorld()->SpawnActor<AVoxelAreaBaseInstance>();

    // ===== 测试 1: 进入编辑器模式 =====
    UE_LOG(LogTemp, Log, TEXT("=== 测试 1: 进入编辑器模式 ==="));

    StateComp->EnterEditorMode(TestArea);

    // 验证状态
    bool bInEditorMode = StateComp->IsInEditorMode();
    AVoxelAreaBaseInstance* CurrentArea = StateComp->GetEditorModeArea();

    UE_LOG(LogTemp, Log, TEXT("IsInEditorMode: %s"), bInEditorMode ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("CurrentArea: %s"), CurrentArea ? *CurrentArea->GetName() : TEXT("nullptr"));

    // 断言验证
    check(bInEditorMode == true);
    check(CurrentArea == TestArea);

    // ===== 测试 2: 通知 Subsystem =====
    UE_LOG(LogTemp, Log, TEXT("=== 测试 2: 通知 Subsystem ==="));

    UVoxelEditorModeSubsystem* Subsystem = GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>();
    bool bNotifySuccess = Subsystem->NotifyPlayerEnteredEditorMode(PC, TestArea);

    UE_LOG(LogTemp, Log, TEXT("通知 Subsystem: %s"), bNotifySuccess ? TEXT("成功") : TEXT("失败"));

    check(bNotifySuccess == true);

    // ===== 测试 3: 通过 Subsystem 查询玩家状态 =====
    UE_LOG(LogTemp, Log, TEXT("=== 测试 3: 查询玩家状态 ==="));

    bool bPlayerInEditorMode = Subsystem->IsPlayerInEditorMode(PC);

    UE_LOG(LogTemp, Log, TEXT("通过 Subsystem 查询 IsPlayerInEditorMode: %s"),
           bPlayerInEditorMode ? TEXT("true") : TEXT("false"));

    check(bPlayerInEditorMode == true);

    // ===== 测试 4: 退出编辑器模式 =====
    UE_LOG(LogTemp, Log, TEXT("=== 测试 4: 退出编辑器模式 ==="));

    StateComp->ExitEditorMode();

    // 验证状态
    bInEditorMode = StateComp->IsInEditorMode();
    CurrentArea = StateComp->GetEditorModeArea();

    UE_LOG(LogTemp, Log, TEXT("IsInEditorMode: %s"), bInEditorMode ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("CurrentArea: %s"), CurrentArea ? *CurrentArea->GetName() : TEXT("nullptr"));

    check(bInEditorMode == false);
    check(CurrentArea == nullptr);

    // ===== 测试 5: 通知 Subsystem 退出 =====
    UE_LOG(LogTemp, Log, TEXT("=== 测试 5: 通知 Subsystem 退出 ==="));

    bNotifySuccess = Subsystem->NotifyPlayerExitedEditorMode(PC);

    UE_LOG(LogTemp, Log, TEXT("通知 Subsystem 退出: %s"), bNotifySuccess ? TEXT("成功") : TEXT("失败"));

    check(bNotifySuccess == true);

    // 清理
    TestArea->Destroy();

    UE_LOG(LogTemp, Log, TEXT("=== 所有测试通过 ==="));
}
```

---

### 性能测试

**测试目标**: 验证频繁进入/退出编辑器模式的性能

```cpp
// 性能测试: 连续进入/退出 100 次
void PerformanceTest_EditorModeToggle()
{
    AVoxelPlayerController* PC = GetWorld()->GetFirstPlayerController<AVoxelPlayerController>();
    AVoxelAreaBaseInstance* TestArea = GetWorld()->SpawnActor<AVoxelAreaBaseInstance>();

    double StartTime = FPlatformTime::Seconds();

    for (int32 i = 0; i < 100; i++)
    {
        PC->StateComp->EnterEditorMode(TestArea);
        PC->StateComp->ExitEditorMode();
    }

    double EndTime = FPlatformTime::Seconds();
    double Duration = (EndTime - StartTime) * 1000.0;  // 转换为毫秒

    UE_LOG(LogTemp, Log, TEXT("性能测试: 100 次进入/退出耗时 %.2f ms"), Duration);
    UE_LOG(LogTemp, Log, TEXT("平均每次: %.2f ms"), Duration / 200.0);  // 200 次操作

    TestArea->Destroy();

    // 预期: 平均每次 < 1ms
    check(Duration / 200.0 < 1.0);
}
```

---

### 总结

**关键变化**:

1. **状态管理从 Subsystem 移至 StateComponent**
   - 旧: `Subsystem->GetCurrentState()`
   - 新: `PC->StateComp->IsInEditorMode()`

2. **支持多人独立状态**
   - 每个玩家的 `StateComp` 独立管理状态
   - Subsystem 提供服务，不持有状态

3. **接口签名变化**
   - 旧: `Subsystem->EnterEditorMode(Area)`
   - 新: `PC->StateComp->EnterEditorMode(Area)` + `Subsystem->NotifyPlayerEnteredEditorMode(PC, Area)`

**测试通过标准**:
- ✅ 玩家可以通过 `StateComp` 进入/退出编辑器模式
- ✅ `IsInEditorMode()` 正确反映状态
- ✅ `GetEditorModeArea()` 返回正确区域
- ✅ Subsystem 通知成功（可选）
- ✅ 多玩家状态独立（如果支持）

---

**测试场景 1 修订完成**

**下次审核**: 代码实施后验证

**相关文档**:
- 架构澄清: `VoxelEditingSystem-ArchitectureClarification.md`
- 完整测试指南: `Phase1-TestGuide.md`
