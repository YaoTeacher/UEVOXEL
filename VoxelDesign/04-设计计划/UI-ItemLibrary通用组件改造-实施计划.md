# UI-ItemLibrary 通用组件改造实施计划

> 基于决策记录《UI-ItemLibrary通用组件改造-决策记录.md》方案 C，覆盖 M1 建造素材库、M1b 模板库、M12b 区域类型库三个列表的统一改造。背包列表暂不实施。

---

## 0. 决策对齐（与决策记录的偏差与补充）

| 项 | 决策 |
|---|---|
| Slot 点击兼容 | **彻底移除** `UBuildSlotWidget::ChangeHandItem` 短路，统一走 `IItemLibraryDataSource::OnEntryActivated` |
| 数据条目 | 引入 `FItemLibraryEntry`，UI 层不再持有 `FVoxelItemBase*` / `FAreaTypeBase*` |
| 委托贯通 | `OnEntryClicked` 由容器对外广播；副作用由 DataSource 处理 |
| 阶段粒度 | 合并为 3 个大阶段 |
| BP 改造 | 统一通过 `mcp_ue-editor-mcp` 在编辑器中以脚本方式落地 |
| 背包 | **本计划不涉及**，仅在接口设计上预留扩展点 |
| `FVoxelItemBase` 字段修正 | 没有 `bAbstract` / `TypeTag` / `DisplayName`，分类完全依赖 `ItemTag` 的 GameplayTag 层级；展示名用 `ItemName` |

---

## 1. 目标

1. `UItemLibraryWidgetBase` 升级为 **可插拔通用列表容器**：Search / Tab / Slot / Pagination 全部通过基类扩展；DataSource + CategoryProvider 注入业务。
2. M1 / M1b / M12b 三个模块统一使用同一个 `UItemLibraryWidgetBase`，删除 M12b 重复实现。
3. Slot 点击事件全链路打通：Slot → 容器 → DataSource 副作用 + 对外广播 `OnEntryClicked`。
4. 三个 Library 模块 Blueprint 统一为底部横向布局（参考 Cities: Skylines 风格）。
5. `UBuildSlotWidget` 不再直接访问 `AVoxelPlayerController`，改为只负责显示 + 上行点击事件。

---

## 2. 阶段总览（3 个大阶段）

| 阶段 | 名称 | 关键产出 | 验证方式 |
|---|---|---|---|
| **Phase A** | 抽象基础设施 + DataSource/Provider | 全部新增基类、接口、`FItemLibraryEntry`、两个 DataSource 实现、`UBuildSlotWidget` 重构 | C++ 编译通过；现有 BP 仍可加载（旧 BindWidget 暂保留） |
| **Phase B** | 容器化 `UItemLibraryWidgetBase` + 三模块 C++ 接入 | `UItemLibraryWidgetBase` 重写；M1/M1b/M12b 模块 C++ 全部改用 DataSource；M12b 删除独立实现 | C++ 编译通过；PIE 中三个库列表/搜索/分页/TAB 切换/点击副作用全部工作 |
| **Phase C** | Blueprint 容器化 + 底部横向布局 + 收尾 | `WBP_BuildItemLib` / 三个 ModuleWBP 的 UEEditorMCP 脚本与手动步骤；样式统一；旧字段删除 | 三个 BP 编译通过；PIE 视觉一致；最终回归 |

每个阶段结束都必须保证 C++ 编译通过且现存功能不回归，未完成阶段不得进入下一阶段。

---

## 3. Phase A：抽象基础设施 + DataSource/Provider

### A.1 新增数据条目结构

**文件**：[ItemLibraryEntry.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryEntry.h)（新增）

```cpp
USTRUCT(BlueprintType)
struct FItemLibraryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FGameplayTag      EntryTag;
    UPROPERTY(BlueprintReadWrite) FText             DisplayName;
    UPROPERTY(BlueprintReadWrite) FText             Description;
    UPROPERTY(BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(BlueprintReadWrite) FLinearColor      TintColor = FLinearColor::White;
    UPROPERTY(BlueprintReadWrite) int32             Count    = INDEX_NONE;
    UPROPERTY(BlueprintReadWrite) int32             Price    = INDEX_NONE;
    UPROPERTY(BlueprintReadWrite) bool              bDisabled = false;

    // 透传业务指针（可选），UI 层不解读
    const void*       SourcePtr  = nullptr;
    UScriptStruct*    SourceType = nullptr;
};

USTRUCT(BlueprintType)
struct FItemLibraryCategory
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString      CategoryId;
    UPROPERTY(BlueprintReadWrite) FText        DisplayName;
    UPROPERTY(BlueprintReadWrite) FGameplayTag CategoryTag;     // 复用 TagUIDef
    UPROPERTY(BlueprintReadWrite) int32        SortOrder = 0;
    UPROPERTY(BlueprintReadWrite) bool         bIsDefault = false;
};
```

> 沿用 `FItemCategoryConfig` 的字段语义，但重命名为 `FItemLibraryCategory` 以与新接口配套。旧 `FItemCategoryConfig` 在 Phase B 删除。

### A.2 新增数据源接口

**文件**：[ItemLibraryDataSource.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryDataSource.h)（新增）

