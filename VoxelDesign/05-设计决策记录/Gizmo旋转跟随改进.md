# Gizmo 旋转跟随改进

**日期**: 2026-01-18
**类型**: 功能改进
**状态**: 已实现

---

## 问题描述

**原有问题**：
1. **FaceGizmo** 不跟随盒体旋转 - 当盒体旋转后，FaceGizmo 仍保持原始方向
2. **RotateGizmo** 不反映当前旋转状态 - 旋转环始终在默认轴向，不显示盒体的实际旋转

**用户需求**：
> "FaceGizmo 能跟着盒体的实际位置旋转，RotateGizmo 能在旋转中心点跟着角度旋转"

---

## 解决方案

### 1. 修改方法签名

为 Gizmo 更新方法添加 `GeometryRotation` 参数：

```cpp
// 头文件
void UpdateRotateGizmosPosition(FVector Center, FRotator GeometryRotation);
void UpdateFaceGizmosPosition(const FBox& Bounds, FRotator GeometryRotation);
void UpdateAllGizmosPosition(FVector Center, const FBox& Bounds, FRotator GeometryRotation);
```

---

### 2. RotateGizmo 实现

**原理**：
- 每个 RotateGizmo 有一个基础旋转（初始化时设定的轴向）
- 叠加几何旋转，使旋转环跟随盒体旋转

**实现**：
```cpp
void UVoxelControllerVisualUIComponent::UpdateRotateGizmosPosition(FVector Center, FRotator GeometryRotation)
{
	// 更新位置
	UpdateGizmoGroupLocation(RotateGizmos, Center);

	// 应用几何旋转到 RotateGizmo
	for (int32 i = 0; i < RotateGizmos.Num(); ++i)
	{
		if (RotateGizmos[i])
		{
			// 基础旋转（初始化时设定的轴向）
			FRotator BaseRotation;
			switch (i)
			{
			case 0: BaseRotation = FRotator(0, 0, 0); break;    // X 轴
			case 1: BaseRotation = FRotator(0, 90, 90); break;  // Y 轴
			case 2: BaseRotation = FRotator(90, 0, 0); break;   // Z 轴
			}

			// 叠加几何旋转
			FRotator FinalRotation = BaseRotation + GeometryRotation;
			RotateGizmos[i]->SetActorRotation(FinalRotation);
		}
	}
}
```

---

### 3. FaceGizmo 实现

**原理**：
- FaceGizmo 的位置需要从包围盒空间转换到世界空间（应用几何旋转）
- FaceGizmo 的旋转需要叠加基础旋转和几何旋转

**实现**：
```cpp
void UVoxelControllerVisualUIComponent::UpdateFaceGizmosPosition(const FBox& Bounds, FRotator GeometryRotation)
{
	if (FaceGizmos.Num() != 6) return;

	FVector Min = Bounds.Min;
	FVector Max = Bounds.Max;
	FVector Center = Bounds.GetCenter();

	// 定义面配置（基础位置和旋转）
	struct FFaceConfig { FVector Position; FRotator BaseRotation; };
	FFaceConfig FaceConfigs[6];
	FaceConfigs[0] = {FVector(Min.X, Center.Y, Center.Z), FRotator(90, 0, 0)};   // -X
	FaceConfigs[1] = {FVector(Max.X, Center.Y, Center.Z), FRotator(-90, 0, 0)};  // +X
	FaceConfigs[2] = {FVector(Center.X, Min.Y, Center.Z), FRotator(0, 0, 90)};   // -Y
	FaceConfigs[3] = {FVector(Center.X, Max.Y, Center.Z), FRotator(0, 0, -90)};  // +Y
	FaceConfigs[4] = {FVector(Center.X, Center.Y, Min.Z), FRotator(0, 0, 180)};  // -Z
	FaceConfigs[5] = {FVector(Center.X, Center.Y, Max.Z), FRotator::ZeroRotator}; // +Z

	// 更新每个 FaceGizmo 的位置和旋转
	for (int32 i = 0; i < 6; ++i)
	{
		if (FaceGizmos[i])
		{
			// 位置转换：局部 -> 世界（应用几何旋转）
			FVector LocalOffset = FaceConfigs[i].Position - Center;
			FVector WorldOffset = GeometryRotation.RotateVector(LocalOffset);
			FVector WorldPosition = Center + WorldOffset;

			FaceGizmos[i]->SetActorLocation(WorldPosition);

			// 旋转叠加：基础旋转 + 几何旋转
			FRotator FinalRotation = FaceConfigs[i].BaseRotation + GeometryRotation;
			FaceGizmos[i]->SetActorRotation(FinalRotation);
		}
	}
}
```

