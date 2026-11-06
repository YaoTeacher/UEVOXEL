# 设计决策记录：AREA编辑前后端链路修复

**日期**: 2026-06-05
**最后更新**: 2026-11-04（仅本规则冲突修订，代码待迁移）
**状态**: 已批准
**作者**: AI 助手
**相关模块**: M8, M12a, M12b, M13a, M16, VoxelControllerInteractComponent

---

## 1. 背景与问题

Area 编辑工具（创建/修改）的 UI 模块与后端 StateComponent 之间存在多处链路断裂，导致用户在前端输入的数据无法传递到后端，确认按钮也未按工具类型正确分发。

### 发现的问题清单

| 优先级 | 问题 | 影响 |
|--------|------|------|
| P0 | M12a 创建信息与 STATE 草稿链断裂 | 同侧栏必要名称/类型/经营意图无法进入组合确认 |
| P0 | M13a导航与普通编辑混入经营字段 | 需迁移为只读/导航，关系与营业状态不得经区域 Setter 写入 |
| P0 | M16 确认按钮统一调用 `RequestConfirmVoxelEdit()` | AreaEdit 工具下不创建/修改 Area 实例，变成体素编辑 |
| P1 | M8 在 Create 模式下 CurrentArea 为 null | 显示"未选择区域"，用户看不到正在创建的临时信息 |
| P1 | M13a暴露区域经营状态输入 | 应移除经营状态 Setter，改由实体营业业务及只读派生显示 |
| P2 | BuildAreaModifyWidget 旧面板已废弃 | 与 ToolDetailPanel 双轨并行，维护两套逻辑 |

---

## 2. 决策

### 2.1 草稿写入与正式业务

草稿只有 STATE 一个来源，输入经 Controller 草稿方法更新，预览与 UI 订阅；不得以草稿 Setter 直接保存产权、经营关系或合同。M13a 纯导航作为待迁移代码事实保留，不限制最终创建面板。

经营实体是资产与业务唯一所有者，商圈/店铺/图外生意共用基础模型、按类型组合模块，存 `Global.BusinessEntities`。BusinessEntityId 全局稳定，03 逻辑类型 FBusinessEntityId 底层待定；ShopId 为其别名；可选空间组件引用 AreaRef={SceneId,AreaId}。区域只空间范围/父子/定位/准入校验，不镜像 Owner/Manager、租约、bOpen、目录、订单、声誉；经营与空间注册分开。Area.Type.Shop.* 等既有 Tag 作分类，迁移待定；资产产权日志保留独立来源，不因空间包含转移租户资产。

### 2.2 M16 确认按钮行为

- 商圈/店铺创建：同一建造侧栏拉范围/摆模板预览 → 填名称、类型/业态、经营意图等 → 单次 `CreateBusinessWithArea` 概念请求，经 Controller → 业务门面 → 实体/空间存储 → 事件 → UI。请求含 OperatorId、BusinessKind、Name、BusinessIntent、ParentBusinessEntityId、空间草稿等，class 待定；创建实体+空间+经营关系+必要 Lease/首期，先有效 Lease 再真写租户建筑/家具。不要求先建区域后跳详情补建生意或手动绑定。
- 子区域创建：只创建空间、关联现有主体，不新建经营实体。
- 区域修改：仅空间范围/功能类型等，经对应业务校验有效授权；普通资料名称/LOGO归相应实体/空间业务，合同不走普通 Setter。
- 普通几何工具维持 `RequestConfirmVoxelEdit()` 执行，经营创建不能直接落入该分支绕过租约；不改变体素/撤销算法。

### 2.3 营业状态

M13a 不提供 EAreaStatus 全枚举输入。实体创建默认 bOpen=false，不等于开业；独立营业按钮只手动写入并存档 bOpen，bOperating=bOpen&&有效时段 纯派生，无自动资格复检。

### 2.4 ParentArea 字段

保留空间父级只读/导航，创建草稿中的父空间与 ParentBusinessEntityId 分属空间/经营业务，不由空间包含推导经营身份。

### 2.5 Owner / Manager

只读展示实体经营关系与有效授权的解析结果，不输入数字 ID、不以任意角色选择取得权利；出租/任命/产权业务专用流程维护。住客为限时使用者，不是客房 Manager，门查询有效授权。

V1 只预置商圈玩家标准租杂货店；NPC 邀请空店或模板预览同一连续面板，候选同意条款及产权授权后先 Lease 再真落位，NPC 模板招商/完整客房后续。初始两钱包/业务账户人物持有。换租户沿用同一经营实体、删区域不注销实体（2026-11-06 已定）；招图外换工待定。