```cpp
UINTERFACE(BlueprintType)
class UItemLibraryDataSource : public UInterface { GENERATED_BODY() };

class IItemLibraryDataSource
{
    GENERATED_BODY()
public:
    // 读
    UFUNCTION(BlueprintNativeEvent) TArray<FItemLibraryEntry> GetAllEntries() const;
    UFUNCTION(BlueprintNativeEvent) bool MatchesSearch(const FItemLibraryEntry& Entry, const FString& Keyword) const;

    // 通知 UI 刷新（可变源使用，不可变源忽略）
    DECLARE_MULTICAST_DELEGATE(FOnDataSourceChanged);
    virtual FOnDataSourceChanged* GetOnChanged() { return nullptr; }

    // 副作用入口（点击）
    UFUNCTION(BlueprintNativeEvent)
    void OnEntryActivated(const FItemLibraryEntry& Entry, AVoxelPlayerController* PC);
};
```

### A.3 新增分类提供者接口

**文件**：[ItemLibraryCategoryProvider.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryCategoryProvider.h)（新增）

```cpp
UINTERFACE(BlueprintType)
class UItemLibraryCategoryProvider : public UInterface { GENERATED_BODY() };

class IItemLibraryCategoryProvider
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent)
    TArray<FItemLibraryCategory> GenerateCategories(const TArray<FItemLibraryEntry>& All) const;

    UFUNCTION(BlueprintNativeEvent)
    bool BelongsToCategory(const FItemLibraryEntry& Entry, const FItemLibraryCategory& Category) const;
};
```

提供两个默认实现：

| 类 | 文件 | 规则 |
|---|---|---|
| `UTagHierarchyCategoryProvider` | [TagHierarchyCategoryProvider.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/TagHierarchyCategoryProvider.h)（新增） | Common = `IsDirectChildOfTag(EntryTag, RootTag)`；动态 Tab = RootTag 的直接子标签；用 `EntryTag.MatchesTag(CategoryTag)` 判断归属 |
| `UDynamicParentCategoryProvider` | [DynamicParentCategoryProvider.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/DynamicParentCategoryProvider.h)（新增） | Common = 直挂叶子；动态 Tab = RootTag 的直接子标签且其下还有后代；归属规则同上 |

两类都暴露 `UPROPERTY(EditAnywhere) FGameplayTag RootTag` 作为分类根。

### A.4 新增 Slot / Search / Tab / Pagination 基类

> 全部放在 `Public/Core/UI/` 下，文件名见下表。所有基类用 `UCLASS(Abstract, Blueprintable)`，BindWidget 用 `BindWidgetOptional` 标记，便于子类按需重写。

| 基类 | 文件 | 关键成员 |
|---|---|---|
| `UItemLibrarySlotBase` | [ItemLibrarySlotBase.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibrarySlotBase.h)（新增） | `virtual void InitFromEntry(const FItemLibraryEntry&)`；委托 `FOnSlotClicked OnSlotClicked(const FItemLibraryEntry&)`；属性 `FItemLibraryEntry CurrentEntry` |
| `UItemLibrarySearchBase` | [ItemLibrarySearchBase.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibrarySearchBase.h)（新增） | `BindWidgetOptional UEditableTextBox* SearchTextBox`；委托 `FOnSearchTextChanged OnSearchChanged(FString)` |
| `UItemLibraryTabButtonBase` | [ItemLibraryTabButtonBase.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryTabButtonBase.h)（新增） | `virtual void SetCategory(const FItemLibraryCategory&)`；`SetSelected(bool)`；委托 `FOnTabClicked OnTabClicked(FString)` |
| `UItemLibraryTabContainerBase` | [ItemLibraryTabContainerBase.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryTabContainerBase.h)（新增） | `BindWidgetOptional UPanelWidget* TabHost`；`virtual void RebuildTabs(const TArray<FItemLibraryCategory>&, TSubclassOf<UItemLibraryTabButtonBase>)`；`virtual void SetSelectedCategory(FString)`；上行 `FOnTabClicked OnTabClicked(FString)` |
| `UItemLibraryPaginationBase` | [ItemLibraryPaginationBase.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/ItemLibraryPaginationBase.h)（新增） | `BindWidgetOptional UButton* PrevButton/NextButton`、`UTextBlock* PageText`；`virtual void SetPage(int32 Cur, int32 Total)`；委托 `FOnPageChanged OnPageChanged(int32 Delta)` |

### A.5 改造 `UCategoryTabButtonWidget`

文件：[CategoryTabButtonWidget.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/CategoryTabButtonWidget.h)、[CategoryTabButtonWidget.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/CategoryTabButtonWidget.cpp)

- 父类改为 `UItemLibraryTabButtonBase`
- 现有 `SetCategoryData / SetSelected / OnSelectedStateChanged / RootButton / IconImage / LabelText` 全部保留
- 把 `RootButton->OnClicked` 在 `NativeConstruct` 中绑定到内部 handler，并通过基类委托 `OnTabClicked.Broadcast(CategoryId)` 上行

### A.6 改造 `UBuildSlotWidget`（移除 ChangeHandItem）

