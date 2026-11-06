// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/NPCUI/CreatureInfromWidget.h"

#include "CreatureBaseInformWidget.h"
#include "GameplayTagContainer.h"
#include "StatRowWidget.h"
#include "Components/WrapBox.h"
#include "Core/CoreTags.h"
#include "Core/UI/DemandRowWidget.h"
#include "GameManager/GameInstanceManager/ConfigDataSubsystem.h"
#include "Utils/CreatureGeneratorUtils.h"
#include "Core/UI/NPCUI/EventHistoryRowWidget.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"
#include "AI/NPC/VoxelNpcBaseInstance.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Core/UI/TendencyRowWidget.h"
#include "Kismet/GameplayStatics.h"

void UCreatureInfromWidget::UpdateCreatureInfo(const FCreatureGenerationResult& Result)
{
{
    // 0. 获取配置子系统 (用于排序和查名字)
    UConfigDataSubsystem* ConfigSubsystem = nullptr;
    if (UGameInstance* GI = GetGameInstance())
    {
        ConfigSubsystem = GI->GetSubsystem<UConfigDataSubsystem>();
    }

    // 1. 更新基础信息 (名字、种族、职业等)
    if (BaseInformText)
    {
        BaseInformText->UpdateCreatureInfo(Result.TrueData); 
    }

    // 2. 清空所有容器
    if (BaseStateContainer) BaseStateContainer->ClearChildren();
    if (CombatStateContainer) CombatStateContainer->ClearChildren();
    if (WorkStateContainer) WorkStateContainer->ClearChildren();
    if (DynamicContainer) DynamicContainer->ClearChildren(); 
    if (DemandContainer) DemandContainer->ClearChildren();
	if (PersonalityContainer) PersonalityContainer->ClearChildren();

    // ========================================================================
    // [排序辅助] 定义排序 Lambda
    // ========================================================================
    auto SortTagsLambda = [ConfigSubsystem](const FGameplayTag& A, const FGameplayTag& B) -> bool
    {
        int32 OrderA = 9999; 
        int32 OrderB = 9999;

        if (ConfigSubsystem)
        {
            // 注意：TagUIMap 需在 Subsystem 中可访问
            if (const FTagUIDefinition* ConfigA = ConfigSubsystem->TagUIMap.Find(A))
                OrderA = ConfigA->SortOrder;
            if (const FTagUIDefinition* ConfigB = ConfigSubsystem->TagUIMap.Find(B))
                OrderB = ConfigB->SortOrder;
        }

        if (OrderA != OrderB) return OrderA < OrderB; // 权重小的在前
        return A.GetTagName().LexicalLess(B.GetTagName()); // 字母序兜底
    };

    // ========================================================================
    // 3. 更新 静态属性 (FinalAttributes) - 基础/战斗/工作
    // ========================================================================
    if (StatClass)
    {
        // A. 提取 Key 并排序
        TArray<FGameplayTag> SortedAttrKeys;
        Result.InitialState.FinalAttributes.GetKeys(SortedAttrKeys);
        SortedAttrKeys.Sort(SortTagsLambda);

        // B. 遍历
        for (const FGameplayTag& Tag : SortedAttrKeys)
        {
            // [过滤] 1. 跳过动态属性 (血量/饥饿等由下方逻辑处理)
            if (Tag.MatchesTag(FCoreTags::Creature_Attribute_Dynamic)) continue;
        	if (Tag.MatchesTag(FCoreTags::Creature_PersonalityDim)) continue;

            float Value = Result.InitialState.FinalAttributes[Tag];

            // [分类] 放入不同容器
            if (Tag.MatchesTag(FCoreTags::Creature_Attribute_Combat))
            {
                AddStatToContainer(CombatStateContainer, Tag, Value);
            }
            else if (Tag.MatchesTag(FCoreTags::Creature_Attribute_Social)) // 或 Work
            {
                AddStatToContainer(WorkStateContainer, Tag, Value);
            }
            else if (Tag.MatchesTag(FCoreTags::Creature_Attribute_Primary))
            {
                AddStatToContainer(BaseStateContainer, Tag, Value);
            }
            else
            {
                // 兜底放入基础栏
                //AddStatToContainer(BaseStateContainer, Tag, Value);
            }
        }
    }

    // ========================================================================
    // 4. 更新 动态属性 & 需求 (Demands) - 血条/蓝条/饥饿
    // ========================================================================
    // 假设你的动态值都存在 Demands 里面 (包含 Status.Health 和 Demand.Hunger)
    if (DynamicContainer) // 或者 DemandContainer，看你用哪个
    {
        // A. 提取 Key 并排序
        TArray<FGameplayTag> SortedDynamicKeys;
        Result.InitialState.Dynamics.GetKeys(SortedDynamicKeys);
        SortedDynamicKeys.Sort(SortTagsLambda);
    	

        // 定义匹配根节点
        static FGameplayTag StatusRoot = FGameplayTag::RequestGameplayTag(FName("Creature.Attribute.Dynamic")); 
        static FGameplayTag DemandRoot = FGameplayTag::RequestGameplayTag(FName("Creature.Demand"));
    	

        // B. 遍历
        for (const FGameplayTag& Tag : SortedDynamicKeys)
        {
            float CurrentVal = Result.InitialState.Dynamics.FindRef(Tag);
            float MaxVal = Result.InitialState.FinalAttributes.FindRef(Tag); 

            // --- 分支 A: 状态资源 (Creature.Attribute.Dynamic) ---
            if (Tag.MatchesTag(StatusRoot)) 
            {
                if (Result.InitialState.FinalAttributes.Contains(Tag))
                {
                    MaxVal = Result.InitialState.FinalAttributes[Tag];
                }

                // 创建控件 (通常用 DynamicClass / 进度条)
                if (DynamicClass)
                {
                    UDemandRowWidget* Widget = CreateWidget<UDemandRowWidget>(this, DynamicClass);
                    if (Widget)
                    {
                        Widget->UpdateWidgetData(CurrentVal, MaxVal, Tag);
                        DynamicContainer->AddChildToWrapBox(Widget);
                    }
                }
            }
            // --- 分支 B: 需求 (Demand.Hunger) ---
            // 逻辑: 去 CachedDemandRules 找配置的 Max，找不到则默认 100
            else if (Tag.MatchesTag(DemandRoot))
            {
                // 尝试从规则缓存中读取 Max
                if (const FRuntimeDemandRule* Rule = Result.InitialState.CachedDemandRules.Find(Tag))
                {
                    // 注意：前提是你的 FRuntimeDemandRule 结构体里存了 Max 值
                    // 如果没存，可能需要回查 ConfigSubsystem 或者默认 100
                    // MaxVal = Rule->Max; 
                    MaxVal = 100.0f; // 暂时默认 100
                }

                // 创建控件 (通常用 DemandClass / 文本或细条)
                if (DemandClass)
                {
                    UDemandRowWidget* Widget = CreateWidget<UDemandRowWidget>(this, DemandClass);
                    if (Widget)
                    {
                        Widget->UpdateWidgetData(CurrentVal, MaxVal, Tag);
                        // 如果你有专门的 DemandContainer 就放那里，否则放 DynamicContainer
                        if (DemandContainer) DemandContainer->AddChildToWrapBox(Widget);
                        else DynamicContainer->AddChildToWrapBox(Widget);
                    }
                }
            }
        }
    }
	// ========================================================================
	// [新增] 更新 性格模型 (Personality Model)
	// ========================================================================
	if (PersonalityContainer && TendencyRowClass)
	{
		static FGameplayTag PDimRoot = FGameplayTag::RequestGameplayTag(FName("Creature.PersonalityDim"));

		// A. 从 FinalAttributes 中筛选 Key
		TArray<FGameplayTag> SortedPersonalityKeys;
    
		// 遍历 FinalAttributes 里的所有属性
		for (const auto& Pair : Result.InitialState.FinalAttributes)
		{
			if (Pair.Key.MatchesTag(PDimRoot))
			{
				SortedPersonalityKeys.Add(Pair.Key);
			}
		}

		// B. 排序
		SortedPersonalityKeys.Sort(SortTagsLambda);

		// 调试日志
		//UE_LOG(LogTemp, Warning, TEXT("[UI] Personality (FinalAttrs) Count: %d | CreatureName: %s"),
		//    SortedPersonalityKeys.Num(),
		//    *Result.TrueData.RealName);

		// C. 遍历生成 Widget
		for (const FGameplayTag& Tag : SortedPersonalityKeys)
		{
			// [核心修改]：直接从 FinalAttributes 获取数值
			// 因为前面筛选过 Key，这里一定存在，可以用 FindRef 或 []
			float Val = Result.InitialState.FinalAttributes.FindRef(Tag);

			UTendencyRowWidget* RowWidget = CreateWidget<UTendencyRowWidget>(this, TendencyRowClass);
			if (RowWidget)
			{
				RowWidget->UpdateTendencyData(Tag, Val, ConfigSubsystem);
				PersonalityContainer->AddChildToWrapBox(RowWidget);
			}
		}
	}
	if (PreviewActorClass)
	{
		// 1. 查找场景里的 BP_CharStudio
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), PreviewActorClass, FoundActors);
    
		if (FoundActors.Num() > 0)
		{
			// 2. 获取 Actor 实例
			if (AVoxelNpcBaseInstance* PreviewNPC = Cast<AVoxelNpcBaseInstance>(FoundActors[0]))
			{
				// A. 初始化数据 (这一步会 RebuildBodyVisuals，动态生成新的手/脚组件)
				PreviewNPC->InitializeFromGenerationResult(Result);

				// B. [核心代码] 查找下属的 SceneCapture 组件
				USceneCaptureComponent2D* CaptureComp = PreviewNPC->FindComponentByClass<USceneCaptureComponent2D>();

				if (CaptureComp)
				{
					// C. 设置渲染模式为 "只渲染白名单" (非常重要，否则它会渲染整个世界或什么都不渲染)
					CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

					// D. 更新白名单
					CaptureComp->ShowOnlyActors.Empty(); // 先清空，防止重复添加
					CaptureComp->ShowOnlyActors.Add(PreviewNPC); // 将 Actor 自己加进去

					// 解释：将 Actor 加入 ShowOnlyActors 后，引擎会自动递归渲染它身上
					// 所有 Attach 的子组件（包括 MainMesh 和你动态生成的 BodyParts）
                
					// E. (可选) 如果你的 Capture 不是 "Every Frame"，需要手动刷新一次
					// CaptureComp->CaptureScene();
				}
			}
		}
	}
    // 5. 更新事件历史记录
    UpdateEventHistory(Result);
}
}

