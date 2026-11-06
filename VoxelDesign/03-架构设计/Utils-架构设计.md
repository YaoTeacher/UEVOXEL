# Utils-架构设计

**版本**: 1.0
**创建日期**: 2026-01-24
**最后更新**: 2026-01-24

---

## 1. 组件概述

### 1.1 组件职责

**Utils（工具函数）** 提供全局可用的静态工具方法，包括：
- **坐标转换**：世界坐标 ↔ 体素索引 ↔ Chunk 索引
- **方向和旋转**：6 向离散旋转、体积旋转
- **数学计算**：对齐、投影、几何计算
- **数据验证**：参数验证、边界检测

**设计原则：**
- 所有方法为 `static`，无状态
- 纯函数式编程，无副作用
- 线程安全，可安全多线程调用
- 不依赖游戏状态，可在任何上下文调用

### 1.2 核心类

```cpp
class VOXEL_API UChunkUtilsFunction
{
public:
    // ==================== 坐标转换 ====================

    /**
     * 世界坐标 → 体素索引
     * @param WorldPosition 世界坐标（单位：米）
     * @param BlockLength 方块边长（单位：米）
     * @return 体素索引
     */
    static FIntVector GetBlockIndexByPosition(FVector WorldPosition, float BlockLength);

    /**
     * 体素索引 → 世界坐标（体素中心）
     * @param BlockIndex 体素索引
     * @param BlockLength 方块边长
     * @return 世界坐标（米）
     */
    static FVector GetPositionByBlockIndex(FIntVector BlockIndex, float BlockLength);

    /**
     * 世界坐标增量 → 体素索引增量
     * @param WorldDelta 世界坐标增量（米）
     * @param BlockLength 方块边长
     * @return 体素索引增量
     */
    static FIntVector WorldDeltaToIndexDelta(FVector WorldDelta, float BlockLength);

    /**
     * 单轴世界坐标增量 → 体素索引增量
     */
    static int32 WorldDeltaToIndexDelta_SingleAxis(float WorldDelta, float BlockLength);

    /**
     * 体素索引 → Chunk 索引
     * @param BlockIndex 体素索引
     * @param ChunkSize Chunk 尺寸（体素数量）
     * @return Chunk 索引
     */
    static FIntVector Get3DChunkIndexByBlockIndex(FIntVector BlockIndex, FIntVector ChunkSize);

    /**
     * 对齐世界坐标到体素网格
     */
    static FVector AlignPositionToVoxelGrid(FVector WorldPosition, float BlockLength);

    // ==================== 方向和旋转 ====================

    /**
     * 获取生成方向的旋转（用于 Actor 旋转）
     * @param InputDirection 6 向枚举
     * @return 对应的 FRotator
     */
    static FRotator GetSpawnDirection(EActorDirection InputDirection);

    /**
     * 旋转体积（浮点坐标，用于家具/物品）
     * @param InputDirection 6 向枚举
     * @param InputVolume 原始体积 (X,Y,Z)
     * @return 旋转后的体积
     */
    static FVector GetRotationVolume(EActorDirection InputDirection, FVector InputVolume);

    /**
     * 旋转整数坐标（用于模板/体素）
     * @param LocalPos 本地坐标（以原点为基准）
     * @param Direction 6 向枚举
     * @return 旋转后的整数坐标
     */
    static FIntVector GetRotatedIntVector(FIntVector LocalPos, EActorDirection Direction);

    // ==================== 编辑器工具辅助 ====================

    /**
     * 获取道具方向字符串（用于调试）
     */
    static FString GetDirectionString(EActorDirection Direction);
};
```

---

## 2. 接口定义

### 2.1 6 向离散旋转

**枚举定义：**
```cpp
enum class EActorDirection : uint8
{
    Left      UMETA(DisplayName = "Left"),      // 面向左（西）
    Right     UMETA(DisplayName = "Right"),     // 面向右（东）
    Backward  UMETA(DisplayName = "Backward"),  // 面向后（南）
    Forward   UMETA(DisplayName = "Forward"),   // 面向前（北）
    Up        UMETA(DisplayName = "Up"),        // 面向顶
    Down      UMETA(DisplayName = "Down"),      // 面向底
    Null                                               // 不旋转
};
```

**旋转矩阵定义：**

| 方向 | 旋转变换 | 描述 |
|------|---------|------|
| **Forward** | `(x, y, z)` | 不旋转（基准方向） |
| **Backward** | `(-x, -y, z)` | 绕 Z 轴 180° |
| **Left** | `(y, -x, z)` | 绕 Z 轴 -90° |
| **Right** | `(-y, x, z)` | 绕 Z 轴 +90° |
| **Up** | `(x, -z, y)` | 绕 X 轴 +90° |
| **Down** | `(x, z, -y)` | 绕 X 轴 -90° |

