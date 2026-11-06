#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "VoxelEntityDetailPanel.generated.h"

class AVoxelAreaBaseInstance;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UVoxelEntityDetailPanel;
class UVoxelEntitySubAreaRowWidget;
class UVoxelAreaBusinessEditDialog;

/**
 * 实体详情面板（主 HUD 右列，环境组下方）
 *
 * 区域先行：选中区域时显示区域详情卡（图标/名称/类型/状态/范围/权限/管理者/所有人/子区域/操作），
 * 无选中时显示空态。生物/家居卡后续按同槽位扩展。
 *
 * 数据流：只订阅 UVoxelPlayerStateCacheComponent::OnSelectedEntityChanged 刷新；
 * 写操作（导航/清除/装修/编辑业务入口）全部走业务逻辑层方法，不直改数据。
 * 设计文档：/VoxelDesign/02-系统设计/实体详情面板-系统设计.md
 */
UCLASS()
class VOXEL_API UVoxelEntityDetailPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 由子区域/父链导航调用：切换选中到目标区域 */
	void NavigateToArea(int32 AreaId);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// === 空态 ===

	/** 空态提示（无选中时显示） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> EmptyStateText;

	/** 区域详情卡容器（选中区域时显示，与空态互斥） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UWidget> AreaCard;

	// === 卡片头 ===

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UImage> AreaIconImage;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> AreaNameText;

	/** 副标题：类型显示名 · 状态 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> AreaSubTitleText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> CloseButton;

	// === 父链面包屑（无父区域时整行隐藏） ===

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UWidget> ParentAreaRow;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> ParentAreaButton;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> ParentAreaText;

	// === 属性区（只展示后端已有字段） ===

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> StatusValueText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> RangeValueText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> SizeValueText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> ManagerValueText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> OwnerValueText;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UTextBlock> AuthorityValueText;

	// === 子区域列表（无子区域时整段隐藏；行由 C++ 动态生成） ===

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UWidget> SubAreaSection;

	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UVerticalBox> SubAreaListBox;

	/** 子区域行控件类（控件树与样式在 WBP_EntitySubAreaRow，C++ 只填数据） */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelEntitySubAreaRowWidget> SubAreaRowClass;

	/** 区域业务数据编辑弹窗类（由「编辑业务」按钮 PushModal 打开，Context = 目标区域） */
	UPROPERTY(EditDefaultsOnly, Category = "UI|Class")
	TSubclassOf<UVoxelAreaBusinessEditDialog> AreaEditDialogClass;

	// === 操作行 ===

	/** 编辑业务：打开业务数据表单（表单本体另案，当前为入口占位） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> EditBusinessButton;

	/** 装修此区域：进入建造模式并把建造上下文定位到该区域 */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UButton> DecorateButton;

	/** 浮动业务按钮组容器（外凸于卡片右上角；与 AreaCard 显隐联动，避免空态下残留悬浮按钮） */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	TObjectPtr<UWidget> ActionGroupBox;

private:
	UFUNCTION()
	void HandleSelectedEntityChanged(EVoxelSelectedEntityType NewType, int32 NewEntityId);

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnParentAreaClicked();

	UFUNCTION()
	void OnEditBusinessClicked();

	UFUNCTION()
	void OnDecorateClicked();

	/** 按当前选中状态全量刷新（无选中 → 空态；区域 → 填卡） */
	void RefreshFromSelection();

	/** 用区域数据填充详情卡 */
	void FillAreaCard(AVoxelAreaBaseInstance* Area);

	/** 订阅的区域基础数据变更（编辑弹窗保存后刷新详情卡） */
	UFUNCTION()
	void HandleAreaBaseInfoChanged(AVoxelAreaBaseInstance* Area);

	/** 重建子区域行 */
	void RebuildSubAreaList(AVoxelAreaBaseInstance* Area);

	/** 子区域行点击 → 导航到目标区域 */
	UFUNCTION()
	void HandleSubAreaRowClicked(int32 AreaId);

	/** CreatureId → 显示名（无效 Id → 「—」，找不到生物 → 「#Id」） */
	FText ResolveCreatureName(int32 CreatureId) const;

	UVoxelPlayerStateCacheComponent* GetSelectionComp() const;

	/** 当前展示的父区域 Id（OnParentAreaClicked 无参数，靠它寻址） */
	int32 CurrentParentAreaId = -1;

	/** 已订阅 OnAreaBaseInfoChanged 的区域（切换/销毁时解绑） */
	UPROPERTY()
	TObjectPtr<AVoxelAreaBaseInstance> BoundArea;
};
