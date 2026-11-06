// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/ChunkUtilsFunction.h"

#include "Core/Enum/GameStateEnum.h"
#include "Core/VoxelGameInstance.h"

// 定义方向映射表
static const TMap<EActorDirection, EActorDirection> LeftTurnMap = {
    {EActorDirection::Right,    EActorDirection::Backward},
    {EActorDirection::Backward, EActorDirection::Left},
    {EActorDirection::Left,     EActorDirection::Forward},
    {EActorDirection::Forward,  EActorDirection::Right}
};
    
static const TMap<EActorDirection, EActorDirection> RightTurnMap = {
    {EActorDirection::Right,    EActorDirection::Forward},
    {EActorDirection::Forward,  EActorDirection::Left},
    {EActorDirection::Left,     EActorDirection::Backward},
    {EActorDirection::Backward, EActorDirection::Right}
};

FIntVector UChunkUtilsFunction::GetBlockIndexByPosition(const FVector Position, const float BlockLength)
{
    // FloorToInt 而非 TruncToInt：负坐标需向下取整（如 Z=-50,BlockLength=100 -> -1 而非 0），
    // 否则负半轴每层向零偏一格，导致负 Z 编辑/贴地/判定 off-by-one。
    return FIntVector(FMath::FloorToInt(Position.X / BlockLength), FMath::FloorToInt(Position.Y / BlockLength), FMath::FloorToInt(Position.Z / BlockLength));
}   

FVector UChunkUtilsFunction::GetPositionByBlockIndex(const FIntVector BlockIndex, const float BlockLength)
{
    return FVector(BlockIndex.X * BlockLength, BlockIndex.Y * BlockLength, BlockIndex.Z * BlockLength);
}

int UChunkUtilsFunction::GetBlockIdByIndex(const FIntVector BlockIndex, const  FIntVector WorldSize)
{
    const int SizeX = 2 * WorldSize.X + 1;
    const int SizeY = 2 * WorldSize.Y + 1;
    //const int SizeZ = 2 * WorldSize.Z + 1;
    const FIntVector n =BlockIndex +WorldSize;
    
    return (n.Z) * SizeX * SizeY +
           (n.Y) * SizeX+(n.X);
}

FIntVector UChunkUtilsFunction::GetBlockIndexById(const int BlockId, const  FIntVector WorldSize)
{
    const int SizeX = 2 * WorldSize.X + 1;
    const int SizeY = 2 * WorldSize.Y + 1;
    // const int SizeZ = 2 * WorldSize.Z + 1;
    int Z = BlockId / (SizeX * SizeY);
    int R = BlockId % (SizeX * SizeY);
    int Y = R / SizeX;
    int X = R % SizeX;
    return FIntVector(X, Y, Z)-WorldSize;   
}
FVector2D UChunkUtilsFunction::GetChunkIndexByBlockIndex(const FIntVector BlockIndex, const FIntVector ChunkDimensions)
{
    int X = BlockIndex.X / (ChunkDimensions.X*2+1);
    int Y = BlockIndex.Y / (ChunkDimensions.Y*2+1);
    return   FVector2D(X, Y);
}

FIntVector UChunkUtilsFunction::GetChunkIndexByShift(const FIntVector BlockIndex, int32 ChunkBitShift)
{
    // 利用算术右移 (Arithmetic Shift) 替代除法
    // 现代编译器下，对有符号整数的右移等同于 Floor(x / 2^n)
    // 能够完美处理负数坐标，且比除法快得多
    return FIntVector(
        BlockIndex.X >> ChunkBitShift,
        BlockIndex.Y >> ChunkBitShift,
        BlockIndex.Z >> ChunkBitShift
    );
}

FIntVector UChunkUtilsFunction::CalculateBitShift(FIntVector ChunkSize)
{
    // 内部 Lambda：处理单个轴的逻辑
    auto CalcAxis = [](int32& AxisSize) -> int32 {
        if (AxisSize <= 0) AxisSize = 1; // 防止除0或非法值
        
        if (!FMath::IsPowerOfTwo(AxisSize))
        {
            AxisSize = FMath::RoundUpToPowerOfTwo(AxisSize);
        }
        return FMath::FloorLog2(AxisSize);
    };

    // 这一步会修改传入的 ChunkSize (如果它不是2的幂)，但在函数内是传值的，所以不影响外部
    // 如果需要修正外部的 Size，建议在 Call 这个函数前先手动修正
    int32 ShiftX = CalcAxis(ChunkSize.X);
    int32 ShiftY = CalcAxis(ChunkSize.Y);
    int32 ShiftZ = CalcAxis(ChunkSize.Z);

    return FIntVector(ShiftX, ShiftY, ShiftZ);
}

