# ADR-001: AREA 系统 M13 前端数据持久化方案

## 状态

已接受

## 日期

2026-06-07（创建）；2026-11-04（仅本规则冲突原位修订）

## 上下文

区域编辑草稿曾因模块本地缓存与 STATE 不同步而在切换时丢失。本记录只保留草稿单一来源原则，修正与经营实体/空间分离及连续组合创建冲突的设计，不声明代码已迁移。

## 决策

### 1. 草稿同步与正式持久化

- 输入经 Controller 的草稿业务更新 STATE，模块只显示/采集，预览与 UI 订阅草稿变化；STATE 不是产权、经营关系或合同的正式存储。
- 同一建造侧栏拉范围/摆模板预览 → 填名称、类型/业态、经营意图等必要信息 → 单次确认 `CreateBusinessWithArea`。请求含 OperatorId、BusinessKind、Name、BusinessIntent、ParentBusinessEntityId、空间草稿等，具体 class 待定。
- 确认走 Controller → 业务门面 → 实体/区域存储 → 事件 → UI，创建实体+空间+经营关系+必要 Lease/首期。有效 Lease 先于租户建筑/家具真写入；不要求先建区域后跳详情补建生意，不要求手动绑定。
- M13a 纯导航只作为待迁移代码事实保留，不是最终创建信息采集的约束。

### 2. 数据归属

- 经营实体是资产和业务唯一所有者，商圈/店铺/图外生意共用基础模型、按类型组合模块，存 `Global.BusinessEntities`。
- BusinessEntityId 全局稳定，03 逻辑类型 FBusinessEntityId 底层待定，不自选 GUID/int32；ShopId 为其别名。AreaRef={SceneId,AreaId} 是可选空间组件的引用；子区域只创建空间并关联现有主体。
- 区域只保存范围、父子、定位和准入约束，不在 `FAreaModificationData` 或区域档中新增 Owner/Manager 权属写入、经营关系、租约、bOpen、目录、订单、声誉。经营与空间注册/持久化分开不镜像。
- Area.Type.Shop.* 等既有配置 Tag 作分类，具体迁移待定；物品/结构产权日志保留独立来源，不因空间包含转移租户资产。

### 3. Owner / Manager 与合同

- Owner/Manager 只读显示实体有效经营关系和授权的解析结果，通过 Creature 查询显示名；无父区域或当前创建者身份均不自动授予产权或经营权。
- 创建、出租、任命、产权变更经各自业务校验有效授权与条款，不允许输入数字 ID 或普通下拉/Setter 来取得权利。
- 普通资料只编辑名称/LOGO等，合同变化不经普通保存；子区域类型由空间业务校验，不存在子区域 Manager 字段。
- 住客只是限时房间/设施使用者，门查询有效授权，不用客房 Manager 钥匙。

### 4. 营业状态与版本边界

- 创建默认 bOpen=false，不等于开业；开业/停业单独手动写 bOpen 并入档，bOperating=bOpen&&有效时段 纯派生，无自动资格复检。
- 初始两钱包/业务账户人物持有。V1 只预置商圈玩家标准租杂货店；NPC 空店/模板邀请同一连续面板确认候选同意、条款与产权授权，先 Lease 再真落位，完整 NPC 模板招商/客房后续。
- 换租户沿用同一经营实体、删区域不注销实体（2026-11-06 已定）；招图外换工待定。

### 5. 模块缓存

移除与 STATE 重复的输入缓存；可编辑草稿读取 STATE，正式资料读取对应实体/空间存储。M13a 导航不提交经营关系或状态；创建草稿的延续由 Controller 草稿状态负责，不要求玩家切到详情补录。

## 后果

### 正面
- 草稿单一来源与预览复用保留，切换展示模块不靠本地缓存保留业务输入。
- 同一面板采集、正式数据按归属分别提交；空间引用不兼任经营身份。

### 迁移影响
- 需要迁移创建采集/Confirm分发，移除区域 Owner/Manager/营业状态旁路写入；尚未实施，不把文档修订当作代码完成。

## 待办

- [ ] 创建草稿与空间修改请求分离，不加入 Owner/Manager 权属可写字段。
- [ ] 创建确认接组合业务门面，必要有效 Lease/首期先于租户真实落位。
- [ ] 普通资料保存仅名称/LOGO等，合同走专用业务。
- [ ] M13a 导航与连续创建面板职责分离，草稿实时同步 STATE。

## 相关文件

- `Source/Voxel/Private/Data/Area/VoxelAreaBase.h` — FAreaModificationData 定义
- `Source/Voxel/Private/GameManager/WorldManager/VoxelAreaWorldSubsystem.cpp` — 创建/修改逻辑
- `Source/Voxel/Private/Area/VoxelAreaBaseInstance.cpp` — UpdateBaseAreaInfo
- `Source/Voxel/Private/Core/UI/Build/Modules/M13a_AreaBasicInfoEditModule.h/cpp` — M13 前端
- `Source/Voxel/Private/Core/UI/Build/ToolDetailPanel.cpp` — 确认分发逻辑

## 备注

角色候选查询只服务于有效授权下的 Lease/任命等专用业务，不用于普通资料或区域表单直接设 Manager。本文于 2026-11-04 原位修订冲突规则；相关代码/蓝图仍需按目标链路迁移，不在本次文档编辑中实现。
