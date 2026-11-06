# 建造模式 UI 优化方案

**最后更新**：2026-11-04（组合创建交互目标，代码/资产未实施）

## 当前架构分析

### 架构模式
**动态模块加载策略模式**（非 monolithic）

```
BuildPanelWidget (容器)
├── MainToolContainer (主工具栏 M1-M5)
├── SubToolContainer (子工具栏 - 动态)
└── ToolDetailPanel (详情面板)
    ├── M1 建造库 (常显)
    ├── M2 操作模式 (常显)
    ├── M3 几何形状 (常显)
    └── M7-M15 (动态显示)
        ├── M7 笔刷参数
        ├── M10 采样参数
        ├── M11 魔棒参数
        └── ...
```

### 问题诊断

| 问题 | 现状 | 影响 |
|------|------|------|
| **颜色不统一** | 各模块独立设色，无主题系统 | 视觉杂乱 |
| **字体层级混乱** | 标题/正文/数据值无区分 | 信息层级不清 |
| **间距不一致** | 各模块 padding 随意 | 界面松散 |
| **边框装饰过重** | 多层 Border 嵌套 | 视觉噪音 |
| **按钮样式不一** | 各模块 Button Style 独立 | 交互体验割裂 |

---

## 组合创建交互（2026-11-04）

本项调整创建链路，不改变下文无关主题、几何、渲染或撤销设计，不代表已实现。

```text
同一建造侧栏
 ├─ 拉区域范围 / 摆模板预览（复用当前预览）
 ├─ 名称、类型/业态、经营意图、必要条款
 ├─ 产权/经营人：有效授权及租约/任命解析，只读，不自由填来取得权利
 └─ 一次确认 CreateBusinessWithArea
     → Controller → 业务门面
         ├─ 经营实体+经营关系+必要 Lease/首期 → Global.BusinessEntities
         └─ 空间注册 → AreaRef={SceneId,AreaId}
     → 有效 Lease 后才真写租户建筑/家具 → 事件 → UI
```

- 商圈/店铺采用组合入口，不要求先创建区域、再跳详情补建生意，不要求手动绑定。M12a/M12b 与范围/模板参数形成连续采集，M13a 纯导航是待迁移代码事实，不约束最终创建。
- 概念请求含 OperatorId、BusinessKind、Name、BusinessIntent、ParentBusinessEntityId、空间草稿等，不硬定 class。子区域分支只创建空间并关联现有主体。
- 经营实体是资产和业务唯一所有者，商圈/店铺/图外生意共用基础模型、按类型组合模块；BusinessEntityId 全局稳定，03 逻辑类型 FBusinessEntityId 底层待定，ShopId 为经营 ID 别名。Area.Type.Shop.* 等既有 Tag 作分类，具体迁移待定。
- 可选空间组件关联真实空间；区域仅空间范围、父子、定位、准入校验，不存 Owner/Manager 权属真值、bOpen、目录、订单、声誉、租约。经营与空间编排分开不镜像；物品/结构产权日志保留独立来源，空间包含不转移租户资产。
- 默认 bOpen=false 不等于开业；bOperating=bOpen&&有效时段 纯派生，只 bOpen 手动且入档，无自动资格复检。普通资料编辑仍名称/LOGO，合同变化不走普通 Setter；初始两钱包/业务账户人物持有。
- NPC 邀请空店或模板预览同一连续面板，候选同意条款和产权授权后先 Lease 再真落位；V1 只预置商圈玩家标准租杂货店，NPC 模板招商与完整客房留后续。住客是限时使用者，不是客房 Manager，门查询有效授权。
- 换租户沿用同一经营实体、删区域不注销实体（已定）；招图外换工待定，不增加猜测性防御/重试或新验收。

## 优化策略

### 1. 视觉主题统一

**色彩体系应用**（参照 DESIGN_UMG.md）

