# 编辑系统 Center访问控制 决策记录

> **决策日期**�?026-01-16
> **决策状�?*：✅ 已确�?
> **相关文档**：`编辑系统-FVoxelGeometryDesc数据结构-架构设计.md`

---

## 一、问题背�?

在编辑系统的Geometry数据结构设计中，需要定义旋转中心（Center）的访问控制方式�?

**核心问题**：Center字段应该如何控制访问�?

**问题重要�?*�?
- Center是旋转操作的关键数据
- 访问控制影响数据一致性和操作稳定�?
- 关系到Geometry操作的封装�?

---

## 二、需求分�?

### 2.1 功能需�?

1. **只读访问**：外部需要能够读取Center值用于渲染和Gizmo定位
2. **受控修改**：Center的修改需要受控，不能随意修改
3. **数据一致�?*：Center与Coverage（包围盒）需要保持一致�?
4. **操作稳定�?*：避免Center在操作过程中不断变化

### 2.2 约束条件

- Center是世界坐标，不是体素索引
- Center应该体素对齐
- Center默认固定，只在特定操作时更新

### 2.3 质量属�?

- **可维护�?*：清晰的访问控制，易于维�?
- **安全�?*：防止外部随意修改Center
- **一致�?*：保证Center与Coverage的一致�?

---

## 三、候选方�?

### 方案A：Center设为Public字段

**描述**：Center字段设为Public，外部可以直接访问和修改

**优点**�?
- 访问简单，无需额外方法
- 修改方便，性能最�?

**缺点**�?
- �?外部可以随意修改Center，破坏数据一致�?
- �?无法保证Center与Coverage的一致�?
- �?无法强制使用四种操作方法
- �?容易出现Center偏离问题

**示例**�?
```cpp
// 外部代码可以直接修改
Geometry.Center = NewCenter; // 危险！可能破坏一致�?
```

---

### 方案B：Center设为Private，提供Public Setter

**描述**：Center字段设为Private，提供Public Setter方法修改

**优点**�?
- �?可以在Setter中添加验证逻辑
- �?可以保证数据一致�?

**缺点**�?
- �?仍然允许外部直接修改Center
- �?需要额外的验证逻辑
- �?可能绕过四种操作方法
- �?无法强制使用受控的操作方�?

**示例**�?
```cpp
// 外部代码通过Setter修改
Geometry.SetCenter(NewCenter); // 仍然可以随意修改
```

---

### 方案C：Center设为Private，只通过操作方法修改 �?

**描述**：Center字段设为Private，只通过四种操作方法修改，提供GetCenter()访问�?

**优点**�?
- �?防止外部随意修改Center
- �?保证Center与Coverage的一致�?
- �?强制使用四种操作方法（MoveGEO、ModifyCoverage、Recenter、RotateGEO�?
- �?避免数据不一�?
- �?职责清晰，易于维�?

**缺点**�?
- 需要通过操作方法修改，略微复�?

**示例**�?
```cpp
// 只读访问
FVector Center = Geometry.GetCenter();

// 只能通过操作方法修改
Geometry.MoveGEO(Delta);        // 会更新Center
Geometry.Recenter();             // 会更新Center
// ModifyCoverage �?RotateGEO 不改变Center
```

---

## 四、选择方案

### 选择方案C：Center设为Private，只通过操作方法修改

**决策理由**�?

1. **数据一致�?*�?
   - Center的修改与Coverage的修改同�?
   - 避免Center与Coverage不一�?

2. **操作稳定�?*�?
   - Center默认固定，避免旋转轴不稳�?
   - 只在MoveGEO和Recenter时更新Center

3. **职责清晰**�?
   - 四种操作方法职责单一
   - 每个方法的影响范围明�?

4. **封装性强**�?
   - 外部只能通过GetCenter()访问Center
   - 外部只能通过MoveGEO/Recenter修改Center
   - 避免外部随意修改

5. **符合设计原则**�?
   - 单一职责原则
   - 最小权限原�?
   - 封装原则

---

## 五、影响分�?

### 5.1 正面影响

| 影响�?| 说明 |
|--------|------|
| **数据一致�?* | �?Center与Coverage始终保持一�?|
| **操作稳定�?* | �?避免Center在操作过程中不断变化 |
| **代码可维护�?* | �?职责清晰，易于理解和维护 |
| **防止误用** | �?外部无法随意修改Center |

### 5.2 负面影响

| 影响�?| 说明 | 缓解措施 |
|--------|------|---------|
| **灵活性降�?* | 外部无法直接修改Center | 提供四种操作方法满足所有需�?|
| **略微复杂** | 需要通过方法修改而不是直接赋�?| 方法接口清晰，易于使�?|

### 5.3 兼容性影�?

**向后兼容�?*：✅ 无影�?
- 这是新设计，不存在旧代码兼容问题

**CoreOffsetIndex迁移**�?
- 旧代码使用CoreOffsetIndex（体素索引偏移）
- 新代码使用Center（世界坐标）
- 需要逐步迁移

---

## 六、实施计�?

### 6.1 实施步骤

1. **实现Center字段**
   - 设为Private
   - 添加GetCenter()访问�?

2. **实现四种操作方法**
   - MoveGEO：更新Box和Center
   - ModifyCoverage：只更新Box，Center不变
   - Recenter：只更新Center
   - RotateGEO：只更新Rotation

3. **添加约束检�?*
   - Center体素对齐检�?
   - 数据一致性验�?

### 6.2 验证方法

| 验证�?| 方法 |
|--------|------|
| Center不可直接修改 | 编译时检�?|
| 四种操作方法正确更新Center | 单元测试 |
| 数据一致�?| 集成测试 |

---

## 七、相关决�?

| 相关决策 | 文档 |
|---------|------|
| 废弃CoreOffsetIndex | `编辑系统-FAreaCoverageFragment-废弃CoreOffsetIndex-决策记录.md` |
| FVoxelGeometryDesc操作方法设计 | `编辑系统-FVoxelGeometryDesc操作方法设计-决策记录.md` |

---

## 八、版本历�?

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-01-16 | 初始版本 |

---

**决策状�?*：✅ 已确�?
