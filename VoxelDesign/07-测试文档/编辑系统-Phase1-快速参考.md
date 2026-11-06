# 第一阶段测试 - 快速参考指南

## 快速启动

### 1. 编译项目
```bash
# 在 Visual Studio 2022 中
1. 打开 Voxel.sln
2. 选择 "Development Editor" 配置
3. 生成解决方案（Ctrl + Shift + B）
```

### 2. 创建测试关卡
```
1. 文件 → 新建关卡 → 默认关卡
2. 保存为 Content/Maps/Test/TestMap_Phase1_EditTest
```

### 3. 创建测试材质（可选）
```
1. 内容浏览器 → 右键 → 材质 → 材质
2. 命名为 M_PreviewMaterial
3. 设置半透明颜色（R:0, G:1, B:0, A:0.5）
```

---

## 控制台命令速查

### 基础命令

| 命令 | 功能 |
|------|------|
| `Voxel.ShowStatus` | 显示系统状态 |
| `Voxel.ExitEditorMode` | 退出编辑器模式 |
| `Voxel.TestEdit` | 测试单体素编辑 |
| `Voxel.Undo` | 撤销上一次编辑 |
| `Voxel.Redo` | 重做已撤销的编辑 |
| `Voxel.ClearHistory` | 清空历史记录 |

### 快速测试脚本

复制以下命令到 UE5 控制台（按 ` 键打开）：

```
Voxel.ShowStatus
Voxel.TestEdit
Voxel.ShowStatus
Voxel.Undo
Voxel.ShowStatus
Voxel.Redo
Voxel.ShowStatus
```

---

## 蓝图测试速查

### 测试编辑器模式

**节点序列**:
```
Event BeginPlay
  ↓
Delay 2.0s
  ↓
Get VoxelEditorModeSubsystem → Call EnterEditorMode
  ↓
Print "编辑器模式已进入"
  ↓
Delay 3.0s
  ↓
Call ExitEditorMode
  ↓
Print "编辑器模式已退出"
```

### 测试单体素编辑

**节点序列**:
```
Event BeginPlay
  ↓
Delay 2.0s
  ↓
For Loop (5 次)
  ↓
  Get VoxelBatchEditManager → EditSingleVoxel((Index*10, 0, 0), Dirt, Place)
  ↓
  Delay 0.5s
  ↓
Print "已执行 5 次编辑"
```

### 测试撤销/重做

**节点序列**:
```
Event BeginPlay
  ↓
Delay 2.0s
  ↓
// 执行编辑
EditSingleVoxel((0,0,0), Dirt, Place)
  ↓
Get HistoryManager → SaveSnapshot()
  ↓
// 撤销
Delay 1.0s → Undo()
  ↓
// 重做
Delay 1.0s → Redo()
```

---

## 关键 API 速查

### VoxelEditorModeSubsystem

```cpp
// 获取子系统
UVoxelEditorModeSubsystem* ModeSubsystem = GetWorld()->GetSubsystem<UVoxelEditorModeSubsystem>();

// 进入/退出编辑器模式
ModeSubsystem->EnterEditorMode(TargetArea);
ModeSubsystem->ExitEditorMode();

// 状态查询
bool bInMode = ModeSubsystem->IsInEditorMode();
EEditorModeState State = ModeSubsystem->GetCurrentState();
AVoxelAreaBaseInstance* Area = ModeSubsystem->GetCurrentArea();
```

### VoxelBatchEditManager

```cpp
// 获取子系统
UVoxelBatchEditManager* BatchManager = GetWorld()->GetSubsystem<UVoxelBatchEditManager>();

// 单体素编辑
BatchManager->EditSingleVoxel(
    FIntVector(0, 0, 0),           // 体素位置
    FGameplayTag(),                // 方块类型
    EVoxelControllerEditState::Place  // 编辑模式
);

// 预览编辑
BatchManager->StartPreviewEdit(Request);
BatchManager->UpdatePreview(Request);
BatchManager->ConfirmPreviewEdit();
BatchManager->CancelPreviewEdit();
```

### VoxelEditHistoryManager

```cpp
// 获取子系统
UVoxelEditHistoryManager* HistoryManager = GetWorld()->GetSubsystem<UVoxelEditHistoryManager>();

// 保存快照
HistoryManager->SaveSnapshot();

// 撤销/重做
HistoryManager->Undo();
HistoryManager->Redo();
HistoryManager->ClearHistory();

