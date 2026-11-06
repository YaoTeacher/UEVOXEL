# BP_TopDownPlayerController TAG化改造设计

## 一、设计目标

将 `BP_TopDownPlayerController` 蓝图中的 `ISBATCH` 逻辑全面替换为 `CurrentToolTag` 驱动，消除 `IsBatchModeEnabled` 依赖，统一使用 GameplayTag 状态机管理建造交互。

**核心原则**：不新增枚举，直接通过 `CurrentToolTag` 判断交互模式。C++ 侧的 `HandleGizmoInteraction`、`HandleBuildModeParamsUpdated` 等函数已经通过 `CurrentToolTag` 判断，蓝图只需对齐。

---

## 二、TAG定义与交互模式映射

### 2.1 工具TAG层级

```
Tool                          -- 工具根TAG
├── Tool.Selection            -- 选择工具
│   ├── Tool.Selection.SingleVoxel      -- 单体素选择
│   ├── Tool.Selection.SimpleGeometry   -- 几何体选区
│   ├── Tool.Selection.Magic            -- 魔棒选择
│   └── Tool.Selection.Brush            -- 笔刷选择
│
├── Tool.Edit                   -- 编辑工具
│   ├── Tool.Edit.SingleVoxel         -- 单体素编辑
│   ├── Tool.Edit.SimpleGeometry      -- 几何体编辑(原Batch)
│   ├── Tool.Edit.Selection           -- 选区编辑
│   ├── Tool.Edit.Template            -- 模板编辑
│   └── Tool.Edit.Furniture           -- 家具编辑
│
├── Tool.Sample                 -- 采样工具
│
└── Tool.AreaEdit               -- 区域编辑
    ├── Tool.AreaEdit.Create          -- 创建区域
    └── Tool.AreaEdit.Modify          -- 修改区域
```

### 2.2 交互模式分类

根据工具TAG前缀，将交互分为 **4种模式**：

| 模式 | TAG匹配 | 交互特征 | 蓝图行为 |
|------|--------|----------|----------|
| **单体素模式** | `Tool.Edit.SingleVoxel` | 单击放置/删除单个方块 | Triggered: 射线检测→放置/删除 |
| **Gizmo拖拽模式** | `Tool.Edit.SimpleGeometry`<br>`Tool.AreaEdit.*` | 拖拽Gizmo手柄调整几何体/区域 | Triggered: 检测3DUI→开始拖拽<br>Ongoing: 更新Gizmo位置<br>Completed: 释放Gizmo |
| **选区模式** | `Tool.Selection.*` | 拖拽框选/魔棒/笔刷选择体素 | Triggered: 开始选区<br>Ongoing: 更新选区范围<br>Completed: 确认选区 |
| **模板/家具模式** | `Tool.Edit.Template`<br>`Tool.Edit.Furniture` | 放置预览→确认位置 | Triggered: 创建预览<br>Ongoing: 更新预览位置<br>Completed: 确认放置 |

---

## 三、核心判断逻辑

### 3.1 替代 ISBATCH 的TAG判断

```
原逻辑: IsBatchModeEnabled() == true
新逻辑: CurrentToolTag != Tool.Edit.SingleVoxel

原逻辑: IsBatchModeEnabled() == false
新逻辑: CurrentToolTag == Tool.Edit.SingleVoxel
```

### 3.2 蓝图节点替换对照

| 原节点 | 新节点 | 说明 |
|--------|--------|------|
| `Is Batch Mode Enabled` | `Get CurrentToolTag` → `Matches Tag` | 比较 `Tool.Edit.SingleVoxel` |
| `IfThenElse (ISBATCH)` | `IfThenElse (TAG==SingleVoxel)` | 条件语义反转，需交换THEN/ELSE |

---

## 四、EventGraph 新逻辑设计

### 4.1 蓝图变量变更

| 操作 | 变量名 | 类型 | 说明 |
|------|--------|------|------|
| **保留** | `HitUIActor` | `Voxel3DUIActor` | 当前命中的UI Actor |
| **保留** | `BlockDistance` | `int` | 体素放置距离 |
| **删除** | ~~`ISBATCH`相关引用~~ | -- | 全部移除 |

### 4.2 IA_Select Triggered (按下瞬间)