---

## 3. 具体改动清单

### 3.1 M12a 修复

- **文件**:
  - `Source/Voxel/Public/Core/UI/Build/Modules/M12a_AreaBasicInfoInputModule.h`
  - `Source/Voxel/Private/Core/UI/Build/Modules/M12a_AreaBasicInfoInputModule.cpp`

- **待迁移改动**:
  - M12a 名称/经营意图及类型输入经 Controller写创建草稿，空间范围保留现有预览链路。
  - 不保留 Manager 自由文本输入或缓存；经营关系由有授权的组合/专用业务确认。
  - Confirm 把经营事实与空间草稿交给门面分别写相应存储，不靠创建后详情补录。

### 3.2 M13a 修复

- **文件**:
  - `Source/Voxel/Public/Core/UI/Build/Modules/M13a_AreaBasicInfoEditModule.h`
  - `Source/Voxel/Private/Core/UI/Build/Modules/M13a_AreaBasicInfoEditModule.cpp`

- **待迁移改动**:
  - M13a 只保留空间只读/父子导航，不提交经营人、产权人或经营状态。
  - 普通名称/LOGO 编辑转对应资料业务；空间范围草稿经 Controller 更新 STATE，确认只发空间业务。
  - 移除 Status 全枚举输入、Manager 文本/数字输入及重复缓存；关系与营业状态事件只读刷新。

### 3.3 M16 确认按钮分发

- **文件**: `Source/Voxel/Private/Core/UI/Build/Modules/M16_ActionButtonsModule.cpp`
- **改动**: `OnConfirmClicked()` 增加工具类型判断逻辑

### 3.4 M8 Create 模式支持

- **文件**: `Source/Voxel/Private/Core/UI/Build/Modules/M8_AreaInfoModule.cpp`
- **改动**: `UpdateFromToolState()` 增加 `CurrentArea == nullptr` 时从 `TmpAreaInfo` 读取数据的分支

### 3.5 UMG 调整

- **M13a 蓝图待迁移**: 父空间与区域信息只读/导航，移除经营状态枚举与 Manager输入。
- **M12a 蓝图待迁移**: 同一侧栏采集名称/类型/业态/经营意图和必要条款，Owner/Manager显示业务解析结果，不提供自由输入。

### 3.6 清理废弃代码

- **文件**: `BuildAreaModifyWidget.h/cpp`
- **操作**: 确认无用后删除或标记为废弃

---

## 4. 验证计划

以下为原计划口径修正，未执行、不作为本次验收：

| 原验证项 | 修正后的操作/判据 |
|--------|----------|
| Create 名称/类型 | 同侧栏收集实体名称、类型/业态、经营意图，Confirm 发组合业务，不只写 Area |
| Create 包围盒 | 空间草稿保持 M4a 范围，按空间业务写 Coverage |
| Create 信息显示 | M8显示草稿空间信息；营业状态只从经营实体派生，不作区域字段 |
| Modify 名称 | 普通资料按 BusinessEntityId 或子 AreaRef 发对应资料业务 |
| Modify 状态 | 移除区域状态 Setter；独立营业按钮只写实体 bOpen |
| Modify 父区域 | 空间导航/只读，不授予或迁移产权/经营关系 |
| 体素编辑/其他工具 | 维持已有几何执行与撤销行为，本次不修改 |

---

## 5. 后续工作

| 工作项 | 说明 |
|--------|------|
| 创建采集与 Confirm 迁移 | 同一侧栏单次组合创建，租户实物写入前有效 Lease/首期 |
| 关系只读展示 | 角色候选只用于有授权的专用租约/任命，不替代普通 Manager Setter |
| 经营/空间分离 | 分别注册与存档；换租户沿用同一实体、删区域不注销实体（已定）；招图外换工待定 |

---

## 6. 链路注意事项

| 问题 | 处理边界 |
|------|----------|
| 草稿字段覆盖 | Controller只改相应草稿字段，经营与空间输入分开 |
| Confirm分发 | 按创建意图及工具分发，经营组合入口不落入普通体素确认 |

---

## 7. 结论

2026-11-04 原位修正文档目标：草稿单一来源，经营/空间正式写入分别归属对应业务，创建使用同侧栏单次组合入口，产权/经营人只读解析。代码仍待迁移，不宣称已实现；本次不修改代码/蓝图、不新增验收。

---

**批准**: 已批准
