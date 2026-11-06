#include "Core/VoxelGameUserSettings.h"
#include "Core/VoxelGameInstance.h"
#include "Engine/Engine.h"

void UVoxelGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);

	// 钳制可见范围
	LoadDistance = FMath::Clamp(LoadDistance, 1, 8);

	// 把可见范围推入当前 GameInstance 的 WorldConfig（ProcessChunkStreaming 现读生效）
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		if (UVoxelGameInstance* GI = Cast<UVoxelGameInstance>(Ctx.OwningGameInstance))
		{
			GI->WorldConfig.LoadDistance = LoadDistance;
			break;
		}
	}
}

void UVoxelGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	LoadDistance = FMath::Clamp(LoadDistance, 1, 8);
}

void UVoxelGameUserSettings::SetQualityPreset(int32 Level)
{
	Level = FMath::Clamp(Level, 0, 3);
	ScalabilityQuality.ViewDistanceQuality = Level;
	ScalabilityQuality.AntiAliasingQuality = Level;
	ScalabilityQuality.ShadowQuality = Level;
	ScalabilityQuality.GlobalIlluminationQuality = Level;
	ScalabilityQuality.ReflectionQuality = Level;
	ScalabilityQuality.PostProcessQuality = Level;
	ScalabilityQuality.TextureQuality = Level;
	ScalabilityQuality.EffectsQuality = Level;
	ScalabilityQuality.FoliageQuality = Level;
	ScalabilityQuality.ShadingQuality = Level;
	ScalabilityQuality.LandscapeQuality = Level;
}

void UVoxelGameUserSettings::ApplyQualityPreset(int32 Level)
{
	SetQualityPreset(Level);
	ApplySettings();
	SaveSettings();
}