```
所有模块统一使用：
├── 面板背景: Background_Secondary (#16213E)
├── 卡片背景: Background_Primary (#1A1A2E)  
├── 选中高亮: Accent_Primary (#4FC3F7)
├── 成功/确认: Accent_Success (#66BB6A)
├── 警告: Accent_Warning (#FFA726)
└── 危险: Accent_Danger (#EF5350)
```

**字体层级**

| 元素 | 字体 | 大小 | 颜色 | 粗细 |
|------|------|------|------|------|
| 模块标题 | Roboto | 18px | Text_Primary | Bold |
| 参数标签 | Roboto | 14px | Text_Secondary | Regular |
| 数据值 | Roboto | 14px | Accent_Primary | Bold |
| 按钮文字 | Roboto | 14px | Text_Primary | Bold |

### 2. 组件标准化

**按钮规范**（所有模块统一）

```
尺寸变体:
├── Small: 24x24 (图标按钮)
├── Medium: 32x32 (标准按钮)  
├── Large: 48x48 (面板主按钮)
└── Wide: 120x32 (文字按钮)

状态:
├── 正常: Background=Transparent, Border=Border_Default
├── 悬停: Background=Background_Hover, Border=Accent_Primary
├── 按下: Background=Background_Pressed, Scale=0.98
└── 选中: Border=Accent_Primary, BorderWidth=2
```

**输入框规范**

```
├── 背景: Background_Secondary
├── 边框: Border_Default (正常) / Border_Focus (聚焦)
├── Padding: (8, 6)
├── 高度: 28px (标准)
└── 字体: Roboto 14px Text_Primary
```

**复选框规范**

```
├── 未选中: 边框=Border_Default, 背景=Transparent
├── 选中: 边框=Accent_Primary, 背景=Accent_Primary
└── 尺寸: 16x16 (标准) / 24x24 (大)
```

### 3. 布局优化

**模块间距体系**

```
模块内部:
├── 模块标题到内容: Spacing_LG (12px)
├── 参数组间距: Spacing_MD (8px)
├── 同组元素间距: Spacing_SM (4px)
└── 控件内边距: Spacing_MD (8px)

模块之间:
└── 分隔间距: Spacing_LG (12px)
```

**容器选择**

```
参数布局:
├── 标签+值 横向: HorizontalBox (Slot Padding 右间距 8px)
├── 纵向参数组: VerticalBox (Slot Padding 垂直间距 4px)
├── 网格布局: UniformGridPanel (等分 2-3 列)
└── 可折叠区域: Overlay + 动态 Visibility
```

### 4. 各模块优化要点

#### M1 建造库 (WBP_BuildLib)

**当前问题**: 库列表样式不统一

**优化**:
```
├── 列表项高度: 32px (标准) / 48px (带图标)
├── 选中态: Background_Hover + 左边框 Accent_Primary 2px
├── 交替行: 每偶数行 Background_Secondary + Opacity 0.3
└── 图标+文字间距: Spacing_SM (4px)
```

#### M2 操作模式 (WBP_OpPanel)

**当前问题**: 单选按钮样式不统一

**优化**:
```
├── 使用 CheckBox 组件规范
├── 选项间距: Spacing_MD (8px)
├── 选中态: Accent_Primary 填充
└── 文字标签: Text_Secondary (未选) / Text_Primary (选中)
```

#### M3 几何形状 (WBP_GeoPanel)

**当前问题**: 形状按钮大小不一

**优化**:
```
├── 统一按钮尺寸: 48x48 (Large)
├── 图标尺寸: 32x32
├── 选中态: Border Accent_Primary 2px + Background_Hover
└── 按钮间距: Spacing_SM (4px)
```

#### M7 笔刷参数 (WBP_M7_BrushParams)

**当前问题**: 滑块和输入框样式不一

