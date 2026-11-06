// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/UI/Tool/ToolIconButtonWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Core/CoreTags.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Data/Creature/CreatureDataDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "Voxel/VoxelPlayerController.h"
#include "Voxel/Private/Core/VoxelGameInstance.h"

UToolIconButtonWidget::UToolIconButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ToolTag(FGameplayTag::EmptyTag)
	, bSelected(false)
{
}

void UToolIconButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 绑定自检：BindWidget 型类若蓝图未放置控件会静默失败，必须显式报错
	UE_LOG(LogTemp, Log, TEXT("[ToolIcon] NativeOnInitialized %s：Button=%s IconImage=%s ToolNameText=%s"),
		*GetName(),
		Button ? *Button->GetName() : TEXT("NULL"),
		IconImage ? *IconImage->GetName() : TEXT("NULL"),
		ToolNameText ? *ToolNameText->GetName() : TEXT("NULL"));

	if (!Button) UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s Button 未绑定（蓝图缺少 Button 控件或名字不符）"), *GetName());
	if (!IconImage) UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s IconImage 未绑定"), *GetName());
	if (!ToolNameText) UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s ToolNameText 未绑定"), *GetName());

	if (Button)
	{
		Button->OnClicked.AddDynamic(this, &UToolIconButtonWidget::OnButtonClicked);
	}
}

void UToolIconButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 可见性自检：控件存在但被蓝图折叠/隐藏时文字独显，这里显式暴露
	UE_LOG(LogTemp, Log, TEXT("[ToolIcon] NativeConstruct %s（%s）可见性：Button=%s IconImage=%s ToolNameText=%s Self=%s"),
		*GetName(), *ToolTag.ToString(),
		Button ? (*UEnum::GetValueAsString(Button->GetVisibility())) : TEXT("无控件"),
		IconImage ? (*UEnum::GetValueAsString(IconImage->GetVisibility())) : TEXT("无控件"),
		ToolNameText ? (*UEnum::GetValueAsString(ToolNameText->GetVisibility())) : TEXT("无控件"),
		*UEnum::GetValueAsString(GetVisibility()));
}

void UToolIconButtonWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 几何诊断：第 2 帧执行。首帧时本控件的 MyGeometry 已分配（tick 参数），
	// 但子控件的 GetCachedGeometry() 是上一次 paint 的缓存——新建 Widget 首帧
	// 子控件尚未 paint，读到的必是默认值 0x0，属误报。第 2 帧时首帧 paint 已完成。
	++TickFrameCount;
	if (TickFrameCount == 2)
	{
		const FVector2D SelfSize = MyGeometry.GetLocalSize();
		const FVector2D BtnSize = Button ? Button->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
		const FVector2D IconSize = IconImage ? IconImage->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
		const FVector2D TextSize = ToolNameText ? ToolNameText->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;

		UE_LOG(LogTemp, Log, TEXT("[ToolIcon] 布局几何 %s（%s）：Self=%dx%d Button=%dx%d Icon=%dx%d Text=%dx%d"),
			*GetName(), *ToolTag.ToString(),
			(int)SelfSize.X, (int)SelfSize.Y,
			(int)BtnSize.X, (int)BtnSize.Y,
			(int)IconSize.X, (int)IconSize.Y,
			(int)TextSize.X, (int)TextSize.Y);

		if (Button && BtnSize.X < 1 && BtnSize.Y < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s Button 实际尺寸为 0（按钮背景不可见，仅剩文字），检查蓝图插槽布局"), *ToolTag.ToString());
		}
		if (IconImage && (IconSize.X < 1 || IconSize.Y < 1))
		{
			UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s IconImage 实际尺寸为 0（图标不可见），检查蓝图插槽布局"), *ToolTag.ToString());
		}
	}
}

