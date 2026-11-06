# UI 系统架构设计

> **文档目的**：定义 Voxel 项目前端 UI 的目标架构（对齐 UE5 官方推荐：CommonUI + MVVM + GameplayMessageSubsystem）
> **创建日期**：2026-07-17
> **最后更新**：2026-11-04
> **版本**：v1.0
> **状态**：已确定（目标架构，不考虑向后兼容）

---

## 一、概述

### 1.0 经营业务门面与面板

经营链的 UI 数据流：

```text
Widget（同一面板采集经营事实与空间草稿，不持业务真值）
  → ViewModel Command → Controller → 业务门面
  → 经营实体业务存储（Global.BusinessEntities） / 区域空间存储（按字段职责分发）
  → 数据事件
  → ViewModel / Widget 订阅刷新
```

规则：

1. 门面挂 PlayerController，与编辑域业务组件平级；不持业务数据。
2. 签约、货页五项、贷款三项、开业/停业、营业时段、排班/优先级、招聘/解雇都走门面方法。
3. 工单看板只读列表；玩家不能创建/编辑/指派工单。
4. 人员页是"劳动安排矩阵"（劳动者 × 24 小时 / 劳动者 × 工种），员工与经营人共用。
5. 资产页只有租约列表与维修工单；营业状态只读显示，操作走开业/停业按钮。
6. 营业时段编辑走业务方法，UI 不直接改运行时结构；bOperating=bOpen&&有效时段 纯派生，只有 bOpen 手动且入档，无自动资格复检。
7. 经营实体是资产与业务唯一所有者，商圈、店铺、图外生意共用基础模型、按类型组合模块，存 `Global.BusinessEntities`。经营与空间注册/存档分别编排，不镜像；初始两钱包/业务账户人物持有。
8. `BusinessEntityId` 是全局稳定经营标识（GUID，规则见[02 总纲 §2.7](/VoxelDesign/02-系统设计/AI决策系统-系统设计.md)）；不使用 `ShopId` 别名。可选空间组件引用 `AreaRef={SceneId,AreaId}`；子区域有 AreaId 不是经营实体。
9. 区域只持范围/父子/定位/准入约束，Owner/Manager 展示与产权/经营权限查询实体经营关系和有效授权；不把租约、bOpen、目录、订单、声誉放回区域。物品/结构产权日志保持独立来源，空间包含不转移租户资产。
10. 同一建造侧栏拉范围/摆模板预览 → 填名称、类型/业态、经营意图及必要条款 → 单次 Confirm 概念请求 CreateBusinessWithArea（OperatorId、BusinessKind、Name、BusinessIntent、ParentBusinessEntityId、空间草稿等；不硬定请求 class）→ 门面创建实体+空间+经营关系+必要 Lease/首期。有效 Lease 先于租户建筑/家具真写入，默认 bOpen=false 不等于开业；不要求先建区域后跳详情补建生意、不手动绑定。
11. 普通资料编辑只改名称/LOGO等；产权人/经营人不能自由填值取得权利，合同走有效授权的 Lease/任命/产权业务。创建侧栏复用现有范围/模板预览，M13a 纯导航是待迁移代码事实，不限制最终创建。子区域创建只空间并关联现有主体。
12. NPC 空店/模板邀请同一连续面板确认候选同意、条款、产权授权，先 Lease 再真落位；V1 只预置商圈玩家标准租杂货店，NPC 模板招商/完整客房后续。住客限时使用，不是客房 Manager，区域门按有效授权查询。
13. 换租户沿用同一实体、删区域不注销实体（已定）；招图外换工待定。

### 1.1 定位

UI 系统负责所有 2D 界面（HUD、面板、对话框、提示）的创建、层级、输入与数据展示。

**核心定位**：
- 纯展示层：只读 ViewModel，不直接访问世界子系统，不写业务状态
- 层级与输入由 CommonUI 官方框架托管，不自研
- 跨系统事件走 GameplayMessageSubsystem 消息频道，发布/订阅双方解耦

