// InteractorComponent.cpp

#include "InteractorComponent.h"

#include "Interactable.h"
#include "DancerAgentComponent.h"
#include "DancerAgentSubsystem.h"
#include "Components/SkeletalMeshComponent.h"   // a dancer's posed bounds
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "SibeliusAim.h"           // rays start level with his eyes, even in third person
#include "CollisionQueryParams.h"
#include "HAL/IConsoleManager.h"   // sib.Chat.Enable
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SibeliusHUD.h"   // the prompt draws on the HUD canvas so it survives packaging
#include "HAL/PlatformTime.h"

namespace WebSlotInteraction
{
const FName TVTag(TEXT("CelestialFortuneWebTV"));
bool IsTV(const AActor* Actor) { return Actor && Actor->ActorHasTag(TVTag); }
}

namespace InteractorDetail
{
	/* A PART OF AN INTERACTIVE ACTOR THAT IS ONLY A SURFACE. The basketball court's floor
	   blocks Visibility so Manny's foot IK can stand on it (BasketballCourt.cpp, Solid), and
	   it belongs to the court, which answers E. A hit on a component with this tag counts as
	   furniture, like a tabletop: it stops the ray, and offers nothing. */
	const FName NotInteractiveTag(TEXT("NotInteractive"));

	// Where a dancer's body is this frame, as a vertical capsule: her aim point's spot on the
	// floor plan (her chest, or the shape she wears - GetAimPoint), from her feet to the top
	// of her head. From her bones, never her bounds, which stay where she started.
	struct FBodyCapsule { FVector Bottom, Top; };
	bool BodyCapsule(const UDancerAgentComponent& Agent, FBodyCapsule& Out)
	{
		const USkeletalMeshComponent* Body = Agent.FindDanceMesh();
		if (!Body)
		{
			return false;
		}
		auto Bone = [Body](const TCHAR* Name, FVector& At)
		{
			const int32 Index = Body->GetBoneIndex(Name);
			if (Index != INDEX_NONE)
			{
				At = Body->GetBoneTransform(Index).GetLocation();
			}
			return Index != INDEX_NONE;
		};
		FVector Head, FootL, FootR, Chest;
		if (!Bone(TEXT("head"), Head) || !Bone(TEXT("foot_l"), FootL) || !Bone(TEXT("foot_r"), FootR))
		{
			return false;
		}
		const FVector Middle = Agent.GetAimPoint();
		// A shape she wears may stand taller than she does (a Paragon hero): his chest says so.
		const double Rise = Bone(TEXT("spine_03"), Chest) ? FMath::Max(0.0, Middle.Z - Chest.Z) : 0.0;
		Out.Bottom = FVector(Middle.X, Middle.Y, FMath::Min(FootL.Z, FootR.Z));
		Out.Top = FVector(Middle.X, Middle.Y, Head.Z + 20.0 + Rise);
		return true;
	}

	// The nearest dancer other than Hit whose body the ray passes through within MaxDist of
	// Start, or null (see "A DANCER IN FRONT" in UpdateFocus). Hidden ones are not there, as
	// in the aim assist.
	AActor* DancerOnRay(UWorld* World, const FVector& Start, const FVector& Forward, float MaxDist, const AActor* Hit)
	{
		UDancerAgentSubsystem* Sub = World ? World->GetSubsystem<UDancerAgentSubsystem>() : nullptr;
		if (!Sub || MaxDist <= 0.f)
		{
			return nullptr;
		}
		constexpr double BodyRadius = 40.0;   // shoulders and a dancing arm
		const FVector End = Start + Forward * MaxDist;
		AActor* Best = nullptr;
		double BestAlong = MaxDist;
		for (const TWeakObjectPtr<UDancerAgentComponent>& Weak : Sub->GetDancers())
		{
			const UDancerAgentComponent* Agent = Weak.Get();
			AActor* Owner = Agent ? Agent->GetOwner() : nullptr;
			FBodyCapsule Capsule;
			if (!Owner || Owner == Hit || Owner->IsHidden() || !BodyCapsule(*Agent, Capsule))
			{
				continue;
			}
			FVector OnRay, OnBody;
			FMath::SegmentDistToSegmentSafe(Start, End, Capsule.Bottom, Capsule.Top, OnRay, OnBody);
			if (FVector::Dist(OnRay, OnBody) > BodyRadius)
			{
				continue;
			}
			const double Along = FVector::DotProduct(OnRay - Start, Forward);
			if (Along > 0.0 && Along < BestAlong)
			{
				Best = Owner;
				BestAlong = Along;
			}
		}
		return Best;
	}
}

