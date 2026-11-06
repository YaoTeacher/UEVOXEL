# UMG Design System — Voxel（v6 金属框羊皮纸）

> 原型出处：`.superpowers/brainstorm/vs-20260807/content/voxel-ui-v6.html`
> 本文件是 UI 样式实现的唯一真相来源（Source of Truth）。所有颜色/圆角/透明度的落地实现必须与本文件一致。

## 1. Visual Theme & Atmosphere

### 设计哲学
「金属框 + 羊皮纸底」：暗调做旧的纸上界面——深褐羊皮纸底色、双线金属描边、四角铆钉、金色衬线标题；模式用强调色微区分（经营暖金 / 建造铜赭 / 探索苔绿 / 遭遇绛红）。

### 视觉密度
- [x] 标准（平衡信息与留白，工具面板紧凑但不拥挤）

### 装饰层级
- [x] 中等装饰（面板背景、双层边框、内高光、微投影）

## 2. Color Palette & Roles（v6 权威令牌）

### 基础色板

| Token | Hex | RGBA（UMG 用） | 用途 |
|-------|-----|----------------|------|
| bg | #15100A | (0.082,0.063,0.039,1) | 屏幕底色（世界背后的界面底） |
| gold | #D9A95C | (0.851,0.663,0.361,1) | 主强调金（选中/悬停/数值） |
| gold2 | #8A6A3A | (0.541,0.416,0.227,1) | 暗金（标题/弱强调） |
| line | #4A3A20 | (0.290,0.227,0.125,1) | 默认描边（分组/格/分隔） |
| lineH | #6B4D26 | (0.420,0.302,0.149,1) | 亮描边（按钮/卡片主边） |
| txt | #EFE0BD | (0.937,0.878,0.741,1) | 主文本 |
| mut | #B3A07C | (0.702,0.627,0.486,1) | 弱化文本（标签/提示） |
| faint | #7D6F52 | (0.490,0.435,0.322,1) | 极弱文本（占位/禁用） |
| ok | #96B46A | (0.588,0.706,0.416,1) | 成功/确认 |
| warn | #D99A4E | (0.851,0.604,0.306,1) | 警告 |
| bad | #CF6A52 | (0.812,0.416,0.322,1) | 危险/删除 |
| info | #8AA8C8 | (0.541,0.659,0.784,1) | 信息/链接 |

### 模式强调色（--ac）

| 模式 | Token | Hex | RGBA |
|------|-------|-----|------|
| 经营/通用 | ac | #D9A95C | (0.851,0.663,0.361,1) |
| 建造 | ac | #C8834A | (0.784,0.514,0.290,1) |
| 探索 | ac | #8FAE62 | (0.561,0.682,0.384,1) |
| 遭遇 | ac | #C86456 | (0.784,0.392,0.337,1) |

### 表面色板（组件态）

| 角色 | Hex | 用途 |
|------|-----|------|
| card_top | #2E2413 | 卡片/面板渐变亮端（160deg 渐变上端） |
| card_bot | #221A0E | 卡片/面板渐变暗端 |
| btn_top | #332716 | 按钮渐变亮端（180deg 上端） |
| btn_bot | #241B0E | 按钮渐变暗端 |
| sel | #5C4020 | 选中态底（opt/btn/lay 的 on） |
| hover_bg | rgba(138,95,42,.15) | 行/项悬停底（#8A5F2A @0.15） |
| group_bg | rgba(20,14,7,.55) | 参数分组底（#140E07 @0.55） |
| inp_bg | #1A130C | 输入框底 |
| track | #170F07 | 滑块/进度条轨道 |
| goldbtn_top | #8A5F2A | 金色主按钮渐变亮端 |
| goldbtn_bot | #5F4020 | 金色主按钮渐变暗端 |
| exitbtn_top | #5A2E1C | 退出建造按钮渐变亮端 |
| exitbtn_bot | #3E2012 | 退出建造按钮渐变暗端 |

### 透明度规范
- 卡片底：不透明（渐变本身就是暗底）
- 分组底：0.55
- 悬停底：0.15
- 禁用控件：0.4
- 内高光线：0.25（rgba(214,176,106,.25)）

## 3. Typography Rules

