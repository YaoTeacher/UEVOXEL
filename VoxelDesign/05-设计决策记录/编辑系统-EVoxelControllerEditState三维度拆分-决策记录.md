# 编辑系统-EVoxelControllerEditState三维度拆分-决策记录

**决策日期**: 2026-01-23
**决策类型**: 架构重构
**决策状态**: ✅ 已批准
**Council辩论**: 是 (Architect/Designer/Engineer/Intern 四位专家参与)

---

## 1. 问题陈述

### 1.1 现状

`EVoxelControllerEditState` 枚举混合了三个不同的关注点:

```cpp
enum class EVoxelControllerEditState : uint8
{
    Add = 0,       // 新增 (动作)
    Delete = 1,    // 删除 (动作)
    Select = 2,    // 选择 (工具/模式)
    Replace = 3,   // 替换 (规则)
    Complement = 4,// 填充 (规则)
    None
};
```

### 1.2 问题

1. **概念混淆**: Add/Delete是"动作",Replace/Complement是"规则",Select是"工具模式"
2. **无法组合**: 无法表达复合操作,例如"仅在选区内执行填充空缺的添加"
3. **扩展困难**: 添加新操作需要修改单一枚举,导致组合爆炸
4. **语义不清**: `Delete + Complement` 的实际行为不可预测

### 1.3 影响范围

根据Grep分析,**33+文件**引用了此枚举,涉及:
- 状态组件 (`VoxelControllerStateComponent`)
- 交互逻辑 (`VoxelControllerInteractComponent` 的switch-case分支)
- UI绑定 (`BuildPanelWidget`, `OperationHistoryItemWidget`)
- 执行层 (`VoxelBlockWorldSubsystem`)
- 数据结构 (`FVoxelEditRequest`, `FSingleEditRequest`, `FVoxelEditModification`)

---

## 2. 提议的解决方案

### 2.1 核心思想

将单一枚举拆分为**三个正交维度**,每个维度独立变化:

```
操作类型 (做什么) × 放置规则 (怎么做) × 选择模式 (在哪做)
```

### 2.2 新的枚举定义

#### 维度一: 操作类型 (EVoxelToolOpType)

```cpp
/**
 * 编辑操作类型 - 决定"发生了什么动作"
 */
UENUM(BlueprintType)
enum class EVoxelToolOpType : uint8
{
    // 建造/放置 (对应原来的 Add)
    Build       UMETA(DisplayName = "建造 (Build)"),

    // 破坏/移除 (对应原来的 Delete)
    Destruct    UMETA(DisplayName = "破坏 (Destruct)"),

    // 材质刷 (只改材质不改形状)
    Paint       UMETA(DisplayName = "涂色 (Paint)"),

    // 选取 (对应原来的 Select)
    Select      UMETA(DisplayName = "选择 (Select)"),

    // 吸管/采样
    Sample      UMETA(DisplayName = "吸管 (Sample)")
};
```

#### 维度二: 体素放置规则 (EVoxelPlacementMode)

```cpp
/**
 * 体素合并规则 - 决定"新方块如何覆盖旧方块"
 * 仅在 OpType 为 Build/Paint 时有效
 */
UENUM(BlueprintType)
enum class EVoxelPlacementMode : uint8
{
    // 默认/覆盖：不管原位置有什么，直接写入
    Normal      UMETA(DisplayName = "覆盖 (Normal)"),

    // 填充/补全：只填空气，不破坏现有实体
    Complement  UMETA(DisplayName = "填充 (Complement)"),

    // 替换：只替换非空方块，不填空气
    Replace     UMETA(DisplayName = "替换 (Replace)"),

    // 特定替换：只替换特定类型的方块
    ReplaceMask UMETA(DisplayName = "指定替换 (Masked)")
};
```

#### 维度三: 选择运算模式 (ESelectionOpMode)

