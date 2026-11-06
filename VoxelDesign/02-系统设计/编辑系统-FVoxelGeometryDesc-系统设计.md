# 编辑系统 FVoxelGeometryDesc 系统设计

> **文档目的**：定义FVoxelGeometryDesc模块的系统边界、核心概念、操作流程和数据流
> **创建日期**：2026-01-16
> **版本**：v1.0
> **状态**：已确定

---

## 一、概述

### 1.1 模块定位

**FVoxelGeometryDesc**是编辑系统的核心数据结构，负责描述待编辑体素的几何属性。

**核心职责**：
- 存储包围盒定义（Coverage）
- 存储旋转中心（Center）
- 提供四种核心操作方法
- 保证数据一致性

### 1.2 设计背景

在预览系统的交互设计中，存在一个关键问题：
- 用户拖动面控件调整包围盒时，包围盒的几何中心会发生变化
- 如果旋转中心跟随几何中心实时变化，会导致旋转轴心不稳定
- 用户无法精确控制旋转操作

### 1.3 解决方案核心思想

**将Center的更新时机与操作解耦**：
- Center默认固定，不自动跟随Coverage变化
- 只有在特定操作（MoveGEO、Recenter）时才更新Center
- 保证旋转操作的稳定性

### 1.4 架构原则

```
FVoxelGeometryDesc（核心数据层）
├─ Coverage（包围盒定义）
├─ Center（旋转中心，固定）
├─ Rotation（旋转角度）
└─ ShapeType, Thickness等

PreviewActor（纯渲染层）
└─ 完全基于FVoxelGeometryDesc数据渲染
```

**关键原则**：PreviewActor只是FVoxelGeometryDesc的可视化结果，不存储任何状态。

---

## 二、系统边界

### 2.1 模块范围

FVoxelGeometryDesc模块包含：

```
核心数据：
├─ FVoxelGeometryDesc（几何描述）
│  ├─ FAreaCoverageFragment Box（包围盒）
│  ├─ FRotator Rotation（旋转角度）
│  ├─ EBatchShapeType ShapeType（形状类型）
│  ├─ int32 Thickness（厚度）
│  ├─ bool bIsRegular（规整标志）
│  └─ FVector Center（旋转中心，Private）
│
└─ FAreaCoverageFragment（包围盒片段）
   ├─ FIntVector MinIndex（最小索引）
   ├─ FIntVector MaxIndex（最大索引）
   └─ FIntVector CoreOffsetIndex（已废弃）
```

### 2.2 核心概念

| 概念 | 定义 | 坐标空间 |
|------|------|---------|
| **Coverage** | 包围盒的体素索引范围 | 体素索引空间 |
| **Center** | 旋转中心（世界坐标） | 世界坐标空间 |
| **GeometricCenter** | 基于Coverage计算的几何中心 | 世界坐标空间 |
| **Rotation** | 旋转角度 | 欧拉角 |

### 2.3 模块职责

| 职责 | 说明 |
|------|------|
| **数据存储** | 存储包围盒、旋转、形状等几何数据 |
| **数据访问** | 提供访问器方法读取几何数据 |
| **数据操作** | 提供四种核心方法操作几何数据 |
| **一致性保证** | 保证数据的一致性和完整性 |

---

## 三、核心操作方法

### 3.1 操作分类

| 操作方法 | 影响 | 调用时机 | Center变化 |
|---------|------|---------|------------|
| **MoveGEO** | Coverage + Center | 拖动移动Gizmo | 同时更新 |
| **ModifyCoverage** | Coverage | 拖动面控件 | 不变 |
| **Recenter** | Center | 点击UI按钮 | 重新计算 |
| **RotateGEO** | Rotation | 拖动旋转Gizmo | 不变 |

---

### 3.2 MoveGEO（移动）

#### 职责
移动整个FVoxelGeometryDesc，保持包围盒形状不变。

#### 影响
- `Box.MinIndex` 和 `Box.MaxIndex` 同时加上偏移量
- `Center` 同时移动（保持与包围盒的相对位置不变）
- `Rotation`、`ShapeType`、`Thickness` 不变

#### 调用时机
- 用户拖动移动Gizmo（X/Y/Z轴箭头）

