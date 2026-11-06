// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldObjectPoolSubsystem.h"
void UObjectPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
}
void UObjectPoolSubsystem::Deinitialize()
{
    // 清理所有池
    for (auto& Pair : Pools) {
        Pair.Value->Shrink(Pair.Value->NumTotal());
    }
    Pools.Empty();
    
    // // 注销控制台命令
    // for (auto* Command : ConsoleCommands) {
    //     IConsoleManager::Get().UnregisterConsoleObject(Command);
    // }
    // ConsoleCommands.Empty();
}
void UObjectPoolSubsystem::PreWarm(FName PoolName, int32 Count)
{
    if (TSharedPtr<FObjectPoolBase>* PoolPtr = Pools.Find(PoolName)) {
        (*PoolPtr)->PreWarm(Count);
    }
}
void UObjectPoolSubsystem::AdjustPoolSize(FName PoolName, int32 DesiredSize)
{
    if (TSharedPtr<FObjectPoolBase>* PoolPtr = Pools.Find(PoolName)) {
        const int32 CurrentSize = (*PoolPtr)->NumTotal();
        
        if (CurrentSize < DesiredSize) {
            // 扩展池
            (*PoolPtr)->PreWarm(DesiredSize - CurrentSize);
        } else if (CurrentSize > DesiredSize) {
            // 收缩池
            (*PoolPtr)->Shrink(CurrentSize - DesiredSize);
        }
    }
}
FObjectPoolStats UObjectPoolSubsystem::GetPoolStats(FName PoolName) const
{
    FObjectPoolStats Stats;
    Stats.PoolName = PoolName;
    
    if (const TSharedPtr<FObjectPoolBase>* PoolPtr = Pools.Find(PoolName)) {
        Stats.TotalCount = (*PoolPtr)->NumTotal();
        Stats.ActiveCount = (*PoolPtr)->NumActive();
        Stats.FreeCount = (*PoolPtr)->NumFree();
    }
    
    return Stats;
}
TArray<FObjectPoolStats> UObjectPoolSubsystem::GetAllPoolStats() const
{
    TArray<FObjectPoolStats> StatsArray;
    
    for (const auto& Pair : Pools) {
        FObjectPoolStats Stats;
        Stats.PoolName = Pair.Key;
        Stats.TotalCount = Pair.Value->NumTotal();
        Stats.ActiveCount = Pair.Value->NumActive();
        Stats.FreeCount = Pair.Value->NumFree();
        StatsArray.Add(Stats);
    }
    
    return StatsArray;
}
// 添加缺失的DumpPoolStats实现
void UObjectPoolSubsystem::DumpPoolStats()
{
    UE_LOG(LogTemp, Display, TEXT("==== Object Pool Statistics ===="));
    
    for (const auto& Pair : Pools) {
        UE_LOG(LogTemp, Display, TEXT("Pool [%s]: Total=%d, Active=%d, Free=%d"), 
            *Pair.Key.ToString(), 
            Pair.Value->NumTotal(),
            Pair.Value->NumActive(),
            Pair.Value->NumFree());
    }
}