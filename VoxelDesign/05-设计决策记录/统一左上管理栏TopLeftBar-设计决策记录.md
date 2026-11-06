# 统一左上管理栏（TopLeftBar）- 设计决策记录

**日期**: 2026-08-03
**最后更新**: 2026-11-04（仅空间导航/经营身份冲突修订）
**状态**: 已批准

---

## 1. 背景与目标

当前左上角 HUD（`WBP_VoxelGameHUD`）以 CanvasPanel 绝对定位散装摆放 4 个子控件（SliceHeightPanel / AreaNavigator / CutoutBoxListPanel / ModeToggleButton），视觉与职责分散。目标：整合为**单列垂直堆叠的紧凑管理栏**，新增 Pawn 高度控制与可折叠渲染图层切换区（透视框先行，热力图/风向图预留）。

参考：Cities: Skylines 左上角切换清单。

---

## 2. 设计决策

### 2.1 整体布局：单列垂直堆叠

```
WBP_VoxelTopLeftBar（宽 ~260px，AutoSize，左上角）
├── 区域面包屑（常驻，复用 WBP_VoxelAreaNavigator + 双击下属列表）
├── Pawn 高度行（常驻，Slider + 层数文本）      ← 新增
├── 最大渲染高度行（常驻，Slider + 层数 + 切面开关 + 过滤模式）← 收编 SliceHeightPanel
└── 渲染图层区（可折叠，图标即开关，激活自动展开详情）
    └── 透视框详情（收编 CutoutBoxListPanel）
```

### 2.2 高度单位：体素层（INDEX 空间）

- Pawn 高度与最大渲染高度统一用**体素层**（0~MaxSliceLayer），非厘米
- 换算：`层 * BlockLength`（`UChunkUtilsFunction`），与切面 `GetEditSliceWorldHeight` 同构
- Pawn 高度与渲染高度**本期不联动**（各自独立滑块）

### 2.3 渲染图层区：混合交互（图标即开关 + 详情展开）

- 图标列常驻（点图标=开关图层，激活高亮）
- 激活图层自动在下方展开详情区（可多个叠加）
- 整块可折叠为单个"▣ 渲染图层"按钮
- 折叠态下激活状态保留（状态存 SC）

### 2.4 图层统一注册机制（GameplayTag 驱动，与工具列同构）

```
FCoreTags::Layer（新根标签）
├── Layer_Cutout    ← 透视框（本期实现）
├── Layer_Heatmap   ← 地面热力图（预留）
└── Layer_Wind      ← 风向图（预留）
```

| 层 | 机制 | 与工具列关系 |
|----|------|-------------|
| 标签定义 | `FCoreTags::Layer_*` | 同构 |
| 图标/命名 | `DT_TagUIDefConfig` 加行 → `ConfigDataSubsystem::TagUIMap` | 完全复用 |
| 按钮生成 | `LayerSwitchSectionWidget::InitializeLayers()` 遍历 `RequestGameplayTagChildren(Layer)` 动态创建 `UToolIconButtonWidget` | 复用按钮组件 |
| 详情绑定 | `TMap<FGameplayTag, TSubclassOf<UUserWidget>> LayerDetailClasses`（蓝图可配） | 新建 |
| 激活状态 | SC：`TSet<FGameplayTag> ActiveLayers` + `SetLayerActive` + `OnLayersChanged`（**多选**） | 新建（工具是单选） |

**点击链路**：点图标 → `SC->SetLayerActive(Tag, !Active)` → 广播 `OnLayersChanged` → UI 高亮 + DetailContainer 实例化详情类。

预留图层只需加 Tag 表 + TagUI 表行 + 配置详情类，零改代码。

### 2.5 面包屑交互

