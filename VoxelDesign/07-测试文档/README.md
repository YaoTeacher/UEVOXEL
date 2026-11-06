# 测试文档目录说明

此目录包含体素编辑系统第一阶段（Phase 1 - 基础编辑流程）的所有测试文档和资源。

## 目录结构

```
Testing/
├── README.md                    # 本文件
├── Phase1-TestGuide.md          # 详细测试指南
├── Phase1-TestResults.md        # 测试结果报告模板
├── Phase1-QuickReference.md     # 快速参考指南
├── Phase1_PythonTest.py         # Python 测试脚本示例
│
├── Screenshots/                 # 测试截图目录
│   └── Phase1/                 # 第一阶段截图
│       ├── TestMap_Layout.png
│       ├── EditorMode_Active.png
│       ├── SingleVoxel_Edit.png
│       ├── UndoStack_Status.png
│       └── Bugs/               # Bug 复现截图
│
├── Logs/                        # 测试日志目录
│   └── Phase1/                 # 第一阶段日志
│       ├── OutputLog_Scenario1.txt
│       ├── OutputLog_Scenario2.txt
│       ├── OutputLog_Scenario3.txt
│       └── OutputLog_Performance.txt
│
└── Videos/                      # 测试录像目录
    └── Phase1/                 # 第一阶段录像
        ├── FullTest.mp4
        └── BugReproduction.mp4
```

## 文档说明

### Phase1-TestGuide.md（测试指南）
**用途**: 详细的集成测试指南，包含所有测试场景、步骤和验证方法。

**内容**:
- 测试概述和范围
- 测试前准备（编译、创建关卡、材质等）
- 4 个主要测试场景（编辑器模式、单体素编辑、撤销/重做、预览可视化）
- 控制台命令测试
- 蓝图测试
- 已知限制和故障排查
- 测试检查清单

**使用方法**:
1. 阅读测试概述，了解第一阶段功能
2. 按照测试前准备章节完成环境搭建
3. 逐个执行测试场景
4. 使用提供的验证方法确认结果
5. 填写测试检查清单

---

### Phase1-TestResults.md（测试结果模板）
**用途**: 测试结果报告模板，用于记录测试过程和结果。

**内容**:
- 测试环境信息（构建版本、硬件配置等）
- 测试结果汇总表格
- 详细测试结果（每个场景的每项测试）
- 发现的问题列表（Bug 追踪）
- 性能指标数据
- 截图和日志索引
- 改进建议
- 下一步计划

**使用方法**:
1. 复制此模板创建新的测试结果报告
2. 填写测试环境信息
3. 逐项填写测试结果
4. 记录发现的问题
5. 收集性能数据
6. 附上截图和日志
7. 提供改进建议

**命名规范**:
- 测试报告: `Phase1-TestResults-YYYYMMDD-姓名.md`
- 例如: `Phase1-TestResults-20260107-张三.md`

---

### Phase1-QuickReference.md（快速参考）
**用途**: 快速参考指南，提供常用命令、API 和调试技巧。

**内容**:
- 快速启动步骤
- 控制台命令速查表
- 蓝图测试节点序列
- 关键 API 速查
- 事件委托速查
- 调试技巧
- 常见错误和解决方案
- 文件路径速查

**使用方法**:
- 作为日常测试的快速参考
- 在测试过程中快速查找命令和 API
- 排查问题时查看常见错误解决方案

---

### Phase1_PythonTest.py（Python 测试脚本）
**用途**: 通过 Python API 自动化测试体素编辑系统。

**内容**:
- 子系统初始化测试
- 编辑器模式测试
- 单体素编辑测试
- 撤销/重做测试
- 预览系统测试

**使用方法**:
1. 打开虚幻编辑器
2. **文件** → **Python 脚本** → **新建脚本**
3. 粘贴此脚本内容
4. **运行** 脚本
5. 查看输出日志