```
[EnhancedInputAction IA_Select] --Triggered-->
                                              |
                                              ▼
                                    ┌─────────────────────┐
                                    │ Get CurrentToolTag  │
                                    │ from StateComp      │
                                    └─────────────────────┘
                                              |
                                              ▼
                                    ┌─────────────────────┐
                                    │ If: == SingleVoxel? │
                                    └─────────────────────┘
                                              |
                          ┌───────────────────┴───────────────────┐
                          ▼                                       ▼
                    ┌───────────┐                         ┌───────────────┐
                    │ YES       │                         │ NO            │
                    │(单体素)    │                         │(Gizmo/选区等)  │
                    └─────┬─────┘                         └───────┬───────┘
                          │                                       |
                          ▼                                       ▼
                    ┌───────────┐                         ┌───────────────┐
                    │ LineTrace │                         │ LineTrace     │
                    │ 获取Index │                         │ 检测3DUI      │
                    └─────┬─────┘                         └───────┬───────┘
                          │                                       |
                          ▼                                       ▼
                    ┌───────────┐                         ┌───────────────┐
                    │ Modify    │                         │ Cast To       │
                    │ Block by  │                         │ Voxel3DUIActor│
                    │ Index     │                         │ IsValid?      │
                    └───────────┘                         └───────┬───────┘
                                                                  |
                                                        ┌─────────┴─────────┐
                                                        ▼                   ▼
                                                  ┌──────────┐      ┌──────────┐
                                                  │ YES      │      │ NO       │
                                                  │ Handle   │      │ Set      │
                                                  │ Gizmo    │      │ HitUIActor│
                                                  │ Interaction     │ = null   │
                                                  │ (开始拖拽)│      └──────────┘
                                                  └──────────┘
```

**关键变更：**
1. 原 `Triggered` 分支直接 Cast To Voxel3DUIActor，**没有工具判断**
2. 新逻辑在 Triggered 时就判断 `CurrentToolTag`，单体素直接走放置逻辑
3. 非单体素才检测 3DUI 并进入 Gizmo 交互

### 4.3 IA_Select Ongoing (持续按下)

```
[EnhancedInputAction IA_Select] --Ongoing-->
                                            |
                                            ▼
                                  ┌─────────────────────┐
                                  │ Get CurrentToolTag  │
                                  │ from StateComp      │
                                  └─────────────────────┘
                                            |
                                            ▼
                                  ┌─────────────────────┐
                                  │ If: == SingleVoxel? │
                                  └─────────────────────┘
                                            |
                          ┌─────────────────┴─────────────────┐
                          ▼                                   ▼
                    ┌───────────┐                     ┌───────────────┐
                    │ YES       │                     │ NO            │
                    │(单体素)    │                     │(Gizmo模式)     │
                    └───────────┘                     └───────┬───────┘
                                                              |
                                                              ▼
                                                        ┌───────────────┐
                                                        │ HandleGizmo   │
                                                        │ Interaction   │
                                                        │ (更新位置)     │
                                                        └───────────────┘
```

**关键变更：**
1. 单体素模式下 Ongoing **无操作**（避免每帧重复放置）
2. 非单体素模式下调用 HandleGizmoInteraction 更新位置

### 4.4 IA_Select Completed (正常释放)

```
[EnhancedInputAction IA_Select] --Completed-->
                                               |
                                               ▼
                                     ┌─────────────────────┐
                                     │ Get CurrentToolTag  │
                                     └─────────────────────┘
                                               |
                                               ▼
                                     ┌─────────────────────┐
                                     │ If: == SingleVoxel? │
                                     └─────────────────────┘
                                               |
                          ┌────────────────────┴────────────────────┐
                          ▼                                         ▼
                    ┌───────────┐                           ┌───────────────┐
                    │ YES       │                           │ NO            │
                    │(无操作)    │                           │ ReleaseGizmo  │
                    │           │                           │ Interaction   │
                    └───────────┘                           │               │
                                                            └───────┬───────┘
                                                                    |
                                                                    ▼
                                                              ┌──────────┐
                                                              │ Set      │
                                                              │ HitUIActor│
                                                              │ = null   │
                                                              └──────────┘
```

### 4.5 IA_Select Canceled (取消)

```
[EnhancedInputAction IA_Select] --Canceled-->
                                              |
                                              ▼
                                    ┌─────────────────────┐
                                    │ Get CurrentToolTag  │
                                    └─────────────────────┘
                                              |
                                              ▼
                                    ┌─────────────────────┐
                                    │ If: == SingleVoxel? │
                                    └─────────────────────┘
                                              |
                          ┌───────────────────┴───────────────────┐
                          ▼                                       ▼
                    ┌───────────┐                         ┌───────────────┐
                    │ YES       │                         │ NO            │
                    │(无操作)    │                         │ CancelCurrent │
                    │           │                         │ Edit          │
                    └───────────┘                         │               │
                                                          └───────┬───────┘
                                                                  |
                                                                  ▼
                                                            ┌──────────┐
                                                            │ ReturnTo │
                                                            │ Parent   │
                                                            │ Area     │
                                                            └─────┬────┘
                                                                  |
                                                                  ▼
                                                            ┌──────────┐
                                                            │ Set      │
                                                            │ HitUIActor│
                                                            │ = null   │
                                                            └──────────┘
```

