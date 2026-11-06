# UI-ItemLibrary 通用组件改造决策记录

---

## 1. 背景与问题

### 1.1 当前现状

项目中存在三个库模块需要展示可选择的条目列表：

| 模块 | 当前实现 | 数据来源 | 与通用组件关系 |
|------|---------|---------|---------------|
| M1 BuildLibrary（建造库） | 嵌入 `WBP_BuildItemLib` 子组件 | `FVoxelItemBase` | 复用 `ItemLibraryWidgetBase` |
| M1b TemplateLibrary（模板库） | 嵌入 `WBP_BuildItemLib` 子组件 | `FVoxelItemBase` | 复用 `ItemLibraryWidgetBase` |
| M12b AreaTypeLibrary（区域类型库） | 独立实现搜索/TAB/分页/列表 | `FAreaTypeBase` | 未复用，代码重复 |

当前 `ItemLibraryWidgetBase` 的 Search、Tab、Slot 类型均硬编码在 C++ `BindWidget` 中：

- 搜索框固定为 `UEditableTextBox`
- TAB 容器固定为 `UHorizontalBox`
- 列表容器固定为 `UWrapBox`
- Slot 类型固定为 `UBuildSlotWidget`

这导致：

1. **视觉难统一**：M12b 独立实现，任何样式改动需要维护两套逻辑
2. **扩展性差**：背包、材料库、DLC 分类等未来需求无法复用同一组件
3. **点击事件未贯通**：`OnItemClicked` 代理已声明但无实际触发
4. **布局错误**：当前 Blueprint 未设置底部锚点，三个库都挤在左上角

### 1.2 触发原因

本次需求要求将三个库改造为底部横向布局（参考 Cities: Skylines 列表风格）：

- 搜索框在顶部
- TAB 行在搜索下方
- 列表区域占满主面板
- 左侧显示 InfoPanel，支持展开/收起
- 底部面板居中，宽度 80%，半透明背景

这迫使我们必须先解决 `ItemLibraryWidgetBase` 的通用化问题，否则 M12b 仍然无法真正复用。

---

## 2. 决策目标

1. 将 `ItemLibraryWidgetBase` 改造为**真正的通用列表组件**
2. 统一 M1/M1b/M12b 的视觉与交互逻辑
3. 支持 Search / Tab / Slot / Pagination / DataSource 可插拔
4. 为背包、材料库、DLC 分类等未来需求预留扩展点
5. 修复当前 Blueprint 布局错误

---

## 3. 可选方案

### 方案 A：仅统一 M1/M1b，M12b 保持独立实现

**做法**：
- 调整 M1/M1b 的 Blueprint 布局到底部横向
- M12b 继续维护自己的搜索/TAB/分页代码
- 只在视觉上让 M12b 看起来一致

**优点**：
- 改动量最小
- 风险最低

**缺点**：
- M12b 与 M1/M1b 仍是两套实现，后续维护成本高
- 无法支持未来更多库类型
- TAB 组件、Slot 组件、搜索组件不能复用

**结论**：否决。无法从根本上解决问题。

---

### 方案 B：统一 UI 组件，但数据源硬编码

**做法**：
- 将 `ItemLibraryWidgetBase` 改造为容器式组件
- Search / Tab / Slot / Pagination 可插拔
- 但数据来源仍由 `ItemLibraryWidgetBase` 自己从 `ConfigDataSubsystem` 拉取
- 为 M12b 写特殊分支判断

**优点**：
- 三个库 visuals 统一
- 组件可替换

**缺点**：
- `ItemLibraryWidgetBase` 内部仍耦合业务数据拉取逻辑
- 新增背包、材料库等需要改 `ItemLibraryWidgetBase` 源码
- M12b 的分类层级逻辑难以优雅塞入

**结论**：否决。通用性不足，业务耦合未解除。

---

### 方案 C：完整可插拔抽象（推荐）

**做法**：
- `ItemLibraryWidgetBase` 只负责 UI 流程：初始化 → 加载数据 → 渲染 TAB → 渲染列表 → 分页 → 转发事件
- 抽象出 `IItemLibraryDataSource`：提供条目、分类、过滤、搜索匹配
- 抽象出 `IItemLibraryCategoryProvider`：决定分类维度和规则
- 抽象出 `UItemLibrarySearchBase`、`UItemLibraryTabButtonBase`、`UItemLibraryTabContainerBase`、`UItemLibrarySlotBase`、`UItemLibraryPaginationBase`
- M1/M1b 使用 `UVoxelItemDataSource`
- M12b 使用 `UAreaTypeDataSource`
- 三个模块 Blueprint 统一为底部横向布局

**优点**：
- 三个库完全复用同一 UI 组件
- 新增库类型只需新增 DataSource/CategoryProvider
- 支持按物品类型、DLC/MOD、品质等多种维度分类
- 代码结构清晰，职责分离

