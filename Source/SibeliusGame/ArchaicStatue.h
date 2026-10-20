// ArchaicStatue.h
//
// ONE OF THE OLD LANGUAGES, IN MARBLE - the Archaic Language Temple (L_Archaic_Temple), through a
// hidden door by the living room's poker machine. Walt, 2026-10-19: "I would like the old
// languages to be represented by ancient greek statues - immobile figures to contrast with the
// lively AI dancers."
//
// To the agents, every language a person typed by hand is archaic - Python included. So the
// temple keeps five, each a public-domain scan of a Greek cast (SourceArt/Statues/SOURCES.md):
// FORTRAN the Discobolus, COBOL the Doryphoros, BASIC a Kore, Java Zeus of Artemision, Python the
// Laocoon.
//
// A statue never moves and never speaks. It stands on its plinth with a bronze tablet at its
// feet - the language and its year - and E on it shows one line of Walt's memoir about that
// language, the way the power shrines show his messages (ASibeliusHUD::ShowMemoir).
//
// Pick the language in Details; the statue, the tablet and the line follow. LineOverride, when
// set, replaces the line on that one statue.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "ArchaicStatue.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EArchaicLanguage : uint8
{
	Fortran,
	Cobol,
	Basic,
	Java,
	Python
};

UCLASS()
class SIBELIUSGAME_API AArchaicStatue : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AArchaicStatue();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Archaic Statue")
	EArchaicLanguage Language = EArchaicLanguage::Fortran;

	/** This statue's line instead of its language's own (ArchaicStatue.cpp). Empty = the language's. */
	UPROPERTY(EditAnywhere, Category = "Archaic Statue", meta = (MultiLine = true))
	FText LineOverride;

	/** The memoir line E shows. */
	FString Line() const;

	/** The tablet's words: "FORTRAN" over "1957". */
	FString TabletText() const;

	/** The scan for a language, /Game/Statues/... */
	static FString MeshPathFor(EArchaicLanguage InLanguage);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Archaic Statue")
	TObjectPtr<UStaticMeshComponent> Statue;

	/** A bronze plaque at the statue's feet, in front: the statues all face +Y. */
	UPROPERTY(VisibleAnywhere, Category = "Archaic Statue")
	TObjectPtr<UStaticMeshComponent> Tablet;

	UPROPERTY(VisibleAnywhere, Category = "Archaic Statue")
	TObjectPtr<UTextRenderComponent> TabletWords;
};
