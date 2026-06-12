// CathedralDoor.h
//
// SIB-31 — Ch7 entry. The attic door into the cathedral. E (shared IInteractable
// system) travels to TargetLevelName — REFUSED while branched, same guard reasoning
// as RequestDeploy: level travel would strand the snapshot stack (merge or discard
// first).
//
// Deliberately NOT IBranchable: the door must never appear in deploy saves (no
// GUID, no orphan noise on reload).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "Interactable.h"
#include "CathedralDoor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBranchSubsystem;

UCLASS()
class SIBELIUSGAME_API ACathedralDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ACathedralDoor();

	// IInteractable: travel on E (guarded); prompt while focused.
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;

	// The travel guard, factored static so the smoke test can drive it against a
	// NewObject'd subsystem headless (the BranchSmoke pattern). Null subsystem
	// (headless / non-game world) = allowed.
	static bool IsTravelAllowed(const UBranchSubsystem* Branch);

	bool IsRevealed() const { return bRevealed; }

	// Level to open on use. D2: the smoke test asserts this package exists on disk,
	// because OpenLevel on a bad name is a silent no-op.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	FName TargetLevelName = TEXT("L_Cathedral");

	// SIB-43 / CL10: prompt promoted to a property so the cathedral's RETURN
	// door can say where it goes. Default preserves the attic door verbatim.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	FText PromptText = NSLOCTEXT("Sibelius", "CathedralDoorPrompt", "Enter the cathedral [E]");

	// SIB-43 (Walt QA): arriving players spawn within reach of the return
	// door — a reflexive E bounced him straight back. The door refuses to
	// travel for this many seconds after level load. Invisible in normal play.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	float TravelGraceSeconds = 2.5f;

	// When true the door stays hidden + non-interactable until the player's
	// UGenerateComponent has spawned at least once this session. v1 ships ungated.
	UPROPERTY(EditAnywhere, Category = "Cathedral Door")
	bool bRequireGenerateUse = false;

	UPROPERTY(VisibleAnywhere, Category = "Cathedral Door")
	TObjectPtr<USceneComponent> SceneRoot;

	// Mesh assigned in editor (SM_Door_Cathedral_Huge). BlockAll, so it blocks the
	// interactor's ECC_Visibility focus trace.
	UPROPERTY(VisibleAnywhere, Category = "Cathedral Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

protected:
	virtual void BeginPlay() override;

private:
	// ONE path drives visual + collision together (the CV4/CV8 lesson): a gated
	// door drops collision too, so the focus trace can't reach it.
	void ApplyRevealed(bool bNowRevealed);

	// Low-rate poll for the generate gate (runs only while bRequireGenerateUse
	// keeps the door hidden; cleared on reveal).
	void PollGenerateGate();

	UBranchSubsystem* GetBranch() const;

	bool bRevealed = true;

	double TravelReadyTime = 0.0;   // world seconds; set in BeginPlay

	FTimerHandle GatePollHandle;
};