FIntVector UChunkUtilsFunction::Get3DChunkIndexByBlockIndex(const FIntVector BlockIndex, const FIntVector ChunkSize)
{
    const FIntVector ShiftAmount = CalculateBitShift(ChunkSize); // 对应 Size 16
    
    // 右移操作自动处理负数向下取整 (例如 -5 >> 4 结果是 -1)
    int32 X = BlockIndex.X >> ShiftAmount.X;
    int32 Y = BlockIndex.Y >> ShiftAmount.Y;
    int32 Z = BlockIndex.Z >> ShiftAmount.Z;
    return   FIntVector(X,Y,Z);
}

int UChunkUtilsFunction::GetChunkIdByBlockIndex(const FIntVector BlockIndex, const FIntVector ChunkSize,const FIntVector WorldDimension,const FIntVector ChunkDimension)
{
    FIntVector v = Get3DChunkIndexByBlockIndex(BlockIndex,ChunkSize);
    return  v.Z*(ChunkSize.Y*2+1)*(ChunkSize.X*2+1)+v.Y* (ChunkSize.X*2+1)  + v.X;
}

FIntVector UChunkUtilsFunction::GetBlockWorldIndexByBlockChunkIndex(const FIntVector BlockChunkIndex, const  FIntVector ChunkIndex,const FIntVector ChunkSize)
{
    return FIntVector(
       BlockChunkIndex.X + (ChunkIndex.X) * ChunkSize.X,
       BlockChunkIndex.Y + (ChunkIndex.Y) * ChunkSize.Y,
       BlockChunkIndex.Z + (ChunkIndex.Z) * ChunkSize.Z
   );
}

FBox UChunkUtilsFunction::UpdateBox(FBox CurArea, FVector Input)
{
    FBox TempArea = CurArea;

    TempArea.Min.X =std::min(TempArea.Min.X, Input.X);
    if (TempArea.Min.X == CurArea.Min.X)
    {
        TempArea.Max.X =std::max(TempArea.Max.X, Input.X);
    }
    TempArea.Min.Y = std::min(TempArea.Min.Y, Input.Y);
    
    if (TempArea.Min.Y == CurArea.Min.Y)
    {
        TempArea.Max.Y = std::max(TempArea.Max.Y, Input.Y);
    }
    TempArea.Min.Z = std::min(TempArea.Min.Z, Input.Z);
    
    if (TempArea.Min.Z == CurArea.Min.Z)
    {
        TempArea.Max.Z = std::max(TempArea.Max.Z, Input.Z);
    }
    
    return TempArea;
    
}



FBox UChunkUtilsFunction::CheckBox(FBox CheckBox)
{
    

    FVector TempMinIndex=FVector(0);
    FVector TempMaxIndex=FVector(0);
	
    TempMinIndex.X = std::min(CheckBox.Min.X, CheckBox.Max.X);
    // 直接计算并设置CurBatchArea.MaxPosition的X维度
    TempMaxIndex.X = std::max(CheckBox.Min.X, CheckBox.Max.X);
    // 进行范围限制
	
	
    TempMinIndex.Y = std::min(CheckBox.Min.Y, CheckBox.Max.Y);
    TempMaxIndex.Y = std::max(CheckBox.Min.Y, CheckBox.Max.Y);

	
    TempMinIndex.Z = std::min(CheckBox.Min.Z, CheckBox.Max.Z);
    TempMaxIndex.Z = std::max(CheckBox.Min.Z, CheckBox.Max.Z);

	
    FBox Box = FBox(TempMinIndex, TempMaxIndex);
    return Box;
}

EActorDirection UChunkUtilsFunction::UpdateDir(bool IsTurnLeft, EActorDirection CurDir)
{
    const auto& DirectionMap = IsTurnLeft ? LeftTurnMap : RightTurnMap;
    if (DirectionMap.Contains(CurDir))
    {
        return DirectionMap[CurDir];
    }
    return CurDir;
    
}

EActorDirection UChunkUtilsFunction::RotationToDirection(FRotator Rotation)
{
    // 将 Yaw 角度归一化到 [0, 360)
    float Yaw = FMath::Fmod(Rotation.Yaw, 360.0f);
    if (Yaw < 0.0f)
    {
        Yaw += 360.0f;
    }

    // 90° 步进: Forward(0) → Right(90) → Backward(180) → Left(270)
    if (Yaw >= 315.0f || Yaw < 45.0f)
    {
        return EActorDirection::Forward;
    }
    else if (Yaw >= 45.0f && Yaw < 135.0f)
    {
        return EActorDirection::Right;
    }
    else if (Yaw >= 135.0f && Yaw < 225.0f)
    {
        return EActorDirection::Backward;
    }
    else
    {
        return EActorDirection::Left;
    }
}