**缺点**：
- 改动文件多（约 12-15 个 C++ 文件 + 多个 Blueprint）
- 需要把 M12b 的层级分类逻辑完整迁移

**结论**：采纳。

---

## 4. 最终决策

采用 **方案 C：完整可插拔抽象**。

### 4.1 核心架构

```
ItemLibraryWidgetBase（通用 UI 容器）
    ├── SearchContainer  ← UItemLibrarySearchBase
    ├── TabContainer     ← UItemLibraryTabContainerBase + UItemLibraryTabButtonBase
    ├── ListContainer    ← UItemLibrarySlotBase 列表
    └── PaginationContainer ← UItemLibraryPaginationBase
    
    数据来自：IItemLibraryDataSource
    分类来自：IItemLibraryCategoryProvider
```

### 4.2 数据流

```
后端数据（ConfigDataSubsystem / Inventory / AreaTypeRegistry）
    ↓
IDataSource::GetAllItems()
    ↓
FItemLibraryEntry 数组
    ↓
ICategoryProvider::GenerateCategories()
    ↓
渲染 TAB
    ↓
用户点击 TAB / 输入搜索
    ↓
IDataSource::FilterItemByCategory() / MatchesSearch()
    ↓
FilteredItems + 分页
    ↓
渲染 UItemLibrarySlotBase 列表
    ↓
用户点击 Slot
    ↓
广播 OnItemClicked(ItemTag)
```

### 4.3 三个模块映射

| 模块 | DataSource | CategoryProvider | Slot | Tab |
|------|-----------|------------------|------|-----|
| M1 BuildLibrary | `UVoxelItemDataSource`（Item_Type_Block） | 默认（Common + 直接子类型） | `WBP_BuildSlotWidget` | `WBP_CategoryTabButton` |
| M1b TemplateLibrary | `UVoxelItemDataSource`（Item_Type_BuildItem） | 默认 | `WBP_BuildSlotWidget` | `WBP_CategoryTabButton` |
| M12b AreaTypeLibrary | `UAreaTypeDataSource` | 自定义（Common + 有后代的父级） | `WBP_BuildSlotWidget` | `WBP_CategoryTabButton` |

---

## 5. 涉及改造范围

### 5.1 新增 C++ 文件

| 文件 | 说明 |
|------|------|
| `Public/Core/UI/ItemLibraryEntry.h` | 统一数据条目结构 |
| `Public/Core/UI/ItemLibraryDataSource.h` | 数据源接口 |
| `Public/Core/UI/ItemLibraryCategoryProvider.h` | 分类提供者接口 |
| `Public/Core/UI/ItemLibrarySearchBase.h` / `.cpp` | 搜索组件基类 |
| `Public/Core/UI/ItemLibraryTabButtonBase.h` / `.cpp` | TAB 按钮基类 |
| `Public/Core/UI/ItemLibraryTabContainerBase.h` / `.cpp` | TAB 容器基类 |
| `Public/Core/UI/ItemLibrarySlotBase.h` / `.cpp` | Slot 基类 |
| `Public/Core/UI/ItemLibraryPaginationBase.h` / `.cpp` | 分页基类 |
| `Public/Core/UI/Item/VoxelItemDataSource.h` / `.cpp` | 物品数据源实现 |
| `Public/Core/UI/Area/AreaTypeDataSource.h` / `.cpp` | 区域类型数据源实现 |

### 5.2 修改 C++ 文件

| 文件 | 说明 |
|------|------|
| `Public/Core/UI/Item/ItemLibraryWidgetBase.h` | 重写为通用容器 |
| `Private/Core/UI/Item/ItemLibraryWidgetBase.cpp` | 重写核心逻辑 |
| `Public/Core/UI/Build/BuildSlotWidget.h` | 继承 `UItemLibrarySlotBase` |
| `Private/Core/UI/Build/BuildSlotWidget.cpp` | 广播 `OnSlotClicked` |
| `Public/Private M1_BuildLibraryModule.h/.cpp` | 改为创建通用 LIB |
| `Public/Private M1b_TemplateLibraryModule.h/.cpp` | 改为创建通用 LIB |
| `Public/Private M12b_AreaTypeLibraryModule.h/.cpp` | 删除独立实现，使用通用 LIB |

### 5.3 修改/新增 Blueprint

| 文件 | 说明 |
|------|------|
| `WBP_BuildItemLib` | 改为容器式结构 |
| `WBP_CategoryTabButton` | 父类改为 `ItemLibraryTabButtonBase` |
| `WBP_DefaultItemLibrarySearch` | 新增默认搜索组件 |
| `WBP_DefaultItemLibraryTabs` | 新增默认 TAB 容器 |
| `WBP_DefaultItemLibraryPagination` | 新增默认分页组件 |
| `WBP_BuildLibModule` | 底部横向布局 |
| `WBP_TemplateLibModule` | 底部横向布局 |
| `WBP_M12b_AreaTypeLibraryModule` | 底部横向布局 |

---

## 6. 关键设计点

