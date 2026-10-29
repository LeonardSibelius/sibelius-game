#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "DragonsWagerRules.h"
#include "DragonsWager.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class USpotLightComponent;
class UTextRenderComponent;
class APlayerController;
class APawn;
class UMeshComponent;

USTRUCT()
struct FWagerGoldSnapshot
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UMeshComponent> Mesh;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> Materials;
};

/** On-foot keep game. A single runtime actor, no edits to the vendor map.
 * All positions are world space and can be overridden for a placed actor.
 * Start/return use the existing E interface; K decoy, B bank, T raise, X quit.
 */
UCLASS()
class SIBELIUSGAME_API ADragonsWager : public AActor, public IInteractable
{
    GENERATED_BODY()
public:
    ADragonsWager();
    virtual void Tick(float DeltaSeconds) override;
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;
    static void EnsureInKeep(UWorld* World);
    /** Read-only diagnostic snapshot for editor integration tests. */
    UFUNCTION(BlueprintCallable, Category="Dragon's Wager")
    FString GetWagerState() const;
    /** sib.Wager.Key: K, B, T or X through the same handlers and gates as the keyboard. */
    void DevKey(const FString& Key);

    UPROPERTY(EditAnywhere, Category="Dragon's Wager") FVector CrownPosition = FVector(15, 1150, 650);
    UPROPERTY(EditAnywhere, Category="Dragon's Wager") TArray<FVector> WatchPositions;
    UPROPERTY(EditAnywhere, Category="Dragon's Wager") TArray<FVector> JewelPositions;
    UPROPERTY(EditAnywhere, Category="Dragon's Wager") float GazeRange = 2600;
    UPROPERTY(EditAnywhere, Category="Dragon's Wager") float GazeHalfAngle = 19;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Pedestal;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Gold;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Gems;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rays;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> CrownMesh;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> JewelDisplay;
    UPROPERTY() TObjectPtr<UTextRenderComponent> Plaque;
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> WatchLights;
    UPROPERTY() TObjectPtr<UMaterialInterface> GemMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> PlainMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> GoldMaterial;
    UPROPERTY() TArray<FWagerGoldSnapshot> GoldSnapshots;
    TWeakObjectPtr<APlayerController> Player;
    TWeakObjectPtr<APawn> FrozenPawn;
    TArray<int32> AvailableJewels;
    TArray<TWeakObjectPtr<AActor>> WatchOwners;
    DragonsWager::Rules Rules;
    FTransform StartTransform;
    FVector AttentionPosition = FVector::ZeroVector;
    float AttentionUntil = 0;
    FVector DecoyPosition = FVector::ZeroVector;
    float DecoyUntil = 0;
    float Clock = 0;
    float NoticeUntil = 0;
    float WarningSoundAt = 0;
    FString Notice;
    int32 BestScore = 0;
    int32 BestRound = 0;
    bool bFrozen = false;
    bool bSaveFailed = false;
    void Start(APlayerController* PC);
    void Stop();
    void ReleaseFreeze();
    void Bank();
    void Raise();
    void Decoy();
    void Quit();
    void ResetRound();
    void DrawPieces();
    void ShowStatus();
    void Say(const FString& Line);
    bool ClearPath(const FVector& From, const FVector& To, const AActor* Ignore = nullptr, const AActor* IgnoreAlso = nullptr) const;
    bool UpdateWatch(float DeltaSeconds);
    void SaveRecord();
    bool CanPlayInput() const;
    void ResolveWatchOwners();
    void TurnToGold();
    bool Respawn(APawn* Pawn);
};