- **单击**空间面包屑任一层级 = 切换区域（`VoxelAreaItemWidget::OnBtnClicked` → SelectArea）；目标用 `AreaRef={SceneId,AreaId}` 表达。该导航仅空间上下文，不切换/注册经营主体，也不赋予产权或经营权。
- **双击**当前级 = 展开下属空间区域列表（内联面板，读 `SubAreasIndex`；根级按当前场景 `ParentArea==nullptr` 筛选）。子区域有 AreaId，不是经营实体。
- 产业主域如需切换，按 `Global.BusinessEntities` 和有效经营关系列出主体，BusinessEntityId 全局稳定（03 逻辑类型 FBusinessEntityId 底层待定），ShopId 是经营 ID 别名；不能把空间树/AreaIndex 镜像成产业登记。既有 Area.Type.Shop.* 等 Tag 作分类，具体迁移待定。
- 经营与空间注册分别编排；区域不存 Owner/Manager、bOpen、目录、订单、声誉、租约真值，展示权限查询实体经营关系和有效授权。
- M13a/面包屑纯导航若保留为当前代码事实，须标为待迁移；不约束最终创建。建造同一侧栏范围/模板预览 → 必要名称/类型/业态/经营意图 → 单次 CreateBusinessWithArea，经 Controller→业务门面完成实体+空间+经营关系+必要 Lease/首期，先有效 Lease 再真写租户建筑/家具，不要求先建区域后跳详情或手动绑定。
- 当前项放开禁用（现 :86 `SetIsEnabled(!bIsCurrentSelection)`），改用视觉高亮。
- 双击识别：`OnBtnClicked` 内时间窗判断（同 TargetArea 两次点击 < 0.3s = 双击），单击语义不变。

### 2.6 透视框详情

- **列表级**：全部展示 / 全部隐藏（新增 `SetAllCutoutBoxesEnabled`）
- **单项**：位置信息（MinIndex 坐标）、定位移动（新增 `FocusCutoutBox`）、展示/隐藏（现有 `SetBoxEnabled`）、删除（现有 `RemoveCutoutBox` 带历史）

---

## 3. 后端状态与 API（SC 新增）

| 新增 | 签名 | 说明 |
|------|------|------|
| 字段 | `int32 PawnHeightLayer` | Pawn 所在体素层 |
| Getter | `GetPawnHeightLayer()` / `GetPawnWorldHeight()` | |
| Setter | `SetPawnHeightLayer(int32)` | Clamp[Min,Max]，Pawn->SetActorLocation(Z=Layer*BlockLength)，变化才广播 |
| Delegate | `FOnPawnHeightChanged OnPawnHeightChanged` | |
| 批量显隐 | `SetAllCutoutBoxesEnabled(bool)` | 遍历改写 bEnabled，一次广播 |
| 定位 | `FocusCutoutBox(int32 BoxId)` → bool | 中心=(Min+Max+1)*0.5*BlockLength，Pawn 移过去 |
| 图层 | `Set<FGameplayTag> ActiveLayers` / `SetLayerActive(Tag,b)` / `OnLayersChanged` | 多选开关，跨工具常驻 |

---

## 4. 结构性问题修复（顺带）

- MainPanel 补 `NativeDestruct` 解绑（现缺失，委托悬垂风险）
- 删除死代码：`LastNavigatorMode`、`SliceHeightPanelWidget::GetBlockLength()`
- 修正 `BindAllBuildEvents` 订阅粒度：补绑 `OnCurrentAreaChanged`
- 消除 UI 回调双刷新（SetX 广播后手动 RefreshDisplay 重复）
- 统一 SC 获取方式（父级解析一次注入子控件）

---

## 5. 文件清单

| 文件 | 操作 | 职责 |
|------|------|------|
| `VoxelBuildStateComponent.h/.cpp` | 修改 | Pawn 高度、批量显隐、定位、ActiveLayers |
| `VoxelTopLeftBarWidget.h/.cpp` | 新建 | 聚合容器契约类 |
| `LayerSwitchSectionWidget.h/.cpp` | 新建 | 图层切换区 |
| `VoxelAreaNavigatorWidget.h/.cpp` | 修改 | 双击展开下属 |
| `VoxelAreaItemWidget.h/.cpp` | 修改 | 放开当前项 + 双击事件 |
| `CutoutBoxListEntry.h/.cpp` | 修改 | 位置显示 + 定位实现 |
| `CutoutBoxListPanel.h/.cpp` | 修改 | 全部展示/隐藏 |
| `VoxelGameMainPanel.h/.cpp` | 修改 | 解绑、清理、接入 TopLeftBar |
| `SliceHeightPanelWidget.h/.cpp` | 修改 | 逻辑收编（渲染行），保留类供 WBP 兼容 |
| `CoreTags.h/.cpp` | 修改 | Layer 根标签 + 三个子标签 |

---

## 6. 实施顺序

1. SC 后端扩展 → 2. 面包屑双击 → 3. 透视框位置/定位 → 4. 透视框全显全隐 → 5. TopLeftBar C++ → 6. LayerSwitchSection C++ → 7. WBP 蓝图整合（MCP）+ 清理 → 8. 编译 + PIE 冒烟
