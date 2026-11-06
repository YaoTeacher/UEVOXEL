# 编辑系统-工具Tag替代BuildState枚举-决策记录

**决策日期**: 2026-01-25
**决策状态**: ✅ 已确认

---

## 决策内容

废弃`EVoxelControllerBuildState`枚举类，使用GameplayTag工具层级来区分操作对象类型。

## 背景

### 原有架构

使用`EVoxelControllerBuildState`枚举区分三种编辑模式：

```cpp
enum class EVoxelControllerBuildState : uint8
{
    EditConstruction = 0,  // 地形建造（体素编辑）
    EditArea = 1,          // 划分区域
    PlaceFurniture = 2,    // 道具摆放（家具放置）
    OutBuildMode = 3,      // 非建造模式
    None
};
```

### 问题分析

**三个BuildState的差异**:

1. **EditConstruction**（体素编辑）
   - 操作对象: Voxel（体素）
   - 底层接口: `BlockWorld->BatchModifyVoxelData()`
   - 需要系统: 选区系统、几何工具
   - UI面板: BuildConstructPanel
   - 物品列表: Block类型

2. **EditArea**（区域编辑）
   - 操作对象: Area（区域）
   - 底层接口: `AreaWorld->ModifyArea()`
   - 需要系统: Geometry定义（包围盒）
   - UI面板: AreaModifyWidget
   - 物品列表: Area类型

3. **PlaceFurniture**（家具放置）
   - 操作对象: Furniture（家具/蓝图Actor）
   - 底层接口: `FurnitureWorld->PlaceFurniture()`
   - 需要系统: 模板系统
   - UI面板: ItemListPlaceWidget
   - 物品列表: Furniture/BuildItem类型

**核心差异**: 三者操作对象完全不同（Voxel vs Area vs Furniture），BuildState的作用是区分"当前在编辑哪种对象"。

---

## 决策

### 新架构：工具Tag层级管理

**使用GameplayTag替代枚举**:

```ini
; 第一层：工具功能分类
+GameplayTagList=(Tag="Voxel.Tool.Selection",DevComment="选区工具-用于定义选区")
+GameplayTagList=(Tag="Voxel.Tool.Edit",DevComment="编辑工具-用于修改世界")
+GameplayTagList=(Tag="Voxel.Tool.AreaEdit",DevComment="区域编辑工具")

; 第二层：选区工具子类型
+GameplayTagList=(Tag="Voxel.Tool.Selection.SingleVoxel",DevComment="单体素选择")
+GameplayTagList=(Tag="Voxel.Tool.Selection.SimpleGeometry",DevComment="简单几何体选择")
+GameplayTagList=(Tag="Voxel.Tool.Selection.Brush",DevComment="笔刷选择")

; 第二层：编辑工具子类型
+GameplayTagList=(Tag="Voxel.Tool.Edit.SingleVoxel",DevComment="单体素编辑-放置体素/家具")
+GameplayTagList=(Tag="Voxel.Tool.Edit.SimpleGeometry",DevComment="简单几何编辑")
+GameplayTagList=(Tag="Voxel.Tool.Edit.Selection",DevComment="选区操作工具-移动/复制/删除选区内容")
+GameplayTagList=(Tag="Voxel.Tool.Edit.Template",DevComment="模板放置工具")

; 第二层：区域编辑工具子类型
+GameplayTagList=(Tag="Voxel.Tool.AreaEdit.Create",DevComment="创建区域")
+GameplayTagList=(Tag="Voxel.Tool.AreaEdit.Modify",DevComment="修改区域")
```

### 工具分类说明

**选区工具（Voxel.Tool.Selection）**：
- 用于定义选区
- 输出：`FVoxelSelection`
- 示例：点击单体素、拖拽框选、笔刷涂抹

**编辑工具（Voxel.Tool.Edit）**：
- 用于修改世界
- 输出：`FVoxelEditModification`
- 示例：放置方块、建造几何体、移动选区、放置模板

**区域编辑工具（Voxel.Tool.AreaEdit）**：
- 用于编辑区域定义
- 输出：区域数据
- 示例：创建/修改领地

### StateComponent调整

**旧版**:
```cpp
class UVoxelControllerStateComponent
{
    UPROPERTY(BlueprintReadOnly)
    EVoxelControllerBuildState BuildState;  // ❌ 废弃
    
    UPROPERTY(BlueprintReadOnly)
    EVoxelControllerEditState EditState;    // ✅ 保留
};
```