void UToolIconButtonWidget::SetToolTag(FGameplayTag InToolTag)
{
	ToolTag = InToolTag;

	UE_LOG(LogTemp, Log, TEXT("[ToolIcon] SetToolTag %s → %s"), *GetName(), *InToolTag.ToString());

	// 从 ConfigDataSubsystem 获取 Tag UI 定义
	if (UVoxelGameInstance* GameInstance = GetWorld()->GetGameInstance<UVoxelGameInstance>())
	{
		if (UConfigDataSubsystem* CDS = GameInstance->GetSubsystem<UConfigDataSubsystem>())
		{
			if (const FTagUIDefinition* UIDef = CDS->TagUIMap.Find(ToolTag))
			{
				UE_LOG(LogTemp, Log, TEXT("[ToolIcon] TagUIMap 命中 %s：DisplayName='%s' Icon=%s UIColor=%s"),
					*ToolTag.ToString(), *UIDef->DisplayName.ToString(),
					UIDef->Icon.IsValid() ? *UIDef->Icon.ToString() : TEXT("未配置"),
					*UIDef->UIColor.ToString());

				if (IconImage)
				{
					// TSoftObjectPtr 必须同步加载后才能取到纹理（IsValid 只检查引用是否设置）
					if (UTexture2D* LoadedIcon = UIDef->Icon.LoadSynchronous())
					{
						IconImage->SetBrushFromTexture(LoadedIcon);
						UE_LOG(LogTemp, Log, TEXT("[ToolIcon] %s 图标加载成功：%s"), *ToolTag.ToString(), *LoadedIcon->GetName());
					}
					else
					{
						// 无图标兜底：用 Tag UI 配置的 UIColor 渲染纯色块
						// UImage 默认 Brush 为白色方块，tint 后即色块
						IconImage->SetBrushTintColor(FSlateColor(UIDef->UIColor));
						UE_LOG(LogTemp, Warning, TEXT("[ToolIcon] %s 图标未配置或加载失败，使用 UIColor 色块兜底"), *ToolTag.ToString());
					};
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s IconImage 未绑定，无法设置图标"), *ToolTag.ToString());
				}


				// 设置工具名称
				if (ToolNameText)
				{
					ToolNameText->SetText(UIDef->DisplayName);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[ToolIcon] %s ToolNameText 未绑定，无法设置名称"), *ToolTag.ToString());
				}

				// TODO: 可以设置 Tooltip
				// Button->SetToolTipText(UIDef->Description);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[ToolIcon] TagUIMap 未找到 %s（TagUIDef 配置表缺失该行），按钮无名称无图标"), *InToolTag.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[ToolIcon] ConfigDataSubsystem 获取失败，%s 按钮数据未设置"), *InToolTag.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ToolIcon] GameInstance 获取失败，%s 按钮数据未设置"), *InToolTag.ToString());
	}
	SetSelected(false);
}

void UToolIconButtonWidget::SetSelected(bool bIsSelected)
{
	bSelected = bIsSelected;
	UpdateVisualState();
}

void UToolIconButtonWidget::OnButtonClicked()
{
	OnToolButtonClicked.Broadcast(this);
}

void UToolIconButtonWidget::UpdateVisualState()
{
	// TODO: 更新视觉状态（边框、背景色等）;
	// if (SelectedBorder)
	// {
	// 	SelectedBorder->SetVisibility(bSelected ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// }

	// 可以根据选中状态设置不同的颜色或样式
	if (IconImage)
	{
		// 选中状态可以有不同的颜色
		IconImage->SetColorAndOpacity(bSelected ? FLinearColor::White : FLinearColor::Gray);
	}
}

FGameplayTag UToolIconButtonWidget::GetParentTag(FGameplayTag ChildTag) const
{
	// 获取所有父标签
	FGameplayTagContainer ParentsContainer = ChildTag.GetGameplayTagParents();
	TArray<FGameplayTag> Parents = ParentsContainer.GetGameplayTagArray();
	// 找到直接父节点（第二层）
	for (const FGameplayTag& Parent : Parents)
	{
		if (Parent.MatchesTag(FCoreTags::Tool) && Parent != ChildTag&& Parent != FCoreTags::Tool)
		{
			return Parent;
		}
	}
	return FGameplayTag::EmptyTag;
}