UInteractorComponent::UInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UInteractorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* Owner = GetOwner())
	{
		CameraComp = Owner->FindComponentByClass<UCameraComponent>();
	}
}

void UInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFocus();
}

void UInteractorComponent::UpdateFocus()
{
	FocusedActor = nullptr;

	UWorld* World = GetWorld();
	if (!World || !CameraComp.IsValid())
	{
		return;
	}

	/* AIM ALONG THE VIEW HE IS ACTUALLY SEEING. This used the first-person camera's own
	   forward, which is right in first person and wrong in battle form: battle form
	   switches that camera off, and an inactive camera stops following the mouse, so on
	   Kaia's planet E aimed wherever he had been looking when he arrived (it reached her
	   only because she stood straight ahead). The player camera manager is the live view in
	   both modes, and SibeliusAim starts the ray level with his eyes, so in first person
	   this is the same ray as before.

	   ONLY WHILE THE VIEW IS HIM. A table, a slot cabinet or a talk close-up takes the view
	   to a camera of its own; aiming from that would put prompts on whatever that camera
	   sees. While one is up, the old ray from his head stands, exactly as it always has. */
	FVector Forward = CameraComp->GetForwardVector();
	FVector Start = CameraComp->GetComponentLocation();
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (PC && PC->PlayerCameraManager && PC->GetViewTarget() == Pawn)
		{
			Forward = PC->PlayerCameraManager->GetCameraRotation().Vector();
			Start = SibeliusAim::RayStart(Pawn, PC->PlayerCameraManager->GetCameraLocation(), Forward);
		}
	}
	const FVector End = Start + Forward * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractTrace), false, GetOwner());
	Params.AddIgnoredActor(GetOwner());

	// Sphere sweep instead of a razor-thin line: a 71-year-old thumb should not
	// need sniper aim to play a slot machine. Radius 0 falls back to a line.
	// SweepMulti: a book on a table loses SweepSingle to the tabletop. Take the
	// first IInteractable at or just behind that first contact (not through a wall).
	TArray<FHitResult> Hits;
	if (InteractRadius > 0.f)
	{
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility,
			FCollisionShape::MakeSphere(InteractRadius), Params);
	}
	else
	{
		FHitResult LineHit;
		if (World->LineTraceSingleByChannel(LineHit, Start, End, ECC_Visibility, Params))
		{
			Hits.Add(LineHit);
		}
	}

	auto IsInteractTarget = [](AActor* A) -> bool
	{
		return A && (WebSlotInteraction::IsTV(A) || A->Implements<UInteractable>()
			|| A->FindComponentByClass<UDancerAgentComponent>() != nullptr);
	};

	AActor* Target = nullptr;
	float TargetDist = 0.f;
	FText Prompt;
	float FurnitureDist = -1.f;
	constexpr float OnSurfaceSlack = 50.f;   // cm past the table/shelf we just hit
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			continue;
		}
		const UPrimitiveComponent* HitComp = Hit.GetComponent();
		const bool bJustSurface = HitComp && HitComp->ComponentHasTag(InteractorDetail::NotInteractiveTag);
		if (!bJustSurface && IsInteractTarget(HitActor))
		{
			if (FurnitureDist < 0.f || Hit.Distance <= FurnitureDist + OnSurfaceSlack)
			{
				Target = HitActor;
				TargetDist = Hit.Distance;
			}
			break;
		}
		if (FurnitureDist < 0.f)
		{
			FurnitureDist = Hit.Distance;
		}
	}

	/* A DANCER IN FRONT OF WHAT THE RAY HIT. A dancer's body has no collision, only a capsule
	   where she started, and a travelling dance carries her off it - so the ray went through
	   Kaia's belly dance to Isla's capsule 2.8 m behind her, and F opened Isla's menu (Walt,
	   2026-10-13: "now i can't change Kaia's dance"). Her bones do follow the dance: if the ray
	   passes through her body before what it hit - another agent, or her own portal behind
	   her - she is who he is aiming at. */
	if (Target)
	{
		if (AActor* Nearer = InteractorDetail::DancerOnRay(World, Start, Forward, TargetDist, Target))
		{
			Target = Nearer;
		}
	}

	// Nothing under the crosshair — try the dancer aim assist. A travelling dance leaves
	// her body outside her stationary capsule, so the trace above misses her even at
	// arm's length (Walt needed seven presses on Nyra). This asks a different question:
	// is a dancer near the middle of the screen and actually visible?
	if (!Target)
	{
		if (AActor* Dancer = FindDancerByAim(Start, Forward))
		{
			Target = Dancer;
			if (const UDancerAgentComponent* Agent = Dancer->FindComponentByClass<UDancerAgentComponent>())
			{
				Prompt = Agent->GetPrompt();
			}
		}
	}

	if (!Target)
	{
		return;
	}

	FocusedActor = Target;
	if (Prompt.IsEmpty())
	{
		if (WebSlotInteraction::IsTV(Target))
		{
			Prompt = NSLOCTEXT("Sibelius", "WebSlotTVPrompt", "E \u2014 Celestial Fortune web version");
		}
		else if (Target->Implements<UInteractable>())
		{
			Prompt = IInteractable::Execute_GetInteractionPrompt(Target);
		}
		else if (const UDancerAgentComponent* Agent = Target->FindComponentByClass<UDancerAgentComponent>())
		{
			Prompt = Agent->GetPrompt();
		}
	}

	/* THE prompt. Every IInteractable in the game arrives here, which is why this one line
	   mattered more than the other seventeen put together.

	   It used to be AddOnScreenDebugMessage, which Shipping COMPILES OUT — so no player
	   who ever downloaded this game has seen an interaction prompt. Perfect in PIE, gone
	   in the build, nothing in any log. The HUD channel draws on the canvas and survives
	   packaging; it self-expires on a short lease, so this still needs no "stop showing"
	   path, exactly as the 0.15s debug message did not. */
	if (!Prompt.IsEmpty())
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		if (ASibeliusHUD* Hud = PC ? Cast<ASibeliusHUD>(PC->GetHUD()) : nullptr)
		{
			Hud->ShowInteractPrompt(Prompt.ToString());
		}
	}
}

