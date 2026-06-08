// BranchPIEComponent.cpp — SIB-36 PIE consumers of UBranchSubsystem.

#include "BranchPIEComponent.h"
#include "BranchSubsystem.h"
#include "BookPickup.h"
#include "RefuserController.h"

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
	AActor* Owner = GetOwner();
	UCameraComponent* Cam = Owner ? Owner->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Cam)
	{
		return;
	}
	const float Sat = FMath::Clamp(1.0f - Depth * SaturationPerDepth, 0.0f, 1.0f);
	Cam->PostProcessBlendWeight = 1.0f;
	Cam->PostProcessSettings.bOverride_ColorSaturation = true;
	Cam->PostProcessSettings.ColorSaturation = FVector4(Sat, Sat, Sat, 1.0f); // (1,1,1,1) = unchanged
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
			FString::Printf(TEXT("BRANCH x%d   [F7] merge   [F8] discard"), Depth));
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