### 1.2 设计原则

1. **官方框架优先**：层级栈用 `UCommonActivatableWidgetStack`，数据绑定用 MVVM FieldNotify，跨系统消息用 GameplayMessageSubsystem；禁止重造轮子
2. **单向数据流**：业务组件（Controller 组件/世界子系统）→ ViewModel → Widget；Widget 的操作经 ViewModel/消息回流到业务层
3. **Widget 无业务**：Widget 内禁止 `GetSubsystem`、禁止 `GetFirstPlayerController`、禁止直接写业务组件状态
4. **配置驱动**：所有 Widget 类通过 `UPROPERTY(EditDefaultsOnly)` 配置，禁止 `LoadClass` 硬编码路径

### 1.3 废弃项（相对旧架构）

| 旧实现 | 状态 | 替代 |
|---|---|---|
| `AVoxelHUD` 自研 L1–L4 层级（ZOrder 常量/DialogStack/ToastQueue） | 废弃 | `UPrimaryGameLayout` + ActivatableWidgetStack |
| 手写 `SetInputMode`（HUD 与 PlayerController 两处） | 废弃 | CommonUI InputConfig / ActionRouter |
| Widget 手动 `SetText/Refresh` | 废弃 | MVVM FieldNotify 绑定 |
| 59 处 `AddDynamic` 直连委托（跨系统部分） | 废弃 | GameplayMessageSubsystem 频道 |
| 硬编码 `LoadClass` WBP 路径 | 废弃 | `UPROPERTY(EditDefaultsOnly)` 配置 |

---

## 二、组件架构

### 2.1 层次结构

```
UGameViewportClient（DefaultEngine.ini 配置 GameViewportClientClassName）
└─ UVoxelPrimaryGameLayout : UPrimaryGameLayout（per LocalPlayer，引擎自动创建）
     ├─ Layer.Game      : UCommonActivatableWidgetStack → HUD 常驻内容（主面板/交互提示/拖拽进度）
     ├─ Layer.GameMenu  : UCommonActivatableWidgetStack → 建造面板等全屏面板
     ├─ Layer.Modal     : UCommonActivatableWidgetStack → 对话框（自动遮罩/焦点/ESC 返回）
     └─ UVoxelToastWidget（非栈，浮动层）                → 订阅消息频道弹出，FTimer 自动消失

每个 Screen/Dialog 内部：
  UCommonActivatableWidget
    ├─ 功能 Widget（UCommonUserWidget，纯展示）
    └─ 绑定 ViewModel（UMVVMViewModelBase，FieldNotify）
```

### 2.2 组件职责描述

| 类 | 基类 | 职责 |
|---|---|---|
| `UVoxelPrimaryGameLayout` | `UPrimaryGameLayout` | 注册 4 个 Layer；UI 唯一入口；持有全部 Widget 类配置 |
| `UVoxelScreen`（建造面板等） | `UCommonActivatableWidget` | 全屏面板；激活/挂起/恢复由栈托管；声明 InputConfig（Game/Menu）自动管理输入模式 |
| `UVoxelDialog` | `UCommonActivatableWidget` | Modal 对话框；ESC 自动 Pop；关闭自动焦点恢复 |
| `UVoxelToastWidget` | `UCommonUserWidget` | 订阅 `UI.Toast.Show` 频道弹出；`FTimerHandle` 定时消失，不用 Tick |
| 功能 Widget（按钮/列表/参数模块） | `UCommonUserWidget` | 纯展示；只绑 ViewModel；用户操作通过 ViewModel Command 或消息频道回流 |
| `UVoxelXxxViewModel` | `UMVVMViewModelBase` | FieldNotify 属性；订阅业务组件细粒度委托并映射为 VM 属性；向 Widget 暴露绑定源 |
| 基础控件（文本/滑条/输入框/下拉） | `UVoxelTextBlock`/`UVoxelSlider`/`UVoxelEditableTextBox`/`UVoxelComboBoxString`（引擎控件 IS-A 子类） | 从 `DA_VoxelUITheme` 主题资产自应用样式（字体型号/颜色/素材）；只做样式与事件转发，不含业务；详见 [UI-统一控件与主题层-决策记录](/VoxelDesign/05-设计决策记录/UI-统一控件与主题层-决策记录.md) |