### 2.2 坐标转换公式

**世界坐标 → 体素索引：**
```
BlockIndex.X = Floor(WorldPosition.X / BlockLength)
BlockIndex.Y = Floor(WorldPosition.Y / BlockLength)
BlockIndex.Z = Floor(WorldPosition.Z / BlockLength)
```

**体素索引 → 世界坐标（体素中心）：**
```
WorldPosition.X = (BlockIndex.X + 0.5) * BlockLength
WorldPosition.Y = (BlockIndex.Y + 0.5) * BlockLength
WorldPosition.Z = (BlockIndex.Z + 0.5) * BlockLength
```

**体素索引 → Chunk 索引：**
```
ChunkIndex.X = Floor(BlockIndex.X / ChunkSize.X)
ChunkIndex.Y = Floor(BlockIndex.Y / ChunkSize.Y)
ChunkIndex.Z = Floor(BlockIndex.Z / ChunkSize.Z)
```

---

## 3. 使用示例

### 3.1 模板旋转

```cpp
// 场景：模板应用时旋转
FIntVector LocalPos(5, 3, 2);  // 模板中的相对坐标

EActorDirection Direction = EActorDirection::Right;
FIntVector RotatedPos = UChunkUtilsFunction::GetRotatedIntVector(
    LocalPos,
    Direction
);
// RotatedPos = (-3, 5, 2)

FIntVector WorldPos = RotatedPos + AnchorPosition;
```

### 3.2 批量编辑旋转

```cpp
// 场景：批量编辑时旋转几何形状
FVector VolumeSize(100.0f, 200.0f, 50.0f);

EActorDirection Direction = EActorDirection::Backward;
FVector RotatedVolume = UChunkUtilsFunction::GetRotationVolume(
    Direction,
    VolumeSize
);
// RotatedVolume = (-100.0f, -200.0f, 50.0f)
```

### 3.3 坐标转换链路

```cpp
// 场景：鼠标射线检测到世界坐标，转换为体素索引
FVector HitWorldLocation = Result.ImpactPoint;
float BlockLength = 100.0f;

FIntVector BlockIndex = UChunkUtilsFunction::GetBlockIndexByPosition(
    HitWorldLocation,
    BlockLength
);

// 获取方块类型
FGameplayTag BlockType = BlockWorld->GetBlockType(BlockIndex);
```

---

## 4. 数学原理

### 4.1 离散旋转的数学基础

**整数坐标旋转的正确性保证：**
- 所有旋转都是 **90° 的整数倍**
- 旋转变换矩阵的元素为 `{-1, 0, 1}`
- 不存在浮点误差

**示例：绕 Z 轴 +90°（Left）**
```
数学公式：
x' = x*cos(90°) - y*sin(90°) = x*0 - y*1 = -y
y' = x*sin(90°) + y*cos(90°) = x*1 + y*0 = x
z' = z

整数实现：(x, y, z) → (y, -x, z)
```

### 4.2 坐标系统的左手性

**虚幻引擎使用左手坐标系：**
- X 轴：右
- Y 轴：前
- Z 轴：上

**旋转方向遵循左手定则：**
- 拇指指向旋转轴正向
- 四指弯曲方向为旋转正方向

---

## 5. 扩展点

### 5.1 新增旋转方法

**步骤：**
1. 在 `UChunkUtilsFunction` 中添加新的静态方法
2. 使用 `EActorDirection` 枚举定义方向
3. 添加单元测试验证旋转正确性
4. 更新本文档

**示例：**
```cpp
/**
 * 旋转整数向量（绕 Z 轴 45°，非标准用途）
 */
static FIntVector GetRotatedIntVector45(FIntVector LocalPos, EActorDirection Direction);
```

### 5.2 新增坐标转换方法

**步骤：**
1. 遵循命名规范：`Get[Source]To[Target]`
2. 使用 `static` 函数
3. 添加参数验证和边界检测
4. 提供单元测试

---

## 6. 相关文档

- [编辑系统-模板系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-模板系统-架构设计.md)
- [编辑系统-选区系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-选区系统-架构设计.md)
- [编辑系统-编码规范](/VoxelDesign/06-开发规范/编码规范.md)

---

## 版本历史

- 2026-01-24: 初始版本
  - 定义坐标转换接口
  - 定义 6 向离散旋转接口
  - 补充 GetRotatedIntVector 方法（用于模板旋转）
  - Council 辩论：Phase 0 设计澄清