#### 操作流程
```
输入：Delta（FIntVector，体素索引偏移）

流程：
1. Box.MinIndex += Delta
2. Box.MaxIndex += Delta
3. 重新计算 Center（基于新的Coverage）
4. 更新FVoxelGeometryDesc

结果：
- 整个包围盒移动
- Center跟随移动
- 包围盒形状不变
```

#### 用户视角
- 整个预览框沿某个轴向移动
- 旋转中心跟随移动
- Gizmo跟随Center移动

---

### 3.3 ModifyCoverage（变更盒体）

#### 职责
只改变包围盒的形状，不改变位置。

#### 影响
- `Box.MinIndex` 或 `Box.MaxIndex` 的某个轴分量变化
- `Center` **保持不变**（关键）
- `Rotation`、`ShapeType`、`Thickness` 不变

#### 调用时机
- 用户拖动面控件（6个面）

#### 操作流程
```
输入：Axis（轴向），bMaxSide（最大面还是最小面），Delta（偏移量）

流程：
1. 根据Axis和bMaxSide确定要修改的边界
2. 更新 Box.MinIndex 或 Box.MaxIndex 的对应分量
3. Center 保持不变
4. 更新FVoxelGeometryDesc

结果：
- 包围盒形状改变（某个面移动）
- Center 保持不变
- 包围盒的几何中心与Center发生偏离
```

#### 用户视角
- 预览框的某个面被推出/拉入
- 旋转中心保持在原位置
- 如果旋转，会围绕旧的Center，看起来"偏离"

---

### 3.4 Recenter（重置中心）

#### 职责
将旋转中心重置到"旋转后的实际几何中心"，解决"偏离"问题。

#### 核心概念
- **几何中心 C**：基于 Coverage 计算的未旋转几何中心
- **旋转中心 A**：当前的 Center（旋转轴心）
- **旋转后的实际几何中心 B**：将 C 绕 A 旋转 Rotation 角度后的位置

#### 影响
- `Box` 不变
- `Center` 重新计算（旋转后的实际几何中心）
- `Rotation`、`ShapeType`、`Thickness` 不变

#### 调用时机
- 用户点击UI"重置中心点"按钮

#### 操作流程
```
输入：无

流程：
1. 计算当前 Coverage 的几何中心 C（未旋转）
   C = (Box.MinIndex + Box.MaxIndex + 1) / 2 * BlockLength

2. 获取当前旋转中心 A
   A = Center（当前值）

3. 计算旋转后的位置 B
   向量 AC = C - A
   旋转向量 AC：RotatedAC = AC.RotateVector(Rotation)
   B = A + RotatedAC

4. 体素对齐
   Center = FMath::RoundToInt(B / BlockLength) * BlockLength

5. 更新 FVoxelGeometryDesc

结果：
- Center 移动到旋转后的实际几何中心
- 后续旋转围绕新 Center
- 不再"偏离"
```

#### 用户视角
- 旋转中心点移动到"旋转后的包围盒"的中心
- Gizmo 移动到新位置
- 旋转看起来"正常"了

#### 关键问题
**为什么需要旋转计算？**
- Box 存储在索引空间，永远不旋转
- 但视觉上盒子已经旋转了 Rotation 角度
- 用户看到的是旋转后的盒子
- Recenter 应该重置到用户看到的几何中心
- 因此需要将几何中心 C 绕当前 Center A 旋转 Rotation 角度

---

### 3.5 RotateGEO（旋转）

#### 职责
只改变旋转角度，不改变几何。

#### 影响
- `Rotation` 变化
- `Box`、`Center`、`ShapeType`、`Thickness` 不变

#### 调用时机
- 用户拖动旋转Gizmo（X/Y/Z轴圆环）

#### 操作流程
```
输入：DeltaRotation（FRotator，旋转增量）

流程：
1. FVoxelGeometryDesc.Rotation += DeltaRotation
2. Box、Center 保持不变
3. 更新FVoxelGeometryDesc

结果：
- FVoxelGeometryDesc旋转
- 包围盒位置和Center不变
- PreviewActor围绕Center旋转
```

#### 用户视角
- 预览框围绕Center旋转
- Gizmo保持在Center位置
- 旋转轴心稳定

---

## 四、Center管理机制

