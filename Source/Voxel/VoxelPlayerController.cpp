// Copyright Epic Games, Inc. All Rights Reserved.

#include "VoxelPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "VoxelCharacter.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Area/VoxelAreaBaseInstance.h"
#include "Core/VoxelGameInstance.h"
#include "Core/ControllerComponent/VoxelInteractionComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/ControllerComponent/VoxelPreviewComponent.h"
#include "Core/ControllerComponent/VoxelControllerHistoryComponent.h"
#include "Core/ControllerComponent/VoxelPlayerStateCacheComponent.h"
#include "Core/UI/Build/BuildPanelWidget.h"
#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameManager/WorldManager/VoxelAreaWorldSubsystem.h"
#include "GameManager/WorldManager/VoxelBlockWorldSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

// ==================== 构造函�?====================

AVoxelPlayerController::AVoxelPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	// 创建默认子对�?
	InteractionComp = CreateDefaultSubobject<UVoxelInteractionComponent>(TEXT("InteractionComponent"));
	StateCacheComp = CreateDefaultSubobject<UVoxelPlayerStateCacheComponent>(TEXT("PlayerStateCacheComponent"));
	PreviewComp = CreateDefaultSubobject<UVoxelPreviewComponent>(TEXT("PreviewComponent"));
	HistoryComp = CreateDefaultSubobject<UVoxelControllerHistoryComponent>(TEXT("HistoryComponent"));
}

// ==================== 初始�?====================

void AVoxelPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 获取游戏框架引用
	GameMode = Cast<AVoxelGameMode>(GetWorld()->GetAuthGameMode());
	GameInstance = Cast<UVoxelGameInstance>(GetWorld()->GetGameInstance());
	GameState = GameMode ? GameMode->GetVoxelGameState() : nullptr;
	ControlledPawn = Cast<AVoxelControllerPawn>(GetPawn());

	// 获取子系统引�?
	BlockWorldSubsystem = GetWorld()->GetSubsystem<UVoxelBlockWorldSubsystem>();
	if (!BlockWorldSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BlockWorldSubsystem not found!"));
	}

	AreaWorldSubsystem = GetWorld()->GetSubsystem<UVoxelAreaWorldSubsystem>();
	if (!AreaWorldSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AreaWorldSubsystem not found!"));
	}

	// 组件指针自愈：BP CDO 历史污染可能
	// CreateDefaultSubobject 赋的指针序列化为 None
	// 冷启动时 initprops 会用 None 覆盖 ctor 赋值。这里在注入/创建 Layout 之前?
	// FindComponentByClass 重新取回实例指针，保证后续链路（注入、主面板绑定、切面初始化）可用?
	if (!InteractionComp) { InteractionComp = FindComponentByClass<UVoxelInteractionComponent>();        UE_LOG(LogTemp, Warning, TEXT("[ToolChain] SelfHeal: InteractionComp was null, healed=%s"), InteractionComp ? TEXT("OK") : TEXT("FAILED")); }
	if (!StateCacheComp)  { StateCacheComp  = FindComponentByClass<UVoxelPlayerStateCacheComponent>();  UE_LOG(LogTemp, Warning, TEXT("[ToolChain] SelfHeal: StateCacheComp was null, healed=%s"), StateCacheComp ? TEXT("OK") : TEXT("FAILED")); }
	if (!PreviewComp)     { PreviewComp     = FindComponentByClass<UVoxelPreviewComponent>();     UE_LOG(LogTemp, Warning, TEXT("[ToolChain] SelfHeal: PreviewComp was null, healed=%s"), PreviewComp ? TEXT("OK") : TEXT("FAILED")); }
	if (!HistoryComp)     { HistoryComp     = FindComponentByClass<UVoxelControllerHistoryComponent>(); UE_LOG(LogTemp, Warning, TEXT("[ToolChain] SelfHeal: HistoryComp was null, healed=%s"), HistoryComp ? TEXT("OK") : TEXT("FAILED")); }

	// 组件间无注入依赖：InteractionComp ↔ StateCacheComp 经同 Owner FindComponentByClass 惰性互查
	UE_LOG(LogTemp, Log, TEXT("[ToolChain] Components wired: Interaction=%s StateCache=%s"),
		*GetNameSafe(InteractionComp), *GetNameSafe(StateCacheComp));

	// 创建 PrimaryGameLayout（替代旧 AVoxelHUD�?
	// 防御性单例：清理� OpenLevel 残留的旧 Layout（widget 不随 world 销毁）?
	// 保证本地图只有一�?PrimaryLayout，避免输入路由错�?多层堆叠?
	TArray<UUserWidget*> ExistingLayouts;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ExistingLayouts, UVoxelPrimaryGameLayout::StaticClass(), true);
	for (UUserWidget* W : ExistingLayouts)
	{
		W->RemoveFromParent();
	}

	const TSubclassOf<UVoxelPrimaryGameLayout> LayoutClass = PrimaryLayoutClass ? PrimaryLayoutClass : TSubclassOf<UVoxelPrimaryGameLayout>(UVoxelPrimaryGameLayout::StaticClass());
	PrimaryLayout = CreateWidget<UVoxelPrimaryGameLayout>(this, LayoutClass);
	if (PrimaryLayout)
	{
		PrimaryLayout->AddToViewport(0);
	}

	// 输入配置�?CommonUI 通过 CommonGameViewportClient + GetDesiredInputConfig 自动管理
	SetShowMouseCursor(true);

	UE_LOG(LogTemp, Display, TEXT("=== Component Validation ==="));
	UE_LOG(LogTemp, Display, TEXT("InteractionComp: %s (%s)"),
		*GetNameSafe(InteractionComp),
		IsValid(InteractionComp) ? TEXT("VALID") : TEXT("INVALID!"));
	UE_LOG(LogTemp, Display, TEXT("PreviewComp: %s (%s)"),
		*GetNameSafe(PreviewComp),
		IsValid(PreviewComp) ? TEXT("VALID") : TEXT("INVALID!"));

	// ==================== 切面虚化默认值初始化 ====================
	// 默认状态：最大高�?+ 启用切面 + PlayerManaged 模式
	// - EditSliceLayer = MaxSliceLayer：场景完全显示（无体素超过最大高度）
	// - bSliceEnabled = true：用户降低高度时立即生效，无需�?toggle
	// - SliceFilterMode = PlayerManaged：StateComponent 默认�?
	// SetEditSliceLayer/SetSliceEnabled 会广�?OnBuildModeParamsUpdated
	// -> VisualUIComponent::HandleBuildModeParamsUpdated -> ApplySliceVisuals
	if (StateCacheComp)
	{
		const int32 MaxLayer = StateCacheComp->GetMaxSliceLayer();
		StateCacheComp->SetEditSliceLayer(MaxLayer);
		StateCacheComp->SetSliceEnabled(true);
		UE_LOG(LogTemp, Log, TEXT("[Slice] Init: Layer=%d/%d, Enabled=true, Mode=PlayerManaged"),
			StateCacheComp->GetEditSliceLayer(), MaxLayer);
	}

	// ==================== Pawn 高度初始化 ====================
	// 从 Pawn 当前实际位置同步体素层（读档/新游戏都适用）
	if (StateCacheComp)
	{
		StateCacheComp->SyncPawnHeightLayerFromPawn();
	}
}

void AVoxelPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 添加输入映射上下�?
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
 	{
 	Subsystem->AddMappingContext(DefaultMappingContext, 0);
 	}

	// [诊断] �?InputComponent 层绑定鼠标左键，确认点击是否到达 PlayerController
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AVoxelPlayerController::Diagnostic_OnLeftMousePressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AVoxelPlayerController::Diagnostic_OnLeftMouseReleased);

	// ESC：有面板则让�?HandleEscape，无面板则打开暂停菜单�?
	// 防抖避免与蓝�?IA_Escape 双触发�?
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AVoxelPlayerController::OnEscapePressed);

	// 所有按键映射均�?IMC_VoxelDefault 资产中配置（IMC），不在 C++ 中硬编码
	// 所�?IA �?BindAction �?Controller 蓝图中通过 EnhancedInputAction 事件节点处理
	// C++ 仅负责添�?IMC，按键→IA 映射�?IMC 资产中，IA→回调映射在蓝图�?
	// C++ 不直接关联输入、不检查输入键状态，只提�?BlueprintCallable 方法供蓝图调?
	//
	// 当前 IMC 映射总表�?4 个映射，12 个独�?IA）：
	// ┌──────────────────────────┬──────────────────┬──────────┬──────────────────────────────────────�?
	// �?IA                       �?Key              �?Modifier �?功能                                 ?
	// ├──────────────────────────┼──────────────────┼──────────┼──────────────────────────────────────?
	// �?IA_Move                  �?W/A/S/D/方向�?  �?Swizzle  �?相机前后左右移动                     ?
	// �?IA_Look                  �?Mouse2D          �?-        �?鼠标增量（MMB按住�?>CameraOrbit，弦触发器） ?
	// �?IA_Select                �?LeftMouseButton  �?-        �?选择/放置（蓝图绑定）               ?
	// �?IA_Release               �?RightMouseButton �?-        �?取消/释放（蓝图绑定）               ?
	// �?IA_CameraYawRotate       �?Q / E            �?Negate(E)�?相机左右旋转 90°                    ?
	// �?IA_BuildMode_CameraUp    �?X / Z            �?Negate(Z)�?相机�?X�?Z（Axis1D�?             ?
	// �?IA_CameraTargetLength    �?MouseScrollUp/Down│Negate(Dn)�?相机距离调整                        ?
	// �?IA_Escape                �?Escape           �?-        �?取消/退出（蓝图→HandleEscape�?      ?
	// �?IA_Edit_Delete           �?Delete           �?-        �?删除选中家具（蓝图→DeleteSelected�? ?
	// �?IA_Edit_Undo             �?Hyphen (-)       �?-        �?撤销（蓝图→Undo，Ctrl在蓝图中检查）  ?
	// �?IA_Edit_Redo             �?Equals (=)       �?-        �?重做（蓝图→Redo，Ctrl在蓝图中检查）  ?
	// �?IA_CameraOrbit           �?MiddleMouseButton�?-        �?MMB 视角旋转开关（Started/Completed->SetRotationMode�??
	// └──────────────────────────┴──────────────────┴──────────┴──────────────────────────────────────?
	//
	// 蓝图 IA 事件节点 �?C++ 方法 路由表：
	//   IA_Move         Triggered �?CameraMove(ActionValue)
	//   IA_Look         Triggered -> CameraOrbit(X, Y)（弦触发器要�?MMB 按下�?
	//   IA_Select       Completed �?HandleMouseInput
	//   IA_Release      Completed �?HandleMouseCancel
	//   IA_CameraYawRotate Triggered �?CameraYawRotate(ActionValue)
	//   IA_BuildMode_CameraUp Triggered �?CameraUp(ActionValue)
	//   IA_CameraTargetLength Triggered �?CameraDistance(ActionValue)
	//   IA_Escape       Triggered -> VoxelHUD::HandleEscape
	//   IA_Edit_Delete  Triggered → InteractionComp::DeleteSelectedFurniture
	//   IA_Edit_Undo    Triggered → InteractionComp::Undo
	//   IA_Edit_Redo    Triggered → InteractionComp::Redo
	//   IA_CameraOrbit  Started -> VisualUIComp::SetRotationMode(true) / Completed -> VisualUIComp::SetRotationMode(false)
}

