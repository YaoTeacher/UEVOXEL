# 编辑系统 FVoxelGeometryDesc数据结构 架构设计

> **文档目的**：定义编辑系统的核心数据结构FVoxelGeometryDesc及其操作接口
> **创建日期**：2026-01-16
> **版本**：v1.2
> **状态**：已确定

---

## 一、概述

### 1.1 Geometry的作用

Geometry（几何描述）是编辑系统的核心数据结构，用于描述待编辑体素的几何属性。

### 1.2 设计原则

1. **单一数据源**：Geometry是预览和编辑的唯一数据源
2. **职责分离**：Geometry负责数据定义，PreviewActor负责渲染
3. **封装操作**：通过四种核心方法操作Geometry，保证数据一致性
4. **固定中心**：Center默认固定，避免旋转轴不稳定

---

## 二、FVoxelGeometryDesc 结构定义

### 2.1 结构体位置

```
命名空间：Voxel
文件位置：
├─ Public/Voxel/Data/VoxelGeometryDesc.h
└─ Private/Voxel/Data/VoxelGeometryDesc.cpp
```

### 2.2 字段列表

| 字段名 | 类型 | 访问权限 | 用途 |
|--------|------|----------|------|
| **Box** | FAreaCoverageFragment | Public | 包围盒定义（MinIndex, MaxIndex） |
| **Rotation** | FRotator | Public | 旋转角度（Pitch, Yaw, Roll） |
| **ShapeType** | EBatchShapeType | Public | 形状类型（盒体、球体、圆柱等） |
| **Thickness** | int32 | Public | 厚度（用于空心几何体） |
| **bIsRegular** | bool | Public | 是否为规整几何体 |
| **Center** | FVector | Private | 旋转中心（世界坐标） |

### 2.3 方法列表

#### 核心操作方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| **MoveGEO** | Delta (FIntVector) | void | 移动整个Geometry |
| **ModifyCoverage** | Axis (EAxis::Type), bMaxSide (bool), Delta (int32) | void | 修改包围盒形状 |
| **Recenter** | 无 | void | 重置旋转中心 |
| **RotateGEO** | DeltaRotation (FRotator) | void | 旋转Geometry |

#### 访问器方法

| 方法名 | 参数 | 返回值 | 说明 |
|--------|------|--------|------|
| **GetCenter** | 无 | FVector | 获取旋转中心 |
| **GetBound** | BlockLength (float) | FBox | 获取世界坐标包围盒 |
| **GetGeometricCenter** | BlockLength (float) | FVector | 获取几何中心 |
| **GetSize** | BlockLength (float) | FVector | 获取包围盒尺寸 |
| **GetExtents** | BlockLength (float) | FVector | 获取包围盒extents |

### 2.4 访问控制设计

```
Public字段（可读写）:
├─ Box - 包围盒定义
├─ Rotation - 旋转角度
├─ ShapeType - 形状类型
├─ Thickness - 厚度
└─ bIsRegular - 规整标志

Private字段（只读）:
└─ Center - 旋转中心

Public方法（访问器）:
├─ GetCenter() - 获取旋转中心
├─ GetBound() - 获取世界坐标包围盒
├─ GetGeometricCenter() - 获取几何中心
├─ GetSize() - 获取包围盒尺寸
└─ GetExtents() - 获取包围盒extents

Public方法（操作）:
├─ MoveGEO() - 移动整个Geometry
├─ ModifyCoverage() - 修改包围盒形状
├─ Recenter() - 重置旋转中心
└─ RotateGEO() - 旋转Geometry
```

**设计理由**：
- **Box, Rotation等Public**：便于外部直接访问和修改
- **Center Private**：防止外部直接修改，只能通过四种操作方法更新
- **访问器方法**：提供只读访问，保护数据完整性

---

## 三、FAreaCoverageFragment 结构定义

### 3.1 结构体位置

