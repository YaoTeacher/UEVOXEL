// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "VoxelUITheme.generated.h"

class UTexture2D;

/**
 * 文本角色：对应 DESIGN_UMG.md §3 字体型号表
 * 统一控件按角色从主题取 {字体族, 字号, 颜色, 字距}
 */
UENUM(BlueprintType)
enum class EVoxelTextRole : uint8
{
	PanelTitle   UMETA(DisplayName = "Panel Title (面板标题)"),
	SectionTitle UMETA(DisplayName = "Section Title (分组标题)"),
	ItemLabel    UMETA(DisplayName = "Item Label (行标签)"),
	Body         UMETA(DisplayName = "Body Text (正文)"),
	Hint         UMETA(DisplayName = "Hint Text (提示/占位)"),
	DataValue    UMETA(DisplayName = "Data Value (数值)"),
	ButtonText   UMETA(DisplayName = "Button Text (按钮文字)"),
	KbdHint      UMETA(DisplayName = "Kbd Hint (快捷键角标)"),
};

/**
 * 单个文本角色的样式
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelTextRoleStyle
{
	GENERATED_BODY()

	/** true 用主题衬线字体（标题/数值），false 用主题正文字体 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	bool bUseSerifFont = false;

	/** 字号（px） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ClampMin = 1))
	int32 Size = 11;

	/** 文字颜色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor Color = FLinearColor(0.937f, 0.878f, 0.741f);

	/** 字距（1/100 点；2px 字距填 200） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ClampMin = 0))
	int32 LetterSpacing = 0;
};

/**
 * 滑条样式（DESIGN_UMG.md §4.6）
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelSliderStyleDesc
{
	GENERATED_BODY()

	/** 轨道厚度（px） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ClampMin = 1.f))
	float BarThickness = 4.f;

	/** 把手直径（px） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ClampMin = 4.f))
	float ThumbSize = 10.f;

	/** 轨道颜色（track #170F07） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor BarColor = FLinearColor(0.090f, 0.059f, 0.027f);

	/** 把手颜色（gold #D9A95C） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor ThumbColor = FLinearColor(0.851f, 0.663f, 0.361f);

	/** 把手悬停颜色（#FFE8C0） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor ThumbHoveredColor = FLinearColor(1.0f, 0.910f, 0.753f);

	/** 可选把手贴图：设置后替代纯色把手（换素材只改这里） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	TSoftObjectPtr<UTexture2D> ThumbTexture;
};

/**
 * 输入框样式（DESIGN_UMG.md §4.5）
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelInputStyleDesc
{
	GENERATED_BODY()

	/** 常态底色（inp_bg #1A130C） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor BackgroundColor = FLinearColor(0.102f, 0.075f, 0.047f);

	/** 聚焦底色（card_top #2E2413） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor FocusedBackgroundColor = FLinearColor(0.180f, 0.141f, 0.075f);

	/** 文字颜色（txt #EFE0BD） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor ForegroundColor = FLinearColor(0.937f, 0.878f, 0.741f);

	/** 内边距（水平 8，垂直 3） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FMargin Padding = FMargin(8.f, 3.f);
};

/**
 * 下拉选项样式
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelComboStyleDesc
{
	GENERATED_BODY()

	/** 下拉行常态底色（inp_bg #1A130C） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor RowColor = FLinearColor(0.102f, 0.075f, 0.047f);

	/** 下拉行悬停/选中底色（sel #5C4020） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor RowHoveredColor = FLinearColor(0.361f, 0.251f, 0.125f);

	/** 文字颜色（txt #EFE0BD） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FLinearColor ForegroundColor = FLinearColor(0.937f, 0.878f, 0.741f);

	/** 内容内边距 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FMargin ContentPadding = FMargin(8.f, 4.f);

	/** 下拉列表最大高度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style", meta = (ClampMin = 32.f))
	float MaxListHeight = 240.f;
};

/**
 * 布局尺寸令牌：行高/列宽/间距/内边距
 * 行控件（VoxelXxxRow）在 PreConstruct 时应用 LabelWidth/ValueWidth 做跨模块列对齐；
 * RowGap/RowSpacing/PanelPadding/ControlHeight 供模块 WBP 与后续控件统一取用。
 * 统一改尺寸只改本资产，不散落在各 WBP 里。
 */
USTRUCT(BlueprintType)
struct VOXEL_API FVoxelUILayoutTokens
{
	GENERATED_BODY()