// ==================== ESC / 暂停菜单 ====================

void AVoxelPlayerController::OnEscapePressed()
{
	// 防抖：避�?C++ BindKey 与蓝�?IA_Escape 同帧双触�?
	if (UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		if (Now - LastEscapePressedTime < 0.2) return;
		LastEscapePressedTime = Now;
	}

	if (!PrimaryLayout) return;

	// 1. 若当前有面板，先让它处理 ESC（关闭自身）
	if (UObject* Current = PrimaryLayout->GetCurrentScreen())
	{
		if (UVoxelScreenBase* Screen = Cast<UVoxelScreenBase>(Current))
		{
			if (Screen->HandleEscape())
			{
				return; // 面板自行处理?
			}
		}
	}

	// 2. 有选中实体时优先清除选中（实体详情面板关闭）
	if (StateCacheComp && StateCacheComp->HasSelectedEntity())
	{
		StateCacheComp->ClearSelectedEntity();
		return;
	}

	// 3. 无面板或未处理：打开暂停菜单
	if (PauseMenuWidgetClass)
	{
		PrimaryLayout->PushGameMenu(PauseMenuWidgetClass);
	}
}

// ==================== 相机控制 ====================

void AVoxelPlayerController::CameraMove(FVector2D Value)
{
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->MoveForward(Value.Y);
	ControlledPawn->MoveRight(Value.X);
}

void AVoxelPlayerController::CameraYawRotate(float RotationAmount)
{
	if (!ControlledPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid or missing VoxelControllerPawn! %s "), *GetPawn()->GetClass()->GetName());
		return;
	}

	ControlledPawn->RotateCameraYaw(RotationAmount);
}

void AVoxelPlayerController::CameraUp(float UpAmount)
{
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->MoveUp_World(UpAmount);
}

void AVoxelPlayerController::CameraPitchRotate(float WheelDelta)
{
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->RotateCameraPitch(WheelDelta);
}

void AVoxelPlayerController::CameraDistance(float WheelDelta)
{
	if (!ControlledPawn)
	{
		return;
	}

	ControlledPawn->AdjustCameraDistance(WheelDelta);
}

void AVoxelPlayerController::CameraOrbit(float YawDelta, float PitchDelta)
{
	if (!ControlledPawn)
	{
		return;
	}

	// 直接�?Pawn（视觉中�?屏幕中心）旋转：转弹簧臂即可，无需外部旋转中心�?
	ControlledPawn->OrbitCamera(YawDelta, PitchDelta);
}

void AVoxelPlayerController::ShowRotationRings()
{
	if (PreviewComp)
	{
		PreviewComp->ShowRotationRings();
	}
}

void AVoxelPlayerController::HideRotationRings()
{
	if (PreviewComp)
	{
		PreviewComp->HideRotationRings();
	}
}

// ==================== 输入模式 ====================

// ==================== 子系统访问器 ====================

AVoxelGameMode* AVoxelPlayerController::GetGameMode()
{
	return GameMode;
}

UVoxelGameInstance* AVoxelPlayerController::GetInstance()
{
	return GameInstance;
}

AVoxelGameState* AVoxelPlayerController::GetSateReference()
{
	if (!GameState && GameMode)
	{
		GameState = GameMode->GetVoxelGameState();
	}
	return GameState;
}

// ==================== 内部辅助方法 ====================

// ==================== [诊断] 鼠标按键追踪 ====================

void AVoxelPlayerController::Diagnostic_OnLeftMousePressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[Mouse] LeftMouseButton PRESSED (reached InputComponent)"));
}

void AVoxelPlayerController::Diagnostic_OnLeftMouseReleased()
{
	UE_LOG(LogTemp, Warning, TEXT("[Mouse] LeftMouseButton RELEASED (reached InputComponent)"));
}


