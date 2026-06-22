// TravelTransitionSubsystem.h
//
// ONE travel-transition system for every level-door OpenLevel (the Sauce / Many-Worlds
// door, the O back-to-office key, the cathedral/temple travel doors, the curio return
// doors). On a travel trigger it shows a themed cover (fade to black + animated throbber +
// context line) for immediate "the press registered" feedback, THEN OpenLevel; on the
// destination map load it fades back in. Lives on the GameInstance so it survives the
// level load. The MoviePlayer loading screen that covers the BLOCKING load itself is
// registered separately at module startup (SibeliusGame.cpp), reusing the same widget.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TravelTransitionSubsystem.generated.h"

class STravelShimmerScreen;
class UGameViewportClient;

UCLASS()
class SIBELIUSGAME_API UTravelTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** THE single travel entry point for every door / OpenLevel. Resolves the subsystem from
	    any world-context object and shows the cover, then OpenLevel; falls back to a direct
	    OpenLevel if there is no game instance / viewport (headless / commandlet). */
	static void Travel(const UObject* WorldContext, FName LevelName);

	/** Destination level name -> themed context line, shared by the cover and the MoviePlayer
	    loading screen so both read the same. Substring match (PIE-prefix safe). */
	static FText ContextLineForLevel(const FString& LevelName);

	/** Begin a travel from this subsystem instance (see Travel). */
	void BeginTravel(FName LevelName);

private:
	void OnPostLoadMap(UWorld* LoadedWorld);
	void DoOpenLevel(FName LevelName);
	void RemoveCover();
	UGameViewportClient* GetViewport() const;

	TSharedPtr<STravelShimmerScreen> CoverWidget;
	bool bTravelInProgress = false;
	FTimerHandle OpenLevelTimer;
	FDelegateHandle PostLoadMapHandle;
};
