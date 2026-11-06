 # 前端架构 UE5 规范化 - 设计决策记录

> **系统**：前端控制层（Controller / UI / 预览）
> **创建日期**：2026-07-17

---

## 决策1：废弃 AHUD 自研 UI 层级，迁移 CommonUI + MVVM

**日期**：2026-07-17

**决策背景**：
现有 `AVoxelHUD` 自研 L1–L4 层级（ZOrder 常量、DialogStack、ToastQueue、手写 SetInputMode 两处分散），与 UE5 官方推荐架构整体偏离，且存在输入模式互相覆盖、关闭弹窗无焦点恢复、Toast 用 Tick 手写倒计时等问题。

**考虑的选项**：
- A. 保留自研层级，修补问题（输入模式收敛单点、Toast 改 Timer）
- B. 迁移 UE5 官方栈：CommonUI（PrimaryGameLayout + ActivatableWidgetStack）+ UMG MVVM

**选择的方案**：B

**选择理由**：
- 官方栈自带激活/挂起/恢复、ESC 返回栈、焦点恢复、输入路由，消除整类手写缺陷
- MVVM FieldNotify 消除 44:12 的绑定/解绑失衡与手动 SetText 刷新
- 用户明确要求"最正确和规范的框架"，不考虑向后兼容

**权衡和风险**：
- 迁移面大：全部 Widget 基类替换、HUD 删除、蓝图 WBP 需同步调整
- 团队需熟悉 CommonUI/MVVM 概念

---

## 决策2：跨系统事件采用 GameplayMessageSubsystem，组件内保留细粒度委托

**日期**：2026-07-17

**决策背景**：
现状 59 处 `AddDynamic` 直连委托，订阅方必须拿到对方实例，且 `OnBuildModeParamsUpdated` 空参数广播导致订阅方全量重读+各自做 diff。

**考虑的选项**：
- A. 全部改为 GameplayMessageSubsystem 频道
- B. 跨系统走消息频道，同组件内/组件→其 ViewModel 保留细粒度委托

**选择的方案**：B

**选择理由**：
- 消息频道解决跨系统解耦（世界子系统→UI、工具→Toast），发布/订阅双方互不知晓
- 组件内部直连委托类型安全、零额外开销，无需强行频道化
- 频道 Tag 复用现有 NativeGameplayTags 体系，零成本定义

**权衡和风险**：
- 需启用 GameplayMessageRouter 插件
- 频道负载结构体需统一约定

---

## 决策3：输入路由保留在蓝图，轮询归 C++

**日期**：2026-07-17

**决策背景**：
现状全部 IA→回调绑定在 BP_PlayerController 蓝图，C++ 仅 AddMappingContext。评审建议迁移 C++ 绑定以获得编译期防护，但用户明确要求输入保留蓝图绑定（策划可改键）。

**考虑的选项**：
- A. 输入绑定全部迁移 C++（BindAction + UPROPERTY IA）
- B. 蓝图绑定保留，C++ 提供 BlueprintCallable 入口 + UPROPERTY 资产引用兜底；Hover 等非事件轮询收归 C++ 定时器

**选择的方案**：B

**选择理由**：
- 尊重用户决策：策划可改键是明确需求
- 蓝图 Tick→HandleMouseHover 的每帧调用是轮询而非事件，收归 C++ 低频定时器（50ms）后可与 GroundMarker/Brush 共享同一次射线结果，同时蓝图不再跑每帧逻辑
- UPROPERTY 兜底使绑定断链可审计

**权衡和风险**：
- 输入路由无法静态检查，重构时需在 C++ 路由表注释同步登记

---

## 决策4：状态组件按域拆分，TmpGeometry 归建造域

**日期**：2026-07-17

**决策背景**：
`UVoxelControllerStateComponent` 715 行 God Object，20+ 种状态共用一个 `OnBuildModeParamsUpdated` 广播，任何变更触发预览/网格全量重建。评审提出拆分后需回答 TmpGeometry 等多工具公用数据的归属。

**考虑的选项**：
- A. 按状态字段逐个拆组件（每字段一个组件）
- B. 按业务域拆分：建造会话域（BuildStateComponent）+ 选区域（SelectionComponent）

**选择的方案**：B

**选择理由**：
- TmpGeometry 的完整链路（工具写入 → 存储 → 预览重建）都在建造域内，归 BuildStateComponent
- 多工具"公用"不构成问题：工具经 BuildStateComponent 读写，与拆分前等价；拆分解决的是事件粒度而非共享
- 按字段拆组件会产生组件爆炸与跨组件拼装成本

**权衡和风险**：
- 拆分破坏现有蓝图对 StateComponent 的绑定，需同步迁移

---

## 决策5：统一鼠标射线——一条世界射线，多次按需追踪

**日期**：2026-07-17

