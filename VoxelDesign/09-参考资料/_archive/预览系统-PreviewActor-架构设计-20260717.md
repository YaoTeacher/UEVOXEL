# PreviewActor 架构设计

> **文档目的**：定义PreviewActor的代码结构、接口设计和组件关系
> **系统定位**：PreviewActor是预览系统的核心渲染组件，**服务于编辑系统**，负责实时显示编辑操作的可视化反馈
> **创建日期**：2026-01-16
> **版本**：v2.1
> **状态**：已实现

**v2.0 更新**：重大重构，移除所有Gizmo管理代码，PreviewActor专注纯渲染
**v2.1 更新**：明确PreviewActor服务于编辑系统的定位

---

## 一、概述

### 1.1 PreviewActor定位

**PreviewActor**是VisualUIComponent管理的独立Actor，专注于建造预览的**纯渲染**。

**核心定位**：
- 是一个独立的AActor（不是UActorComponent）
- 被VisualUIComponent持有和管理
- **只涉及渲染处理，不涉及Gizmo管理**
- 存储渲染状态，但接收VisualUIComponent的控制指令

### 1.2 设计原则

1. **Actor自治**：存储自己的渲染状态，提供接口供外部调用
2. **职责单一**：专注于预览渲染，不处理业务逻辑，不管理Gizmo
3. **只读业务数据**：通过FVoxelEditRequest接收完整的渲染数据
4. **单向数据流**：接收VisualUIComponent的渲染指令，不反向调用

---

## 二、类设计

### 2.1 类基本信息

```
类名：AVoxelPreviewActor
基类：AActor
命名空间：Voxel
文件位置：
├─ Public/Voxel/Editor/VoxelPreviewActor.h
└─ Private/Voxel/Editor/VoxelPreviewActor.cpp
```

### 2.2 核心成员变量

#### 渲染状态

| 变量名 | 类型 | 访问权限 | 用途 |
|--------|------|----------|------|
| **PreviewState** | EPreviewInteractionState | Private | 流程状态（HIDDEN/BASE/EDIT/CONFIRM） |
| **CurrentMode** | EPreviewMode | Private | 预览模式（None/SingleVoxel/SimpleGeometry/ComplexBlueprint） |
| **CurrentBounds** | FBox | Private | 当前包围盒 |
| **bHasValidBounds** | bool | Private | 是否有有效包围盒 |

#### Controller引用

| 变量名 | 类型 | 访问权限 | 用途 |
|--------|------|----------|------|
| **OwningController** | TWeakObjectPtr<AVoxelPlayerController> | Private | 所属Controller（弱引用，避免循环引用） |

**设计说明**：
- 使用弱引用避免循环引用
- 仅用于可能的辅助功能，主要数据通过FVoxelEditRequest传入

### 2.3 核心组件

| 组件名 | 类型 | 访问权限 | 用途 |
|--------|------|----------|------|
| **WireframeMesh** | UStaticMeshComponent | Protected | 外框网格组件 |
| **InnerMesh** | UStaticMeshComponent | Protected | 内部网格组件 |
| **ThicknessMesh** | UStaticMeshComponent | Protected | 厚度网格组件 |
| **BlueprintMesh** | UProceduralMeshComponent | Protected | 复杂蓝图组件 |

---

## 三、接口设计

### 3.1 初始化接口

#### SetOwningController

**签名**：
```cpp
void SetOwningController(AVoxelPlayerController* Controller)
```

**参数**：
- Controller - 所属的PlayerController

**调用时机**：VisualUIComponent创建PreviewActor后立即调用

**用途**：建立PreviewActor与Controller的连接（弱引用）

---

### 3.2 渲染状态控制接口

#### SetPreviewState

**签名**：
```cpp
void SetPreviewState(EPreviewInteractionState NewState)
```

**参数**：
- NewState - 新的流程状态（HIDDEN/BASE/EDIT/CONFIRM）

**调用者**：VisualUIComponent

**用途**：设置流程状态，触发内部渲染更新

---

#### SetPreviewMode

**签名**：
```cpp
void SetPreviewMode(EPreviewMode NewMode)
```

**参数**：
- NewMode - 预览模式（None/SingleVoxel/SimpleGeometry/ComplexBlueprint）

**调用者**：VisualUIComponent（内部调用）

**用途**：设置预览模式，更新渲染配置

---

#### GetPreviewState / GetPreviewMode

**签名**：
```cpp
EPreviewInteractionState GetPreviewState() const
EPreviewMode GetPreviewMode() const
```

**返回值**：当前流程状态 / 预览模式

**用途**：外部查询当前状态

---

### 3.3 渲染数据更新接口

#### UpdateFromEditRequest（统一更新入口）⭐核心接口

**签名**：
```cpp
void UpdateFromEditRequest(const FVoxelEditRequest& Request)
```

