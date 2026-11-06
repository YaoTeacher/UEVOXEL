// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/UI/Build/ToolDetailModuleBase.h"
#include "M9_AreaWallBridgeModule.generated.h"

class UButton;

/**
 * M9: 区域建墙桥接模块
 * 职责：提供「以区域边界建墙」入口按钮——把当前选中区域的包围盒装配为
 *      几何体编辑请求（盒体 + 内壳 + 默认四面墙），并切换到几何体编辑工具
 *
 * 显示条件: Tool.AreaEdit
 *
 * 链路（单向数据流）:
 * - 按钮 → InteractionComp->BeginWallShellFromArea()
 *   → SC 写 TmpGeometry（盒=区域边界, Thickness=1 内壳, ShellFaces=四面）
 *   → SwitchTool(Tool.Edit.SimpleGeometry)
 *   → 墙壁的厚度/面/方块类型（手持）在几何体编辑侧调整，M16 确认执行
 *
 * UI控件:
 * - BridgeHintText: 说明文本
 * - WallBridgeButton: 「以区域边界建墙」按钮
 */
UCLASS()
class VOXEL_API UM9_AreaWallBridgeModule : public UToolDetailModuleBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldShowForTool(FGameplayTag ToolTag) const override;

protected:
	virtual void NativeOnInitialized() override;

	// === UI 控件绑定 ===

	/** 「以区域边界建墙」按钮 */
	UPROPERTY(meta = (BindWidget))
	UButton* WallBridgeButton;

	// === 事件处理 ===

	UFUNCTION()
	void OnWallBridgeClicked();
};