**关键点**：
- 使用 `GeometryRotation.RotateVector()` 将局部偏移转换为世界偏移
- 使用 `FRotator` 的 `+` 运算符叠加旋转

---

## 修改文件

### 头文件
- `Source/Voxel/Public/Core/ControllerComponent/VoxelControllerVisualUIComponent.h`
  - 修改 3 个方法签名，添加 `FRotator GeometryRotation` 参数

### 实现文件
- `Source/Voxel/Private/Core/ControllerComponent/VoxelControllerVisualUIComponent.cpp`
  - 实现 `UpdateRotateGizmosPosition` 的旋转应用逻辑
  - 实现 `UpdateFaceGizmosPosition` 的位置转换和旋转应用
  - 更新 `ShowRotateGizmo` 方法，从 StateComponent 获取旋转
  - 更新 `HandleTmpEditRequestChanged` 方法，传递旋转参数
  - 更新 Scale Gizmo 显示逻辑，传递旋转参数

---

## 测试验证

### 测试步骤

1. **RotateGizmo 测试**：
   - 进入批量编辑模式
   - 使用 RotateGizmo 旋转盒体
   - 验证：旋转环是否跟随盒体旋转

2. **FaceGizmo 测试**：
   - 进入批量编辑模式
   - 使用 RotateGizmo 旋转盒体
   - 验证：FaceGizmo 是否始终贴在旋转后的盒体表面

3. **组合测试**：
   - 先旋转盒体
   - 再使用 FaceGizmo 拖动表面
   - 验证：拖动方向是否正确

### 预期效果

**RotateGizmo**：
- ✅ 旋转环跟随盒体旋转
- ✅ 旋转轴向与盒体当前轴向对齐
- ✅ 视觉反馈直观，符合用户预期

**FaceGizmo**：
- ✅ 面 Gizmo 始终贴在盒体表面
- ✅ 面 Gizmo 的法线方向正确
- ✅ 旋转后仍能正确操作

---

## 技术要点

### 1. 旋转叠加

```cpp
FRotator FinalRotation = BaseRotation + GeometryRotation;
```

- `BaseRotation`: Gizmo 的初始轴向旋转
- `GeometryRotation`: 盒体的当前旋转
- 相加后得到 Gizmo 的最终世界旋转

### 2. 位置转换

```cpp
FVector LocalOffset = FaceConfigs[i].Position - Center;
FVector WorldOffset = GeometryRotation.RotateVector(LocalOffset);
FVector WorldPosition = Center + WorldOffset;
```

- 计算局部偏移（相对于中心）
- 应用旋转矩阵转换偏移
- 加上中心点得到世界位置

### 3. 旋转中心

```cpp
FVector Center = Geometry.GetCenter(BlockLength);
```

- 使用几何的旋转中心（而非包围盒中心）
- 支持自定义旋转中心

---

## 后续优化建议

### 1. 性能优化
- 缓存 `GeometryRotation`，避免重复访问 StateComponent
- 只在旋转变化时更新 Gizmo 旋转

### 2. 视觉增强
- 添加旋转动画过渡
- 显示当前旋转角度数值

### 3. 交互改进
- 支持旋转吸附（Snap to Angle）
- 支持旋转约束（限制旋转轴）

---

## 参考资料

- **Unreal Engine 文档**: `FRotator::RotateVector()`
- **FVoxelGeometryDesc**: 定义在 `VoxelEditorData.h:27`
- **Gizmo 初始化**: `VoxelControllerVisualUIComponent.cpp:118-158`

---

**修改完成日期**: 2026-01-18