**参数**：
- Request - 编辑请求（包含 Geometry + BuildState + EditState + BlockType）

**调用者**：VisualUIComponent.HandleTmpEditRequestChanged()

**用途**：
- 统一更新入口：接收Request后自动更新所有渲染子项
- PreviewActor根据Request中的完整信息自动决定渲染模式
- 内部调用UpdateRenderFromRequest()更新渲染

**设计原则**：
- **Geometry保持纯粹**：只包含几何数学描述（位置、大小、形状、旋转）
- **Request包含状态**：BuildState/EditState/BlockType 是渲染需要的业务数据
- **无重复查询**：不需要在PreviewActor中再次查询StateComponent
- **数据完整**：Request包含渲染需要的所有信息
- **单向数据流**：StateComponent → VisualUIComponent → PreviewActor

**数据流**：
```
StateComponent.SetTmpGeometry()
  ↓
广播 OnTmpEditRequestChanged(Request)
  ↓
VisualUIComponent.HandleTmpEditRequestChanged(Request)
  ↓
PreviewActor.UpdateFromEditRequest(Request)
  └→ UpdateRenderFromRequest(Request)
      ├→ 使用 Request.BuildState
      ├→ 使用 Request.EditState
      ├→ 使用 Request.BlockType
      ├→ 使用 Request.Geometry
      └→ 调用UpdateXxxPreview()更新渲染
```

**注意事项**：
- 此方法是主要的公共接口，外部应该优先调用此方法
- PreviewActor**不再查询**StateComponent，所有业务状态从Request获取
- PreviewActor**不再管理Gizmo**，所有Gizmo由VisualUIComponent管理

---

#### UpdateSingleVoxelPreview（内部实现）

**签名**：
```cpp
void UpdateSingleVoxelPreview(
    FVector Position,
    FGameplayTag BlockType,
    FLinearColor Color,
    float Size = 100.0f)
```

**参数**：
- Position - 位置（世界坐标）
- BlockType - 方块类型
- Color - 颜色
- Size - 尺寸

**调用者**：UpdateRenderFromRequest（内部调用）

**用途**：更新单体素预览渲染（内部实现方法）

---

#### UpdateSimpleGeometryPreview

**签名**：
```cpp
void UpdateSimpleGeometryPreview(
    FLinearColor Color,
    FVoxelGeometryDesc Geo,
    float BlockLength)
```

**参数**：
- Color - 预览颜色
- Geo - 几何描述
- BlockLength - 方块边长

**调用者**：UpdateRenderFromRequest（内部调用）

**用途**：更新简单几何体预览渲染（内部实现方法）

---

#### UpdateComplexBlueprintPreview

**签名**：
```cpp
void UpdateComplexBlueprintPreview(FBox Bounds, const FChunkMeshData& MeshData, FGameplayTag BlockType)
```

**参数**：
- Bounds - 包围盒
- MeshData - 网格数据
- BlockType - 方块类型

**调用者**：UpdateRenderFromRequest（内部调用）

**用途**：更新复杂蓝图预览渲染（内部实现方法）

---

### 3.4 可见性控制接口

#### SetPreviewVisible

**签名**：
```cpp
void SetPreviewVisible(bool bVisible)
```

**参数**：
- bVisible - 是否可见

**调用者**：VisualUIComponent

**用途**：控制PreviewActor整体可见性

---

#### ClearPreview

**签名**：
```cpp
void ClearPreview()
```

**用途**：清除预览可视化（隐藏所有组件）

---

### 3.5 变换控制接口

#### MovePreview / RotatePreview

**签名**：
```cpp
void MovePreview(FVector DeltaPosition)
void RotatePreview(FRotator Rotation)
```

**用途**：移动/旋转预览（由VisualUIComponent调用）

---

## 四、状态管理设计

### 4.1 状态定义

```
流程状态（EPreviewInteractionState）:
├─ HIDDEN - 初始/隐藏状态（无预览）
├─ BASE - 拖动定义底面
├─ EDIT - 推拉任意面调整包围盒
└─ CONFIRM - 确认状态

预览模式（EPreviewMode）:
├─ None - 无预览
├─ SingleVoxel - 单体素预览
├─ SimpleGeometry - 简单几何体预览
└─ ComplexBlueprint - 复杂蓝图预览
```

### 4.2 状态转换处理

**方法**：SetPreviewState

**签名**：
```cpp
void SetPreviewState(EPreviewInteractionState NewState)
```

**触发时机**：
- VisualUIComponent在Request更新时通过UpdateFromEditRequest传入新状态
- 内部状态变更需要切换状态时

**职责**：
- 验证状态转换是否合法（通过CanTransitionTo）
- 更新PreviewState成员变量
- 根据新状态执行对应的状态控制逻辑（使用switch语句）