**关键变更：**
1. Completed 和 Canceled **分离处理**
2. Completed：释放Gizmo（确认操作）
3. Canceled：CancelCurrentEdit → ReturnToParentArea（回滚操作）

### 4.6 IA_Release (右键/取消键)

```
[EnhancedInputAction IA_Release] --Triggered-->
                                               |
                                               ▼
                                     ┌─────────────────────┐
                                     │ Get CurrentToolTag  │
                                     └─────────────────────┘
                                               |
                                               ▼
                                     ┌─────────────────────┐
                                     │ If: == SingleVoxel? │
                                     └─────────────────────┘
                                               |
                          ┌────────────────────┴────────────────────┐
                          ▼                                         ▼
                    ┌───────────┐                           ┌───────────────┐
                    │ YES       │                           │ NO            │
                    │(无操作)    │                           │ CancelCurrent │
                    │           │                           │ Edit          │
                    └───────────┘                           │               │
                                                            └───────┬───────┘
                                                                    |
                                                                    ▼
                                                              ┌──────────┐
                                                              │ ReturnTo │
                                                              │ Parent   │
                                                              │ Area     │
                                                              └─────┬────┘
                                                                    |
                                                                    ▼
                                                              ┌──────────┐
                                                              │ Set      │
                                                              │ HitUIActor│
                                                              │ = null   │
                                                              └──────────┘
```

**关键变更：**
1. 删除 `Switch on EVoxelControllerMode`
2. 直接判断 `CurrentToolTag`
3. 单体素模式下右键无操作
4. 非单体素模式下执行取消逻辑

### 4.7 Tick (每帧)

```
[Event Tick] -->
                 |
                 ▼
       ┌─────────────────────┐
       │ Get ControllerMode  │
       └─────────────────────┘
                 |
                 ▼
       ┌─────────────────────┐
       │ == SceneBuildMode?  │
       └─────────────────────┘
                 |
       ┌─────────┴─────────┐
       ▼                   ▼
   ┌─────────┐       ┌─────────────┐
   │ 其他模式 │       │ SceneBuild  │
   │(无操作)  │       │ Mode        │
   └─────────┘       └──────┬──────┘
                            |
                            ▼
                   ┌─────────────────┐
                   │ Get CurrentTool │
                   │ Tag             │
                   └─────────────────┘
                            |
                            ▼
                   ┌─────────────────┐
                   │ == SingleVoxel? │
                   └─────────────────┘
                            |
                   ┌────────┴────────┐
                   ▼                 ▼
               ┌─────────┐     ┌─────────────┐
               │ YES     │     │ NO          │
               │(无操作)  │     │(需要预览)   │
               └─────────┘     └──────┬──────┘
                                      |
                                      ▼
                             ┌─────────────────┐
                             │ Update Preview  │
                             │ (调用VisualComp)│
                             └─────────────────┘
```

**关键变更：**
1. 删除每帧 `LineTrace` + `BlockIndexMath`
2. 只在非单体素模式下更新预览
3. 射线检测移到输入事件中按需执行

### 4.8 Tab键 (切换游戏/UI模式)

```
[InputKey Tab] --Pressed-->  Set Game Mode To Game / Set Game Mode To UI

[InputKey Tab] --Released--> (无操作)
```

**关键变更：**
1. **只在 Pressed 时切换**，Released 无操作
2. 修复按Tab无效果的BUG

---

## 五、蓝图节点改造清单

### 5.1 删除节点

| 节点 | 位置 | 原因 |
|------|------|------|
| `Is Batch Mode Enabled` (F9B5E405) | EventGraph ~4656,-3584 | 替换为TAG判断 |
| `Is Batch Mode Enabled` (E3D5A9E7) | EventGraph ~5520,-1696 | 替换为TAG判断 |
| `IfThenElse 1E38112D` | EventGraph ~5824,-1648 | 死代码，无上游连接 |
| `Cancel Current Edit` (9D31038F) | 1E38112D下游 | 死代码 |
| `Return to Parent Area` (55323B16) | 9D31038F下游 | 死代码 |

### 5.2 新增节点

| 节点 | 类型 | 位置 | 用途 |
|------|------|------|------|
| `Get CurrentToolTag` | 函数调用 | Triggered/Ongoing/Completed/Canceled | 从StateComp获取TAG |
| `Matches Tag` | 函数调用 | 条件判断 | 匹配 `Tool.Edit.SingleVoxel` |

### 5.3 修改节点