### 6.1 ItemLibraryWidgetBase 不再直接访问业务数据

旧逻辑：

```cpp
void InitializeItemsByType()
{
    ConfigDataSubsystem->GetItemTypeArrayByTag(MaxQueryTypeTag, ...);
}
```

新逻辑：

```cpp
void InitializeLibrary()
{
    if (DataSource)
    {
        AllItems = IDataSource::Execute_GetAllItems(DataSource);
        Categories = CategoryProvider ? ... : IDataSource::Execute_GenerateCategories(DataSource, AllItems);
    }
}
```

### 6.2 Slot 点击事件贯通

旧逻辑：

```cpp
// BuildSlotWidget 内部直接 ChangeHandItem，父级无法监听
```

新逻辑：

```cpp
// BuildSlotWidget 点击时同时广播 OnSlotClicked
// ItemLibraryWidgetBase 收到后广播 OnItemClicked(ItemTag)
// 父模块订阅 OnItemClicked 更新 InfoPanel
```

### 6.3 分类规则可替换

默认分类规则（物品）：

- `Common`：直接子标签的条目
- 其他 TAB：`MatchesTag(CategoryTag)`

区域类型分类规则（M12b）：

- `Common`：直接挂在 `Area.Type` 下的叶子节点
- 动态父级 TAB：直接挂在 `Area.Type` 下且拥有子标签数据的节点

这些规则写在各自的 `ICategoryProvider` 实现中。

### 6.4 兼容现有行为

- `BuildSlotWidget` 保留原有 `ChangeHandItem` 逻辑
- M1/M1b 的 `MaxQueryTypeTag` 配置方式不变
- 原有 `BindWidget` 名称在默认实现中保留，通过 `OptionalWidget` 标记可选

---

## 7. 风险与缓解措施

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 改动文件多，容易遗漏 | 编译/运行时错误 | 分阶段执行，每阶段编译验证 |
| M12b 分类逻辑迁移出错 | 区域类型显示/筛选异常 | 保留原函数作为对照，单元测试 Common/父级TAB 边界 |
| Blueprint BindWidget 失效 | 编译报错 | 每次 C++ 改动后重新编译相关 Blueprint |
| Slot 点击事件重复触发 | 手持物品设置异常 | 确保 `OnSlotClicked` 广播后仍调用原有 `ChangeHandItem` |
| 布局改动影响其他模块 | 工具面板显示异常 | 只改三个 Library Module，不动 `ToolLibraryPanel` 主体结构 |
| 性能退化 | 列表卡顿 | 保持分页机制，Slot 继续复用 |

---

## 8. 验证标准

### 8.1 编译验证

- [ ] C++ 项目编译通过
- [ ] `WBP_BuildItemLib` 编译通过
- [ ] `WBP_BuildLibModule` 编译通过
- [ ] `WBP_TemplateLibModule` 编译通过
- [ ] `WBP_M12b_AreaTypeLibraryModule` 编译通过

### 8.2 功能验证（PIE）

- [ ] M1 BuildLibrary 底部横向显示
- [ ] M1b TemplateLibrary 底部横向显示
- [ ] M12b AreaTypeLibrary 底部横向显示
- [ ] TAB 切换刷新列表
- [ ] 搜索过滤生效
- [ ] 分页上一页/下一页生效
- [ ] 点击 Slot 更新左侧 InfoPanel
- [ ] 点击 Slot 正确设置手持物品/区域类型
- [ ] 左侧展开/收起按钮生效

### 8.3 数据验证

- [ ] M1 只显示方块类物品
- [ ] M1b 只显示建造/模板类物品
- [ ] M12b 显示 Common + 动态父级 TAB
- [ ] M12b `bAbstract` 不影响分类（仅用于区域重叠逻辑）

---

## 9. 后续扩展点

| 未来需求 | 实现方式 |
|---------|---------|
| 背包系统 | 新增 `UInventoryDataSource` |
| 材料库 | 新增 `UMaterialDataSource` |
| 按 DLC/MOD 分类 | 新增 `UDLCCategoryProvider` |
| 按品质/重量分类 | 新增 `UQualityCategoryProvider` |
| 自定义搜索 UI | 继承 `UItemLibrarySearchBase` |
| 垂直 TAB / 侧边 TAB | 继承 `UItemLibraryTabContainerBase` |

---

## 10. 相关文档

- [工具栏 UI 双层工具列设计](工具栏UI-双层工具列设计.md)
- [编辑系统-选区与模板系统设计决策记录](编辑系统-选区与模板系统设计决策记录.md)
- 实施计划：`VoxelDesign/04-设计计划/UI-ItemLibrary通用组件改造-实施计划.md`（待生成）

---

## 11. 决策记录

| 项目 | 内容 |
|------|------|
| 决策人 | AI 助手 + 项目开发者 |
| 决策日期 | 2026-06-19 |
| 决策结果 | 采用方案 C：完整可插拔抽象 |
| 状态 | 已批准，待实施 |