**状态控制逻辑**：
```cpp
switch (NewState)
{
case EPreviewInteractionState::HIDDEN:
    SetActorHiddenInGame(true);
    ClearPreview();
    break;
case EPreviewInteractionState::BASE:
    SetActorHiddenInGame(false);
    // 显示2D包围盒预览
    break;
case EPreviewInteractionState::EDIT:
    SetActorHiddenInGame(false);
    // 显示3D包围盒预览
    break;
case EPreviewInteractionState::CONFIRM:
    SetActorHiddenInGame(false);
    // 显示确认预览
    break;
}
```

**设计原则**：
- ✅ 单一入口：所有状态更新通过SetPreviewState完成
- ✅ 逻辑集中：每个状态的控制逻辑都在一个方法内（switch语句）
- ✅ 职责清晰：PreviewActor只负责自身的渲染控制
- ✅ 数据驱动：状态变化由VisualUIComponent通过UpdateFromEditRequest传入

---

## 五、数据接收原则

### 5.1 通过Request接收完整数据

**接口**：`UpdateFromEditRequest(const FVoxelEditRequest& Request)`

**数据来源**：
```
StateComponent.GetCurrentEditRequest()
  ├─ Geometry（几何数学描述）
  ├─ BuildState（建造状态）
  ├─ EditMode（编辑模式）
  └─ BlockType（方块类型）
```

**数据流**：
```
StateComponent
  ↓ 广播 OnTmpEditRequestChanged
VisualUIComponent.HandleTmpEditRequestChanged
  ↓ 调用
PreviewActor.UpdateFromEditRequest(Request)
  ↓ 直接使用
UpdateRenderFromRequest(Request)
  ├→ Request.BuildState（无需查询）
  ├→ Request.EditMode（无需查询）
  ├→ Request.BlockType（无需查询）
  └→ Request.Geometry（几何数据）
```

### 5.2 设计原则

```
✅ 允许：
├─ 从Request中读取所有渲染需要的数据
└─ 使用Request.Geometry计算渲染位置和形状

❌ 禁止：
├─ 主动查询StateComponent（避免重复查询）
├─ 修改Request中的数据
├─ 直接调用Geometry的操作方法
└─ 写入任何业务数据到StateComponent
```

**关键改进**：
- ✅ **无重复查询**：所有业务状态从Request获取，不需要查询StateComponent
- ✅ **Geometry保持纯粹**：只包含几何数学描述，不混杂业务状态
- ✅ **数据完整性**：Request包含渲染需要的所有信息
- ✅ **单向数据流**：StateComponent → VisualUIComponent → PreviewActor

---

## 六、组件关系

### 6.1 依赖关系

```
AVoxelPreviewActor
├─ 依赖：AVoxelPlayerController（弱引用）
├─ 接收：FVoxelEditRequest（包含完整渲染数据）
├─ 被持有：UVoxelControllerVisualUIComponent
└─ 不管理：Gizmo（由VisualUIComponent管理）
```

**关键变化**：
- ❌ 不再访问 StateComponent（避免重复查询）
- ❌ 不再管理 Gizmo（职责已移除）
- ✅ 接收完整的 FVoxelEditRequest
- ✅ Request 包含所有渲染需要的数据

### 6.2 交互关系

```
VisualUIComponent
├─ 初始化：
│   └─ InitPreviewActor() → 创建PreviewActor实例
├─ 监听广播：
│   └─ HandleTmpEditRequestChanged(Request) → 接收StateComponent广播
├─ 调用PreviewActor：
│   └─ UpdateFromEditRequest(Request) → 统一更新接口
└─ 管理Gizmo：
    └─ 所有Gizmo的创建、更新、显示控制

PreviewActor
├─ 接收数据：
│   └─ UpdateFromEditRequest(Request) → 接收完整渲染数据
└─ 内部更新：
    └─ UpdateRenderFromRequest(Request) → 使用Request中的状态
```

---

## 七、相关决策记录

本文档涉及的关键设计决策记录在以下文档中：

| 决策 | 文档 |
|------|------|
| PreviewActor统一更新接口传递Request | `05-设计决策记录/预览系统-PreviewActor传递Request-决策记录.md` |
| PreviewActor不管理Gizmo | `05-设计决策记录/预览系统-Gizmo移至VisualUIComponent-决策记录.md` |
| 分组管理Gizmo | `05-设计决策记录/预览系统-Gizmo分组管理-决策记录.md` |

**最新决策（2025-01）**：
- ✅ **传递FVoxelEditRequest**：StateComponent广播完整的EditRequest，PreviewActor接收Request后不再查询StateComponent
- ✅ **Geometry保持纯粹**：Geometry只包含几何数学描述，不混杂业务状态
- ✅ **无重复查询**：所有业务状态（BuildState/EditState/BlockType）从Request获取
- ✅ **Gizmo职责分离**：所有Gizmo管理移至VisualUIComponent，PreviewActor专注渲染