文件：[BuildSlotWidget.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Build/BuildSlotWidget.h)、[BuildSlotWidget.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/BuildSlotWidget.cpp)

- 父类改为 `UItemLibrarySlotBase`
- **删除 `ChangeHandItem()` 方法及其全部 PlayerController/InteractComp/StateComp 引用**
- `OnClickButton` 改为：
  ```cpp
  void UBuildSlotWidget::OnClickButton()
  {
      OnSlotClicked.Broadcast(CurrentEntry);
  }
  ```
- 重写 `InitFromEntry(const FItemLibraryEntry& Entry)`：
  - `CurrentEntry = Entry`
  - 异步加载 `Entry.Icon` 设置 `IconImage`
  - （保留 `InitSlotData(FGameplayTag, TSoftObjectPtr<UTexture2D>)` 作为兼容封装，内部组一个临时 Entry 调用 `InitFromEntry`，标 `UE_DEPRECATED`）

### A.7 新增两个 DataSource 实现

#### A.7.1 `UVoxelItemDataSource`

文件：[VoxelItemDataSource.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Item/VoxelItemDataSource.h)（新增）

- 配置：`UPROPERTY(EditAnywhere) FGameplayTag QueryTag`
- `GetAllEntries`：
  - `CDS->GetItemTypeArrayByTag<FVoxelItemBase>(QueryTag)`
  - 每条转 `FItemLibraryEntry`：`EntryTag = ItemTag`，`DisplayName = ItemName`，`Description = ItemDescription`，`Icon = AssetFragment.Icon`，`Price = CirculateDataFragment.Value`，`SourcePtr = Item`，`SourceType = FVoxelItemBase::StaticStruct()`
- `MatchesSearch`：对 `DisplayName.ToString()` 和 `EntryTag.ToString()` 做 `Contains(Keyword, ESearchCase::IgnoreCase)`
- `OnEntryActivated`：
  - `PC->InteractComp->SetItemInHand(EInteractableType::Place, Entry.EntryTag, 1)`

#### A.7.2 `UAreaTypeDataSource`

文件：[AreaTypeDataSource.h/.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Area/AreaTypeDataSource.h)（新增）

- `GetAllEntries`：
  - `CDS->GetAllAreaTypes()`，过滤 `bAbstract == false`
  - 每条转 `FItemLibraryEntry`：`EntryTag = AreaTypeTag`，`DisplayName = DisplayName`，`Description = Description`，`Icon = AssetFragment.Icon`，`SourcePtr = AreaType`，`SourceType = FAreaTypeBase::StaticStruct()`
- `MatchesSearch`：同上
- `OnEntryActivated`：
  - `PC->InteractComp->SetAreaType(Entry.EntryTag)`

### A.8 Phase A 编译验收

- [ ] C++ 工程编译通过
- [ ] 现存 BP（`WBP_BuildItemLib` / `WBP_BuildSlotWidget` / `WBP_CategoryTabButton`）仍能打开（BindWidget 旧字段在 Phase B 才删除）
- [ ] PIE 启动后 M1/M1b/M12b 旧路径不报红（此阶段不改 Module C++，因此功能仍走旧链路）

> Phase A 结束时旧的 `UItemLibraryWidgetBase` 与新的基类并存；功能上不切换。

---

## 4. Phase B：容器化 + 三模块 C++ 接入

### B.1 重写 `UItemLibraryWidgetBase`

文件：[ItemLibraryWidgetBase.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Item/ItemLibraryWidgetBase.h)、[ItemLibraryWidgetBase.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Item/ItemLibraryWidgetBase.cpp)

#### B.1.1 BindWidget（全部 Optional）

```cpp
UPROPERTY(meta=(BindWidget, BindWidgetOptional)) UPanelWidget*           SearchHost;
UPROPERTY(meta=(BindWidget, BindWidgetOptional)) UPanelWidget*           TabHost;
UPROPERTY(meta=(BindWidget))                     UPanelWidget*           ListHost;        // WrapBox / VerticalBox 任意
UPROPERTY(meta=(BindWidget, BindWidgetOptional)) UPanelWidget*           PaginationHost;
```

> 旧的 `SearchTextBox / CategoryTabContainer / ItemListContainer / PrevPageButton / NextPageButton / PageNumberText / FItemCategoryConfig` 全部删除。

#### B.1.2 配置项

```cpp
UPROPERTY(EditAnywhere, Instanced) UObject*                                  DataSource;        // 实现 IItemLibraryDataSource
UPROPERTY(EditAnywhere, Instanced) UObject*                                  CategoryProvider;  // 实现 IItemLibraryCategoryProvider
UPROPERTY(EditDefaultsOnly)        TSubclassOf<UItemLibrarySearchBase>       SearchWidgetClass;
UPROPERTY(EditDefaultsOnly)        TSubclassOf<UItemLibraryTabContainerBase> TabContainerClass;
UPROPERTY(EditDefaultsOnly)        TSubclassOf<UItemLibraryTabButtonBase>    TabButtonClass;
UPROPERTY(EditDefaultsOnly)        TSubclassOf<UItemLibrarySlotBase>         SlotClass;
UPROPERTY(EditDefaultsOnly)        TSubclassOf<UItemLibraryPaginationBase>   PaginationWidgetClass;
UPROPERTY(EditAnywhere)            int32                                     ItemsPerPage = 20;
```