```cpp
/**
 * 选择运算模式 - 决定"新选区如何影响旧选区"
 * 仅在 OpType 为 Select 时有效
 * 通常绑定按键：无=New, Shift=Add, Alt=Subtract
 */
UENUM(BlueprintType)
enum class ESelectionOpMode : uint8
{
    // 新选：替换掉当前所有选择
    New         UMETA(DisplayName = "新选 (New)"),

    // 加选：并集
    Add         UMETA(DisplayName = "加选 (Union)"),

    // 减选：差集
    Subtract    UMETA(DisplayName = "减选 (Subtract)"),

    // 交选：交集
    Intersect   UMETA(DisplayName = "交选 (Intersect)")
};
```

### 2.3 核心数据结构

```cpp
/**
 * 体素编辑命令 - 三维度组合
 */
USTRUCT(BlueprintType)
struct FVoxelEditCommand
{
    GENERATED_BODY()

    // 操作类型 (核心维度)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Editing")
    EVoxelToolOpType OpType = EVoxelToolOpType::Build;

    // 放置规则 (仅在 Build/Paint 时有效)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Editing")
    EVoxelPlacementMode PlacementMode = EVoxelPlacementMode::Normal;

    // 选择模式 (仅在 Select 时有效)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Editing")
    ESelectionOpMode SelectionMode = ESelectionOpMode::New;

    // 便利构造函数
    static FVoxelEditCommand BuildNormal();
    static FVoxelEditCommand BuildComplement();
    static FVoxelEditCommand DestructAll();
    static FVoxelEditCommand PaintReplace();
    static FVoxelEditCommand SelectNew();

    // Builder模式
    struct Builder
    {
        EVoxelToolOpType OpType;
        EVoxelPlacementMode PlacementMode;
        ESelectionOpMode SelectionMode;

        Builder& SetOpType(EVoxelToolOpType Type);
        Builder& SetPlacementMode(EVoxelPlacementMode Mode);
        Builder& SetSelectionMode(ESelectionOpMode Mode);
        FVoxelEditCommand Create();
    };

    // 验证组合有效性
    bool IsValid() const;
};
```

---

## 3. Council 辩论记录

### 3.1 参与者

- **Architect (Serena Blackwood)**: 系统设计、长期架构、设计模式
- **Designer (Aditi Sharma)**: 用户体验、交互设计、认知负荷
- **Engineer (Marcus Webb)**: 实现成本、技术债务、向后兼容
- **Intern (Dev Patel)**: 新人视角、学习曲线、直觉理解

### 3.2 辩论过程

#### Round 1: 初始立场

| 专家 | 立场 | 核心观点 |
|------|------|---------|
| Architect | 支持 | 三维度拆分符合正交性、组合性原则,60种状态从12个值,长期收益 |
| Designer | 有条件支持 | 架构正确,但UI不能暴露三个独立控件,需预设组合+高级选项折叠 |
| Engineer | 保守支持 | 33+文件引用,建议渐进式重构,硬编码switch-case风险高 |
| Intern | 支持 | 单一枚举困惑,三维度符合日常思考,担心代码变繁琐 |

#### Round 2: 互动回应

**Architect**: 同意Designer的预设方案,提出双轨架构+18个月迁移窗口,Builder模式解决易用性

**Designer**: 强调UI必须同步演进,否则架构改进用户无感知,两级暴露模式(主界面预设+高级面板)

**Engineer**: 调整为支持拆分,提出三阶段计划(桥接层→双API共存→清理),2-4周时间表

**Intern**: 担心新旧API并存困惑新人,需要明确迁移指南和清晰分界线

#### Round 3: 最终综合

**收敛点**:
- 三维度拆分是正确方向 ✅
- 双轨架构/双API并存 ✅
- 渐进式迁移 ✅
- Blueprint兼容是硬约束 ✅
- 语义化API价值 ✅

**分歧点**:
- 迁移时间表: 6周 (Architect) vs 2-4周 (Engineer) vs 18个月 (初版)
- UI主交互模式: 预设按钮 vs 三维度控件
- 工厂方法是否引入新枚举

### 3.3 最终建议

