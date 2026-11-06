// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Creature/CreatureDataDefinitions.h"
void UBodyTypeDefinition::GetOrderedSlots(TArray<FGameplayTag>& OutSlots) const
{
	OutSlots.Empty();

	// 1. 安全检查：必须要有根节点定义
	if (!RootSlot.IsValid()) 
	{
		return;
	}

	// 2. 准备一个递归 Lambda 函数
	// 逻辑：加入自己 -> 找所有以自己为父节点的孩子 -> 递归
	TFunction<void(FGameplayTag)> RecurseNode = [&](FGameplayTag CurrentSlotTag)
	{
		// A. 把当前部位加入列表
		OutSlots.Add(CurrentSlotTag);

		// B. 遍历整个 Map，寻找所有 "Parent == Current" 的部位
		// (虽然每次都遍历 Map 效率是 O(N^2)，但因为身体部位通常少于 50 个，
		// 这种微秒级的开销完全可以忽略，比维护额外的树结构更简单)
		for (const auto& Pair : BodyStructure)
		{
			const FBodyStructureEntry& Entry = Pair.Value;
            
			if (Entry.ParentSlot == CurrentSlotTag)
			{
				// 找到了孩子，继续递归
				RecurseNode(Pair.Key);
			}
		}
	};

	// 3. 从根节点开始启动递归
	// 注意：我们要确保 Root 也在 BodyStructure 里，或者至少从 Root 开始找
	// 如果 Root 本身也是一个需要生成的部件（比如 Chest），它应该在 Structure 里
	if (BodyStructure.Contains(RootSlot))
	{
		RecurseNode(RootSlot);
	}
	else
	{
		// 特殊情况处理：如果 RootSlot 只是一个标记，不在 Map 里
		// 直接开始找它的孩子
		// RecurseNode(RootSlot); // 这取决于你的 Chest 是否在 Map 中配置
	}
}