AActor* UInteractorComponent::FindDancerByAim(const FVector& Start, const FVector& Forward) const
{
	UWorld* World = GetWorld();
	UDancerAgentSubsystem* Sub = World ? World->GetSubsystem<UDancerAgentSubsystem>() : nullptr;
	if (!Sub)
	{
		return nullptr;
	}

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(DancerAssistAngle));
	const float RangeSq = DancerAssistRange * DancerAssistRange;

	AActor* Best = nullptr;
	float BestDot = MinDot;   // anything accepted must beat the cone, then each other

	for (const TWeakObjectPtr<UDancerAgentComponent>& Weak : Sub->GetDancers())
	{
		const UDancerAgentComponent* Agent = Weak.Get();
		AActor* Owner = Agent ? Agent->GetOwner() : nullptr;
		if (!Owner)
		{
			continue;
		}

		/* A HIDDEN DANCER IS NOT THERE (docs/FUN_PLAN_2.md A4).

		   This search is a cone and a line of sight over the registry — it never needs to
		   HIT her, so hiding an actor does not remove her from it and switching her
		   collision off does not either. Without this, a guide who has stood down after
		   the launch is an invisible woman still answering E from across the street,
		   which is a stranger bug than the one that made her stand down.

		   Written as a general rule rather than a Nyra rule: nothing invisible should ever
		   be talkable, whatever hid it. */
		if (Owner->IsHidden())
		{
			continue;
		}

		// Her mesh bounds, so this tracks the pose rather than the actor origin.
		const FVector AimPoint = Agent->GetAimPoint();
		const FVector ToDancer = AimPoint - Start;

		if (ToDancer.SizeSquared() > RangeSq)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(Forward, ToDancer.GetSafeNormal());
		if (Dot <= BestDot)
		{
			continue;   // outside the cone, or a worse candidate than one we have
		}

		// Line of sight, so a dancer through a wall is not selectable. Ignore her own
		// actor: we are asking whether anything is IN THE WAY, not whether we hit her.
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(DancerSight), false, GetOwner());
		SightParams.AddIgnoredActor(GetOwner());
		SightParams.AddIgnoredActor(Owner);

		FHitResult Blocker;
		if (World->LineTraceSingleByChannel(Blocker, Start, AimPoint, ECC_Visibility, SightParams))
		{
			continue;
		}

		Best = Owner;
		BestDot = Dot;
	}

	return Best;
}