**优化**:
```
├── 滑块规范:
│   ├── 轨道高度: 8px
│   ├── 已填充: Accent_Primary
│   ├── 未填充: Border_Default
│   └── 把手: 16x16
├── 数值显示: Data_Value 字体 (14px Bold Accent_Primary)
└── 参数组标题: Section_Title (18px Text_Primary)
```

#### M10 采样参数 / M11 魔棒参数

**当前问题**: 不同工具的参数面板视觉风格不一

**优化**:
```
├── 统一模块标题栏: 18px Bold + 底部 1px Border_Default 分隔线
├── 参数项模板: 标签(左, 120px固定宽) + 控件(右, 自适应)
├── 间距统一: 参数组间距 12px, 组内间距 8px
└── 空状态提示: Text_Disabled 颜色 + 斜体
```

### 5. 动效与反馈

**过渡时间**

```
├── 按钮按压: 0.1s Linear
├── 颜色变化: 0.2s EaseOut
├── 面板展开: 0.3s EaseInOut
└── 模块切换: 0.2s (淡入淡出)
```

**Widget Animation 规范**

```
模块显示:
├── Opacity: 0→1
├── TranslateY: 10px→0
└── Delay: 基于 DisplayOrder 的 stagger (0.05s * order/10)

选中高亮:
└── BorderColor: 变化 0.2s
```

### 6. 响应式适配

**DPI Scale 策略**

```
├── 基准: 1920x1080 @ 100%
├── 控件最小尺寸: 24x24 (键鼠) / 32x32 (触控)
├── 字体随 DPI 等比缩放
└── 间距固定像素 (不随 DPI 变化)
```

---

## 实施步骤

### Phase 1: 基础样式组件 (优先级: 高)

1. **创建 WBP_StyleBase** (所有模块继承)
   - 统一背景色
   - 统一字体规范
   - 统一间距变量

2. **标准化现有模块**
   - M1, M2, M3 (常显模块优先)
   - 统一按钮、输入框、复选框样式

### Phase 2: 动态模块统一 (优先级: 中)

1. **M7-M15 模块样式统一**
   - 统一模块标题栏
   - 统一参数布局模板
   - 统一空状态/加载状态

2. **动效添加**
   - 模块切换淡入淡出
   - 按钮交互反馈
   - 选中高亮动画

### Phase 3: 主题系统 (优先级: 低)

1. **动态材质实例**
   - 背景颜色通过 Material Parameter 控制
   - 支持主题切换 (亮色/暗色)

2. **设计 token 导出**
   - 生成 DESIGN_UMG.md 设计文档

---

## 检查清单

### 每个模块验证项

- [ ] 背景色使用 Background_Secondary
- [ ] 标题字体 18px Bold
- [ ] 正文/标签字体 14px Regular
- [ ] 数据值字体 14px Bold Accent_Primary
- [ ] 按钮尺寸符合规范
- [ ] 输入框高度 28px
- [ ] 间距使用 Spacing_MD (8px) 基准
- [ ] 选中态使用 Accent_Primary
- [ ] 禁用态 Opacity 0.4
- [ ] 无边框或 1px Border_Default

---

## 参考文件

| 文件 | 路径 |
|------|------|
| C++ 基类 | `Source/Voxel/Public/Core/UI/Build/ToolDetailModuleBase.h` |
| 主面板 | `Source/Voxel/Public/Core/UI/Build/BuildPanelWidget.h` |
| M1-M15 模块 | `Source/Voxel/Public/Core/UI/Build/Modules/` |
| 蓝图资源 | `/Game/Resource/UI/BuildModeUI/BuildDetailMode/` |
| 设计系统 | `.opencode/skills/umg-design/SKILL.md` |

---

## 预期效果

**优化前**
- 各模块视觉风格不一
- 颜色杂乱无主题
- 交互反馈不一致

**优化后**
- 统一的设计语言
- 清晰的视觉层级
- 一致的交互体验
- 符合 DESIGN_UMG.md 规范