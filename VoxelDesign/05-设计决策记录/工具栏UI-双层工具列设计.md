# 工具栏 UI 双层工具列设计

**日期**: 2026-01-25
**状态**: 已批准

---

## 1. 设计目标

实现基于 GameplayTag 的两层工具栏 UI 系统：
- **主工具列**（左侧）：显示第二层标签（Selection、Edit、AreaEdit）
- **子工具列**（左侧第二列）：显示第三层标签（具体工具）
- **详情栏**（右侧）：显示选中工具的配置参数

---

## 2. 核心组件

### 2.1 UToolIconButtonWidget
**职责**: 可复用的工具图标按钮

**核心功能**:
- 显示 FTagUIDefinition 配置的图标
- 处理选中/未选中状态切换
- 处理点击事件，直接调用 `InteractComp->SwitchTool(ToolTag)`
- 支持 Tooltip（悬停 0.5 秒显示工具名称）

**关键方法**:
```cpp
void SetToolTag(FGameplayTag InToolTag);
void SetSelected(bool bIsSelected);
FGameplayTag GetToolTag() const;
```

### 2.2 BuildPanelWidget
**职责**: 管理工具栏布局和状态同步

**UI 结构**:
- `MainToolContainer` (VerticalBox) - 主工具列
- `SubToolContainer` (VerticalBox) - 子工具列
- `DetailsPanel` - 详情栏（复用或扩展现有组件）

**核心方法**:
```cpp
void InitializeMainTools();           // 动态创建主工具按钮
void UpdateToolButtonsSelection();    // 更新按钮选中状态
void UpdateSubToolVisibility();       // 控制子工具列显示
void UpdateDetailsPanel();            // 更新详情栏
void HandleToolTagChanged(FGameplayTag NewToolTag); // 状态变化回调
```

---

## 3. 状态管理

### 3.1 数据存储（VoxelControllerStateComponent）
```cpp
UPROPERTY(VisibleInstanceOnly, category = "State")
FGameplayTag CurrentToolTag;  // 唯一状态源
```

**可能值**:
- `Voxel.Tool.Selection` (第二层 - 只选主工具)
- `Voxel.Tool.Selection.SingleVoxel` (第三层 - 选了子工具)
- 其他主工具/子工具标签

### 3.2 层级判断逻辑
```cpp
// 获取主工具（第二层）
FGameplayTag GetParentTag(FGameplayTag ChildTag);

// 判断是否只选了主工具（未选子工具）
bool IsMainToolOnly(FGameplayTag Tag);
```

---

## 4. 数据流

```
用户点击 Button
    ↓
Button.OnClicked → InteractComp->SwitchTool(ToolTag)
    ↓
InteractComp → StateComp->SetCurrentToolTag(ToolTag)
    ↓
StateComp 广播 OnCurrentToolTagChanged
    ↓
BuildPanelWidget.HandleToolTagChanged()
    ↓
更新工具按钮选中状态
更新子工具列显示
更新详情栏
```

**关键原则**: 单向数据流，UI 不直接修改状态，只监听状态变化

---

## 5. 初始化流程

```cpp
void NativeConstruct()
{
    // 1. 创建所有主工具按钮（未选中状态）
    InitializeMainTools();

    // 2. 绑定状态变化监听
    StateComp->OnCurrentToolTagChanged.AddUObject(
        this, &UBuildPanelWidget::HandleToolTagChanged);

    // 3. 根据当前状态同步 UI
    HandleToolTagChanged(StateComp->GetCurrentToolTag());
}
```

---

## 6. 动态工具生成

### 6.1 主工具初始化
```cpp
void InitializeMainTools()
{
    // 遍历 Voxel.Tool 的所有直接子标签（第二层）
    TArray<FGameplayTag> MainTools = GetDirectChildTags(FCoreTags::Voxel_Tool);

    for (const FGameplayTag& ToolTag : MainTools)
    {
        UToolIconButtonWidget* Button = CreateWidget<UToolIconButtonWidget>(this);
        Button->SetToolTag(ToolTag);
        Button->SetSelected(false);  // 初始未选中
        MainToolContainer->AddChildToVerticalBox(Button);
    }
}
```

### 6.2 子工具动态加载
```cpp
void RefreshSubTools(FGameplayTag MainToolTag)
{
    SubToolContainer->ClearChildren();

    // 获取主工具的所有子标签（第三层）
    TArray<FGameplayTag> SubTools = UGameplayTagsManager::Get()
        .RequestGameplayTagChildren(MainToolTag);

    for (const FGameplayTag& SubToolTag : SubTools)
    {
        UToolIconButtonWidget* Button = CreateWidget<UToolIconButtonWidget>(this);
        Button->SetToolTag(SubToolTag);
        Button->SetSelected(false);
        SubToolContainer->AddChildToVerticalBox(Button);
    }
}
```

---

## 7. 选中状态更新

### 7.1 通用按钮选中逻辑
```cpp
void UpdateContainerSelection(UVerticalBox* Container, FGameplayTag NewToolTag)
{
    for (int32 i = 0; i < Container->GetChildrenCount(); i++)
    {
        if (UToolIconButtonWidget* Button = Cast<UToolIconButtonWidget>(
            Container->GetChildAt(i)))
        {
            // 精确匹配或父子关系都算选中
            bool bIsSelected = (Button->GetToolTag() == NewToolTag) ||
                              NewToolTag.MatchesTag(Button->GetToolTag());
            Button->SetSelected(bIsSelected);
        }
    }
}
```

### 7.2 子工具列显示控制
```cpp
void UpdateSubToolVisibility(FGameplayTag NewToolTag)
{
    // 如果只选了主工具（第二层），显示子工具列
    if (IsMainToolOnly(NewToolTag))
    {
        if (SubToolContainer->GetChildrenCount() == 0)
        {
            RefreshSubTools(NewToolTag);
        }
        SubToolContainer->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        SubToolContainer->SetVisibility(ESlateVisibility::Collapsed);
    }
}
```

---

## 8. 交互行为

### 8.1 点击主工具
- **首次点击**: 选中主工具，显示子工具列
- **再次点击**: 取消选中，隐藏子工具列

### 8.2 点击子工具
- **首次点击**: 选中子工具，更新详情栏
- **再次点击**: 退回到主工具状态（不取消主工具）

### 8.3 切换主工具
- 自动显示新主工具的子工具列
- 子工具重置为未选中状态

---

## 9. 实现优先级

1. ✅ 创建 `UToolIconButtonWidget` 类
2. ✅ 在 `BuildPanelWidget.h` 添加容器成员变量
3. ✅ 实现 `InitializeMainTools()` 动态遍历逻辑
4. ✅ 实现选中状态更新逻辑
5. ✅ 实现子工具列显示/隐藏
6. ⏳ 实现详情栏更新（后续扩展）
7. ⏳ 添加 Tooltip 支持（后续扩展）

---

## 10. 技术要点

- **完全数据驱动**: 通过 GameplayTag 层级自动生成，无需硬编码
- **职责分离**: Button 处理点击，Panel 处理布局，StateComp 管理状态
- **单向数据流**: 状态变化驱动 UI 更新，UI 不直接修改状态
- **通用组件**: UToolIconButtonWidget 可复用于主工具和子工具

---

## 11. 待清理的旧代码

- BuildPanelWidget 中原有的硬编码工具切换逻辑
- 枚举类型 `EVoxelControllerBuildState` 相关代码
- 旧的工具面板组件（如有）