	/** 行标签列最小宽度（跨模块对齐「标签：控件」） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = 0.f))
	float LabelWidth = 96.f;

	/** 行右侧数值列最小宽度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = 0.f))
	float ValueWidth = 56.f;

	/** 行内元素间距（标签与控件之间） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = 0.f))
	float RowGap = 8.f;

	/** 模块内相邻行间距 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = 0.f))
	float RowSpacing = 6.f;

	/** 控件统一高度（输入框/下拉/按钮） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = 8.f))
	float ControlHeight = 24.f;

	/** 模块面板内边距 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FMargin PanelPadding = FMargin(12.f, 10.f);
};

/**
 * Voxel UI 主题资产（唯一样式真相源）
 *
 * 字段默认值镜像 VoxelDesign/DESIGN_UMG.md（v6 金属框羊皮纸）：
 * - 基础色板 / 表面色：§2
 * - 文本角色：§3
 * - 控件样式：§4.5（Input）/ §4.6（Slider）
 *
 * 统一控件（UVoxelTextBlock / UVoxelSlider / UVoxelEditableTextBox / UVoxelComboBoxString）
 * 在 SynchronizeProperties 时通过 UVoxelUITheme::Get() 拉取样式，
 * 编辑器预览与运行时同源；改样式只改本资产。
 */
UCLASS(BlueprintType)
class VOXEL_API UVoxelUITheme : public UDataAsset
{
	GENERATED_BODY()

public:
	UVoxelUITheme();

	/**
	 * 全局主题入口：读取 UVoxelUISettings::DefaultTheme 软引用并加载。
	 * UMG 设计器 PreConstruct 阶段无 GameInstance，故不依赖任何子系统。
	 * 未配置主题时返回 nullptr，控件保持引擎默认样式（优雅降级）。
	 */
	static const UVoxelUITheme* Get();

	/** 生成角色字体信息：字体资产缺失时退回引擎默认字体（仅应用字号/字距） */
	FSlateFontInfo MakeFontInfo(EVoxelTextRole Role) const;

	/** 取角色样式 */
	const FVoxelTextRoleStyle& GetRoleStyle(EVoxelTextRole Role) const;

	// === 字体族 ===

	/** 衬线字体（标题/数值）：Font 或 FontFace 资产 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts")
	TSoftObjectPtr<UObject> SerifFont;

	/** 正文字体：留空用引擎默认 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fonts")
	TSoftObjectPtr<UObject> BodyFont;

	// === 基础色板（§2）===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Background = FLinearColor(0.082f, 0.063f, 0.039f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Gold = FLinearColor(0.851f, 0.663f, 0.361f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Gold2 = FLinearColor(0.541f, 0.416f, 0.227f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Line = FLinearColor(0.290f, 0.227f, 0.125f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor LineHighlight = FLinearColor(0.420f, 0.302f, 0.149f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Text = FLinearColor(0.937f, 0.878f, 0.741f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Muted = FLinearColor(0.702f, 0.627f, 0.486f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Faint = FLinearColor(0.490f, 0.435f, 0.322f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Ok = FLinearColor(0.588f, 0.706f, 0.416f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Warn = FLinearColor(0.851f, 0.604f, 0.306f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Bad = FLinearColor(0.812f, 0.416f, 0.322f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor Info = FLinearColor(0.541f, 0.659f, 0.784f);

	/** 金色标题文字色（#FFE8C0，比 gold 亮一档） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Base")
	FLinearColor TitleGold = FLinearColor(1.0f, 0.910f, 0.753f);

	// === 表面色（§2 表面色板）===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor CardTop = FLinearColor(0.180f, 0.141f, 0.075f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor CardBottom = FLinearColor(0.133f, 0.102f, 0.055f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor ButtonTop = FLinearColor(0.200f, 0.153f, 0.086f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor ButtonBottom = FLinearColor(0.141f, 0.106f, 0.055f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor Selection = FLinearColor(0.361f, 0.251f, 0.125f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor HoverBackground = FLinearColor(0.541f, 0.373f, 0.165f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor GroupBackground = FLinearColor(0.078f, 0.055f, 0.027f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor InputBackground = FLinearColor(0.102f, 0.075f, 0.047f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor Track = FLinearColor(0.090f, 0.059f, 0.027f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor GoldButtonTop = FLinearColor(0.541f, 0.373f, 0.165f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor GoldButtonBottom = FLinearColor(0.373f, 0.251f, 0.125f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor ExitButtonTop = FLinearColor(0.353f, 0.180f, 0.110f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors|Surface")
	FLinearColor ExitButtonBottom = FLinearColor(0.243f, 0.125f, 0.071f);

	// === 文本角色（§3）===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle PanelTitleStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle SectionTitleStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle ItemLabelStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle BodyStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle HintStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle DataValueStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle ButtonTextStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TextRoles")
	FVoxelTextRoleStyle KbdHintStyle;

	// === 控件样式（§4.5/4.6）===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FVoxelSliderStyleDesc SliderStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FVoxelInputStyleDesc InputStyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	FVoxelComboStyleDesc ComboStyle;

	// === 布局令牌（统一改尺寸只改这里） ===

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FVoxelUILayoutTokens Layout;

	/** 搜索框图标贴图（搜索框控件用；留空则隐藏图标位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls")
	TSoftObjectPtr<UTexture2D> SearchIconTexture;

	/** 搜索框图标边长（px） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controls", meta = (ClampMin = 4.f))
	float SearchIconSize = 14.f;
};