```
命名空间：Voxel
文件位置：
├─ Public/Voxel/Data/AreaDataFragment.h
└─ Private/Voxel/Data/AreaDataFragment.cpp
```

### 3.2 字段列表

| 字段名 | 类型 | 访问权限 | 用途 | 状态 |
|--------|------|----------|------|------|
| **MinIndex** | FIntVector | Public | 包围盒最小体素索引（包含） | 使用中 |
| **MaxIndex** | FIntVector | Public | 包围盒最大体素索引（包含） | 使用中 |
| **CoreOffsetIndex** | FIntVector | Public | 中心偏移索引 | **已废弃** |

### 3.3 包围盒约束

```
几何约束：
├─ MinIndex.X <= MaxIndex.X
├─ MinIndex.Y <= MaxIndex.Y
└─ MinIndex.Z <= MaxIndex.Z

坐标空间：
└─ 体素索引空间（非世界坐标）
```

### 3.4 CoreOffsetIndex 废弃说明

**废弃原因**：
- 该字段是体素索引偏移，不适合表示世界坐标的旋转中心
- 新的 `FVoxelGeometryDesc::Center` 字段更清晰
- 保留该字段是为了保持数据兼容性

**迁移建议**：
- 新代码使用 `FVoxelGeometryDesc::Center`（世界坐标）
- 旧代码可以继续使用 `CoreOffsetIndex`，但应逐步迁移
- 未来版本可能完全移除此字段

---

## 四、核心操作方法

### 4.1 操作影响矩阵

| 操作方法 | Box | Rotation | ShapeType | Thickness | bIsRegular | Center | 影响说明 |
|---------|-----|----------|-----------|-----------|------------|--------|---------|
| **MoveGEO** | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ | 移动整个Geometry |
| **ModifyCoverage** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | 改变包围盒形状 |
| **Recenter** | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | 重置Center到旋转后的实际几何中心 |
| **RotateGEO** | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | 旋转Geometry |

### 4.2 操作职责说明

#### MoveGEO（移动）

**输入**：Delta (FIntVector) - 体素索引偏移

**影响**：
- Box.MinIndex += Delta
- Box.MaxIndex += Delta
- Center += Delta * BlockLength

**约束**：无特殊约束

**调用时机**：用户拖动移动Gizmo

---

#### ModifyCoverage（变更盒体）

**输入**：
- Axis (EAxis::Type) - 轴向
- bMaxSide (bool) - 最大面还是最小面
- Delta (int32) - 偏移量

**影响**：
- Box.MinIndex 或 Box.MaxIndex 的对应分量 += Delta
- Center 保持不变

**约束**：
- 必须保证 MinIndex <= MaxIndex
- 实时检查约束，违反时钳位到有效范围

**调用时机**：用户拖动面控件

---

#### Recenter（重置中心）

**输入**：无

**影响**：
- Center = 旋转后的实际几何中心（体素对齐）

**计算流程**：
```
步骤1：计算未旋转的几何中心 C
  C = (Box.MinIndex + Box.MaxIndex + 1) / 2 * BlockLength

步骤2：获取当前旋转中心 A
  A = Center（当前值）

步骤3：计算旋转后的位置 B
  向量 AC = C - A
  旋转向量 AC：RotatedAC = AC.RotateVector(Rotation)
  B = A + RotatedAC

步骤4：体素对齐
  Center = FMath::RoundToInt(B / BlockLength) * BlockLength
```

**核心思想**：
- 包围盒定义（Box）存储在索引空间，永远不旋转
- 但视觉上盒子已经旋转了 Rotation 角度
- Recenter 的目标：将旋转中心重置到"旋转后的实际几何中心"
- 因此需要将几何中心 C 绕当前 Center A 旋转 Rotation 角度

**约束**：
- Box 约束检查：MinIndex <= MaxIndex（所有轴）
- Center 有效性检查：非 NaN、非 Infinity
- Rotation 不检查：允许任意旋转角度

**调用时机**：用户点击"重置中心点"按钮

