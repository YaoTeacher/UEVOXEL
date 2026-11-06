#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Core/UI/System/VoxelUISubsystemTypes.h"
#include "VoxelToastWidget.generated.h"

class UVoxelPrimaryGameLayout;

// L4 Toast 基类：自动消失提示（金币不足/保存成功等）
// 继承 UCommonActivatableWidget，由 VoxelPrimaryGameLayout 的 ToastQueue 管理生命周期
UCLASS(Abstract)
class VOXEL_API UVoxelToastWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 初始化 Toast（由 Queue->AddWidget 的 InitFunc 调用）
	void InitializeToast(const FText& Message, float InDuration, EToastType Type);

	// Toast 显示时调用（子类重写：播入场动画）
	virtual void OnToastShown() {}

	// Toast 过期时调用（子类重写：播出场动画）
	virtual void OnToastExpired() {}

	// 显示时长
	float GetDuration() const { return Duration; }

	// 获取所属 Layout
	UFUNCTION(BlueprintPure, Category = "Voxel|UI")
	UVoxelPrimaryGameLayout* GetLayout() const;

	/** Toast 是非交互提示，不得接管输入：返回与 PrimaryLayout 全局一致的
	 *  All + CaptureDuringMouseDown，避免 Toast 短暂成为 leaf-most node 时
	 *  应用默认配置（Menu/NoCapture）导致弹窗关闭后场景点击被吞。 */
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel|UI|Toast")
	FText ToastMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel|UI|Toast")
	EToastType ToastType = EToastType::Info;

	UPROPERTY(BlueprintReadOnly, Category = "Voxel|UI|Toast")
	float Duration = 3.0f;

private:
	FTimerHandle ExpiryTimerHandle;
};