| 角色 | 字体 | 大小 | 颜色 | 备注 |
|------|------|------|------|------|
| Panel_Title | Georgia/宋体系（衬线） | 14-19px | #FFE8C0 | 面板标题、金额大数字 |
| Section_Title | 衬线 | 9-12px | gold2 (#8A6A3A) | 分组标题，letter-spacing 2px |
| Item_Label | 默认 | 11px | mut | 行标签 |
| Body_Text | 默认 | 11px | txt | 正文 |
| Hint_Text | 默认 | 10px | faint | 提示/占位 |
| Data_Value | 衬线 | 11-15px | gold/FFE8C0 | 数值，等宽数字（tabular） |
| Button_Text | 默认 | 11px | txt | 按钮文字 |
| Kbd_Hint | 默认 | 8-9px | mut | 快捷键角标 |

- 文本截断：列表/名称用省略号；数值不截断
- 金色标题文字色统一 #FFE8C0（比 gold 更亮一档，用于标题/当前值）

## 4. Component Styling（v6 落地规范）

### 4.1 Card（羊皮纸卡，.v6-card）
- 背景：card_top #2E2413（UMG 渐变近似：Brush 纯色 #2E2413，或叠一层暗部 Image；无材质时用纯色）
- 边框：BorderColor lineH #6B4D26，宽度 1px
- 圆角：9px
- 内高光：若结构允许，卡片内加 1px 边框 Border，颜色 #D6B06A、透明度 0.25
- 投影：UE UMG 无投影，忽略（视觉差异可接受）

### 4.2 Button（按钮，.v6-btn/.v6-bbtn）
- 正常态：Background btn_top #332716；BorderColor lineH #6B4D26；圆角 7px；Padding (8,4)
- 悬停态：BorderColor gold；文字 #FFE8C0
- 按压态：亮度降低（近似 Background #241B0E）
- 选中态（on）：Background sel #5C4020；BorderColor gold；文字 #FFE8C0
- 金色主按钮（.gold）：Background goldbtn_top #8A5F2A；BorderColor gold；文字 #FFE8C0 加粗
- 退出按钮（.exit）：Background exitbtn_top #5A2E1C；BorderColor ac(建造 #C8834A)；文字 #F0B8A0
- 危险按钮（.warn）：BorderColor #7A4A2A；文字 warn
- 成功按钮（.ok）：Background #7A9A4A；BorderColor ok；文字 #101A06
- 禁用态：Opacity 0.4
- 尺寸：底带工具钮 高34 圆角7；图标钮 32×34；环境圆钮 38×38 圆角50%；弹窗/表单钮 Padding (6,10)

### 4.3 Option/Chip（选项钮，.v6-opt / 图层开关 .v6-lay）
- 正常：Background #2C2113；BorderColor line；圆角 5px；文字 mut
- 选中：Background sel #5C4020；BorderColor gold；文字 #FFE8C0

### 4.4 Group（参数分组，.v6-group）
- Background #140E07、Opacity 0.55；BorderColor line；圆角 7px；Padding (8,10)
- 分组内小标题（.v6-gt）：10px gold2

### 4.5 Input（输入框，.v6-inp）
- Background inp_bg #1A130C；BorderColor line #4A3A20；圆角 4px
- 文字 txt；Padding (3,8)；宽度 56px（参数数值）

### 4.6 Slider（滑块，.v6-strack）
- 轨道：高 4px、Background track #170F07、圆角 2px
- 把手：圆形 10px、金色 #D9A95C
- 数值文本：金色 gold、10px、右对齐

### 4.7 Section 标题（.v6-sec）
- 文字：9px 衬线、gold2、letter-spacing 2px
- 右侧渐变横线：高 2px、渐变透明→#6B4D26→#C9A25E→透明（UMG 近似：纯色 #6B4D26 透明度 0.6 的 Image 线）

### 4.8 底带（.v6-b）
- 高 54px；Background #161009、Opacity 0.85~0.96
- 上边框 1px lineH #6B4D26
- 组标签（.glb）：8px 衬线 gold2、字距 2px，钉在组左上

### 4.9 库行（.v6-lib）
- 尺寸：720×152、居中、底边距 104px
- Background card_top #2E2413；BorderColor lineH；圆角 10px
- 内部：搜索框 110px（v6-inp）；网格 min 66px/格；右侧信息栏 176px（左边框 line 分隔）

### 4.10 工具配置卡（.v6-cfg）
- 尺寸：264px 宽、底边距 108px、左 10px、向上生长 max-height 300px
- 卡片样式同 4.1；头部：工具名衬线 #FFE8C0 + 组别 mut + 右侧 ⟲ 重置

## 5. Layout Principles

- 锚点规范不变（见 v5 文档 5.1）
- 建造模式布局（v6 心智模型）：
  - 底带（一级工具，居中）+ 二级工具排（底带上 46px）
  - 库行（底带上方居中，跟随工具开合）
  - 左列工具配置卡（左下，纯参数，向上生长）
  - 右上环境组（小地图 + 视图控制 + 时间/高度抽屉）
  - 建造态隐藏左轨玩家域
- 间距：XS 2 / SM 4 / MD 8 / LG 12 / XL 16

## 6. 环境域控件规范（右上环境组）

| 元素 | 样式 |
|------|------|
| 环境卡（envcard） | 4.1 卡片，padding (8,9,7)，垂直居中排列 |
| 时钟 | 衬线 15px #FFE8C0，tabular 数字 |
| 日期/天气 | 10px mut，hover 金边 |
| 小地图 | 圆形 118×118；边框 3px #7A5A30；hover 金边 |
| 地图按钮行 | 26×30 小按钮（4.2 基础样式，圆角 6） |
| 板缘拉片（时间/高度抽屉） | 宽 18、高 64，向左滑出；on 态金边金字 |
| 时间倍速组 | 分段按钮 .v6-spd：on 底 sel、金字 |
| 高度滑块 | 4.6 滑块；行标签 PAWN/渲染 34px 宽 mut |
| 图层开关 | 4.3 Option on/off |
| 剖切列表行 | 行高、行底部分隔 line @0.4、hover 底 #8A5F2A @0.12 |

## 7. 建造模式专用规范（v6 三元组：工具×库×参数）

1. **底带 = 历史│工具**：历史组（↺ ↻ 🕘 32×34 图标钮）钉左；一级工具（组钮+独立工具钮）居中；无执行按钮（确认/取消在世界内浮标）
2. **库行 = 对象**：内容完全跟随当前工具（方块库/模板库/家具库/区域类型库）；无库对象工具显示占位文案
3. **左列 = 参数**：纯参数无操作按钮；含选区分区（选区统计 + 移动/复制/粘贴/删除/保存模板/清空）
4. **选中态统一**：CheckBox/按钮/选项的 on 态 = sel 底 #5C4020 + gold 边 + #FFE8C0 文字
5. **模块卡片**：ToolDetailPanel 各模块卡按 4.1 卡片样式；模块标题按 4.7
6. **键位提示**：参数卡底部 faint 小字「✓ Enter 确认 · ✕ RMB 逐级取消 · Del 删选区」

## 8. 实施约束（重要）

- **不得删除/改名任何 C++ BindWidget 控件**（M1 的 ItemLibrary、M4a 的 Input_MinX 等）；只改样式属性（Brush/ColorAndOpacity/BorderColor/Padding/Slot），增装饰控件只在根下追加
- **颜色落地统一用 FLinearColor RGBA**：所有颜色直接引用本文第 2 节 RGBA 值，禁止另造近似色
- 修改后必须 `CompileWidgetBlueprint` 并 `save_assets`
- 涉及资产：WBP_VoxelGameHUD、WBP_VoxelTopLeftBar、WBP_BuildPanelWidget、WBP_ToolDetailPanel、WBP_ToolLibraryPanel、WBP_ToolContainer（若存在）、WBP_BuildItemLib、WBP_BuildSlotWidget、WBP_ToolIcon、WBP_CutoutBoxListPanel 及 BuildDetailMode 下 16 个模块 WBP

## 9. 主题资产落地映射（2026-09-02 起生效）

本设计文档的全部令牌已落地为**唯一主题资产** `DA_VoxelUITheme`（`/Game/Resource/UI/CoreUI/Theme/`），由 `UVoxelUITheme::Get()` 统一分发。改样式/素材/字号**只改主题资产**，不再逐 WBP 手工设置。

### 9.1 令牌 → 资产字段映射

| 文档令牌（§2/§3/§4） | DA_VoxelUITheme 字段 |
|---|---|
| §2 基础色板（bg/gold/gold2/line/lineH/txt/mut/faint/ok/warn/bad/info、#FFE8C0） | Colors\|Base：Background/Gold/Gold2/Line/LineHighlight/Text/Muted/Faint/Ok/Warn/Bad/Info/TitleGold |
| §2 表面色板（card/btn/sel/hover_bg/group_bg/inp_bg/track/goldbtn/exitbtn） | Colors\|Surface：CardTop~ExitButtonBottom |
| §3 字体型号表（8 角色） | TextRoles：PanelTitleStyle~KbdHintStyle（字体族/字号/颜色/字距） |
| §4.5 Input | Controls：InputStyle（底色/聚焦色/文字色/Padding） |
| §4.6 Slider | Controls：SliderStyle（轨道厚度/颜色、把手尺寸/颜色/贴图） |
| 下拉选项（无文档节，新增） | Controls：ComboStyle（行底色/悬停色/文字色/内边距/列表高） |
| 衬线/正文字体 | Fonts：SerifFont（Noto Serif CJK SC）/ BodyFont（空=引擎默认） |

### 9.2 统一控件对照（新 UI 必须使用）

| 需求 | 控件类 | 说明 |
|---|---|---|
| 文本（任意） | `UVoxelTextBlock` | 选 TextRole 即得 §3 对应型号；`SizeOverride` 特例改字号 |
| 滑条 | `UVoxelSlider` | 自动应用 §4.6；把手贴图走 SliderStyle.ThumbTexture |
| 输入框 | `UVoxelEditableTextBox` | 自动应用 §4.5 |
| 下拉选项 | `UVoxelComboBoxString` | 自动应用 ComboStyle |
| 标签+滑条+数值 行 | `UVoxelSliderRow`（WBP_UI_SliderRow） | 组合控件，事件转发 |
| 标签+输入框 行 | `UVoxelInputRow`（WBP_UI_InputRow） | 组合控件，OnTextCommitted 转发 |
| 标签+下拉 行 | `UVoxelComboRow`（WBP_UI_ComboRow） | 组合控件，OnSelectionChanged 转发 |

- 特例需要脱离主题手工调样式：把控件的 `bUseThemeStyle` 关掉
- 存量 WBP 渐进换装：把控件**替换类为 UVoxel* 子类、保持控件名不变**，C++ BindWidget 无需改动
- 主题资产在项目设置 Game → Voxel UI Settings → Default Theme 配置；未配置时控件保持引擎默认样式
