// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldObjectPoolSubsystem.generated.h"

// 对象池统计信息
USTRUCT(BlueprintType)
struct FObjectPoolStats
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FName PoolName;
    
    UPROPERTY(BlueprintReadOnly)
    int32 TotalCount = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 ActiveCount = 0;
    
    UPROPERTY(BlueprintReadOnly)
    int32 FreeCount = 0;
};
UCLASS()
class UObjectPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
    
public:
    // 注册对象池（模板实现放在头文件中）
    template<typename T>
    void RegisterPool(FName PoolName, TFunction<T*()> CreateFunc, TFunction<void(T*)> ResetFunc, int32 InitialSize = 0)
    {
        if (Pools.Contains(PoolName)) return;
        
        TSharedPtr<FObjectPoolBase> NewPool = MakeShared<TObjectPool<T>>(
            CreateFunc, ResetFunc, InitialSize
        );
        
        Pools.Add(PoolName, NewPool);
    }
    
    // 获取对象（模板实现放在头文件中）
    template<typename T>
    T* Acquire(FName PoolName)
    {
        if (TSharedPtr<FObjectPoolBase>* PoolPtr = Pools.Find(PoolName)) {
            return static_cast<T*>((*PoolPtr)->Acquire());
        }
        return nullptr;
    }
    
    // 归还对象（模板实现放在头文件中）
    template<typename T>
    void Release(FName PoolName, T* Object)
    {
        if (TSharedPtr<FObjectPoolBase>* PoolPtr = Pools.Find(PoolName)) {
            (*PoolPtr)->Release(Object);
        }
    }
    
    // 预生成对象
    void PreWarm(FName PoolName, int32 Count);
    
    // 调整池大小
    void AdjustPoolSize(FName PoolName, int32 DesiredSize);
    
    // 获取池统计
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    FObjectPoolStats GetPoolStats(FName PoolName) const;
    
    // 获取所有池统计
    UFUNCTION(BlueprintCallable, Category = "Object Pool")
    TArray<FObjectPoolStats> GetAllPoolStats() const;
    
    // 调试命令（添加实现声明）
    UFUNCTION(Exec, Category = "Object Pool")
    void DumpPoolStats();
protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
private:
    // 对象池基类
    class FObjectPoolBase
    {
    public:
        virtual ~FObjectPoolBase() = default;
        virtual void* Acquire() = 0;
        virtual void Release(void* Object) = 0;
        virtual int32 NumTotal() const = 0;
        virtual int32 NumActive() const = 0;
        virtual int32 NumFree() const = 0;
        virtual void PreWarm(int32 Count) = 0;
        virtual void Shrink(int32 Count) = 0;
    };
    
    // 模板对象池实现（在头文件中定义）
    template<typename T>
    class TObjectPool : public FObjectPoolBase
    {
    public:
        TObjectPool(TFunction<T*()> InCreateFunc, TFunction<void(T*)> InResetFunc, int32 InitialSize = 0)
            : CreateFunc(InCreateFunc), ResetFunc(InResetFunc)
        {
            PreWarm(InitialSize);
        }
        
        virtual void* Acquire() override
        {
            if (FreeObjects.Num() > 0) {
                T* Object = FreeObjects.Pop();
                ActiveObjects.Add(Object);
                return Object;
            }
            
            T* NewObject = CreateFunc();
            if (NewObject) {
                ActiveObjects.Add(NewObject);
            }
            return NewObject;
        }
        
        virtual void Release(void* ObjectPtr) override
        {
            T* Object = static_cast<T*>(ObjectPtr);
            if (ActiveObjects.Remove(Object) > 0) {
                ResetFunc(Object);
                FreeObjects.Add(Object);
            }
        }
        
        virtual int32 NumTotal() const override
        {
            return FreeObjects.Num() + ActiveObjects.Num();
        }
        
        virtual int32 NumActive() const override
        {
            return ActiveObjects.Num();
        }
        
        virtual int32 NumFree() const override
        {
            return FreeObjects.Num();
        }
        
        virtual void PreWarm(int32 Count) override
        {
            for (int32 i = 0; i < Count; i++) {
                T* Object = CreateFunc();
                if (Object) {
                    FreeObjects.Add(Object);
                }
            }
        }
        
        virtual void Shrink(int32 Count) override
        {
            Count = FMath::Min(Count, FreeObjects.Num());
            for (int32 i = 0; i < Count; i++) {
                T* Object = FreeObjects.Pop();
                if (Object) {
                    if constexpr (std::is_base_of_v<UObject, T>) {
                        if (IsValid(Object)) Object->ConditionalBeginDestroy();
                    } else {
                        delete Object;
                    }
                }
            }
        }
        
    private:
        TArray<T*> FreeObjects;
        TArray<T*> ActiveObjects;
        TFunction<T*()> CreateFunc;
        TFunction<void(T*)> ResetFunc;
    };
    
    TMap<FName, TSharedPtr<FObjectPoolBase>> Pools;
    TArray<FAutoConsoleCommand*> ConsoleCommands;
};