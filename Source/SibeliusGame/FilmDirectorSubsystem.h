// FilmDirectorSubsystem.h
//
// THE DIRECTOR. Plays a film from FilmCatalog in whatever level he is standing in, with
// whichever agents are there. See FilmTypes.h for what a film is.
//
// WHAT IT TAKES OVER, AND HANDS BACK ON EVERY EXIT (end of film, Esc, level teardown):
//   - the view            a spawned ACameraActor, cut per shot; his own view returns on a blend
//                         (it carries a small eye light, on in close and medium shots only)
//   - the cast's dances   played straight onto the dance mesh, NOT through PlayDance, so a
//                         film never overwrites the dance he chose for her (PlayDance saves)
//   - his pawn            hidden, so a wide shot does not find him standing in it
//   - the HUD             a HoldCinematic lease renewed every tick; it lapses by itself
//   - the keyboard        UFilmScreenWidget takes it, so he cannot walk about mid-film
//
// A subsystem and not a component for the reason DancerAgentComponent.h records at length:
// components attached to a live MetaHuman at runtime do not tick. A tickable world
// subsystem does, and it needs nothing placed in any level.
//
// Console: sib.Film.List, sib.Film.Play [id], sib.Film.Generate [about what], sib.Film.Stop.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FilmTypes.h"
#include "FilmDirectorSubsystem.generated.h"

class ACameraActor;
class UAnimationAsset;
class UDancerAgentComponent;
class UFilmScreenWidget;
class USkeletalMeshComponent;

UCLASS()
class SIBELIUSGAME_API UFilmDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Start a film. Star is the agent whose menu it was chosen from; null takes the agent
	 * nearest the player. False, with one log line saying why, when there is no such film
	 * or nobody in the level to be in it.
	 */
	bool Play(FName FilmId, UDancerAgentComponent* Star = nullptr);

	/** Play a film that is not in the catalog - one [G] has just written. It is copied,
	    so the caller's may go out of scope. Same false as Play. */
	bool PlayFilm(const FFilm& InFilm, UDancerAgentComponent* Star = nullptr);

	/** Write a film from a wish (FilmGenerator.h) and play it. An empty wish is a film
	    about something of the generator's choosing. */
	bool PlayGenerated(const FString& Wish, UDancerAgentComponent* Star = nullptr);

	/** End it now and give everything back. Safe when nothing is playing. */
	void Stop();

	/** The player's Esc. Ignored for the first moments, so the key that started the film
	    cannot also end it. */
	void Skip();

	bool IsPlaying() const { return Film != nullptr; }

	// ---- what the screen draws (UFilmScreenWidget polls these) ----
	bool IsCard() const;
	const FString& GetCaption() const { return Caption; }
	/** 0 clear .. 1 black. Cards are black; shots fade through black next to a card. */
	float GetBlackAlpha() const;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return Film != nullptr; }
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

	/** Real gameplay worlds only, as UDancerAgentSubsystem. */
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FCastMember
	{
		TWeakObjectPtr<UDancerAgentComponent> Agent;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		// What she was doing before the film, to be put back exactly.
		TWeakObjectPtr<UAnimationAsset> SavedAnim;
		float SavedPosition = 0.0f;
		float SavedRate = 1.0f;
	};

	void StartShot(int32 Index);
	void UpdateCamera(float DeltaTime);
	const FFilmShot* CurrentShot() const;
	FCastMember* LeadOf(const FFilmShot& Shot);

	/** The point the shot looks at: between the middle of her and her head. */
	static FVector TargetPoint(const USkeletalMeshComponent& Mesh, float HeadBias);

	const FFilm* Film = nullptr;
	int32 ShotIndex = 0;
	float ShotTime = 0.0f;
	float FilmTime = 0.0f;

	TArray<FCastMember> Troupe;
	FString StarName;
	FString Caption;

	/** Her facing when the shot began. Read once per shot: a dance turns her, and a
	    camera that re-read it every frame would swing round the room with her hips. */
	FVector ShotFacing = FVector::ForwardVector;

	/** The look-at point, eased. She moves fast; the operator follows, he does not twitch. */
	FVector SmoothedTarget = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UFilmScreenWidget> Screen;

	TWeakObjectPtr<AActor> SavedViewTarget;
	TWeakObjectPtr<APawn> HiddenPawn;
};