// 辅助函数实现
void UCreatureInfromWidget::AddStatToContainer(UWrapBox* TargetContainer, FGameplayTag Tag, float Value)
{
	if (!TargetContainer || !StatClass) return;

	UStatRowWidget* StatWidget = CreateWidget<UStatRowWidget>(this, StatClass);
	if (StatWidget)
	{
		StatWidget->UpdateStat(Tag, Value);
		TargetContainer->AddChildToWrapBox(StatWidget);
	}

}

// 事件历史相关函数实现
void UCreatureInfromWidget::UpdateEventHistory(const FCreatureGenerationResult& Result)
{

	// 显示加载状态
	ShowLoadingState();

	// 异步处理事件历史数据，模拟网络请求或复杂计算
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Result]() {
		// 保存缓存
		//CachedGenerationResult = Result;
		
		// 获取配置子系统
		UConfigDataSubsystem* ConfigSubsystem = nullptr;
		if (UGameInstance* GI = GetGameInstance())
		{
			ConfigSubsystem = GI->GetSubsystem<UConfigDataSubsystem>();
		}

		// 清空事件历史容器
		if (EventHistoryContainer)
		{
			EventHistoryContainer->ClearChildren();
		}

		// 检查配置和数据有效性
		if (!ConfigSubsystem)
		{
			ShowErrorState(TEXT("配置系统初始化失败，无法加载事件历史"));
			return;
		}

		if (!EventHistoryRowClass)
		{
			ShowErrorState(TEXT("未配置事件历史行Widget类"));
			return;
		}

		// 复制事件历史数据并按年龄降序排序
		TArray<FLifeHistoryEntry> SortedHistory = Result.TrueData.LifeHistory;
		auto SortByAgeDescLambda = [](const FLifeHistoryEntry& A, const FLifeHistoryEntry& B) -> bool
		{
			return A.OccuredAge > B.OccuredAge;
		};
		SortedHistory.Sort(SortByAgeDescLambda);

		// 创建事件历史行
		for (const FLifeHistoryEntry& Entry : SortedHistory)
		{
			UEventHistoryRowWidget* EventRowWidget = CreateWidget<UEventHistoryRowWidget>(this, EventHistoryRowClass);
			if (EventRowWidget)
			{
				EventRowWidget->UpdateEventData(Entry, ConfigSubsystem, Result.TrueData.RealName);
				if (EventHistoryContainer)
				{
					EventHistoryContainer->AddChildToWrapBox(EventRowWidget);
				}
			}
		}

		// 显示事件历史内容
		ShowEventHistoryContent();

		// 标记加载完成
		// bIsLoading = false;
	}, 0.5f, false); // 延迟0.5秒，模拟异步加载
}

void UCreatureInfromWidget::ShowLoadingState()
{

	// 隐藏内容和错误提示
	if (EventHistoryContainer)
	{
		EventHistoryContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCreatureInfromWidget::ShowEventHistoryContent()
{

	// 显示内容
	if (EventHistoryContainer)
	{
		EventHistoryContainer->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCreatureInfromWidget::ShowErrorState(const FString& ErrorMessage)
{
	if (EventHistoryContainer)
	{
		EventHistoryContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}