#### B.1.3 公开 API

```cpp
void InitializeLibrary();                                  // 一键初始化
void SetDataSource(UObject* InDataSource);
void SetCategoryProvider(UObject* InProvider);
void SwitchToCategory(const FString& CategoryId);
void RefreshList();
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntryClicked, const FItemLibraryEntry&, Entry);
UPROPERTY(BlueprintAssignable) FOnEntryClicked OnEntryClicked;
```

#### B.1.4 内部流程

```
InitializeLibrary
  → 创建 SearchWidget (SearchWidgetClass) → 加入 SearchHost
  → 创建 TabContainer  (TabContainerClass)→ 加入 TabHost
  → 创建 Pagination    (PaginationWidgetClass) → 加入 PaginationHost
  → 绑定子组件委托：
        Search.OnSearchChanged       → HandleSearchChanged
        TabContainer.OnTabClicked    → HandleTabClicked
        Pagination.OnPageChanged     → HandlePageChanged
  → 绑定 DataSource.OnChanged (如有) → ReloadFromDataSource
  → ReloadFromDataSource

ReloadFromDataSource
  → AllEntries     = IDataSource::GetAllEntries(DataSource)
  → Categories     = ICategoryProvider::GenerateCategories(AllEntries)
  → TabContainer.RebuildTabs(Categories, TabButtonClass)
  → SwitchToCategory(默认分类)

SwitchToCategory(CategoryId)
  → 选中 Tab、CurrentPage=0
  → ApplyFilters → RenderPage

ApplyFilters
  → Filtered = AllEntries
                .Filter(BelongsToCategory(Current))
                .Filter(MatchesSearch(Keyword))

RenderPage
  → ListHost->ClearChildren
  → for entry in 当前页:
        Slot = CreateWidget<UItemLibrarySlotBase>(this, SlotClass)
        Slot->InitFromEntry(entry)
        Slot->OnSlotClicked.AddDynamic(this, &HandleSlotClicked)
        ListHost->AddChild(Slot)
  → Pagination->SetPage(CurrentPage, TotalPages)

HandleSlotClicked(Entry)
  → if (DataSource) IDataSource::OnEntryActivated(DataSource, Entry, GetOwningPlayer<AVoxelPlayerController>())
  → OnEntryClicked.Broadcast(Entry)
```

#### B.1.5 删除项（清单）

- `FItemCategoryConfig`
- `FItemClickedSignature`（被 `FOnEntryClicked` 替换）
- `MaxQueryTypeTag` / `bAutoGenerateAllCategory` / `CategoryTabButtonClass` / `ItemSlotWidgetClass`（迁移到 DataSource/Provider/SlotClass）
- `SetMaxQueryTypeTag` / `SetCategories` / `SetCategoryTabButtonClass`
- 内部 `InitializeItemsByType` / `IsDirectChildOfTag` / `GenerateBasicCategories` / `OnItemSlotClicked(FVoxelItemBase*)`

### B.2 改造 M1 BuildLibraryModule

文件：[M1_BuildLibraryModule.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Build/Modules/M1_BuildLibraryModule.h)、[M1_BuildLibraryModule.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/Modules/M1_BuildLibraryModule.cpp)

- 删除 `MaxQueryTypeTag` 字段（迁移到 DataSource 配置）
- 改用：
  ```cpp
  UPROPERTY(EditDefaultsOnly) TSubclassOf<UVoxelItemDataSource>            DataSourceClass;
  UPROPERTY(EditDefaultsOnly) TSubclassOf<UTagHierarchyCategoryProvider>   CategoryProviderClass;
  UPROPERTY(EditDefaultsOnly) FGameplayTag                                 QueryTag = FCoreTags::Item_Type_Block;
  ```
- `NativeConstruct`：
  - 创建 `ItemLibraryComponent` → `ItemLibraryContainer`
  - `auto DS = NewObject<UVoxelItemDataSource>(ItemLibraryComponent, DataSourceClass); DS->QueryTag = QueryTag;`
  - `auto CP = NewObject<UTagHierarchyCategoryProvider>(ItemLibraryComponent, CategoryProviderClass); CP->RootTag = QueryTag;`
  - `ItemLibraryComponent->SetDataSource(DS); ItemLibraryComponent->SetCategoryProvider(CP);`
  - `ItemLibraryComponent->OnEntryClicked.AddDynamic(this, &UM1_BuildLibraryModule::HandleEntryClicked);`
  - `ItemLibraryComponent->InitializeLibrary();`
- 新增 `HandleEntryClicked(const FItemLibraryEntry&)`：更新 `SelectedItemNameText / SelectedItemPriceText / SelectedItemQuantityText`（用 Entry 字段，无需再查 ItemMap）
- `UpdateFromToolState`：保留对 `OnBuildModeParamsUpdated` 的订阅与 `HandItemSlot->UpdateVisual` 的逻辑（手持显示与列表互不干扰）

### B.3 改造 M1b TemplateLibraryModule

