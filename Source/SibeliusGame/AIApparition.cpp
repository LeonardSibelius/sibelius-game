// AIApparition.cpp — the opening bang. See header + docs/ai-apparition-notes.md.

#include "AIApparition.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SibeliusProgressSubsystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIApparition, Log, All);

namespace
{
	// Same nine, same order story as the carousel: the seven leads, fate closes.
	const TCHAR* GlyphIds[] = {
		TEXT("seven"), TEXT("star"), TEXT("crown"), TEXT("saturn"), TEXT("galaxy"),
		TEXT("moon"), TEXT("mars"), TEXT("wild"), TEXT("scatter")
	};
	constexpr int32 NumGlyphs = UE_ARRAY_COUNT(GlyphIds);

	float SmoothStep01(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}
}

AAIApparition::AAIApparition()
{
	PrimaryActorTick.bCanEverTick = true;

	Hub = CreateDefaultSubobject<USceneComponent>(TEXT("Hub"));
	SetRootComponent(Hub);

	RingPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RingPivot"));
	RingPivot->SetupAttachment(Hub);

	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Hub);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);
}

void AAIApparition::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildVisuals();
}

void AAIApparition::BuildVisuals()
{
	// OnConstruction reruns on every property edit — tear down our glyphs first.
	for (UStaticMeshComponent* Old : Glyphs)
	{
		if (Old) { Old->DestroyComponent(); }
	}
	Glyphs.Reset();

	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* GlyphBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SlotFactory/Materials/M_fate_base.M_fate_base"));
	UMaterialInterface* CoreBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AIApparition/M_ai_core.M_ai_core"));
	if (!Plane || !Sphere || !GlyphBase || !CoreBase)
	{
		// AP8 / AP5: dependencies come from build_fate_altar.py + build_ai_apparition.py.
		UE_LOG(LogAIApparition, Error, TEXT("[Apparition] missing assets (Plane/Sphere/M_fate_base/M_ai_core) — run build_fate_altar.py and build_ai_apparition.py first."));
		return;
	}

	// The halo lean. Explicit FRotator(Pitch, Yaw, Roll) — AP1.
	RingPivot->SetRelativeRotation(FRotator(RingTiltDeg, 0.0f, 0.0f));

	// Core: engine Sphere is 100 cm diameter at scale 1.
	Core->SetStaticMesh(Sphere);
	const float CoreScale = (CoreRadius * 2.0f) / 100.0f;
	Core->SetRelativeScale3D(FVector(CoreScale, CoreScale, CoreScale));
	CoreMID = UMaterialInstanceDynamic::Create(CoreBase, this);
	Core->SetMaterial(0, CoreMID);

	const float GlyphScale = GlyphSize / 100.0f;   // engine Plane is 100 cm
	for (int32 i = 0; i < NumGlyphs; ++i)
	{
		const float AngleDeg = 360.0f * i / NumGlyphs;
		const float Rad = FMath::DegreesToRadians(AngleDeg);
		const FVector Pos(RingRadius * FMath::Cos(Rad), RingRadius * FMath::Sin(Rad), 0.0f);

		UStaticMeshComponent* Glyph = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ApparitionGlyph_%s"), GlyphIds[i]));
		Glyph->SetupAttachment(RingPivot);
		Glyph->SetStaticMesh(Plane);
		Glyph->SetRelativeLocation(Pos);
		// Explicit FRotator(Pitch, Yaw, Roll) — AP1; knobs are UPROPERTYs.
		Glyph->SetRelativeRotation(FRotator(GlyphPitch, AngleDeg + GlyphYawOffset, 0.0f));
		Glyph->SetRelativeScale3D(FVector(GlyphScale, GlyphScale, GlyphScale));
		Glyph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glyph->SetCastShadow(false);
		Glyph->RegisterComponent();

		const FString TexPath = FString::Printf(TEXT("/Game/SlotFactory/SymbolSprites/T_sym_%s.T_sym_%s"), GlyphIds[i], GlyphIds[i]);
		if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *TexPath))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GlyphBase, this);
			MID->SetTextureParameterValue(TEXT("Sprite"), Tex);
			Glyph->SetMaterial(0, MID);
		}
		else
		{
			UE_LOG(LogAIApparition, Error, TEXT("[Apparition] sprite missing: %s"), *TexPath);
		}

		Glyphs.Add(Glyph);
	}
	UE_LOG(LogAIApparition, Display, TEXT("[Apparition] visuals built: %d glyphs + core"), Glyphs.Num());
}