| 原节点 | 修改内容 |
|--------|----------|
| `IfThenElse 46575082` (Triggered条件) | 条件从 `IsBatchModeEnabled` 改为 `CurrentToolTag == Tool.Edit.SingleVoxel`，**交换THEN/ELSE** |
| `IfThenElse C643F08B` (Completed/Canceled) | 条件从 `IsBatchModeEnabled` 改为 `CurrentToolTag != Tool.Edit.SingleVoxel`，**分离Completed和Canceled** |
| `IA_Release` 分支 | 删除 `Switch on EVoxelControllerMode`，改为 `If: CurrentToolTag != SingleVoxel` |
| `Tick` 分支 | 删除 `LineTrace` + `BlockIndexMath`，改为条件判断后更新预览 |
| `Tab` Released | 断开所有连接，设为空操作 |

---

## 六、C++ 侧配合修改

### 6.1 无需新增枚举

`ESelectInteractionState` 枚举**不需要添加**。原因：

1. `CurrentToolTag` 本身就是状态标识
2. `HandleGizmoInteraction` 已经通过 `CurrentToolTag` 判断（第646行）
3. `HandleBuildModeParamsUpdated` 已经通过 `CurrentToolTag` 判断（第604行）
4. 蓝图只需直接读取 `CurrentToolTag` 做判断

### 6.2 现有C++判断逻辑（已就绪）

```cpp
// VoxelControllerInteractComponent.cpp:646
if (CurrentTool == FCoreTags::Tool_Edit_SingleVoxel)
{
    // 单体素编辑模式，忽略Gizmo交互
    return;
}

// VoxelControllerVisualUIComponent.cpp:604
if (CurrentToolTag == FCoreTags::Tool_Edit_SingleVoxel)
{
    // 单体素预览
}
else if (CurrentToolTag == FCoreTags::Tool_Edit_SimpleGeometry ||
         CurrentToolTag == FCoreTags::Tool_AreaEdit_Create ||
         CurrentToolTag == FCoreTags::Tool_AreaEdit_Modify)
{
    // 几何体预览
}
```

---

## 七、实施步骤

### 步骤1：删除ISBATCH节点和死代码 (10分钟)
1. 删除 `Is Batch Mode Enabled` (F9B5E405)
2. 删除 `Is Batch Mode Enabled` (E3D5A9E7)
3. 删除 `IfThenElse 1E38112D` 及其下游死代码

### 步骤2：改造Triggered分支 (20分钟)
1. 在 IA_Select Triggered 后添加 `Get CurrentToolTag`
2. 添加 `Matches Tag` 节点比较 `Tool.Edit.SingleVoxel`
3. 连接分支：
   - YES (单体素): LineTrace → ModifyBlockByIndex
   - NO (其他): LineTrace → Cast To Voxel3DUIActor → HandleGizmoInteraction

### 步骤3：改造Ongoing分支 (15分钟)
1. 添加 `Get CurrentToolTag`
2. 添加 `Matches Tag` 比较 `Tool.Edit.SingleVoxel`
3. YES: 空操作
4. NO: HandleGizmoInteraction

### 步骤4：分离Completed和Canceled (20分钟)
1. 复制原分支为两个独立分支
2. Completed: `CurrentToolTag != SingleVoxel` → ReleaseGizmoInteraction
3. Canceled: `CurrentToolTag != SingleVoxel` → CancelCurrentEdit → ReturnToParentArea

### 步骤5：改造IA_Release (10分钟)
1. 删除 `Switch on EVoxelControllerMode`
2. 添加 `Get CurrentToolTag`
3. `!= SingleVoxel` → CancelCurrentEdit → ReturnToParentArea

### 步骤6：改造Tick (10分钟)
1. 删除 `LineTrace` + `BlockIndexMath`
2. 添加条件：`CurrentToolTag != SingleVoxel` → Update Preview

### 步骤7：修复Tab键 (5分钟)
1. 断开 Tab Released 的所有连接

### 步骤8：编译验证 (10分钟)
1. 编译蓝图
2. 测试各工具类型的交互

---

## 八、验证清单

| 测试项 | 预期行为 |
|--------|----------|
| 单体素工具 + 单击 | 放置/删除一个方块 |
| 单体素工具 + 按住 | 无连续放置 |
| 几何体工具 + 单击Gizmo | 开始拖拽Gizmo |
| 几何体工具 + 拖拽 | Gizmo跟随鼠标 |
| 几何体工具 + 释放 | 释放Gizmo，应用编辑 |
| 几何体工具 + 取消 | 取消编辑，回滚 |
| 选区工具 + 拖拽 | 显示选区框 |
| 选区工具 + 释放 | 确认选区 |
| 选区工具 + 右键 | 清除选区 |
| 模板工具 + 移动 | 预览跟随鼠标 |
| 模板工具 + 单击 | 确认放置 |
| Tab键按下 | 切换游戏/UI模式 |
| Tab键释放 | 无操作 |
| 右键(任何状态) | 取消当前交互 |