**注意**:
- 需要 UE5 Python API 支持
- 某些测试需要有效的区域实例
- 可以根据需要修改测试参数

---

## 目录使用规范

### Screenshots/（截图目录）
用于存储测试过程中的截图。

**命名规范**:
- 功能截图: `[场景]-[描述].png`
  - 例如: `Scenario1-EditorModeActive.png`
- Bug 截图: `Bug-[BugID]-[描述].png`
  - 例如: `Bug-BUG001-NullPointerError.png`

**截图内容建议**:
- 测试关卡布局
- 编辑器模式状态
- 编辑效果可视化
- 错误信息和堆栈跟踪
- 性能分析截图

---

### Logs/（日志目录）
用于存储测试过程中的日志文件。

**命名规范**:
- 场景日志: `OutputLog-[场景].txt`
  - 例如: `OutputLog-Scenario1.txt`
- 日期日志: `Log_YYYY-MM-DD_HH-MM-SS.txt`
- 性能日志: `PerformanceLog-[日期].txt`

**日志内容建议**:
- 完整的编辑器输出日志
- 特定场景的日志片段
- 性能分析日志
- 错误和警告日志

**日志收集方法**:
1. **窗口** → **开发者工具** → **输出日志**
2. 筛选关键日志（搜索关键词）
3. 右键 → **保存到文件**
4. 复制到 Logs 目录

---

### Videos/（录像目录）
用于存储测试过程的视频录像。

**命名规范**:
- 完整测试: `FullTest-[日期].mp4`
- Bug 复现: `BugReproduction-[BugID].mp4`
- 功能演示: `Demo-[功能名称].mp4`

**录像内容建议**:
- 完整的测试流程演示
- Bug 复现步骤
- 关键功能演示
- 性能对比录像

**录像工具推荐**:
- OBS Studio
- 虚幻编辑器内置录像
- Windows Xbox Game Bar

---

## 测试流程

### 1. 测试前准备
1. 阅读 `Phase1-TestGuide.md`
2. 编译项目
3. 创建测试关卡和材质
4. 准备测试蓝图

### 2. 执行测试
1. 按照 `Phase1-TestGuide.md` 逐个执行测试场景
2. 使用 `Phase1-QuickReference.md` 快速查找命令
3. 或使用 `Phase1_PythonTest.py` 自动化测试
4. 记录测试结果和截图

### 3. 填写报告
1. 使用 `Phase1-TestResults.md` 模板创建报告
2. 填写测试环境和结果
3. 记录发现的问题
4. 附上截图和日志

### 4. 提交和审核
1. 提交测试报告
2. 报告发现的问题
3. 等待审核和反馈
4. 跟进问题修复

---

## 测试资源准备

### 必需资源
- [x] 测试指南文档 (`Phase1-TestGuide.md`)
- [x] 测试结果模板 (`Phase1-TestResults.md`)
- [x] 快速参考指南 (`Phase1-QuickReference.md`)
- [x] Python 测试脚本 (`Phase1_PythonTest.py`)
- [x] 测试目录结构 (`Screenshots/`, `Logs/`, `Videos/`)

### 待创建资源
- [ ] 测试关卡: `Content/Maps/Test/TestMap_Phase1_EditTest.umap`
- [ ] 测试材质: `Content/Materials/M_PreviewMaterial.uasset`
- [ ] 测试蓝图: `Content/Blueprints/Test/BP_TestActor_EditSystem.uasset`

---

## 联系方式

如有问题或建议，请联系：
- 技术问题: [填写邮箱或联系方式]
- 文档问题: [填写邮箱或联系方式]
- 测试支持: [填写邮箱或联系方式]

---

## 版本历史

| 版本 | 日期 | 修改内容 | 作者 |
|------|------|---------|------|
| 1.0 | 2026-01-07 | 初始版本，创建测试文档目录和模板 | Claude |

---

**最后更新**: 2026-01-07
**维护者**: [填写]