### 4.1 Center的固定性

**核心原则**：Center默认固定，不自动跟随Coverage变化。

```
正常操作：
- MoveGEO：Center 更新（跟随移动）
- ModifyCoverage：Center 不变（保持固定）
- RotateGEO：Center 不变
- Recenter：Center 更新（手动重置）
```

### 4.2 Center计算方法

#### 几何中心计算
```
GetGeometricCenter():
1. MinPos = FVector(Box.MinIndex)
2. MaxPos = FVector(Box.MaxIndex) + FVector(1.0f)
3. Return (MinPos + MaxPos) * 0.5f * BlockLength
```

#### 体素对齐中心计算
```
1. 计算包围盒的几何中心
2. 对每个轴分别对齐到最近的体素中心
3. 返回体素对齐的世界坐标
```

### 4.3 Center更新时机

| 操作 | Center更新 | 原因 |
|------|-----------|------|
| **MoveGEO** | 是 | 整体移动，Center跟随 |
| **ModifyCoverage** | 否 | 只改变形状，保持固定 |
| **Recenter** | 是 | 手动重置到几何中心 |
| **RotateGEO** | 否 | 只改变旋转，保持固定 |

---

## 五、数据流设计

### 5.1 MoveGEO 数据流

```
用户拖动移动Gizmo
    ↓
蓝图检测输入并计算体素索引
    ↓
InteractComponent处理业务逻辑
    ↓
FVoxelGeometryDesc.MoveGEO(Delta)
    ├─ Box.MinIndex += Delta
    ├─ Box.MaxIndex += Delta
    └─ Center 重新计算
    ↓
StateComponent更新
    ↓
VisualUIComponent读取FVoxelGeometryDesc
    ↓
PreviewActor更新渲染
```

### 5.2 ModifyCoverage 数据流

```
用户拖动面控件
    ↓
蓝图检测输入并计算体素索引
    ↓
InteractComponent处理业务逻辑
    ↓
FVoxelGeometryDesc.ModifyCoverage(Axis, bMaxSide, Delta)
    ├─ Box.MinIndex.X 或 Box.MaxIndex.X += Delta
    └─ Center 保持不变
    ↓
StateComponent更新
    ↓
VisualUIComponent读取FVoxelGeometryDesc
    ↓
PreviewActor更新渲染
    ↓
用户感觉：包围盒形状改变，旋转中心还在原位置
```

### 5.3 Recenter 数据流

```
用户点击"重置中心点"按钮
    ↓
VisualUIComponent检测UI事件
    ↓
调用FVoxelGeometryDesc.Recenter()
    ├─ 计算几何中心 C（未旋转）
    ├─ 获取当前旋转中心 A
    ├─ 计算 B = 将 C 绕 A 旋转 Rotation 角度
    ├─ 体素对齐 B
    └─ 更新 Center
    ↓
StateComponent更新
    ↓
VisualUIComponent读取FVoxelGeometryDesc
    ↓
PreviewActor更新渲染
    ├─ Gizmo移动到新Center（旋转后的实际几何中心）
    └─ 后续旋转围绕新Center
```

### 5.4 RotateGEO 数据流

```
用户拖动旋转Gizmo
    ↓
蓝图检测输入并计算体素索引
    ↓
InteractComponent处理业务逻辑
    ↓
FVoxelGeometryDesc.RotateGEO(DeltaRotation)
    └─ Rotation += DeltaRotation
    ↓
StateComponent更新
    ↓
VisualUIComponent读取FVoxelGeometryDesc
    ↓
PreviewActor更新渲染
    ├─ 围绕Center旋转
    └─ Gizmo保持在Center位置
```

---

## 六、与PreviewActor的关系

### 6.1 渲染原则

PreviewActor**完全基于FVoxelGeometryDesc数据渲染**，不存储任何状态。

### 6.2 渲染流程

```
每帧/数据更新时：
1. 从StateComponent读取FVoxelGeometryDesc
2. 读取FVoxelGeometryDesc.Box → 计算包围盒
3. 读取FVoxelGeometryDesc.Rotation → 设置旋转
4. 读取FVoxelGeometryDesc.Center → 设置Gizmo位置
5. 渲染包围盒和Gizmo
```