### 2.3 组件依赖关系

```
Widget ──绑定──> ViewModel ──订阅──> Controller 组件（细粒度委托）
Widget ──发送──> GameplayMessageSubsystem ──接收──> 业务系统
业务系统 ──发布──> GameplayMessageSubsystem ──接收──> Widget/ViewModel

禁止：
  Widget ──X──> GetSubsystem / GetFirstPlayerController / 直接写业务组件
```

---

## 三、接口定义

### 3.1 UVoxelPrimaryGameLayout

```cpp
UCLASS()
class UVoxelPrimaryGameLayout : public UPrimaryGameLayout
{
    // Widget 类配置（UPROPERTY(EditDefaultsOnly)，禁止 LoadClass）
    // - MainPanelClass / BuildPanelClass / ToastWidgetClass / 各 DialogClass

    // Layer 注册（NativeOnInitialized）
    // - Layer.Game / Layer.GameMenu / Layer.Modal

    // 对业务层暴露的接口
    void ShowMainPanel();                                   // 推入 Layer.Game
    void ToggleBuildPanel();                                // 推入/移除 Layer.GameMenu
    void PushDialog(TSubclassOf<UCommonActivatableWidget>); // 推入 Layer.Modal
};
```

### 3.2 ViewModel

```cpp
UCLASS()
class UVoxelBuildPanelViewModel : public UMVVMViewModelBase
{
    // FieldNotify 属性（示例）
    // - CurrentToolTag（FGameplayTag，FieldNotify）
    // - GeometryParams（FVoxelGeometryDesc，FieldNotify）
    // - CanUndo / CanRedo（bool，FieldNotify）

    // 数据源：NativeConstruct 时获取 PC 上的 BuildStateComponent，
    // 订阅其细粒度委托（OnToolChanged / OnGeometryChanged...），映射为 VM 属性
    // NativeDestruct 成对解绑
};
```

### 3.3 消息频道（GameplayMessageSubsystem）

频道 Tag 复用现有 NativeGameplayTags 体系，负载为 USTRUCT：

| 频道 Tag | 负载 | 发布者 | 订阅者 |
|---|---|---|---|
| `UI.Toast.Show` | 文本 + 级别 | 任意系统 | ToastWidget |
| `UI.Mode.Changed` | 新旧模式 | BuildStateComponent | 主面板 / PrimaryGameLayout |
| `Build.Selection.Changed` | 选区信息 | SelectionComponent | 预览组件 / 属性面板 |

**使用规则**：跨系统（世界子系统→UI、工具→Toast、Controller→UI 层）一律走消息频道；同组件内、组件→其 ViewModel 仍用细粒度委托。

> 注：本节是**前端通知**通道，与 02《事件系统》的业务事实登记层（`UVoxelEventSubsystem`）不是同一层，互不替代。

---

## 四、数据流

### 4.1 展示流（业务 → 界面）

```
BuildStateComponent.SetTool(Tag)
  → OnToolChanged 广播（细粒度）
  → BuildPanelViewModel 更新 CurrentToolTag（FieldNotify）
  → 建造面板按钮高亮自动刷新（MVVM 绑定）
```

### 4.2 操作流（界面 → 业务）

```
用户点击工具按钮
  → Widget 调 ViewModel 的 RequestSwitchTool(Tag)
  → ViewModel 调 Controller 工具切换业务 → BuildStateComponent.SetTool(Tag)
  → 细粒度广播 → 预览/其他 VM 更新
```