详细的问题分析、候选方案对比和决策理由请查阅对应的决策记录文档。

---

## 八、实现建议

### 8.1 文件组织

```
Source/Voxel/
├── Public/Voxel/Editor/
│   └── VoxelPreviewActor.h
└── Private/Voxel/Editor/
    └── VoxelPreviewActor.cpp
```

### 8.2 实现优先级

```
阶段1：基础框架
├─ 实现类结构
├─ 实现核心组件
└─ 实现基础接口

阶段2：状态管理
├─ 实现状态转换逻辑
├─ 实现状态处理方法
└─ 实现组件可见性控制

阶段3：渲染系统
├─ 实现网格渲染
├─ 实现预览模式切换
└─ 实现包围盒计算
```

### 8.3 测试建议

#### 单元测试重点

1. **状态转换测试**
   - 测试HIDDEN → BASE → EDIT转换
   - 测试状态处理方法调用

2. **组件可见性测试**
   - 测试不同预览模式的组件显示

#### 集成测试重点

1. **与VisualUIComponent交互测试**
   - 测试状态同步
   - 测试数据流完整性

2. **只读访问测试**
   - 测试只读Request数据
   - 测试不修改业务数据

---

## 九、附录

### 9.1 相关文档

- `预览系统-Gizmo管理-系统设计.md` - Gizmo详细设计
- `编辑系统-Controller-架构设计.md` - Controller整体架构
- `预览系统-状态管理-系统设计.md` - 状态管理设计
- `编辑系统-FVoxelGeometryDesc-系统设计.md` - FVoxelGeometryDesc操作设计
- `编辑系统-FVoxelGeometryDesc数据结构-架构设计.md` - FVoxelGeometryDesc数据结构

### 9.2 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |
| v1.1 | 2026-01-16 | 重写，去掉具体实现代码，添加Geometry操作接口 |
| v2.0 | 2026-01-18 | **重大重构**：移除所有Gizmo管理代码，PreviewActor专注纯渲染 |

---

## 十、与编辑系统的关系

### 10.1 服务定位

PreviewActor是**编辑系统的可视化反馈组件**，具有以下特点：

1. **渲染专职**：PreviewActor只负责渲染，不涉及编辑逻辑
   - 接收编辑请求：通过`FVoxelEditRequest`接收完整的渲染数据
   - 显示可视化：根据编辑类型和状态显示对应的预览效果
   - 不存储业务数据：所有业务数据来自编辑系统

2. **数据驱动**：
   ```
   编辑系统（VoxelControllerStateComponent）
     ↓ 存储业务数据
   FVoxelEditRequest（完整数据包）
     ↓ 传递给
   VisualUIComponent（协调层）
     ↓ 调用
   PreviewActor（渲染层）
     ↓ 显示
   可视化预览
   ```

3. **单向数据流**：
   - 数据流向：编辑系统 → VisualUIComponent → PreviewActor
   - PreviewActor不反向调用编辑系统
   - 所有交互事件通过VisualUIComponent中转

### 10.2 接口边界

**编辑系统 → PreviewActor**：
- `FVoxelEditRequest`：包含渲染需要的所有数据
  - Geometry：几何描述（位置、大小、形状、旋转）
  - BuildState：建造状态（决定预览模式）
  - EditState：编辑状态
  - BlockType：方块类型

**PreviewActor渲染输出**：
- 单体素预览：InnerMesh显示单个方块
- 简单几何体：WireframeMesh + InnerMesh + ThicknessMesh
- 复杂蓝图：BlueprintMesh显示模板网格

### 10.3 与编辑系统组件的交互

**VoxelControllerStateComponent**：
- 存储编辑状态和临时几何数据
- 广播`OnTmpEditRequestChanged`事件
- PreviewActor不直接访问，通过Request接收数据

**VoxelControllerVisualUIComponent**：
- 持有和管理PreviewActor
- 监听StateComponent的事件
- 调用PreviewActor的`UpdateFromEditRequest()`
- 管理Gizmo（PreviewActor不负责）

**VoxelControllerInteractComponent**：
- 协调编辑操作
- 更新StateComponent的业务数据
- 不直接操作PreviewActor

### 10.4 相关文档

**编辑系统文档**：
- [编辑系统-系统设计](/VoxelDesign/02-系统设计/编辑系统-系统设计.md) - 编辑系统总览（第3.5节预览系统交互）
- [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md) - 编辑系统架构（第2.5节预览系统接口）

**预览系统文档**：
- [预览系统-状态管理-系统设计](/VoxelDesign/02-系统设计/预览系统-状态管理-系统设计.md) - 状态管理设计

---

**文档状态**：✅ 已实现