namespace
{
	/**
	 * Typed conversation. ON EVERYWHERE EXCEPT SHIPPING.
	 *
	 * Two switches on purpose: the per-agent bChatEnabled for a level that wants one
	 * talkative dancer, and this console variable for a whole-session test run.
	 *
	 * SHIPPING STAYS 0, AND THAT IS NOT CAUTION - IT IS A STATEMENT ALREADY MADE. The
	 * Steam and itch submissions describe a game that generates nothing at runtime, and
	 * the store build is the one those submissions are about (pkg-v140.log:
	 * clientconfig=Shipping). Changing this default for Shipping is a storefront
	 * decision, not a code one, and it is Walt's to make.
	 *
	 * Everything else - PIE, the editor, and any Development package - now has chat on
	 * with no console line to type. Those builds are not what anybody submitted.
	 */
	TAutoConsoleVariable<int32> CVarChatEnable(
		TEXT("sib.Chat.Enable"),
#if UE_BUILD_SHIPPING
		0,
#else
		1,
#endif
		TEXT("1 lets E open a typed conversation with an AI agent."),
		ECVF_Default);

	bool IsChatAvailable(const UDancerAgentComponent* Agent)
	{
		return Agent && (Agent->bChatEnabled || CVarChatEnable.GetValueOnGameThread() != 0);
	}
}

void UInteractorComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return;
	}

	if (WebSlotInteraction::IsTV(Target))
	{
		// Refresh the trace so pressing E cannot activate a TV after looking away.
		UpdateFocus();
		if (FocusedActor.Get() != Target) return;
		/* SHOW THE ADDRESS, NEVER OPEN IT (Steam review, build 25330029, 2026-09-22).
		   The TV used to LaunchURL the web game, and Steam failed the build for it:
		   a game may not send players out of Steam to play somewhere else. So the TV
		   now puts the address on screen and the player decides what to do with it. */
		// Prevent a held/repeated E from stacking the same toast.
		static double LastShown = -10.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastShown < 2.0) return;
		LastShown = Now;
		ASibeliusHUD::Toast(this, TEXT("Celestial Fortune web version:  leonardsibelius.com/celestial-fortune"),
			8.0f, SibeliusToast::Prize);
		return;
	}
	if (Target->Implements<UInteractable>())
	{
		IInteractable::Execute_Interact(Target, GetOwner());
	}
	else if (UDancerAgentComponent* Agent = Target->FindComponentByClass<UDancerAgentComponent>())
	{
		/* AUTHORED FIRST, ALWAYS (docs/NPC_CHAT_REVIEW.md, step 4).

		   A power still in her hand, or a guide speech not yet heard to the end, owns
		   E — HasAuthoredBusinessPending says which. Only once the authored game has
		   nothing left to do here can a typed conversation have the key, and even then
		   only when chat is switched on, which it is not in a shipping 1.0 build. */
		if (IsChatAvailable(Agent) && !Agent->HasAuthoredBusinessPending())
		{
			Agent->BeginChat();
		}
		else
		{
			Agent->Greet();
		}
	}
}
