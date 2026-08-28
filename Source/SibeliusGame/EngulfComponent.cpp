// EngulfComponent.cpp — see header for why the Architects have no teeth.

#include "EngulfComponent.h"

#include "BattleFormComponent.h"
#include "RefuserController.h"
#include "SibeliusGame.h"
#include "SibeliusHUD.h"

#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UEngulfComponent::UEngulfComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 10 Hz. A crowd does not close on you inside 16 ms, and this ticks in the office
	// too, where the answer is always zero.
	// EVERY FRAME now, not 10 Hz. The shove has to be applied per frame through
	// AddMovementInput or it arrives in stutters; the expensive overlap is what runs at
	// 10 Hz, throttled inside the tick.
	PrimaryComponentTick.TickInterval = 0.0f;
}

ACharacter* UEngulfComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

void UEngulfComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* C = GetCharacterOwner())
	{
		if (const UCharacterMovementComponent* Move = C->GetCharacterMovement())
		{
			// Captured once. Everything after this scales FROM the real value, so the
			// slow can never ratchet the player's speed permanently downward — the bug
			// every multiplicative slow has when it forgets where it started.
			BaseWalkSpeed = Move->MaxWalkSpeed;
		}
	}
}

float UEngulfComponent::GetOverruleFraction() const
{
	return OverruleSeconds > 0.0f ? FMath::Clamp(OverruleClock / OverruleSeconds, 0.0f, 1.0f) : 0.0f;
}

/* ONE OVERLAP, NOT AN ITERATION - see the header. The physics scene already knows who is
   within three metres; walking 400 actors to re-derive it would cost more the bigger the
   army gets, which is precisely the shape of mistake the navigation invokers just made. */
int32 UEngulfComponent::CountCrowd()
{
	ACharacter* C = GetCharacterOwner();
	UWorld* World = GetWorld();
	if (!C || !World)
	{
		return 0;
	}

	const FVector Here = C->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(Engulf), false, C);
	World->OverlapMultiByChannel(Overlaps, Here, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(EngulfRadius), Q);

	int32 Count = 0;
	FVector Away = FVector::ZeroVector;

	for (const FOverlapResult& O : Overlaps)
	{
		ACharacter* Other = Cast<ACharacter>(O.GetActor());
		// The SAME test the slap uses to decide what is a Refuser. Two different answers
		// to "is this one of Mrs. Hall's" would eventually disagree, and the one that
		// disagreed would be whichever was not being looked at.
		if (!Other || Other == C || Cast<ARefuserController>(Other->GetController()) == nullptr)
		{
			continue;
		}
		++Count;

		/* UNIT VECTORS, SUMMED. Evenly surrounded, these cancel and the net shove is
		   nothing - you are pinned, not thrown, which is the right feeling and is not
		   special-cased anywhere. Pressed from one side, the sum points away from the
		   press and moves you off your ground. */
		const FVector Delta = Here - Other->GetActorLocation();
		Away += Delta.GetSafeNormal2D();
	}

	/* Cache the direction and how lopsided it is; ApplyShove spends it every frame.
	   Away.Size() is a COUNT of how one-sided the press is, not a speed - reading it as
	   cm/s is what launched him downfield. */
	ShoveDir = Away.GetSafeNormal2D();
	ShoveMag = (Count > PressureFloor) ? Away.Size() : 0.0f;

	return Count;
}

/* THROUGH THE MOVEMENT COMPONENT, not AddActorWorldOffset - and this is the half that
   fixes the skating as well as the launching.

   AddActorWorldOffset teleports a pawn a small distance every frame. It produces NO
   velocity, so the AnimBlueprint sees Speed = 0 and plays Idle while the world slides
   past: exactly the skating Greystone did while being blown across the meadow.
   AddMovementInput goes through CharacterMovement, which means it accelerates him,
   gives him real velocity, respects the ground, and ANIMATES. */
void UEngulfComponent::ApplyShove()
{
	ACharacter* C = GetCharacterOwner();
	if (!C || ShoveMag <= 0.0f || ShoveDir.IsNearlyZero())
	{
		return;
	}
	const float Scale = FMath::Min(ShoveMag * ShovePerRefuser, MaxShoveFraction);
	C->AddMovementInput(ShoveDir, Scale);
}

void UEngulfComponent::ApplySlow()
{
	ACharacter* C = GetCharacterOwner();
	if (!C || BaseWalkSpeed < 0.0f)
	{
		return;
	}
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const int32 Over = FMath::Max(0, Pressure - PressureFloor);
	const float Fraction = FMath::Max(MinSpeedFraction, 1.0f - Over * SlowPerRefuser);
	Move->MaxWalkSpeed = BaseWalkSpeed * Fraction;
}

void UEngulfComponent::NoteSwing()
{
	LastSwingTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UEngulfComponent::Overrule()
{
	OverruleClock = 0.0f;
	bWarnedThisSiege = false;

	UE_LOG(LogSibeliusGame, Display, TEXT("[Engulf] overruled at pressure %d."), Pressure);

	/* THEY DO NOT KILL HIM. They outnumber him and put him back in his place - the avatar
	   drops and he is a pair of eyes again. That is the sentence this whole mechanic
	   exists to say, so it is the only thing that happens here. */
	if (AActor* Owner = GetOwner())
	{
		if (UBattleFormComponent* Battle = Owner->FindComponentByClass<UBattleFormComponent>())
		{
			if (Battle->IsInBattleForm())
			{
				Battle->ExitBattleForm();
			}
		}
	}

	ASibeliusHUD::Toast(this, TEXT("OVERRULED"), 3.0f, SibeliusToast::Bad);
	OnOverruled.Broadcast();
}

void UEngulfComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The overlap at 10 Hz; the shove every frame off the cached direction.
	SinceCount += DeltaTime;
	if (SinceCount >= 0.1f)
	{
		SinceCount = 0.0f;
		Pressure = CountCrowd();
		ApplySlow();
	}
	ApplyShove();

	// Swinging holds them off - see SwingHoldsThemOff. A player still fighting is not
	// being buried, however many of them are touching him.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bStillFighting = (Now - LastSwingTime) < SwingHoldsThemOff;

	if (Pressure >= OverrulePressure && !bStillFighting)
	{
		if (!bWarnedThisSiege)
		{
			// One warning per siege. The slow is the continuous feedback - the player
			// feels it the moment it starts - so this only has to name what is happening.
			bWarnedThisSiege = true;
			ASibeliusHUD::Toast(this, TEXT("SURROUNDED"), 2.5f, SibeliusToast::Warn);
		}
		OverruleClock += DeltaTime;
		if (OverruleClock >= OverruleSeconds)
		{
			Overrule();
		}
	}
	else
	{
		// Backing out is a real way out, not a delay: the clock unwinds faster than it
		// filled. A meter that only ever rises is a countdown wearing a costume.
		OverruleClock = FMath::Max(0.0f, OverruleClock - DeltaTime * RecoveryRate);
		if (OverruleClock <= 0.0f)
		{
			bWarnedThisSiege = false;
		}
	}
}