// 状态查询
int32 UndoCount = HistoryManager->GetUndoCount();
int32 RedoCount = HistoryManager->GetRedoCount();
bool bCanUndo = HistoryManager->CanUndo();
bool bCanRedo = HistoryManager->CanRedo();
```

---

## 事件委托速查

### VoxelEditorModeSubsystem 事件

| 事件 | 触发时机 | 参数 |
|------|---------|------|
| OnEditorModeEntered | 进入编辑器模式 | AVoxelAreaBaseInstance* |
| OnEditorModeExited | 退出编辑器模式 | 无 |
| OnEditorStateChanged | 状态改变 | EEditorModeState |

### VoxelBatchEditManager 事件

| 事件 | 触发时机 | 参数 |
|------|---------|------|
| OnSingleEditExecuted | 单体素编辑执行 | VoxelPosition, BlockType, EditMode, bSuccess |
| OnPreviewStarted | 预览开始 | FEditPreviewInfo |
| OnPreviewUpdated | 预览更新 | FEditPreviewInfo |
| OnPreviewConfirmed | 预览确认 | FEditPreviewInfo |
| OnPreviewCancelled | 预览取消 | 无 |

### VoxelEditHistoryManager 事件

| 事件 | 触发时机 | 参数 |
|------|---------|------|
| OnHistoryChanged | 历史记录变更 | 无 |
| OnUndoExecuted | 撤销执行 | FText Description |
| OnRedoExecuted | 重做执行 | FText Description |

---

## 调试技巧速查

### 1. 添加日志输出

```cpp
// 在 C++ 代码中
UE_LOG(LogTemp, Log, TEXT("编辑器状态: %d"), (int32)CurrentState);
UE_LOG(LogTemp, Warning, TEXT("警告: 子系统未初始化"));
UE_LOG(LogTemp, Error, TEXT("错误: 空指针"));
```

### 2. 蓝图 Print String

```
Print String "子系统: " + (IsValid ? "有效" : "无效")
Print String "位置: " + VoxelPosition.ToString()
Print String "栈大小: " + GetUndoCount()
```

### 3. 检查指针有效性

```cpp
if (!IsValid(Subsystem))
{
    UE_LOG(LogTemp, Error, TEXT("子系统未初始化"));
    return;
}
```

### 4. 断点位置

推荐的调试断点位置：
- `VoxelEditorModeSubsystem::EnterEditorMode()`
- `VoxelEditorModeSubsystem::SetEditorState()`
- `VoxelBatchEditManager::EditSingleVoxel()`
- `VoxelEditHistoryManager::SaveSnapshot()`
- `VoxelEditHistoryManager::Undo()`

---

## 常见错误速查

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| 子系统为 nullptr | 未初始化 | 检查 ShouldCreateSubsystem() |
| 编辑器模式无法进入 | 目标区域为空 | 传入有效的区域实例 |
| 撤销无效果 | 栈为空 | 检查 CanUndo() 返回值 |
| 预览不显示 | 材质未设置 | 设置 PreviewMaterial |
| 事件未触发 | 未绑定 | 使用 Bind Event 节点 |

---

## 测试检查清单（简化版）

### 编译
- [ ] 项目成功编译
- [ ] 无关键警告

### 运行时
- [ ] 编辑器启动成功
- [ ] 测试关卡加载成功

### 功能测试
- [ ] 编辑器模式进入/退出正常
- [ ] 单体素编辑正常
- [ ] 撤销/重做正常
- [ ] 事件触发正常

### 性能
- [ ] 无明显卡顿
- [ ] 无内存泄漏

---

## 文件路径速查

### 关键源文件
```
Source/Voxel/Public/Voxel/Editor/
├── VoxelEditorModeSubsystem.h
└── Managers/
    ├── VoxelBatchEditManager.h
    └── VoxelEditHistoryManager.h

Source/Voxel/Private/Voxel/Editor/
├── VoxelEditorModeSubsystem.cpp
└── Managers/
    ├── VoxelBatchEditManager.cpp
    └── VoxelEditHistoryManager.cpp
```

### 测试文档
```
Docs/Testing/
├── Phase1-TestGuide.md          # 详细测试指南
├── Phase1-TestResults.md        # 测试结果模板
└── Phase1-QuickReference.md     # 本文件
```

### 测试资源
```
Content/
├── Maps/Test/
│   └── TestMap_Phase1_EditTest.umap
├── Materials/
│   └── M_PreviewMaterial.uasset
└── Blueprints/Test/
    └── BP_TestActor_EditSystem.uasset
```

---

## 下一步

完成基础测试后：

1. **填写测试结果**: 使用 `Phase1-TestResults.md` 模板
2. **报告问题**: 记录所有发现的 Bug
3. **性能分析**: 收集性能数据
4. **第二阶段准备**: 规划后续开发任务

---

**提示**: 此文档提供快速参考，详细信息请查看 `Phase1-TestGuide.md`。
