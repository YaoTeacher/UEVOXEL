# UI-统一控件与主题层 决策记录

> **日期**：2026-09-02
> **状态**：已实施（基础控件层 + 主题层 + 试点换装）
> **关联文档**：[DESIGN_UMG.md](/VoxelDesign/DESIGN_UMG.md)、[UI系统-架构设计](/VoxelDesign/03-架构设计/UI系统-架构设计.md)

---

## 1. 背景与问题

### 1.1 当前现状

项目所有界面直接摆放引擎原生控件：

| 控件类型 | 用量（BindWidget） | 样式现状 |
|---------|-------------------|---------|
| `UTextBlock` | 60+ 处 | 逐处手工设字体/字号/颜色 |
| `UEditableTextBox` | 约 15 处 | 逐处手工设置 |
| `USlider` | 约 5 处 | 引擎默认样式 |
| `UComboBoxString` | 约 3 处 | 引擎默认样式 |

- **无字体资产**：Content 下没有任何字体文件，全部使用引擎默认字体（Roboto）。DESIGN_UMG.md §3 要求的衬线标题字体未落地。
- **无样式资产层**：DESIGN_UMG.md 定义了完整的 v6 令牌（颜色/字号/控件样式），但仅是文档，落实靠逐个 WBP 手工/MCP 设置，改一处样式要动几十处资产。
- 现有 UI 框架（PrimaryGameLayout / Layer 栈 / Screen·Dialog·Toast / M1–M16 功能模块）管生命周期与层级，**缺少最底层的基础控件层**。

### 1.2 触发原因

需要统一的滑条 / 下拉选项 / 输入框组件，并要求能快速统一修改样式、素材图片、区分各型号字体（标题/正文等）的大小。

---

## 2. 决策目标

1. 改样式只动一份资产（颜色/素材图片/字体型号集中管理）
2. 统一控件与现有框架兼容：BindWidget 不换名、单向数据流铁律不变、Widget 无业务
3. 存量 WBP 可渐进迁移，不要求一次性重写
4. 样式资产可被 MCP ObjectTools 直接编辑（AI 驱动工作流）

---

## 3. 可选方案

### 方案 A：CommonUI 内建 Style 体系（CommonTextStyle 等）

**做法**：文本用 `UCommonTextBlock + UCommonTextStyle` 样式类，按钮用 `UCommonButtonStyle`。

**优点**：
- 引擎官方体系，与 CommonUI 基类同宗

**缺点**：
- **只覆盖文本和按钮**：Slider / ComboBox / EditableTextBox 没有对应的 CommonUI 样式类，仍需自研
- 样式类是 UClass（蓝图类）而非扁平资产，AI 批量编辑/批量对比不如 DataAsset 直观

**结论**：否决。覆盖面不全，仍要自研控件样式通道。

### 方案 B：FSlateStyleSet（Slate 样式集）

**做法**：注册项目级 FSlateStyleSet，控件按样式名引用。

**优点**：
- 引擎原生样式机制

**缺点**：
- 对 UMG BindWidget 控件不直观（需在代码里逐一 SetStyle 按名查找）
- 样式定义在 C++/配置中，**MCP 无法直接编辑**，违背 AI 驱动工作流
- 设计师无法在资产编辑器里可视化调样式

**结论**：否决。

### 方案 C：主题 DataAsset + 控件子类（**选定**）

**做法**：
- `DA_VoxelUITheme`（`UVoxelUITheme : UDataAsset`）：镜像 DESIGN_UMG.md 全部令牌（基础色/表面色/8 个文本角色/控件样式/字体族/素材 Brush）
- `UVoxelUISettings : UDeveloperSettings`：配置默认主题资产路径
- 统一控件为引擎控件 **IS-A 子类**（`UVoxelSlider : USlider` 等），`SynchronizeProperties` 时经 `UVoxelUITheme::Get()` 自应用样式
- 组合行控件（标签+控件+数值）继承 `UVoxelWidgetBase`，纯展示转发

**优点**：
- 单资产覆盖颜色/图片/字体型号全部维度，MCP ObjectTools 三段式可直接改
- 存量 WBP 换装 = 控件「替换类、不改名」，C++ BindWidget（IS-A 校验）零改动
- 编辑器 PreConstruct/SynchronizeProperties 与运行时同源，设计器实时预览
- 与 MVVM 目标正交：叶子控件不影响后续绑定改造

**缺点**：
- 需维护一套自定义控件类（但每个类仅一个 ApplyThemeStyle 方法，逻辑极薄）
- UMG 设计器内改主题后需重新打开 WBP 才刷新预览（无事件推送，可接受）

**结论**：选定。

---

## 4. 最终决策

采用**方案 C**，落地三层：

```
主题层：DA_VoxelUITheme（UDataAsset）+ UVoxelUISettings（DeveloperSettings 指定默认主题）
控件层：UVoxelTextBlock / UVoxelSlider / UVoxelEditableTextBox / UVoxelComboBoxString（IS-A 子类，自应用主题）
        UVoxelSliderRow / UVoxelInputRow / UVoxelComboRow（组合行，继承 UVoxelWidgetBase）
页面层：存量 WBP 渐进换装（替换类不改名）；新 UI 一律使用统一控件
```

关键规则：

1. **数据流铁律不变**：统一控件只做样式自应用 + 事件转发，不含业务、不 GetSubsystem、不碰 StateComponent
2. **主题访问不依赖子系统**：UMG 设计器 PreConstruct 阶段无 GameInstance，经 `UVoxelUITheme::Get()`（DeveloperSettings 软引用）静态获取，故不挂 ConfigDataSubsystem
3. **特例逃生口**：每个统一控件有 `bUseThemeStyle`（默认 true），关掉即保留手工样式；`UVoxelTextBlock` 另有 `SizeOverride`
4. **字体**：衬线用 Noto Serif CJK SC（OFL 开源协议，Regular+Bold，`Content/Resource/UI/Fonts/`），正文暂用引擎默认

---

## 5. 影响与后续

### 5.1 新增资产/代码

- `Source/Voxel/Public|Private/Core/UI/Theme/`：VoxelUITheme、VoxelUISettings
- `Source/Voxel/Public|Private/Core/UI/Widgets/`：VoxelTextBlock、VoxelSlider、VoxelEditableTextBox、VoxelComboBoxString、VoxelSliderRow、VoxelInputRow、VoxelComboRow
- `Content/Resource/UI/Fonts/`：NotoSerifCJKsc-Regular.otf、NotoSerifCJKsc-Bold.otf、OFL.txt
- `Content/Resource/UI/CoreUI/Theme/DA_VoxelUITheme.uasset`
- `Content/Resource/UI/CoreUI/Widgets/WBP_UI_SliderRow / WBP_UI_InputRow / WBP_UI_ComboRow.uasset`
- Build.cs 新增 `DeveloperSettings` 模块依赖

### 5.2 后续可选工作（不在本次范围）

- 存量 WBP 批量换装（约 20 个 WBP，逐面板替换控件类不改名）
- Button / CheckBox 统一封装（DESIGN_UMG.md §4.2/§4.3 已有样式定义）
- MVVM 绑定改造（UI系统-架构设计.md 既有规划）
