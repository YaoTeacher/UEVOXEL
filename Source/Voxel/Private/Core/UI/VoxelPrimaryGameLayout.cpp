#include "Core/UI/VoxelPrimaryGameLayout.h"
#include "Core/UI/HUD/VoxelGameMainPanel.h"
#include "Core/UI/Build/BuildPanelWidget.h"
#include "Core/UI/Base/VoxelToastWidget.h"
#include "Core/UI/Base/VoxelDragProgressWidget.h"
#include "Core/UI/Base/VoxelInfoTooltip.h"
#include "Core/UI/Base/VoxelScreenBase.h"
#include "Core/UI/Base/VoxelDialogBase.h"
#include "Core/UI/Base/VoxelWidgetBase.h"
#include "Core/UI/Build/VoxelEditConfirmWidget.h"
#include "Voxel/VoxelPlayerController.h"
#include "CommonUI/Public/Widgets/CommonActivatableWidgetContainer.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Input/CommonUIInputTypes.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "TimerManager.h"

UVoxelPrimaryGameLayout* UVoxelPrimaryGameLayout::GetFromPC(APlayerController* PC)
{
	if (!PC) return nullptr;
	if (AVoxelPlayerController* VPC = Cast<AVoxelPlayerController>(PC))
	{
		return VPC->PrimaryLayout;
	}
	// 默认 PC（如 MainMenu）：从视口搜索已添加的 UVoxelPrimaryGameLayout
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(PC, Widgets, UVoxelPrimaryGameLayout::StaticClass(), true);
	for (UUserWidget* W : Widgets)
	{
		if (UVoxelPrimaryGameLayout* Layout = Cast<UVoxelPrimaryGameLayout>(W))
		{
			return Layout;
		}
	}
	return nullptr;
}

void UVoxelPrimaryGameLayout::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	if (RootCanvas)
	{
		WidgetTree->RootWidget = RootCanvas;
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// Layer.GameMenu Stack - 建造面板等全屏面板
		GameMenuStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("GameMenuStack"));
		if (GameMenuStack)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(GameMenuStack))
			{
				CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				CanvasSlot->SetOffsets(FMargin(0));
			}
			GameMenuStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		// Layer.Modal Stack - 模态弹窗
		ModalStack = WidgetTree->ConstructWidget<UCommonActivatableWidgetStack>(UCommonActivatableWidgetStack::StaticClass(), TEXT("ModalStack"));
		if (ModalStack)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(ModalStack))
			{
				CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				CanvasSlot->SetOffsets(FMargin(0));
			}
			ModalStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		// Layer.Toast Queue - 自动消失提示
		ToastQueue = WidgetTree->ConstructWidget<UCommonActivatableWidgetQueue>(UCommonActivatableWidgetQueue::StaticClass(), TEXT("ToastQueue"));
		if (ToastQueue)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(ToastQueue))
			{
				CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				CanvasSlot->SetOffsets(FMargin(0));
			}
			ToastQueue->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	// 常驻 MainPanel：非 Activatable，作为 RootCanvas 子控件
	if (MainPanelClass)
	{
		UUserWidget* Panel = CreateWidget(this, MainPanelClass);
		if (Panel)
		{
			if (RootCanvas)
			{
				if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Panel))
				{
					CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
					CanvasSlot->SetOffsets(FMargin(0));
				}
			}
			else
			{
				Panel->AddToViewport(0);
			}
		}
	}

	// 常驻编辑确认/取消场景弹窗：RootCanvas 子控件，默认隐藏，
	// 显隐与锚点跟随由控件自身订阅 StateComponent 广播 + 每帧投影完成
	if (EditConfirmWidgetClass && RootCanvas)
	{
		EditConfirmWidget = CreateWidget<UVoxelEditConfirmWidget>(this, EditConfirmWidgetClass);
		if (EditConfirmWidget)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(EditConfirmWidget))
			{
				// 浮动弹窗：左上锚点 + AutoSize，位置由控件自身每帧投影设置
				CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
				CanvasSlot->SetAlignment(FVector2D::ZeroVector);
				CanvasSlot->SetAutoSize(true);
			}
			EditConfirmWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 手动设置输入配置：覆盖 CommonUI 硬编码的 NoCapture
	// NoCapture 会导致 FSceneViewport::OnMouseButtonDown 不调用 InputKey，鼠标按下不传递到游戏
	// CaptureDuringMouseDown 在 ApplyUIInputConfig 中不调用 UseHighPrecisionMouseMovement（保持光标自由移动），
	// 同时在 OnMouseButtonDown 中 bTemporaryCapture=true，PRESSED 能传递到游戏
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UCommonUIActionRouterBase* ActionRouter = LP->GetSubsystem<UCommonUIActionRouterBase>())
		{
			ActionRouter->SetActiveUIInputConfig(
				FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::CaptureDuringMouseDown, false),
				this
			);
			UE_LOG(LogTemp, Warning, TEXT("[PrimaryLayout] SetActiveUIInputConfig: All + CaptureDuringMouseDown"));
		}
	}
}

void UVoxelPrimaryGameLayout::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (DragProgressWidget)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			FVector2D MousePos(0, 0);
			FVector2D ViewportSize = MyGeometry.GetLocalSize();
			float MouseX = 0.0f, MouseY = 0.0f;
			PC->GetMousePosition(MouseX, MouseY);
			MousePos = FVector2D(MouseX, MouseY);

			if (UVoxelDragProgressWidget* DPW = Cast<UVoxelDragProgressWidget>(DragProgressWidget))
			{
				DPW->UpdatePosition(MousePos, ViewportSize);
				DPW->UpdateProgress(CurrentDragProgress);
			}
		}
	}
}