文件：[M1b_TemplateLibraryModule.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Build/Modules/M1b_TemplateLibraryModule.h)、[M1b_TemplateLibraryModule.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/Modules/M1b_TemplateLibraryModule.cpp)

- 与 M1 同结构，`QueryTag` 默认 `FCoreTags::Item_Type_BuildItem`
- 移除空壳 `BindItemLibraryDelegates`，改为在 `NativeConstruct` 中绑定 `OnEntryClicked` 到 `HandleEntryClicked`
- `HandleEntryClicked` 更新 `SelectedTemplateNameText`；`MaterialRequirementsText` 暂保留 `CheckMaterialRequirements()` 占位（背包未实现时仅显示"已就绪"）

### B.4 改造 M12b AreaTypeLibraryModule（**重点**）

文件：[M12b_AreaTypeLibraryModule.h](file:///f:/UEProject/Voxel/Source/Voxel/Public/Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.h)、[M12b_AreaTypeLibraryModule.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.cpp)

#### 删除清单
- BindWidget：`SearchTextBox`、`CategoryTabContainer`、`TypeListWrapBox`、`PrevPageButton`、`NextPageButton`、`PageNumberText`
- 字段：`CachedAreaTypeTags`、`FilteredAreaTypeTags`、`DynamicTabButtons`、`TabButtonToCategoryMap`、`CurrentPage`、`TotalPages`、`ItemsPerPage`、`SelectedCategoryId`、`CurrentSearchFilter`、`FAreaCategoryConfig`、`OnAreaTypeSelected` 委托
- 方法：`LoadAreaTypesFromSubsystem`、`GenerateCategoriesFromHierarchy`、`InitializeCategoryTabs`、`CreateCategoryTabButton`、`UpdateCategoryTabStates`、`FilterAreaTypesByCategory`、`ApplySearchFilter`、`UpdateTypeList`、`RenderCurrentPage`、`UpdatePageNumberDisplay`、`CreateAreaTypeSlot`、`OnSearchTextChanged`、`OnTabButtonClicked`、`OnPrevPageClicked`、`OnNextPageClicked`、`OnTypeClicked`

#### 新增
```cpp
UPROPERTY(meta=(BindWidget)) UPanelWidget* ItemLibraryContainer;
UPROPERTY() UItemLibraryWidgetBase* ItemLibraryComponent;
UPROPERTY(EditDefaultsOnly) TSubclassOf<UItemLibraryWidgetBase>            ItemLibraryWidgetClass;
UPROPERTY(EditDefaultsOnly) TSubclassOf<UAreaTypeDataSource>               DataSourceClass;
UPROPERTY(EditDefaultsOnly) TSubclassOf<UDynamicParentCategoryProvider>    CategoryProviderClass;
UPROPERTY(EditDefaultsOnly) FGameplayTag                                   AreaRootTag;  // = Area.Type
```
`NativeConstruct` 与 M1 相同模式：创建 LIB → 注入 DS/CP → 订阅 `OnEntryClicked` → InitializeLibrary。
新增 `HandleEntryClicked` 仅做信息面板更新（点击副作用由 `UAreaTypeDataSource::OnEntryActivated` 完成 `SetAreaType`）。

### B.5 修复 ToolLibraryPanel 短路调用

文件：[ToolLibraryPanel.cpp](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/ToolLibraryPanel.cpp#L195)

`InteractComp->SetAreaType(TypeTag)` 这一类直接调用如果与 M12b 改造冲突，需要确认调用上下文是否仍由非 LIB 路径触发；本计划不修改它，仅在回归测试中验证不出现重复触发。

### B.6 Phase B 编译与功能验收

- [ ] C++ 工程编译通过
- [ ] 现有 BP（`WBP_BuildItemLib` 等）此时由于 BindWidget 名称不一致**会编译失败**——这是预期的，需要 Phase C 同步修改 BP；Phase B 验收时使用临时手段（在 BP 中重命名根容器为 `ListHost`）打通最低可运行路径
- [ ] PIE 中：
  - M1 列表显示，TAB/搜索/分页生效，点击 Slot 设置手持物品（来自 DataSource）
  - M1b 同上，点击 Slot 设置手持物品
  - M12b 列表显示，使用与 M1 相同的 LIB 组件，点击 Slot 设置临时区域类型
- [ ] 旧 `UBuildSlotWidget::ChangeHandItem` 已被删除，全局 grep 无残留

---

## 5. Phase C：Blueprint 容器化 + 底部横向布局 + 收尾

> 本阶段所有 BP 改造**优先**通过 `mcp_ue-editor-mcp` 自动化执行；个别需要手动美术对齐的步骤显式标出。

### C.1 UEEditorMCP 自动化脚本（按顺序执行）

> 执行前先 `ue_ping`，确认编辑器在线。脚本均通过 `ue_actions_run` 调用具体 action；具体 action 名见 `ue_actions_search`。下表只描述意图，落地时通过 `ue_batch` 串联。

#### C.1.1 改造 `WBP_BuildItemLib`

| 步骤 | 操作 | 备注 |
|---|---|---|
| 1 | 备份原 BP | `unreal_asset_history_get` 记录基线 |
| 2 | 重命名根 `WrapBox: ItemListContainer` → `ListHost` | 与 `UItemLibraryWidgetBase::ListHost` 匹配 |
| 3 | 在根 VerticalBox 顶部插入空 `HorizontalBox` 命名 `SearchHost` | 留给 SearchWidget |
| 4 | 在 SearchHost 下方插入空 `HorizontalBox` 命名 `TabHost` | 留给 TabContainer |
| 5 | 在 ListHost 下方插入空 `HorizontalBox` 命名 `PaginationHost` | 留给 Pagination |
| 6 | 删除旧子控件：`SearchTextBox` / `CategoryTabContainer` / `PrevPageButton` / `NextPageButton` / `PageNumberText` | 由动态创建的子组件替换 |
| 7 | Compile + Save | `ue_actions_run BlueprintCompile` |

#### C.1.2 创建默认子组件 BP

| 资产 | 路径 | 内容 |
|---|---|---|
| `WBP_DefaultItemLibrarySearch` | `/Game/Resource/UI/CoreUI/` | 父类 `UItemLibrarySearchBase`；`HorizontalBox` 内含 `EditableTextBox` 命名 `SearchTextBox` |
| `WBP_DefaultItemLibraryTabs` | `/Game/Resource/UI/CoreUI/` | 父类 `UItemLibraryTabContainerBase`；`HorizontalBox` 命名 `TabHost` |
| `WBP_DefaultItemLibraryPagination` | `/Game/Resource/UI/CoreUI/` | 父类 `UItemLibraryPaginationBase`；`HorizontalBox` + `Button(PrevButton)` + `TextBlock(PageText)` + `Button(NextButton)` |
| `WBP_BuildSlotWidget`（已存在） | 原路径 | 父类改为 `UBuildSlotWidget`（自动继承 `UItemLibrarySlotBase`），保持现有视觉 |
| `WBP_CategoryTabButton`（已存在） | 原路径 | 父类改为 `UCategoryTabButtonWidget`（自动继承 `UItemLibraryTabButtonBase`） |

> Reparent 通过 `ue_actions_run BlueprintReparent`；新建 BP 通过 `BlueprintCreate`。

#### C.1.3 改造三个模块 WBP

> 三者根布局统一，仅 M1 多一个 HandItemSlot。

```
CanvasPanel_0  (Anchors: Bottom-Center, Align: 0.5/1.0, AlignmentXY 居中)
└── SizeBox (WidthOverride 屏幕宽度 80%)
    └── HorizontalBox  RootHorizontalBox
        ├── Button       ToggleButton              (展开/收起)
        ├── VerticalBox  LeftArea
        │   ├── Border   HandItemSlotContainer     (仅 M1 保留)
        │   └── Border   InfoPanel
        │       ├── TextBlock  SelectedItemNameText / SelectedTemplateNameText / SelectedAreaTypeNameText
        │       └── ...        其余信息控件
        └── Border       ItemLibraryContainer      (运行时承载 WBP_BuildItemLib)
```

逐个 WBP 的步骤：

1. **`WBP_BuildLibModule`**：保留 `HandItemSlotContainer`；新增 `ToggleButton / LeftArea / InfoPanel / ItemLibraryContainer`；删除任何旧 BindWidget 的 `ItemLibraryContainer` 旧位置控件冲突项。
2. **`WBP_TemplateLibModule`**：删除 `HandItemSlotContainer`；其余结构与 M1 相同；保留 `SelectedTemplateNameText / MaterialRequirementsText`，移到 `InfoPanel` 下。
3. **`WBP_M12b_AreaTypeLibraryModule`**：删除所有旧 BindWidget（已在 Phase B 标注）；新增与 M1 相同结构的 `ToggleButton / LeftArea / InfoPanel / ItemLibraryContainer`；新增 `SelectedAreaTypeNameText` 用于显示当前选中区域类型。

每个 WBP 的 ClassDefaults 中：
- `ItemLibraryWidgetClass = WBP_BuildItemLib`
- `DataSourceClass`、`CategoryProviderClass` 选对应类
- M1：`QueryTag = Voxel.Item.Type.Block`
- M1b：`QueryTag = Voxel.Item.Type.BuildItem`
- M12b：`AreaRootTag = Area.Type`

#### C.1.4 样式

通过 UEEditorMCP 设置 `WBP_BuildItemLib` 的根 `Border`：
- `Brush.TintColor = (R=0,G=0,B=0,A=0.6)` 半透明深色背景
- 内 `VerticalBox` 间距 4

ToggleButton 视觉由蓝图美术自行细化，C++ 不强制。

### C.2 ToggleButton 行为（C++）

在 `UToolDetailModuleBase`（或三个子类各自）添加：

```cpp
UPROPERTY(meta=(BindWidgetOptional)) UButton*       ToggleButton;
UPROPERTY(meta=(BindWidgetOptional)) UPanelWidget*  LeftArea;
UPROPERTY()                          bool           bIsLeftAreaExpanded = true;

UFUNCTION() void OnToggleLeftAreaClicked();  // 切换 LeftArea Visibility
```

`NativeConstruct` 中：`ToggleButton->OnClicked.AddDynamic(this, &OnToggleLeftAreaClicked)`。

### C.3 收尾清理

| 项 | 操作 |
|---|---|
| 全局搜索 `ChangeHandItem` | 应只剩历史 commit；当前代码无残留 |
| 全局搜索 `FItemCategoryConfig` / `FAreaCategoryConfig` | 应已删除 |
| 全局搜索 `MaxQueryTypeTag` | 应只剩 `UVoxelItemDataSource::QueryTag` 间接使用 |
| `M1b_TemplateLibraryModule.h::TemplateSlotWidgetClass` 默认值类型不匹配 | 删除该字段（已被 LIB 的 SlotClass 取代） |
| `UItemLibraryWidgetBase::OnItemSlotClicked(FVoxelItemBase*)` | 已删除 |
| `UVoxelAreaItemWidget` 等无关组件 | 不动 |

### C.4 Phase C 验收

#### 编译
- [ ] C++ 编译通过
- [ ] `WBP_BuildItemLib`、三个模块 WBP、3 个新建默认子组件 BP、`WBP_BuildSlotWidget`、`WBP_CategoryTabButton` 全部编译通过
- [ ] `unreal_asset_diff_get` 检查 BP 改动符合预期，无意外字段丢失

#### PIE 功能
- [ ] M1 BuildLibrary 底部居中横向布局，宽度约 80%
- [ ] M1b TemplateLibrary 同上
- [ ] M12b AreaTypeLibrary 同上
- [ ] 三库 SearchHost / TabHost / ListHost / PaginationHost 都被运行时填充
- [ ] TAB 切换刷新列表
- [ ] 搜索过滤生效
- [ ] 分页上一页/下一页生效
- [ ] 点击 Slot：
  - M1：手持物品被设置（验证 `SC->GetInteractableDataInHand`）
  - M1b：模板被设置（同上）
  - M12b：`SC->GetTmpAreaInfo().AreaType` 被设置
- [ ] 点击 Slot：左侧 InfoPanel 文本（名称/价格/描述）随之刷新
- [ ] ToggleButton 切换 LeftArea 展开/收起
- [ ] M1 `OnBuildModeParamsUpdated` 仍正确驱动 HandItemSlot 视觉更新

#### 数据
- [ ] M1 仅显示 `Voxel.Item.Type.Block` 子树物品
- [ ] M1b 仅显示 `Voxel.Item.Type.BuildItem` 子树物品
- [ ] M12b 显示 Common（直接挂在 `Area.Type` 下的叶子）+ 有后代的直接子标签 TAB
- [ ] M12b `bAbstract == true` 的条目不出现在列表里

---

## 6. 文件改动清单

### 6.1 新增 C++

```
Source/Voxel/Public/Core/UI/ItemLibraryEntry.h
Source/Voxel/Public/Core/UI/ItemLibraryDataSource.h
Source/Voxel/Public/Core/UI/ItemLibraryCategoryProvider.h
Source/Voxel/Public/Core/UI/TagHierarchyCategoryProvider.h
Source/Voxel/Private/Core/UI/TagHierarchyCategoryProvider.cpp
Source/Voxel/Public/Core/UI/DynamicParentCategoryProvider.h
Source/Voxel/Private/Core/UI/DynamicParentCategoryProvider.cpp
Source/Voxel/Public/Core/UI/ItemLibrarySlotBase.h
Source/Voxel/Private/Core/UI/ItemLibrarySlotBase.cpp
Source/Voxel/Public/Core/UI/ItemLibrarySearchBase.h
Source/Voxel/Private/Core/UI/ItemLibrarySearchBase.cpp
Source/Voxel/Public/Core/UI/ItemLibraryTabButtonBase.h
Source/Voxel/Private/Core/UI/ItemLibraryTabButtonBase.cpp
Source/Voxel/Public/Core/UI/ItemLibraryTabContainerBase.h
Source/Voxel/Private/Core/UI/ItemLibraryTabContainerBase.cpp
Source/Voxel/Public/Core/UI/ItemLibraryPaginationBase.h
Source/Voxel/Private/Core/UI/ItemLibraryPaginationBase.cpp
Source/Voxel/Public/Core/UI/Item/VoxelItemDataSource.h
Source/Voxel/Private/Core/UI/Item/VoxelItemDataSource.cpp
Source/Voxel/Public/Core/UI/Area/AreaTypeDataSource.h
Source/Voxel/Private/Core/UI/Area/AreaTypeDataSource.cpp
```

### 6.2 修改 C++

```
Source/Voxel/Public/Core/UI/Item/ItemLibraryWidgetBase.h         （重写）
Source/Voxel/Private/Core/UI/Item/ItemLibraryWidgetBase.cpp      （重写）
Source/Voxel/Public/Core/UI/CategoryTabButtonWidget.h            （改父类）
Source/Voxel/Private/Core/UI/CategoryTabButtonWidget.cpp         （绑定基类委托）
Source/Voxel/Public/Core/UI/Build/BuildSlotWidget.h              （改父类，删 ChangeHandItem）
Source/Voxel/Private/Core/UI/Build/BuildSlotWidget.cpp           （删除 PC/InteractComp 引用）
Source/Voxel/Public/Core/UI/Build/Modules/M1_BuildLibraryModule.h
Source/Voxel/Private/Core/UI/Build/Modules/M1_BuildLibraryModule.cpp
Source/Voxel/Public/Core/UI/Build/Modules/M1b_TemplateLibraryModule.h
Source/Voxel/Private/Core/UI/Build/Modules/M1b_TemplateLibraryModule.cpp
Source/Voxel/Public/Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.h
Source/Voxel/Private/Core/UI/Build/Modules/M12b_AreaTypeLibraryModule.cpp
```

### 6.3 新增 / 修改 Blueprint

```
新增：
  /Game/Resource/UI/CoreUI/WBP_DefaultItemLibrarySearch
  /Game/Resource/UI/CoreUI/WBP_DefaultItemLibraryTabs
  /Game/Resource/UI/CoreUI/WBP_DefaultItemLibraryPagination

修改：
  /Game/Resource/UI/CoreUI/WBP_BuildItemLib
  /Game/Resource/UI/BuildModeUI/WBP_BuildSlotWidget                 （仅父类继承自动生效）
  /Game/Resource/UI/CoreUI/WBP_CategoryTabButton                    （仅父类继承自动生效）
  /Game/Resource/UI/BuildModeUI/BuildDetailMode/WBP_BuildLibModule
  /Game/Resource/UI/BuildModeUI/BuildDetailMode/WBP_TemplateLibModule
  /Game/Resource/UI/BuildModeUI/BuildDetailMode/WBP_M12b_AreaTypeLibraryModule
```

---

## 7. 风险与缓解

| 风险 | 触发条件 | 缓解 |
|---|---|---|
| Phase B 完成时 BP BindWidget 名称不匹配导致编译失败 | 重命名 `ItemListContainer` 等控件的过渡期 | Phase B 验收前先在 BP 中临时重命名根容器；Phase C 一次到位 |
| `UBuildSlotWidget::ChangeHandItem` 被其他模块（非 LIB）依赖 | grep 已确认仅 LIB 路径调用，无外部依赖 | 删除前最终再 grep 一次 `ChangeHandItem`、`SetItemInHand` 调用方，确保只剩 DataSource 触发 |
| `UAreaTypeDataSource::OnEntryActivated` 与 `ToolLibraryPanel.cpp:195` 的 `SetAreaType` 重复触发 | M12b 在两处都生效 | 排查 `ToolLibraryPanel.cpp:195` 调用上下文，必要时移除（确认是否为旧版"快速选择"按钮） |
| BindWidget Optional 未生效 | UE 5.7 BindWidgetOptional 标记位置错误 | 用 `meta=(BindWidget, OptionalWidget)` 形式（与现有 `UCategoryTabButtonWidget::IconImage` 一致） |
| `UPROPERTY(Instanced)` DataSource 初始化时机 | 子模块在 `NativeConstruct` 创建 LIB 后立即注入，避免 LIB 自身在编辑器默认值阶段尝试访问 | 仅通过 `SetDataSource` 注入；LIB 内部允许 DataSource 为空（不刷新） |
| 分页大小 / 列表高度变化导致每页项目数与 Pagination 不一致 | `ItemsPerPage` 改动 | 保持默认 20；后续按布局重新校准 |

---

## 8. 验收 Checklist 总表

### 8.1 C++
- [ ] 全部新增 21 个 C++ 文件按规范命名、有 `#pragma once`、版权行
- [ ] `UItemLibraryWidgetBase` 不再 `#include` `ConfigDataSubsystem`
- [ ] `UBuildSlotWidget` 不再 `#include` `VoxelPlayerController` / 控制器组件
- [ ] 编译 0 warning（与项目当前 baseline 一致）

### 8.2 BP
- [ ] `WBP_BuildItemLib` 仅含 `SearchHost / TabHost / ListHost / PaginationHost` 四个 BindWidget
- [ ] 三个 ModuleWBP 含 `ToggleButton / LeftArea / InfoPanel / ItemLibraryContainer`
- [ ] 默认子组件 BP（Search/Tabs/Pagination）BindWidget 命名匹配基类

### 8.3 行为
- 见 [C.4 Phase C 验收](#c4-phase-c-验收)

---

## 9. 计划信息

| 项目 | 内容 |
|---|---|
| 计划编制 | AI 助手 |
| 编制日期 | 2026-06-19 |
| 修订 | v2（粗粒度 3 阶段；移除 ChangeHandItem；UEEditorMCP BP 改造；不含背包） |
| 阶段数 | 3 |
| 当前状态 | 待实施（Phase A 可立即开始） |

---

## 10. 参考

- [UI-ItemLibrary 通用组件改造决策记录](../05-设计决策记录/UI-ItemLibrary通用组件改造-决策记录.md)
- [工具栏 UI 双层工具列设计](../05-设计决策记录/工具栏UI-双层工具列设计.md)
- [VoxelControllerInteractComponent.cpp#L397](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/ControllerComponent/VoxelControllerInteractComponent.cpp#L397)
- [VoxelControllerInteractComponent.cpp#L569](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/ControllerComponent/VoxelControllerInteractComponent.cpp#L569)
- [BuildSlotWidget.cpp 现状](file:///f:/UEProject/Voxel/Source/Voxel/Private/Core/UI/Build/BuildSlotWidget.cpp)
