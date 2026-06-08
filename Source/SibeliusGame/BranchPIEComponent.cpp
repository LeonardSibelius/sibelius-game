// BranchPIEComponent.cpp — SIB-36 PIE consumers of UBranchSubsystem.

#include "BranchPIEComponent.h"
#include "BranchSubsystem.h"
#include "BookPickup.h"
#include "RefuserController.h"
#include "SibeliusGameCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "BrainComponent.h"
#include "EngineUtils.h"            // TActorIterator
#include "Engine/Engine.h"          // GEngine
#include "Engine/World.h"

UBranchPIEComponent::UBranchPIEComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBranchSubsystem* UBranchPIEComponent::GetBranch() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UBranchSubsystem>() : nullptr;
}

void UBranchPIEComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UBranchSubsystem* Branch = GetBranch())
	{
		DepthHandle = Branch->OnBranchDepthChanged.AddUObject(this, &UBranchPIEComponent::OnDepthChanged);
		OnDepthChanged(Branch->GetDepth()); // sync to current state (depth 0 at start)
	}
}

void UBranchPIEComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UBranchSubsystem* Branch = GetBranch())
	{
		Branch->OnBranchDepthChanged.Remove(DepthHandle);
	}
	Super::EndPlay(Reason);
}

void UBranchPIEComponent::OnDepthChanged(int32 Depth)
{
	ApplyDesaturation(Depth);
	UpdateHudMarker(Depth);
	SetPickupsInert(Depth >= 1);
	FreezeRefusers(Depth >= 1);
}

void UBranchPIEComponent::ApplyDesaturation(int32 Depth)
{
	// Target the camera the player view actually renders THROUGH. FindComponentByClass
	// returns the first UCameraComponent on the owner, which isn't guaranteed to be the
	// FP camera — write the wrong one and nothing changes on screen. Pin it to the
	// character's real FP camera; fall back to a generic find only if the owner isn't
	// our character type.
	AActor* Owner = GetOwner();
	UCameraComponent* Cam = nullptr;
	if (ASibeliusGameCharacter* Char = Cast<ASibeliusGameCharacter>(Owner))
	{
		Cam = Char->GetFirstPersonCameraComponent();
	}
	if (!Cam && Owner)
	{
		Cam = Owner->FindComponentByClass<UCameraComponent>();
	}
	if (!Cam)
	{
		return;
	}

	// The camera's own post process blends in LAST (on top of any PostProcessVolume),
	// so this wins as long as the weight is up and each channel is flagged for override.
	Cam->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& PP = Cam->PostProcessSettings;

	if (Depth <= 0)
	{
		// Depth 0 must be EXACTLY normal — release every channel we drive so the
		// office reads as authored (warm), not a faint leftover of the branch grade.
		PP.bOverride_ColorSaturation = false;
		PP.bOverride_ColorContrast = false;
		PP.bOverride_ColorGain = false;
		return;
	}

	// Branched: unmistakably "a different reality".
	//  - saturation collapses hard with depth (depth 1 already strongly drained);
	//  - a cold blue cast fights the warm office lighting;
	//  - a slight contrast lift makes it read crisp/clinical rather than just dull.
	const float Sat = FMath::Clamp(1.0f - Depth * SaturationPerDepth, 0.0f, 1.0f);
	PP.bOverride_ColorSaturation = true;          // flag MUST be set or the value is ignored
	PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0f); // (1,1,1,1) = unchanged, 0 = greyscale

	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.0f); // >1 = crisper midtone separation

	PP.bOverride_ColorGain = true;                // per-channel multiplier: pull warmth, push cold
	PP.ColorGain = FVector4(0.82f, 0.92f, 1.20f, 1.0f); // R down, B up = cold blue cast
}

void UBranchPIEComponent::UpdateHudMarker(int32 Depth)
{
	if (!GEngine)
	{
		return;
	}
	constexpr uint64 MarkerKey = 28036; // stable on-screen slot for the branch marker
	if (Depth >= 1)
	{
		GEngine->AddOnScreenDebugMessage(MarkerKey, 1.0e8f, FColor(190, 160, 255),
			FString::Printf(TEXT("BRANCH x%d   [7] merge   [8] discard"), Depth));
	}
	else
	{
		GEngine->RemoveOnScreenDebugMessage(MarkerKey);
	}
}

void UBranchPIEComponent::SetPickupsInert(bool bInert)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Pickups are suspended while branched (locked product decision), so the set
	// is stable for the branch's duration; iterate and flip each.
	for (TActorIterator<ABookPickup> It(World); It; ++It)
	{
		It->SetInert(bInert);
	}
}

void UBranchPIEComponent::FreezeRefusers(bool bFreeze)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Spike T2: FREEZE dynamic actors, never snapshot them. Halting the pawn's
	// time dilation stops movement/anim; stopping the AI brain stops the chase.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		ARefuserController* RC = Pawn ? Cast<ARefuserController>(Pawn->GetController()) : nullptr;
		if (!RC)
		{
			continue;
		}
		Pawn->CustomTimeDilation = bFreeze ? 0.0f : 1.0f;
		if (UBrainComponent* Brain = RC->GetBrainComponent())
		{
			if (bFreeze)
			{
				Brain->StopLogic(TEXT("BranchReality"));
			}
			else
			{
				Brain->RestartLogic();
			}
		}
	}
}

void UBranchPIEComponent::Toast(const FString& Msg, const FColor& Color) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, Color, Msg);
	}
}

void UBranchPIEComponent::Debug_Enter()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->EnterBranch()
		? FString::Printf(TEXT("EnterBranch -> depth %d"), Branch->GetDepth())
		: TEXT("EnterBranch refused"), FColor::Cyan);
}

void UBranchPIEComponent::Debug_Merge()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->MergeBranch()
		? FString::Printf(TEXT("Merge: kept live -> depth %d"), Branch->GetDepth())
		: TEXT("Merge: nothing to resolve"), FColor::Green);
}

void UBranchPIEComponent::Debug_Discard()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	Toast(Branch->DiscardBranch()
		? FString::Printf(TEXT("Discard: restored -> depth %d"), Branch->GetDepth())
		: TEXT("Discard: nothing to resolve"), FColor::Orange);
}

void UBranchPIEComponent::Debug_Deploy()
{
	UBranchSubsystem* Branch = GetBranch();
	if (!Branch)
	{
		return;
	}
	if (Branch->RequestDeploy())
	{
		Toast(TEXT("Deploy allowed (at Main)"), FColor::Green);
	}
	else
	{
		// Deploy refusal must be legible to the player, not just the log.
		Toast(TEXT("Deploy REFUSED — merge or discard the branch first"), FColor::Red);
	}
}