UCommonActivatableWidget* UVoxelPrimaryGameLayout::PushGameMenu(TSubclassOf<UCommonActivatableWidget> WidgetClass, UObject* Context)
{
	if (!WidgetClass || !GameMenuStack) return nullptr;

	UCommonActivatableWidget* Widget = GameMenuStack->AddWidget<UCommonActivatableWidget>(WidgetClass);
	(void)Context;

	return Widget;
}

UCommonActivatableWidget* UVoxelPrimaryGameLayout::PushModal(TSubclassOf<UCommonActivatableWidget> WidgetClass, UObject* Context)
{
	if (!WidgetClass || !ModalStack) return nullptr;

	// 重入保护：同类弹窗已在栈顶时拒绝重复推入（重复推入会叠加实例，
	// 关闭一层后下层残留实例使输入模式卡在 Menu，场景无法点击）
	if (UCommonActivatableWidget* ActiveWidget = ModalStack->GetActiveWidget())
	{
		if (ActiveWidget->GetClass() == WidgetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PrimaryLayout] PushModal 拒绝重复推入同类弹窗: %s"), *GetNameSafe(WidgetClass));
			return ActiveWidget;
		}
	}

	// 构造回调在激活（NativeOnActivated → OnDialogOpened）之前注入 Context，
	// 修复原实现丢弃 Context 导致对话框拿不到数据的问题
	return ModalStack->AddWidget<UCommonActivatableWidget>(WidgetClass,
		[Context](UCommonActivatableWidget& Widget)
		{
			if (UVoxelDialogBase* Dialog = Cast<UVoxelDialogBase>(&Widget))
			{
				Dialog->DataContext = Context;
			}
		});
}

void UVoxelPrimaryGameLayout::ShowToast(const FText& Message, float Duration)
{
	if (!ToastClass || !ToastQueue) return;

	ToastQueue->AddWidget<UVoxelToastWidget>(ToastClass,
		[Message, Duration](UVoxelToastWidget& Toast)
		{
			Toast.InitializeToast(Message, Duration, EToastType::Info);
		});
}

void UVoxelPrimaryGameLayout::ShowDragProgress()
{
	if (DragProgressWidget || !DragProgressClass) return;

	DragProgressWidget = CreateWidget(this, DragProgressClass);
	if (DragProgressWidget)
	{
		if (RootCanvas)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(DragProgressWidget))
			{
				CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				CanvasSlot->SetOffsets(FMargin(0));
			}
		}
		else
		{
			DragProgressWidget->AddToViewport(400);
		}
		DragProgressWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		CurrentDragProgress = 0.0f;
	}
}

void UVoxelPrimaryGameLayout::HideDragProgress()
{
	if (DragProgressWidget)
	{
		DragProgressWidget->RemoveFromParent();
		DragProgressWidget = nullptr;
		CurrentDragProgress = 0.0f;
	}
}

void UVoxelPrimaryGameLayout::UpdateDragProgress(float Progress)
{
	CurrentDragProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
}

void UVoxelPrimaryGameLayout::ShowInfoTooltip(TSubclassOf<UVoxelInfoTooltip> TooltipClass, UObject* Context)
{
	if (ActiveTooltip)
	{
		HideInfoTooltip();
	}

	ActiveTooltip = CreateWidget(this, TooltipClass);
	if (ActiveTooltip)
	{
		if (UVoxelWidgetBase* VW = Cast<UVoxelWidgetBase>(ActiveTooltip))
		{
			VW->DataContext = Context;
		}
		if (UVoxelInfoTooltip* Tooltip = Cast<UVoxelInfoTooltip>(ActiveTooltip))
		{
			Tooltip->OnTooltipShown(Context);
		}
		if (RootCanvas)
		{
			if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(ActiveTooltip))
			{
				CanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				CanvasSlot->SetOffsets(FMargin(0));
			}
		}
		else
		{
			ActiveTooltip->AddToViewport(300);
		}
		ActiveTooltip->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UVoxelPrimaryGameLayout::HideInfoTooltip()
{
	if (ActiveTooltip)
	{
		if (UVoxelInfoTooltip* Tooltip = Cast<UVoxelInfoTooltip>(ActiveTooltip))
		{
			Tooltip->OnTooltipHidden();
		}
		ActiveTooltip->RemoveFromParent();
		ActiveTooltip = nullptr;
	}
}

UObject* UVoxelPrimaryGameLayout::GetCurrentScreen() const
{
	if (GameMenuStack)
	{
		return Cast<UObject>(GameMenuStack->GetActiveWidget());
	}
	return nullptr;
}

void UVoxelPrimaryGameLayout::CloseGameMenu()
{
	if (!GameMenuStack) return;

	if (UCommonActivatableWidget* Active = Cast<UCommonActivatableWidget>(GameMenuStack->GetActiveWidget()))
	{
		Active->DeactivateWidget();
		return;
	}

	// 兜底：栈内有内容但无「激活」面板（激活流程未完成/被中断），直接清空
	if (GameMenuStack->GetWidgetList().Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PrimaryLayout] CloseGameMenu: no active widget, force clearing %d widget(s)"),
			GameMenuStack->GetWidgetList().Num());
		GameMenuStack->ClearWidgets();
	}
}
