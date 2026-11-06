// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/VoxelItemListPanelWidget.h"
#include "Components/WrapBox.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/Enum/GameStateEnum.h"
#include "Core/UI/Build/BuildSlotWidget.h"
#include "Data/Item/VoxelItemBase.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Voxel/VoxelGameMode.h"
#include "Voxel/VoxelPlayerController.h"


void UVoxelItemListPanelWidget::UpdateList()
{
    // 0. 初始化清理
    BuildListWrapBox->ClearChildren();
    // 动态计算 WrapBox 高度/大小 (根据你的逻辑)
    BuildListWrapBox->SetWrapSize(PageSize * 128 + 20);

    // =========================================================
    // [定义中间结构] 适配器
    // 无论来源是 Area 还是 Block，都统一转成这个格式
    // =========================================================
    struct FUnifiedRenderData
    {
        FGameplayTag Tag;
        TObjectPtr<UTexture2D> Icon;
        // 如果你的 CreateBuildSlot 支持颜色或模型预览，也可以加在这里
        // FLinearColor Color;
    };

    TArray<FUnifiedRenderData> RenderQueue;

    // 1. 获取子系统和控制器
    auto* GI = GetWorld()->GetGameInstance();
    if (!GI) return;
    auto* CDS = GI->GetSubsystem<UConfigDataSubsystem>();

    auto* PC = GetOwningPlayer<AVoxelPlayerController>();
    if (!CDS || !PC || !PC->StateCacheComp)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateList: Subsystem or PlayerController not ready"));
        return;
    }

    // 2. 根据工具类型分流，填充 RenderQueue
    FGameplayTag CurrentToolTag = PC->StateCacheComp->GetCurrentToolTag();

    // 区域编辑工具
    if (CurrentToolTag.MatchesTag(FCoreTags::Tool_AreaEdit))
    {
        // [Area 逻辑]
        // 获取所有区域类型 (假设你在 CDS 里加了 GetAreaTypeArray)
        TArray<const FAreaTypeBase*> AreaList;
		CDS->AreaMap.GenerateValueArray(AreaList);

        for (const auto* Area : AreaList)
        {
            // 过滤抽象类或无效数据
            if (Area && !Area->bAbstract)
            {
                // 转换：直接读取 Area 的 Icon
                RenderQueue.Add({ Area->AreaTypeTag, Area->AssetFragment.Icon });
            }
        }
    }
    // 方块编辑工具（包括单体素和几何体编辑）
    else if (CurrentToolTag == FCoreTags::Tool_Edit_SingleVoxel ||
             CurrentToolTag == FCoreTags::Tool_Edit_SimpleGeometry)
    {
        // [Block 逻辑] 统一用基类查询，避免不安全的子类型 static_cast
        TArray<FVoxelItemBase*> BlockList = CDS->GetItemTypeArrayByCategory<FVoxelItemBase>(FCoreTags::Item_Type_Block);

        for (const auto* Item : BlockList)
        {
            if (Item)
            {
                RenderQueue.Add({ Item->ItemTag, Item->AssetFragment.Icon });
            }
        }
    }
    // 家具放置工具
    else if (CurrentToolTag == FCoreTags::Tool_Edit_Furniture)
    {
        // [Furniture 逻辑] 统一用基类查询
        TArray<FVoxelItemBase*> FurnitureList = CDS->GetItemTypeArrayByCategory<FVoxelItemBase>(FCoreTags::Item_Type_BuildItem);

        for (const auto* Item : FurnitureList)
        {
            if (Item)
            {
                RenderQueue.Add({ Item->ItemTag, Item->AssetFragment.Icon });
            }
        }
    }
    // 模板放置工具
    else if (CurrentToolTag == FCoreTags::Tool_Edit_Template)
    {
        // [Template 逻辑] 查询运行时模板列表
        TArray<FVoxelTemplate*> TemplateList = CDS->GetAllTemplates();

        for (const auto* Tmpl : TemplateList)
        {
            if (Tmpl)
            {
                RenderQueue.Add({ Tmpl->ItemTag, Tmpl->AssetFragment.Icon });
            }
        }
    }

    // 3. 统一分页计算 (针对 RenderQueue)
    int32 TotalItems = RenderQueue.Num();

    // 安全计算页数 (避免除以0)
    if (PageSize <= 0) PageSize = 10;
    MaxPage = FMath::CeilToInt(static_cast<float>(TotalItems) / PageSize);

    // 修正当前页码 (防止切页签时页码溢出)
    if (CurPageNum >= MaxPage)
    {
        CurPageNum = FMath::Max(0, MaxPage - 1);
    }

    int32 StartIndex = CurPageNum * PageSize;
    int32 EndIndex = FMath::Min(StartIndex + PageSize, TotalItems);

    // 4. 统一渲染循环
    for (int32 i = StartIndex; i < EndIndex; i++)
    {
        const FUnifiedRenderData& Data = RenderQueue[i];

        // 调用创建插槽的函数
        // 假设 CreateBuildSlot 接受 Tag, Icon, Count
        UUserWidget* SlotWidget = CreateBuildSlot(Data.Tag, Data.Icon, 0);

        if (SlotWidget)
        {
            BuildListWrapBox->AddChildToWrapBox(SlotWidget);
        }
    }
}

UBuildSlotWidget* UVoxelItemListPanelWidget::CreateBuildSlot(FGameplayTag InputTag,UTexture2D* InputIcon,int InputQ)
{
	UBuildSlotWidget* S = CreateWidget<UBuildSlotWidget>(this, BuildSlotWidgetClass);
	if (S)
	{
		S->InitSlotData(InputTag,InputIcon);
		// 注意：不在此处 AddToViewport，由调用者通过 AddChildToWrapBox 添加到父容器
	}
	return S;
}
void UVoxelItemListPanelWidget::PageButtonActionCheck(int PageChangeIndex)
{
	if (PageChangeIndex+CurPageNum<0)
	{
		CurPageNum=MaxPage;
		
	}
	else if (PageChangeIndex+CurPageNum>MaxPage)
	{
		CurPageNum=0;
	}
	else
	{
		CurPageNum=CurPageNum+PageChangeIndex;
	}
	UpdateList();
}

void UVoxelItemListPanelWidget::ResetPage()
{
	
	CurPageNum=0;
	MaxPage=1;
	UpdateList();
}