**决策背景**：
现状同一条 Channel1 地面射线每帧被 3 处各跑一遍；同时家具检测（Channel2）、区域检测（Channel3）、Gizmo（Channel6）使用不同通道、逻辑不同。评审问题："一个射线能处理吗？"

**考虑的选项**：
- A. 所有追踪合并为一次多通道射线
- B. 世界射线（DeprojectMouse）唯一 + 地面追踪每周期一次共享 + 业务通道追踪由当前激活工具按需发起

**选择的方案**：B

**选择理由**：
- 不同通道命中语义不同（地面/家具/区域/Gizmo），无法合并为一次追踪
- 但世界射线（起止点）完全可共享：区域工具现有代码已复用地面射线起止点做 Channel3 追踪
- 业务通道追踪只在对应工具激活时发起，非激活工具零开销
- 消除的是"同一地面追踪重复 3 次"，而非通道差异

**权衡和风险**：
- 需引入共享的 FVoxelMouseHit 结果结构与分发机制

---

## 决策6：删除 VoxelEditorModeSubsystem

**日期**：2026-07-17

**决策背景**：
该子系统的 `bEditorModeActive` 无任何读取方（IsEditorModeActive 0 调用）、两个事件委托 0 绑定、工具类注册表 0 调用点（与 InteractComp 工具实例注册表平行且为死系统）、Tick 为空。真正的模式权威是状态组件的 PlayerMode。

**选择的方案**：整体删除

**选择理由**：
- 全部功能为死代码，删除后无任何玩家可见行为变化
- 减少"模式状态三处维护"的混乱（EditorModeSubsystem / StateComponent / HUD → 只留 BuildStateComponent 一处权威）

**同步处理点**：
- `VoxelPlayerController.h/cpp`：删成员、getter、缓存逻辑
- `BuildPanelWidget.cpp`：删 Enter/Exit 通知块（保留显隐逻辑）
- `VoxelEditorConsoleCommands.cpp`：删两个调试命令
- `VoxelToolBase.h:56` 注释、`VoxelEditorData.h:405,410` 委托声明清理

**风险**：
- 丢失两个调试控制台命令（可接受）

---

## 决策7：长按/倒计时等一律 FTimerHandle，禁止 Tick 轮询

**日期**：2026-07-17

**决策背景**：
家具长按用 Tick 比对 `GetTimeSeconds`、Toast 倒计时手写 HUD::Tick、DragProgress 每帧跟手，均为 Tick 轮询反模式。

**选择的方案**：全部改 `FTimerHandle`

**选择理由**：
- UE 官方定时机制，语义明确、无每帧开销
- 消除 HUD::Tick 与 InteractComp Tick 的整段轮询代码

**例外**：Brush 拖拽涂抹、Gizmo 拖拽跟随等需要逐帧连续性的场景，保留在 ToolComponent 统一射线周期（50ms 定时器）内驱动，不单独开 Tick。

---

## 决策8：业务事件层自研，与前端 UI 通知决策分开

**日期**：2026-11-05

**决策背景**：
审阅时曾把 02《事件系统》的 `UVoxelEventSubsystem` 与决策2 的 `GameplayMessageSubsystem` 判成"双总线冲突"。复核后确认是两层不同的东西：

- **业务事件层**（02 事件系统）：登记**业务事实**——统一恒定结构（主动者/接收者/值参数/因果链/结果段），类型语义只由 `EventType` 一个 Tag 承载，需要目录校验与结果段反查。
- **前端通知层**（决策2）：UI 与系统之间的提示与状态通知（世界子系统→UI、工具→Toast），载荷是 UI 关心的少量数据。

**结论**：

1. 业务事件层由自研 `UVoxelEventSubsystem` 承载，**不引入** `GameplayMessageSubsystem`——后者以"一个频道绑定一种强类型 USTRUCT"为前提，与统一事实结构不匹配，且不提供目录校验、因果链与结果段反查。
2. **本决策不改变决策2**：前端 UI 通知仍按决策2（跨系统走频道、组件内保留细粒度委托），其插件启用与实现另由前端架构推进。
3. 两层互不越界：业务事实不得为了给 UI 看而改走前端频道；UI 提示不得记为业务事件进入事件目录。

**权衡和风险**：
- 业务事件层需自研分发与订阅（约 300 行），必须保证载荷只读、无首处理器、订阅者各自事务
- `UpstreamEventIds` / `Outcomes` 在 V1 只定义不使用，避免完整版回头改所有上报点
- 逻辑层内部通知（Gizmo、控件值变化、工具切换、选中变化）继续走组件委托，既不是业务事件也不进前端频道

---

**相关文档**：
- [UI系统-架构设计](/VoxelDesign/03-架构设计/UI系统-架构设计.md)
- [编辑系统-Controller-架构设计](/VoxelDesign/03-架构设计/编辑系统-Controller-架构设计.md)
- [预览系统-PreviewActor-架构设计](/VoxelDesign/03-架构设计/预览系统-PreviewActor-架构设计.md)
