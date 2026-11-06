# 编辑系统 废弃CoreOffsetIndex 决策记录

> **决策日期**�?026-01-16
> **决策状�?*：✅ 已确�?
> **相关文档**：`编辑系统-FVoxelGeometryDesc数据结构-架构设计.md`

---

## 一、问题背�?

在编辑系统的Geometry数据结构设计中，存在旧的`FAreaCoverageFragment::CoreOffsetIndex`字段用于表示中心偏移。新的设计引入了`FVoxelGeometryDesc::Center`字段�?

**核心问题**：是否废弃CoreOffsetIndex字段？如何处理迁移？

**问题重要�?*�?
- 关系到新旧设计的平滑过渡
- 影响代码可维护�?
- 关系到数据表示的清晰�?

---

## 二、现状分�?

### 2.1 CoreOffsetIndex字段

**当前定义**�?
```cpp
struct FAreaCoverageFragment
{
    FIntVector MinIndex;
    FIntVector MaxIndex;
    FIntVector CoreOffsetIndex;  // 中心偏移索引
};
```

**字段属�?*�?
- 类型：FIntVector（体素索引）
- 访问权限：Public
- 当前用途：表示中心偏移（实际使用情况不明）

### 2.2 新的Center字段

**新设�?*�?
```cpp
struct FVoxelGeometryDesc
{
    FAreaCoverageFragment Box;
    FRotator Rotation;
    ...
private:
    FVector Center;  // 旋转中心（世界坐标）
};
```

**字段属�?*�?
- 类型：FVector（世界坐标）
- 访问权限：Private
- 用途：旋转中心

---

## 三、问题分�?

### 3.1 CoreOffsetIndex的问�?

| 问题 | 说明 |
|------|------|
| �?**坐标空间不匹�?* | CoreOffsetIndex是体素索引，Center应该是世界坐�?|
| �?**语义不清** | "中心偏移索引"含义模糊，不清楚是相对什么偏�?|
| �?**位置不合�?* | 放在Coverage（包围盒）结构中，但Center是Geometry层面的概�?|
| �?**使用不便** | 体素索引需要转换才能用于渲染和交互 |

### 3.2 新Center的优�?

| 优势 | 说明 |
|------|------|
| �?**坐标空间正确** | Center是世界坐标，直接用于渲染和Gizmo定位 |
| �?**语义清晰** | "旋转中心"含义明确 |
| �?**位置合�?* | 放在Geometry结构中，与Rotation等字段同一层次 |
| �?**使用方便** | 无需转换，直接使�?|

---

## 四、候选方�?

### 方案A：保留CoreOffsetIndex，与Center共存

**描述**：保留CoreOffsetIndex字段，与新的Center字段共存

**优点**�?
- �?向后兼容性好
- �?旧代码无需修改

**缺点**�?
- �?两个字段含义重叠，造成混乱
- �?需要维护两个字段的一致�?
- �?新开发者不清楚应该使用哪个字段
- �?代码冗余

---

### 方案B：立即删除CoreOffsetIndex

**描述**：立即删除CoreOffsetIndex字段，只保留Center

**优点**�?
- �?代码清晰，无冗余
- �?避免混淆

**缺点**�?
- �?破坏向后兼容�?
- �?可能有旧代码正在使用
- �?需要大规模重构

---

### 方案C：标记废弃，保留但不再使�?�?

**描述**：标记CoreOffsetIndex为废弃字段，保留在结构中但不使用，未来可能移�?

**优点**�?
- �?保持向后兼容�?
- �?给旧代码迁移时间
- �?明确废弃状态，避免新代码使�?
- �?未来可以完全移除

**缺点**�?
- ⚠️ 字段仍然存在，略微冗�?
- ⚠️ 需要文档说明废弃状�?

**实施细节**�?
```cpp
struct FAreaCoverageFragment
{
    FIntVector MinIndex;
    FIntVector MaxIndex;

    /** 中心偏移索引（已废弃，保留用于兼容性） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data/Area/Coverage")
    FIntVector CoreOffsetIndex;  // DEPRECATED
};
```

---

## 五、选择方案

### 选择方案C：标记废弃，保留但不再使�?

**决策理由**�?

1. **向后兼容�?*�?
   - 保留字段，避免破坏旧代码
   - 给旧代码迁移时间

2. **明确废弃状�?*�?
   - 通过注释和文档明确标记为废弃
   - 警告新代码不要使�?

3. **平滑迁移**�?
   - 新代码使用Center字段
   - 旧代码逐步迁移
   - 未来版本可以完全移除

4. **风险评估�?*�?
   - 保留字段风险低，只是略微冗余
   - 相比立即删除，更安全

---

## 六、迁移指�?

### 6.1 迁移路径

```
旧代码：
├─ 使用 CoreOffsetIndex（体素索引偏移）
└─ 计算旋转中心

新代码：
├─ 使用 Center（世界坐标）
├─ 通过 GetCenter() 访问
└─ 通过 MoveGEO/Recenter 修改
```

### 6.2 迁移示例

**旧代�?*�?
```cpp
FIntVector Offset = Geometry.Box.CoreOffsetIndex;
FVector Center = CalculateCenterFromOffset(Offset);
```

**新代�?*�?
```cpp
FVector Center = Geometry.GetCenter();
```

### 6.3 迁移步骤

1. **识别使用�?*
   - 搜索所有使用CoreOffsetIndex的代�?
   - 记录使用场景

2. **逐步替换**
   - 将旧代码迁移到Center字段
   - 测试功能正确�?

3. **清理阶段**
   - 所有旧代码迁移完成�?
   - 下一个大版本可以完全移除CoreOffsetIndex

---

## 七、影响分�?

### 7.1 正面影响

| 影响�?| 说明 |
|--------|------|
| **代码清晰�?* | �?明确废弃状态，避免混淆 |
| **向后兼容** | �?旧代码无需立即修改 |
| **平滑过渡** | �?给旧代码迁移时间 |
| **新代码质�?* | �?新代码使用清晰的Center字段 |

### 7.2 负面影响

| 影响�?| 说明 | 缓解措施 |
|--------|------|---------|
| **略微冗余** | 字段保留但不�?| 文档明确说明，影响极�?|
| **需要清�?* | 未来需要移�?| 记录技术债务，定期review |

### 7.3 风险评估

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 旧代码仍在使�?| �?| �?| 标记废弃，添加警�?|
| 新开发者误�?| �?| �?| 文档说明，代码注�?|
| 迁移遗漏 | �?| �?| Code Review，测试覆�?|

---

## 八、实施计�?

### 8.1 实施步骤

1. **标记废弃**
   - 添加注释说明废弃状�?
   - 更新文档

2. **通知团队**
   - 通知开发人员字段废�?
   - 提供迁移指南

3. **监控使用**
   - 定期检查CoreOffsetIndex使用情况
   - 提醒相关开发人员迁�?

4. **未来移除**
   - 下个大版本评估是否移�?
   - 确认无代码使用后完全删除

### 8.2 验证方法

| 验证�?| 方法 |
|--------|------|
| 废弃标记正确 | 代码审查 |
| 文档更新 | 文档审查 |
| 新代码使用Center | 静态分�?|

---

## 九、相关决�?

| 相关决策 | 文档 |
|---------|------|
| Center访问控制 | `编辑系统-FVoxelGeometryDesc-Center访问控制-决策记录.md` |
| FVoxelGeometryDesc操作方法设计 | `编辑系统-FVoxelGeometryDesc操作方法设计-决策记录.md` |

---

## 十、版本历�?

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状�?*：✅ 已确�?