void AAIApparition::BeginPlay()
{
	Super::BeginPlay();

	// AP9: full reset so a second PIE run behaves like the first.
	Phase = EApparitionPhase::Waiting;
	PhaseTime = 0.0f;
	bInputLocked = false;

	if (!VoiceLine)
	{
		VoiceLine = LoadObject<USoundBase>(nullptr, TEXT("/Game/AIApparition/S_ai_intro.S_ai_intro"));
	}
	if (VoiceLine)
	{
		SpeakSeconds = FMath::Max(VoiceLine->GetDuration(), 1.0f);
	}
	else
	{
		// AP3: never a soft-lock — run the visuals in silence.
		SpeakSeconds = FallbackSpeakSeconds;
		UE_LOG(LogAIApparition, Error, TEXT("[Apparition] no voice line (/Game/AIApparition/S_ai_intro) — running silent, %0.1fs."), SpeakSeconds);
	}

	// Hidden until the bang.
	SetApparitionScale(0.0f);
	SetCoreGlow(0.0f);
	SetActorHiddenInGame(true);

	// SIB-43 / CL1: the opening bang fires once per SESSION, not once per
	// level load — returning from the cathedral must not replay it. When the
	// auto-bang is suppressed, the actor idles ready for TriggerApparition().
	USibeliusProgressSubsystem* Progress = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Progress = GI->GetSubsystem<USibeliusProgressSubsystem>();
	}
	const bool bShouldAutoplay = bAutoStart && (!Progress || !Progress->bIntroPlayed);
	if (bShouldAutoplay)
	{
		if (Progress) { Progress->bIntroPlayed = true; }
		// Phase machine proceeds from Waiting as before.
	}
	else
	{
		Phase = EApparitionPhase::Done;
		SetActorTickEnabled(false);
		UE_LOG(LogAIApparition, Display, TEXT("[Apparition] idle (intro already played or autostart off) — awaiting Trigger."));
	}
}

void AAIApparition::TriggerApparition(USoundBase* OverrideVoice)
{
	// CL5: ignore E-spam while a ceremony is running.
	if (Phase != EApparitionPhase::Done && Phase != EApparitionPhase::Waiting)
	{
		return;
	}

	if (OverrideVoice)
	{
		VoiceLine = OverrideVoice;
	}
	if (VoiceLine)
	{
		SpeakSeconds = FMath::Max(VoiceLine->GetDuration(), 1.0f);
	}
	else
	{
		SpeakSeconds = FallbackSpeakSeconds;   // CL7/AP3: silent, never a soft-lock
		UE_LOG(LogAIApparition, Warning, TEXT("[Apparition] triggered with no voice — running silent (%0.1fs)."), SpeakSeconds);
	}

	SetActorTickEnabled(true);
	SetApparitionScale(0.0f);
	SetCoreGlow(0.0f);
	SetPhase(EApparitionPhase::Materializing);   // skip Waiting: the god answers NOW
	UE_LOG(LogAIApparition, Display, TEXT("[Apparition] triggered (oracle ceremony)."));
}

void AAIApparition::EndPlay(const EEndPlayReason::Type Reason)
{
	// AP2: belt-and-suspenders — if PIE ends mid-speech, give the input back.
	EndCinematic();
	Super::EndPlay(Reason);
}