### 4.3 跨系统通知流

```
世界子系统发生需要提示的事件
  → GameplayMessageSubsystem.Broadcast(UI.Toast.Show, Payload)
  → ToastWidget 收到 → 弹出 → FTimer 3 秒后消失
```

---

## 五、生命周期管理

### 5.1 PrimaryGameLayout

- **创建**：LocalPlayer 创建时由引擎自动实例化（GameViewportClient 配置）
- **销毁**：随 LocalPlayer 销毁，无需手动管理

### 5.2 Screen / Dialog

- **创建**：`PushWidget` 推入对应 Layer 栈时由栈创建
- **激活/挂起**：栈顶 Widget 激活，被压栈时自动挂起（CommonUI 托管）
- **销毁**：Pop 出栈后由 GC 回收；ESC 由栈自动处理 Pop，无需手写

### 5.3 ViewModel

- **创建**：Widget `NativeConstruct` 中创建（Outer = Widget）
- **绑定**：创建后订阅业务组件细粒度委托
- **解绑**：Widget `NativeDestruct` 中成对解绑（绑定/解绑必须一一对应）

### 5.4 Toast

- **创建**：收到 `UI.Toast.Show` 消息时创建并加入视口
- **销毁**：`FTimerHandle` 倒计时结束 `RemoveFromParent`；禁止在 Tick 中手写倒计时

---

## 六、类关系图

```
UPrimaryGameLayout
  └─ UVoxelPrimaryGameLayout ──配置──> TSubclassOf<各 Screen/Dialog/Toast>

UCommonActivatableWidget
  ├─ UVoxelScreen
  │    └─ UBuildPanelScreen（原 BuildPanelWidget 迁移）
  └─ UVoxelDialog
       ├─ UTemplateSaveDialog
       └─ UFurnitureDetailDialog

UCommonUserWidget
  ├─ UVoxelToastWidget
  ├─ UMainPanelWidget
  └─ 各功能模块 Widget（原 ToolDetailModule 系列）

UMVVMViewModelBase
  ├─ UVoxelMainPanelViewModel
  ├─ UVoxelBuildPanelViewModel
  └─ 各模块 ViewModel（按需）
```

---

## 七、迁移映射（旧 → 新）

| 旧 | 新 |
|---|---|
| `AVoxelHUD`（VoxelHUD.h/cpp 整体） | `UVoxelPrimaryGameLayout` + Layer 栈 |
| `UVoxelScreenBase` / `UVoxelDialogBase` | `UCommonActivatableWidget` 子类 |
| `EVoxelUIZOrder` 常量 | Layer 注册顺序 |
| `DialogStack` 手写栈 | `Layer.Modal` 栈 |
| Toast 队列 + HUD::Tick | ToastWidget + 消息频道 + FTimer |
| `RefreshInputMode` + PC 两处 SetInputMode | CommonUI InputConfig |
| 手动 SetText/Refresh | MVVM 绑定 |
| 硬编码 LoadClass | EditDefaultsOnly 配置 |

---

## 八、附录

### 8.1 依赖配置

```
插件：CommonUI、CommonGame、GameplayMessageRouter
Build.cs 模块依赖：+ "CommonUI", "CommonGame", "GameplayMessageRuntime", "ModelViewViewModel"
DefaultEngine.ini：GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient（或自定义子类）
```

### 8.2 相关文档

- [编辑系统-Controller-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller-架构设计.md)
- [预览系统-PreviewActor-架构设计](/VoxelDesign/03-架构设计/预览系统-PreviewActor-架构设计.md)
- [前端架构-UE5规范化-决策记录](/VoxelDesign/05-设计决策记录/前端架构-UE5规范化-决策记录.md)

### 8.3 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0 | 2026-07-17 | 初始版本：确立 CommonUI + MVVM + GameplayMessage 目标架构，废弃 AHUD 自研层级 |