---

#### RotateGEO（旋转）

**输入**：DeltaRotation (FRotator) - 旋转增量

**影响**：
- Rotation += DeltaRotation

**约束**：无特殊约束

**调用时机**：用户拖动旋转Gizmo

---

## 五、访问器方法

### 5.1 GetCenter() vs GetGeometricCenter()

| 方法 | 返回值 | 说明 |
|------|--------|------|
| **GetCenter()** | 旋转中心（固定） | 实际的旋转轴心，可能偏离几何中心 |
| **GetGeometricCenter()** | 包围盒几何中心 | 基于当前Coverage计算，动态变化 |

**关键差异**：
- `GetCenter()` 返回的是固定旋转中心，用于旋转操作
- `GetGeometricCenter()` 返回的是未旋转的包围盒几何中心（基于 Box 计算得出）
- ModifyCoverage 后两者会偏离，Recenter 后 `GetCenter()` 返回"旋转后的实际几何中心"

### 5.2 访问器用途

| 方法 | 用途 |
|------|------|
| **GetCenter()** | Gizmo定位、旋转轴心 |
| **GetBound()** | 渲染包围盒、碰撞检测 |
| **GetGeometricCenter()** | 可视化、偏离检测 |
| **GetSize()** | 缩放计算、尺寸显示 |
| **GetExtents()** | 边界计算、范围检查 |

---

## 六、数据一致性保证

### 6.1 一致性规则

```
规则1：Box 约束
└─ MinIndex <= MaxIndex（所有轴）

规则2：Center 对齐
└─ Center 应该体素对齐到体素中心

规则3：Center 固定性
└─ ModifyCoverage 和 RotateGEO 不改变Center

规则4：Center 跟随性
└─ MoveGEO 时 Center 跟随移动
```

### 6.2 约束检查

#### ModifyCoverage 约束检查

**检查逻辑**：
1. 计算修改后的新值
2. 检查是否违反 MinIndex <= MaxIndex 约束
3. 如果违反，钳位到有效范围
4. 应用修改

### 6.3 初始化流程

```
创建Geometry时：
1. 初始化 Box.MinIndex 和 Box.MaxIndex
2. 初始化 Rotation = ZeroRotator
3. 初始化 ShapeType, Thickness, bIsRegular
4. 计算并初始化 Center（基于初始Box的几何中心）
5. 体素对齐Center

保证：初始状态下 Center 与 GeometricCenter 重合
```

---

## 七、与其他组件的关系

### 7.1 组件交互图

```
UVoxelControllerStateComponent
└─ 存储 FVoxelGeometryDesc Geometry
    ├─ InteractComponent 修改 Geometry
    │  ├─ MoveGEO()
    │  ├─ ModifyCoverage()
    │  ├─ Recenter()
    │  └─ RotateGEO()
    └─ VisualUIComponent 读取 Geometry
       └─ 传递给 PreviewActor 渲染

AVoxelPreviewActor
└─ 读取 Geometry 数据（只读）
   ├─ GetCenter() → 设置Gizmo位置
   ├─ GetBound() → 计算包围盒
   ├─ GetGeometricCenter() → 可视化
   └─ Rotation → 设置旋转
```

### 7.2 数据所有权

| 组件 | 所有权 | 操作权限 |
|------|--------|---------|
| **StateComponent** | 存储 Geometry | 完全控制 |
| **InteractComponent** | 修改 Geometry | 通过操作方法 |
| **VisualUIComponent** | 读取 Geometry | 只读访问器 |
| **PreviewActor** | 渲染 Geometry | 只读访问器 |

### 7.3 调用关系