### 6.3 Transform计算

```
PreviewActor.WorldLocation:
  - 不直接使用
  - 或者设置为FVoxelGeometryDesc.Center（用于可视化）

PreviewActor.WorldRotation:
  - FVoxelGeometryDesc.Rotation

包围盒显示：
  - 基于 Box.MinIndex/MaxIndex
  - 应用 Rotation 变换
  - 不受 Center 影响

Gizmo位置：
  - 始终在 FVoxelGeometryDesc.Center
  - 不受其他因素影响
```

---

## 七、操作场景示例

### 7.1 场景1：正常编辑流程

```
1. BASE完成 → 初始化Center（基于几何中心）

2. 用户拖动顶面向上
   → ModifyCoverage
   → 包围盒变高，几何中心上移
   → Center 保持不变

3. 用户旋转
   → RotateGEO
   → 围绕原始Center旋转
   → 用户看到"偏离"（盒子变高了，但旋转轴还在原位置）

4. 用户点击"重置中心点"
   → Recenter
   → 计算几何中心 C（未旋转的新中心）
   → 将 C 绕旧 Center A 旋转 Rotation 角度
   → 得到旋转后的实际几何中心 B
   → Center 移动到 B

5. 用户再旋转
   → RotateGEO
   → 围绕新Center旋转
   → 旋转"正常"
```

### 7.2 场景2：先旋转后调整

```
1. BASE完成 → 初始化Center
2. 用户先旋转
   → RotateGEO
   → 围绕Center旋转
3. 用户拖动顶面向上
   → ModifyCoverage
   → 包围盒变高
   → Center 保持不变
4. 用户旋转
   → 围绕原始Center旋转
   → 看起来"正常"（因为Center还在包围盒内）
```

### 7.3 场景3：整体移动

```
1. 用户拖动移动Gizmo
   → MoveGEO
   → Center 移动
   → 包围盒跟随移动
   → Gizmo跟随Center
2. 用户旋转
   → RotateGEO
   → 围绕新Center旋转
   → 看起来"正常"
```

---

## 八、测试建议

### 8.1 单元测试

- 测试MoveGEO：Coverage和Center是否同时更新
- 测试ModifyCoverage：Coverage变化，Center是否不变
- 测试Recenter：
  * Center是否正确更新到旋转后的实际几何中心
  * 体素对齐是否正确
  * 边界情况处理（Box无效、Center无效等）
  * 旋转计算是否正确
- 测试RotateGEO：只有Rotation变化

### 8.2 集成测试

- 测试完整操作流程
- 测试数据流：Interact → State → Visual → Preview
- 测试Center的固定性

### 8.3 用户体验测试

- 测试"偏离"现象是否解决
- 测试Recenter后的旋转是否正常
- 测试移动后旋转是否正常

---

## 九、相关决策记录

本文档涉及的关键设计决策记录在以下文档中：

| 决策 | 文档 |
|------|------|
| Center字段设为Private | `05-设计决策记录/编辑系统-FVoxelGeometryDesc-Center访问控制-决策记录.md` |
| 废弃CoreOffsetIndex | `05-设计决策记录/编辑系统-FAreaCoverageFragment-废弃CoreOffsetIndex-决策记录.md` |
| 四种操作方法设计 | `05-设计决策记录/编辑系统-FVoxelGeometryDesc操作方法设计-决策记录.md` |

详细的问题分析、候选方案对比和决策理由请查阅对应的决策记录文档。

---

## 十、相关文档

### 10.1 架构设计

- `编辑系统-FVoxelGeometryDesc数据结构-架构设计.md` - FVoxelGeometryDesc数据结构详细设计

### 10.2 系统设计

- `预览系统-Gizmo管理-系统设计.md` - Gizmo交互设计
- `预览系统-状态管理-系统设计.md` - 状态管理设计

### 10.3 架构设计

- `编辑系统-Controller-架构设计.md` - Controller整体架构
- `预览系统-PreviewActor-架构设计.md` - PreviewActor代码结构

---

## 十一、版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |
| v1.1 | 2026-01-17 | 完善 Recenter 设计：添加旋转后的实际几何中心计算逻辑、核心概念说明和详细操作流程 |

---

**文档状态**：✅ 已确定