**Architect**:
- 内部纯三维度,外部语义化函数,薄包装层
- 6周完成迁移
- 工厂返回结构体,不引入新枚举

**Designer**:
- 两级暴露+渐进式揭示
- 主界面8个高频预设,高级选项默认折叠
- 智能默认:参数从预设继承

**Engineer**:
- 三阶段计划:桥接层(1-2天)→双API共存(1-2周)→清理(1周)
- Contract测试验证新旧API等效性
- BlueprintFunctionLibrary提供转换

**Intern**:
- 先看到迁移指南草稿(对照表+哪些可不管+哪些必须改)
- 文档第1页三张图(UI/简单代码/高级代码)
- IDE注释标注`// @deprecated 2026.06后移除`

---

## 4. 最终决策

### 4.1 核心决策

**✅ 批准三维度拆分方案**

将 `EVoxelControllerEditState` 拆分为三个独立维度的枚举:
- `EVoxelToolOpType` (操作类型)
- `EVoxelPlacementMode` (放置规则)
- `ESelectionOpMode` (选择模式)

### 4.2 架构设计

采用**双轨架构**:

```
┌─────────────────────────────────────────┐
│  外部API (语义化)                        │
│  - Paint(), Erase(), Fill()             │
│  - Builder模式                           │
└──────────────┬──────────────────────────┘
               ↓ (薄包装层)
┌─────────────────────────────────────────┐
│  内部核心 (三维度)                       │
│  - FVoxelEditCommand结构体              │
│  - 纯三维度参数                          │
└─────────────────────────────────────────┘
```

**关键原则**:
- 不维护两套平行实现,而是薄包装层
- 工厂方法返回`FVoxelEditCommand`结构体,不引入新枚举
- 预设从语义API列表动态生成,非硬编码

### 4.3 UI设计

采用**两级暴露+渐进式揭示**:

```
┌─────────────────────────────────────────┐
│  主界面 (80%场景)                        │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐       │
│  │房屋 │ │树木 │ │墙体 │ │道路 │       │
│  └─────┘ └─────┘ └─────┘ └─────┘       │
│  [8个高频预设按钮]                       │
├─────────────────────────────────────────┤
│  ⚙️ 高级选项 [默认折叠]                   │
│  ┌───────────────────────────────────┐  │
│  │ 操作: [Build▼] [Destruct] [Paint]│  │
│  │ 规则: [Normal▼] [Complement] [...]│  │
│  │ 选择: [New▼] [Add] [Subtract]    │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

**核心预设** (覆盖80%场景):
- 房屋模板: 墙体、地板、屋顶组合
- 自然元素: 树木、岩石、水体
- 基础几何: 立方体、平面、柱体
- 常用阵列: 线性、环形、网格

**高级选项**:
- 默认隐藏,点击"高级选项"才展开
- 渐进式揭示:选择预设后才显示相关参数
- 智能默认:参数从预设继承,而非空白状态
- 视觉分隔:用分隔线和背景色区分级别

### 4.4 迁移计划

**时间表**: **2-4周** (平衡激进与保守)

#### Phase 1: 桥接层 (1-2天)

**目标**: 零破坏性变更

**任务**:
1. 添加新枚举定义 (`EVoxelToolOpType`, `EVoxelPlacementMode`, `ESelectionOpMode`)
2. 添加`FVoxelEditCommand`结构体
3. 保留旧枚举`EVoxelControllerEditState`,添加`UPROPERTY(meta=(Deprecated))`
4. 旧API内部委托给新实现
5. BlueprintFunctionLibrary提供转换函数

**验证**:
- 编译通过,零蓝图错误
- Contract测试验证新旧API等效性

#### Phase 2: 双API共存 (1-2周)

**目标**: 新功能使用新API,旧代码逐步迁移

**任务**:
1. 实现便利工厂方法 (`Paint()`, `Erase()`, `Fill()` 等)
2. 实现Builder模式
3. 所有新代码使用`FVoxelEditCommand`
4. 编译时警告引导迁移: `static_assert(false, "Use FVoxelEditCommand")`
5. UI同步演进:
   - 主界面添加8个高频预设按钮
   - 高级选项折叠面板
   - 移除旧的单一枚举下拉框
6. 新旧API并存,明确标记废弃路径

**验证**:
- 状态转换矩阵测试(60种组合的决策表)
- 蓝图回归测试套件
- UI可用性测试(新手+专家)

#### Phase 3: 清理优化 (1周)

**目标**: 移除废弃代码,性能优化

**任务**:
1. 删除旧枚举`EVoxelControllerEditState`
2. 移除所有桥接代码
3. 性能基准测试(Pipeline性能、内存占用)
4. 文档更新:
   - API文档
   - 迁移指南(旧API→新API对照表)
   - 新手教程(第1页:三张图,第2页:迁移指南)
5. IDE注释标注`// @deprecated 2026.06后移除`