```
InteractComponent（业务逻辑）:
└─ StateComponent->Geometry.MoveGEO(Delta)
   └─ StateComponent->Geometry.ModifyCoverage(Axis, bMaxSide, Delta)
      └─ StateComponent->Geometry.Recenter()
         └─ StateComponent->Geometry.RotateGEO(DeltaRotation)

VisualUIComponent（渲染）:
└─ StateComponent->Geometry.GetCenter()
   └─ StateComponent->Geometry.GetBound(BlockLength)
      └─ StateComponent->Geometry.GetGeometricCenter(BlockLength)

PreviewActor（渲染执行）:
└─ 读取传入的Geometry数据
   └─ 更新渲染组件
```

---

## 八、相关决策记录

本文档涉及的关键设计决策记录在以下文档中：

| 决策 | 文档 |
|------|------|
| Center字段设为Private | `05-设计决策记录/编辑系统-FVoxelGeometryDesc-Center访问控制-决策记录.md` |
| 废弃CoreOffsetIndex | `05-设计决策记录/编辑系统-FAreaCoverageFragment-废弃CoreOffsetIndex-决策记录.md` |
| 四种操作方法 | `05-设计决策记录/编辑系统-FVoxelGeometryDesc操作方法设计-决策记录.md` |

详细的问题分析、候选方案对比和决策理由请查阅对应的决策记录文档。

---

## 九、实现建议

### 9.1 文件组织

```
Source/Voxel/
├── Public/Voxel/Data/
│   ├── VoxelGeometryDesc.h        // Geometry数据结构
│   └── AreaDataFragment.h         // Coverage数据结构
└── Private/Voxel/Data/
    ├── VoxelGeometryDesc.cpp      // Geometry方法实现
    └── AreaDataFragment.cpp       // Coverage方法实现
```

### 9.2 实现优先级

```
阶段1：基础结构
├─ 实现 FVoxelGeometryDesc 结构
├─ 实现 FAreaCoverageFragment 结构
└─ 实现访问器方法

阶段2：核心操作
├─ 实现 MoveGEO()
├─ 实现 ModifyCoverage()
├─ 实现 Recenter()
└─ 实现 RotateGEO()

阶段3：约束检查
├─ 实现 Box 约束检查
├─ 实现 Center 对齐逻辑
└─ 实现数据一致性验证

阶段4：集成
├─ 集成到 StateComponent
├─ 集成到 InteractComponent
└─ 集成到 VisualUIComponent
```

### 9.3 测试建议

#### 单元测试重点

1. **MoveGEO测试**
   - 验证Box和Center同时更新
   - 验证包围盒形状不变

2. **ModifyCoverage测试**
   - 验证Box变化，Center不变
   - 验证约束检查（MinIndex <= MaxIndex）

3. **Recenter测试**
   - 验证Center移动到旋转后的实际几何中心
   - 验证体素对齐
   - 验证 Box 约束检查（MinIndex <= MaxIndex）
   - 验证 Center 有效性检查（非 NaN、非 Infinity）
   - 边界情况测试：
     * Rotation = 0 时的行为（应直接重置到几何中心）
     * Center 与几何中心重合时的行为（应不变）
     * 包围盒无效时的行为（应失败或钳位）
   - 场景测试：
     * ModifyCoverage 后 Recenter
     * 旋转后 Recenter
     * 先旋转后 ModifyCoverage 再 Recenter

4. **RotateGEO测试**
   - 验证只有Rotation变化
   - 验证Box和Center不变

---

## 十、附录

### 10.1 相关文档

- `编辑系统-FVoxelGeometryDesc-系统设计.md` - FVoxelGeometryDesc四种操作方法的详细设计
- `编辑系统-Controller-架构设计.md` - Controller整体架构
- `预览系统-PreviewActor-架构设计.md` - PreviewActor渲染逻辑

### 10.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |
| v1.1 | 2026-01-16 | 重写，去掉具体实现代码 |
| v1.2 | 2026-01-16 | 重命名为FVoxelGeometryDesc，更新文档引用 |
| v1.3 | 2026-01-17 | 完善 Recenter 计算逻辑：添加旋转后的实际几何中心计算、边界检查和测试用例 |

---

**文档状态**：✅ 已确定