**新版**:
```cpp
class UVoxelControllerStateComponent
{
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag CurrentToolTag;  // ✅ 替代BuildState
    
    UPROPERTY(BlueprintReadOnly)
    EVoxelControllerEditState EditState;  // ✅ 保留（Add/Delete/Replace/Complement）
    
    UPROPERTY(BlueprintReadOnly)
    TMap<FName, FString> ToolParameters;  // ✅ 工具参数（如几何体形状）
};
```

---

## 理由

### 优势

1. **可扩展性**: 新增工具类型无需修改枚举，只需添加新Tag
2. **层级化管理**: 工具Tag自然支持层级结构（功能分类→具体工具）
3. **数据驱动**: 工具配置可通过数据表管理，无需重新编译
4. **统一接口**: 所有工具通过统一的注册表管理
5. **职责清晰**: 选区工具（定义选区）与编辑工具（修改世界）分离

### 劣势

1. **类型安全性降低**: Tag是字符串，不如枚举类型安全
2. **运行时错误**: Tag拼写错误在运行时才发现，不如编译时检查

**缓解措施**:
- 使用`FGameplayTag::RequestGameplayTag()`确保Tag有效性
- 工具注册时验证Tag存在性
- 编写单元测试覆盖Tag相关逻辑

---

## 影响范围

### 需要修改的文件

1. **枚举定义**:
   - `Source/Voxel/Public/Core/Enum/GameStateEnum.h` - 标记`EVoxelControllerBuildState`为废弃（保留一段时间以兼容旧代码）

2. **StateComponent**:
   - `Source/Voxel/Public/Core/ControllerComponent/VoxelControllerStateComponent.h`
   - 移除`BuildState`成员，添加`CurrentToolTag`

3. **UI系统**:
   - `Source/Voxel/Public/Core/UI/Build/BuildPanelWidget.h`
   - 修改`HandleBuildStateChanged()`为`HandleToolChanged()`
   - 添加工具注册表和工具切换逻辑

4. **交互组件**:
   - `Source/Voxel/Public/Core/ControllerComponent/VoxelControllerInteractComponent.h`
   - 修改BuildState相关逻辑为ToolTag相关

### 需要更新的文档

1. `VoxelDesign/02-系统设计/编辑系统-系统设计.md`
2. `VoxelDesign/03-架构设计/编辑系统-架构设计.md`
3. `VoxelDesign/06-开发规范/编码规范.md`

---

## 实施计划

### Phase 1: 数据准备
- [ ] 添加工具Tag到`DefaultGameplayTags.ini`
- [ ] 标记`EVoxelControllerBuildState`为废弃（`UPROPERTY(Deprecated)`）
- [ ] 更新相关文档

### Phase 2: StateComponent重构
- [ ] 添加`CurrentToolTag`成员
- [ ] 添加`ToolParameters`成员
- [ ] 保留`BuildState`成员但标记为废弃
- [ ] 迁移现有逻辑使用`CurrentToolTag`

### Phase 3: UI系统适配
- [ ] 添加工具注册表到`BuildPanelWidget`
- [ ] 实现`InitializeToolRegistry()`
- [ ] 实现`SwitchToTool(FGameplayTag)`
- [ ] 修改UI面板显示逻辑

### Phase 4: 清理旧代码
- [ ] 移除`EVoxelControllerBuildState`枚举（在确认无引用后）
- [ ] 移除StateComponent中的`BuildState`成员
- [ ] 清理所有`GetBuildState()/SetBuildState()`调用

---

## 验证标准

### 功能验证

- [ ] 工具切换正常，UI面板正确显示
- [ ] 物品列表正确过滤（Block/Area/Furniture）
- [ ] 预览系统正常工作
- [ ] 编辑操作正确执行

### 性能验证

- [ ] Tag查询性能可接受（使用TMap索引）
- [ ] 工具切换无延迟

### 兼容性验证

- [ ] 旧代码迁移完成
- [ ] 无编译警告/错误

---

## 相关决策

- [编辑系统-C++与蓝图职责划分-架构设计](/VoxelDesign/03-架构设计/编辑系统-C++与蓝图职责划分-架构设计.md)
- [编辑系统-选区系统三分离架构-决策记录](/VoxelDesign/05-设计决策记录/编辑系统-选区系统三分离架构-决策记录.md)

---

**文档版本**: 1.0
**创建日期**: 2026-01-25
**状态**: ✅ 已确认，待实施