**验证**:
- 性能对比测试(重构前后)
- 文档完整性检查
- 新人onboarding测试

### 4.5 风险缓解

#### 风险1: Blueprint兼容性破坏

**缓解措施**:
- BlueprintFunctionLibrary提供转换函数
- 保留旧枚举直到Phase 3
- 自动化迁移脚本
- 内部项目验证后再推广

#### 风险2: 硬编码switch-case重构引入Bug

**缓解措施**:
- 使用策略表替代逻辑重构: `TMap<EAction, FActionStrategy>`
- Contract测试验证行为等价性
- 增量迁移:每次迁移一个模块,立即测试
- 保留回滚分支

#### 风险3: 新旧API并存导致认知负担

**缓解措施**:
- 编译期警告明确标记废弃路径
- 文档明确标记"推荐用新API"
- 迁移指南提供清晰对照表
- IDE注释标注`// @deprecated`
- 设定明确的迁移终点线

#### 风险4: UI演进不同步

**缓解措施**:
- UI与后端并行迁移,非先后顺序
- 优先重构UI层,否则后端能力被锁住
- UI可用性测试贯穿Phase 2

---

## 5. 成功标准

### 5.1 技术指标

- ✅ 编译通过,零蓝图错误
- ✅ 60种组合的状态转换矩阵测试通过
- ✅ Contract测试验证新旧API等效性
- ✅ 性能对比测试:零性能退化(±5%以内)
- ✅ 蓝图回归测试套件100%通过

### 5.2 用户体验指标

- ✅ 新手onboarding时间: < 30分钟理解基础操作
- ✅ 专家用户:高级选项可访问,无功能损失
- ✅ UI认知负荷测试:80%用户能在5秒内找到所需功能
- ✅ 预设覆盖率:8个预设覆盖≥80%使用场景

### 5.3 工程质量指标

- ✅ 代码覆盖率:新代码≥90%
- ✅ 技术债务:33+文件引用全部迁移
- ✅ 文档完整性:API文档+迁移指南+新手教程
- ✅ 迁移完成率:100%旧代码迁移或标记废弃

---

## 6. 后续行动

### 6.1 立即执行 (Week 1)

1. **创建迁移计划文档** (Owner: Engineer)
   - 影响分析脚本(grep扫描所有调用点)
   - 蓝图依赖分析(UBT --listAffectedFiles)
   - 风险评估矩阵

2. **实现核心结构** (Owner: Architect + Engineer)
   - 新枚举定义
   - `FVoxelEditCommand`结构体
   - 便利工厂方法(前5个高频预设)

3. **UI低保真原型** (Owner: Designer)
   - 两级菜单布局
   - 高级面板交互
   - 8个高频预设定义

### 6.2 短期执行 (Week 2-3)

1. **桥接层实现** (Owner: Engineer)
   - 旧API内部委托给新实现
   - BlueprintFunctionLibrary转换函数
   - Contract测试套件

2. **UI实现** (Owner: Designer + Engineer)
   - 主界面预设按钮
   - 高级选项折叠面板
   - 交互反馈系统

