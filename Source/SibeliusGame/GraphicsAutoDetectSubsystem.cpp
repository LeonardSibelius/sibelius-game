// GraphicsAutoDetectSubsystem.cpp - see header.

#include "GraphicsAutoDetectSubsystem.h"
#include "SibeliusGame.h"                       // LogSibeliusGame
#include "GameFramework/GameUserSettings.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"

bool UGraphicsAutoDetectSubsystem::CanAutoDetect()
{
	return !GIsEditor && !IsRunningCommandlet() && FApp::CanEverRender();
}

void UGraphicsAutoDetectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!CanAutoDetect())
	{
		return;
	}

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return;
	}

	/* ONCE PER PC, NOT ONCE PER LAUNCH. The benchmark result is a saved setting
	   (GameUserSettings.ini, LastGPUBenchmarkResult), -1 until it has ever run. Anything
	   else means this PC has been measured - and whatever the player chose afterwards on
	   the GRAPHICS tab is theirs and must not be overwritten on the next boot. */
	if (Settings->GetLastGPUBenchmarkResult() >= 0.0f)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Graphics] already measured (GPU %.0f); keeping the saved quality %d."),
			Settings->GetLastGPUBenchmarkResult(), Settings->GetOverallScalabilityLevel());
		return;
	}

	DetectAndApply();
}

int32 UGraphicsAutoDetectSubsystem::DetectAndApply()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings)
	{
		return -1;
	}

	const double Start = FPlatformTime::Seconds();
	Settings->RunHardwareBenchmark();            // fills the recommended quality and the scores
	Settings->ApplyHardwareBenchmarkResults();   // applies them and saves GameUserSettings.ini

	const int32 Overall = Settings->GetOverallScalabilityLevel();
	UE_LOG(LogSibeliusGame, Display,
		TEXT("[Graphics] measured this PC in %.1f s: CPU %.0f, GPU %.0f -> overall quality %d ")
		TEXT("(shadows %d, lighting %d, reflections %d, effects %d)"),
		FPlatformTime::Seconds() - Start,
		Settings->GetLastCPUBenchmarkResult(), Settings->GetLastGPUBenchmarkResult(), Overall,
		Settings->GetShadowQuality(), Settings->GetGlobalIlluminationQuality(),
		Settings->GetReflectionQuality(), Settings->GetVisualEffectQuality());
	return Overall;
}