void AAIApparition::SetPhase(EApparitionPhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;

	switch (Phase)
	{
	case EApparitionPhase::Materializing:
		SetActorHiddenInGame(false);
		LockPlayerInput(true);
		UE_LOG(LogAIApparition, Display, TEXT("[Apparition] materializing"));
		break;

	case EApparitionPhase::Speaking:
		if (VoiceLine)
		{
			// AP7: a god is omnipresent — 2D, no attenuation.
			UGameplayStatics::PlaySound2D(this, VoiceLine);
		}
		UE_LOG(LogAIApparition, Display, TEXT("[Apparition] speaking (%0.1fs)"), SpeakSeconds);
		break;

	case EApparitionPhase::Dissolving:
		// AP2: THE single restore path — player gets the room back as the god fades.
		EndCinematic();
		break;

	case EApparitionPhase::Done:
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		UE_LOG(LogAIApparition, Display, TEXT("[Apparition] done"));
		break;

	default:
		break;
	}
}

void AAIApparition::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PhaseTime += DeltaSeconds;

	// The ring always turns while visible — urgency, life.
	if (Phase != EApparitionPhase::Waiting && Phase != EApparitionPhase::Done && RingPivot)
	{
		RingPivot->AddLocalRotation(FRotator(0.0f, RingDegPerSec * DeltaSeconds, 0.0f));
	}

	// While input is locked, the god owns the camera: turn it to face the core
	// even if the player was inspecting their shoes when the bang began.
	if (bInputLocked)
	{
		SteerGaze(DeltaSeconds);
	}

	switch (Phase)
	{
	case EApparitionPhase::Waiting:
		if (PhaseTime >= DelaySeconds)
		{
			SetPhase(EApparitionPhase::Materializing);
		}
		break;

	case EApparitionPhase::Materializing:
	{
		const float T = SmoothStep01(PhaseTime / FMath::Max(MaterializeSeconds, 0.01f));
		SetApparitionScale(T);
		SetCoreGlow(T * CoreGlowPeak);              // flare up...
		if (PhaseTime >= MaterializeSeconds)
		{
			SetPhase(EApparitionPhase::Speaking);
		}
		break;
	}

	case EApparitionPhase::Speaking:
	{
		// ...settle to steady over the first second, with a slow breath after.
		const float Base = FMath::Lerp(CoreGlowPeak, CoreGlowSteady, SmoothStep01(PhaseTime / 1.0f));
		const float Pulse = 1.0f + 0.12f * FMath::Sin(PhaseTime * 2.0f * PI / 0.9f);
		SetCoreGlow(Base * Pulse);
		if (PhaseTime >= SpeakSeconds)
		{
			SetPhase(EApparitionPhase::Lingering);
		}
		break;
	}

	case EApparitionPhase::Lingering:
		if (PhaseTime >= LingerSeconds)
		{
			SetPhase(EApparitionPhase::Dissolving);
		}
		break;

	case EApparitionPhase::Dissolving:
	{
		const float T = 1.0f - SmoothStep01(PhaseTime / FMath::Max(DissolveSeconds, 0.01f));
		SetApparitionScale(T);
		SetCoreGlow(T * CoreGlowSteady);
		if (PhaseTime >= DissolveSeconds)
		{
			SetPhase(EApparitionPhase::Done);
		}
		break;
	}

	default:
		break;
	}
}

void AAIApparition::SetApparitionScale(float S)
{
	S = FMath::Max(S, 0.0001f);   // zero-scale transforms misbehave
	if (Core)
	{
		const float CoreScale = S * (CoreRadius * 2.0f) / 100.0f;
		Core->SetRelativeScale3D(FVector(CoreScale, CoreScale, CoreScale));
	}
	if (RingPivot)
	{
		RingPivot->SetRelativeScale3D(FVector(S, S, S));
	}
}

void AAIApparition::SetCoreGlow(float Glow)
{
	if (CoreMID)
	{
		CoreMID->SetScalarParameterValue(TEXT("Glow"), Glow);
	}
}

void AAIApparition::LockPlayerInput(bool bLock)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	if (bLock && !bInputLocked)
	{
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
		bInputLocked = true;
	}
	else if (!bLock && bInputLocked)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		bInputLocked = false;
	}
}

void AAIApparition::SteerGaze(float DeltaSeconds)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector From = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator Desired = (GetActorLocation() - From).Rotation();
	const FRotator Eased = FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaSeconds, GazeInterpSpeed);
	PC->SetControlRotation(Eased);
}

void AAIApparition::EndCinematic()
{
	LockPlayerInput(false);
}