3. **迁移准备** (Owner: Engineer)
   - 编译时警告配置
   - 自动化迁移脚本
   - 文档草稿

### 6.3 中期执行 (Week 4)

1. **全面迁移** (Owner: 全员)
   - 所有新代码使用新API
   - 旧代码逐步迁移
   - UI同步演进

2. **测试验证** (Owner: Engineer + Intern)
   - 状态转换矩阵测试
   - 蓝图回归测试
   - UI可用性测试

3. **文档完善** (Owner: Designer + Intern)
   - API文档
   - 迁移指南(对照表)
   - 新手教程(三张图)

### 6.4 长期执行 (Week 5-6+)

1. **清理优化** (Owner: Engineer)
   - 删除旧枚举和桥接代码
   - 性能基准测试
   - 代码审查

2. **知识传承** (Owner: Architect + Intern)
   - 团队培训
   - 最佳实践文档
   - 代码示例库

---

## 7. 相关文档

### 7.1 架构文档
- [编辑系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-架构设计.md)
- [编辑系统-选择系统-架构设计](/VoxelDesign/03-架构设计/编辑系统-选择系统-架构设计.md)

### 7.2 系统设计文档
- [编辑系统-选择系统-系统设计](/VoxelDesign/02-系统设计/编辑系统-选择系统-系统设计.md)

### 7.3 实施计划
- [编辑系统-架构改造-实施计划](/VoxelDesign/04-设计计划/编辑系统-架构改造-实施计划.md) (需更新)

---

## 8. 决策审批

| 角色 | 姓名 | 决策 | 日期 |
|------|------|------|------|
| 系统架构师 | Architect | ✅ 批准 | 2026-01-23 |
| UI/UX设计师 | Designer | ✅ 批准 | 2026-01-23 |
| 主工程师 | Engineer | ✅ 批准 | 2026-01-23 |
| 项目负责人 | - | ⏳ 待审批 | - |

---

## 9. 附录

### 9.1 状态转换矩阵

| OpType | PlacementMode | SelectionMode | 有效性 | 说明 |
|--------|--------------|---------------|--------|------|
| Build | Normal | New | ✅ | 标准建造 |
| Build | Complement | New | ✅ | 填充空位 |
| Build | Replace | New | ✅ | 替换已有 |
| Build | Normal | Add | ⚠️ | 仅在选区内建造 |
| Destruct | - | New | ✅ | 破坏(忽略PlacementMode) |
| Destruct | - | Add | ⚠️ | 仅在选区内破坏 |
| Paint | Normal | New | ✅ | 标准涂色 |
| Paint | Replace | New | ✅ | 替换材质 |
| Select | - | New | ✅ | 新选(忽略PlacementMode) |
| Select | - | Add | ✅ | 加选 |
| Sample | - | - | ✅ | 吸管(忽略其他模式) |

### 9.2 旧API→新API对照表

| 旧枚举值 | 新组合 | 语义化工厂 |
|---------|--------|-----------|
| `Add` | `Build + Normal` | `FVoxelEditCommand::BuildNormal()` |
| `Complement` | `Build + Complement` | `FVoxelEditCommand::BuildComplement()` |
| `Replace` | `Build + Replace` | `FVoxelEditCommand::BuildReplace()` |
| `Delete` | `Destruct` | `FVoxelEditCommand::DestructAll()` |
| `Select` | `Select + New` | `FVoxelEditCommand::SelectNew()` |

### 9.3 迁移代码示例

**旧代码**:
```cpp
Controller->SetEditState(EVoxelControllerEditState::Add);
```

**新代码 (简单API)**:
```cpp
Controller->SetEditCommand(FVoxelEditCommand::BuildNormal());
```

**新代码 (高级API)**:
```cpp
using namespace VoxelEditCommands;
Controller->SetEditCommand(
    Build().Complement().InSelection()
);
```

---

**文档版本**: 1.0
**最后更新**: 2026-01-23
**下次审查**: 2026-02-06 (Phase 2完成后)
