// DancerAgentComponent.cpp — see header.

#include "DancerAgentComponent.h"

#include "SibeliusHUD.h"
#include "SibeliusGame.h"                  // LogSibeliusGame

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * The ten Morro mocap dances, retargeted to the MetaHuman skeleton. The raw
	 * MorroMotion assets are on Morro's OWN skeleton and cannot be played on a
	 * MetaHuman — always use the _MH retargets in Content/Characters/Retargeting.
	 *
	 * Buying more dances from Morro: retarget into that folder and add the line here.
	 * Adding it here is what makes it cook (the folder is gitignored).
	 */
	const TCHAR* const DancePaths[] =
	{
		TEXT("/Game/Characters/Retargeting/Anim_High_Rhythm_Dance_13_MH.Anim_High_Rhythm_Dance_13_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_High_Rhythm_Dance_14_MH.Anim_High_Rhythm_Dance_14_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_10_MH.Anim_Mid_Rhythm_Dance_10_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_11_MH.Anim_Mid_Rhythm_Dance_11_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Mid_Rhythm_Dance_12_MH.Anim_Mid_Rhythm_Dance_12_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_11_MH.Anim_Slow_Rhythm_Dance_11_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_12_MH.Anim_Slow_Rhythm_Dance_12_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_13_MH.Anim_Slow_Rhythm_Dance_13_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_14_MH.Anim_Slow_Rhythm_Dance_14_MH"),
		TEXT("/Game/Characters/Retargeting/Anim_Slow_Rhythm_Dance_15_MH.Anim_Slow_Rhythm_Dance_15_MH"),
	};

	/** "BP_Dancer_Kaia_C_2" / "MHC_Nyra" / "BP_MHC_Isla_2" -> "Kaia" / "Nyra" / "Isla". */
	FString PrettyAgentName(const AActor& Actor)
	{
		FString Name = Actor.GetActorNameOrLabel();

		for (const TCHAR* Prefix : { TEXT("BP_Dancer_"), TEXT("BP_MHC_"), TEXT("BP_MH_"),
									 TEXT("BP_"), TEXT("MHC_"), TEXT("MH_") })
		{
			if (Name.StartsWith(Prefix))
			{
				Name.RightChopInline(FCString::Strlen(Prefix));
				break;
			}
		}

		// Trim the instance tail the editor/spawner adds: "_C", "_2", "_C_2".
		Name.RemoveFromEnd(TEXT("_C"));
		while (true)
		{
			int32 Underscore = INDEX_NONE;
			if (!Name.FindLastChar(TEXT('_'), Underscore)) { break; }

			const FString Tail = Name.RightChop(Underscore + 1);
			const bool bDroppable = Tail.IsNumeric() || Tail == TEXT("C");
			if (!bDroppable || Tail.IsEmpty()) { break; }

			Name.LeftInline(Underscore);
		}

		// FIRST NAMES ONLY (Walt, 2026-08-03). The actor labels carry surnames —
		// BP_MHC_NyraSolmere, BP_MHC_IslaRowan — and "I am AI Agent NyraSolmere" reads
		// like a database row rather than someone introducing herself. Cut at the
		// surname boundary: an underscore, or the second capital of a CamelCase name.
		//
		//   NyraSolmere -> Nyra      IslaRowan -> Isla
		//   Kaia -> Kaia             Aisling -> Aisling      Elise -> Elise
		//
		// Set AgentName by hand on a dancer to override this entirely.
		int32 Underscore = INDEX_NONE;
		if (Name.FindChar(TEXT('_'), Underscore) && Underscore > 0)
		{
			Name.LeftInline(Underscore);
		}

		for (int32 i = 1; i < Name.Len(); ++i)
		{
			if (FChar::IsUpper(Name[i]))
			{
				Name.LeftInline(i);
				break;
			}
		}

		return Name.IsEmpty() ? TEXT("Anonymous") : Name;
	}
}

UDancerAgentComponent::UDancerAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Hard references on the CDO — this is what drags the gitignored animations into
	// the pak. See the header. A loop of FObjectFinders is legal inside a constructor.
	for (const TCHAR* Path : DancePaths)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Finder(Path);
		if (Finder.Succeeded())
		{
			Dances.Add(Finder.Object);
		}
	}
}

void UDancerAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AgentName.IsEmpty())
	{
		if (const AActor* Owner = GetOwner())
		{
			AgentName = PrettyAgentName(*Owner);
		}
	}

	// Remember which dance she starts on so the first shuffle is a real change.
	if (const USkeletalMeshComponent* Mesh = FindDanceMesh())
	{
		CurrentDanceIndex = Dances.IndexOfByKey(Mesh->AnimationData.AnimToPlay);
	}
}

bool UDancerAgentComponent::IsKnownDance(const UAnimSequence* Anim)
{
	if (!Anim)
	{
		return false;
	}

	// Compare against the CDO's already-loaded list: a pointer check, not string
	// matching. The scan calls this for every skeletal mesh on every actor every few
	// seconds, and it guarantees "known dance" means exactly "one of the ones the CDO
	// hard-references" — the same list that reaches the pak.
	const UDancerAgentComponent* CDO = GetDefault<UDancerAgentComponent>();
	if (!CDO)
	{
		return false;
	}

	for (const TObjectPtr<UAnimSequence>& Dance : CDO->Dances)
	{
		if (Dance.Get() == Anim)
		{
			return true;
		}
	}
	return false;
}

USkeletalMeshComponent* UDancerAgentComponent::FindDanceMesh() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);

	// Prefer the one already playing a dance we know — on a MetaHuman that is "Body",
	// and it skips the face/hair/clothing meshes, which share the actor.
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && IsKnownDance(Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay)))
		{
			return Mesh;
		}
	}

	// She has been shuffled to a dance that is no longer in the list, or a manual
	// component was added before she was dancing: fall back to a component named Body.
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetName().Contains(TEXT("Body")))
		{
			return Mesh;
		}
	}

	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

FText UDancerAgentComponent::GetPrompt() const
{
	return FText::FromString(FString::Printf(
		TEXT("[E] talk to %s     [F] change her dance"),
		AgentName.IsEmpty() ? TEXT("her") : *AgentName));
}

void UDancerAgentComponent::Greet()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString Line = FString::Printf(
		TEXT("Hi.  I am AI Agent %s.  Wanna Fight?  (I don't really fight, I just dance)"),
		*AgentName);

	// The HUD's subtitle channel, NOT AddOnScreenDebugMessage — screen debug messages
	// are suppressed in Shipping builds, so a debug-message greeting would be invisible
	// to every actual player.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (ASibeliusHUD* HUD = Cast<ASibeliusHUD>(PC->GetHUD()))
		{
			HUD->ShowPresenceLine(Line, GreetingSeconds);
			return;
		}
	}

	// The Elsewhere and the Carousel run their own HUD classes, which have no subtitle
	// channel. Rather than say nothing, log it — and this is why the office/temple/
	// cathedral, where the dancers actually live, all use ASibeliusHUD.
	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s greeted, but this level's HUD has no subtitle channel"), *AgentName);
}

bool UDancerAgentComponent::ShuffleDance()
{
	if (Dances.Num() < 2)
	{
		UE_LOG(LogSibeliusGame, Warning,
			TEXT("[Dancer] %s has %d dance(s) — nothing to shuffle. Are the _MH retargets missing?"),
			*AgentName, Dances.Num());
		return false;
	}

	// One press, one dance. F repeats every frame while held: Walt's log shows a single
	// brief press producing thirteen changes across consecutive frames 389-401, which
	// reads as the dance strobing rather than switching.
	//
	// A repeat returns TRUE, not false — "handled, nothing to do". Returning false would
	// drop through to the fight, so holding F next to a dancer would start swinging at
	// her, which is the one outcome this whole feature exists to prevent.
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastShuffleTime < ShuffleCooldown)
	{
		return true;
	}

	USkeletalMeshComponent* Mesh = FindDanceMesh();
	if (!Mesh)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[Dancer] %s has no skeletal mesh to dance with"), *AgentName);
		return false;
	}

	// Pick a different one: roll across the OTHER n-1 slots, then step past the
	// current index. No rejection loop, so it cannot spin.
	int32 NextIndex = FMath::RandRange(0, Dances.Num() - 2);
	if (CurrentDanceIndex != INDEX_NONE && NextIndex >= CurrentDanceIndex)
	{
		++NextIndex;
	}

	UAnimSequence* NextDance = Dances.IsValidIndex(NextIndex) ? Dances[NextIndex].Get() : nullptr;
	if (!NextDance)
	{
		return false;
	}

	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(NextDance, /*bLooping=*/true);
	CurrentDanceIndex = NextIndex;
	LastShuffleTime = Now;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Dancer] %s -> %s"), *AgentName, *NextDance->GetName());
	return true;
}