FBox UChunkUtilsFunction::ChangeArea(FIntVector Min, FIntVector Max, const FBox WorldSize)
{
    

    // 1. 正序化
    FIntVector TempMin, TempMax;
    TempMin.X = FMath::Min(Min.X, Max.X);
    TempMax.X = FMath::Max(Min.X, Max.X);
    TempMin.Y = FMath::Min(Min.Y, Max.Y);
    TempMax.Y = FMath::Max(Min.Y, Max.Y);
    TempMin.Z = FMath::Min(Min.Z, Max.Z);
    TempMax.Z = FMath::Max(Min.Z, Max.Z);

    // 2. 裁剪到合法范围
    auto Clamp = [&WorldSize](int32 V, int32 Axis) -> int32
    {
        // 如果你确实需要负索引，保留下面这一行；否则改成 0
        int32 Lower = WorldSize.Min[Axis];
        int32 Upper = WorldSize.Max[Axis] - 1;
        return FMath::Clamp(V, Lower, Upper);
    };

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        TempMin[Axis] = Clamp(TempMin[Axis], Axis);
        TempMax[Axis] = Clamp(TempMax[Axis], Axis);
    }

    // 3. 如果裁剪后 Min > Max，再次修正
    TempMin = FIntVector(
        FMath::Min(TempMin.X, TempMax.X),
        FMath::Min(TempMin.Y, TempMax.Y),
        FMath::Min(TempMin.Z, TempMax.Z)
    );
    TempMax = FIntVector(
        FMath::Max(TempMin.X, TempMax.X),
        FMath::Max(TempMin.Y, TempMax.Y),
        FMath::Max(TempMin.Z, TempMax.Z)
    );

    return FBox(FVector(TempMin), FVector(TempMax));
}

FIntVector UChunkUtilsFunction::CheckInputVector(const FIntVector Vector, const FIntVector WorldSize) 
{

    FIntVector TempIndex=FIntVector(0);
	
    TempIndex.X = std::max(-WorldSize.X, Vector.X);
    TempIndex.X = std::min( WorldSize.X - 1, TempIndex.X);

    TempIndex.Y = std::max(-WorldSize.Y, Vector.Y);
    TempIndex.Y= std::min( WorldSize.Y - 1, TempIndex.Y);

    TempIndex.Z = std::max(-WorldSize.Z, Vector.Z);
    TempIndex.Z = std::min( WorldSize.Z - 1, TempIndex.Z);
	
    return TempIndex;
}

FRotator UChunkUtilsFunction::GetSpawnDirection(EActorDirection InputDirection)
{
    switch (InputDirection)
    {
    case EActorDirection::Left:
        return FRotator(0, -90, 0); // 面向左
    case EActorDirection::Right:
        return FRotator(0, 90, 0); // 面向右
    case EActorDirection::Backward:
        return FRotator(0, 180, 0); // 面向后
    case EActorDirection::Forward:
        return FRotator(0, 0, 0); // 面向前
    case EActorDirection::Up:
        return FRotator(90, 0,0 );   // Pitch -90°（面向上）
    case EActorDirection::Down:
        return FRotator(-90, 0,0 );    // Pitch +90°（面向下）
    default:
        return FRotator(0, 0, 0); // 默认面向前
    }
}

FVector UChunkUtilsFunction::GetRotationVolume(EActorDirection InputDirection, FVector InputVolume)
{
    switch (InputDirection)
    {
    case EActorDirection::Left:
        return FVector(InputVolume.Y, -InputVolume.X, InputVolume.Z); // 面向左
    case EActorDirection::Right:
        return FVector(-InputVolume.Y, InputVolume.X, InputVolume.Z); // 面向右
    case EActorDirection::Backward:
        return FVector(-InputVolume.X, -InputVolume.Y, InputVolume.Z); // 面向后
    case EActorDirection::Forward:
        return InputVolume; // 面向前
    default:
        return InputVolume; // 默认面向前
    }
}

// ==================== 坐标转换工具（新增实现）====================

FVector UChunkUtilsFunction::AlignPositionToVoxelGrid(FVector WorldPosition, float BlockLength)
{
    // 先转换为体素索引，再转回世界坐标，实现体素网格对齐
    FIntVector BlockIndex = GetBlockIndexByPosition(WorldPosition, BlockLength);
    return GetPositionByBlockIndex(BlockIndex, BlockLength);
}

FIntVector UChunkUtilsFunction::WorldDeltaToIndexDelta(FVector WorldDelta, float BlockLength)
{
    return FIntVector(
        WorldDeltaToIndexDelta_SingleAxis(WorldDelta.X, BlockLength),
        WorldDeltaToIndexDelta_SingleAxis(WorldDelta.Y, BlockLength),
        WorldDeltaToIndexDelta_SingleAxis(WorldDelta.Z, BlockLength)
    );
}

int32 UChunkUtilsFunction::WorldDeltaToIndexDelta_SingleAxis(float WorldDelta, float BlockLength)
{
    // 增量转换使用 RoundToInt（与绝对坐标的 FloorToInt 不同）
    // 例如：拖拽 150 单位，BlockLength=100 → 应该移动 1 个体素
    return FMath::RoundToInt(WorldDelta / BlockLength);
}

float UChunkUtilsFunction::GetBlockLength(const UObject* WorldContext, float Fallback)
{
    if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull))
    {
        if (UVoxelGameInstance* GI = World->GetGameInstance<UVoxelGameInstance>())
        {
            return GI->GetBlockLength();
        }
    }
    return Fallback;
}


