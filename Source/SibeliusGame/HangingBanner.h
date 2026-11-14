// HangingBanner.h
//
// A BANNER ON TWO CORDS, hung from a ceiling: a picture on both faces, a rod along its top. Made
// for the living room's advert for the wrestling in the AI city (Walt, 2026-11-13: "I want
// advertising for Wrestling in AI City on a banner in the living room"), and general enough for
// the next one.
//
// The actor's origin is where the cords meet the ceiling; the banner hangs Drop below it, facing
// the actor's +X on the front and -X on the back. The picture is any texture, drawn the way the
// hidden doors draw their signs (M_fate_base's Sprite, a little Glow so it reads in a dim room).
//
// -BannerShot on the command line photographs each banner in its level a few seconds after play
// begins (front, back, and from across the room) into Saved/Screenshots - to check where it hangs
// without the editor open.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HangingBanner.generated.h"

class UStaticMeshComponent;
class UTexture;

UCLASS()
class SIBELIUSGAME_API AHangingBanner : public AActor
{
	GENERATED_BODY()

public:
	AHangingBanner();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<UStaticMeshComponent> Front;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<UStaticMeshComponent> Back;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<UStaticMeshComponent> Rod;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<UStaticMeshComponent> CordLeft;

	UPROPERTY(VisibleAnywhere, Category = "Banner")
	TObjectPtr<UStaticMeshComponent> CordRight;

	/** What it shows, on both faces. */
	UPROPERTY(EditAnywhere, Category = "Banner")
	TObjectPtr<UTexture> Picture;

	/** Across, cm. */
	UPROPERTY(EditAnywhere, Category = "Banner", meta = (ClampMin = "10"))
	float Width = 180.0f;

	/** Down, cm; 0 takes the picture's own proportions. */
	UPROPERTY(EditAnywhere, Category = "Banner", meta = (ClampMin = "0"))
	float Height = 0.0f;

	/** The cords: from the ceiling to the banner's top edge, cm. */
	UPROPERTY(EditAnywhere, Category = "Banner", meta = (ClampMin = "0"))
	float Drop = 15.0f;

	/** How much it glows: legible in a dim room without lighting it. */
	UPROPERTY(EditAnywhere, Category = "Banner", meta = (ClampMin = "0"))
	float Glow = 2.0f;

protected:
	virtual void BeginPlay() override;

private:
	void Hang();
	float BannerHeight() const;

	float ShotAge = -1.0f;   // -BannerShot: seconds since play began, or -1
	int32 ShotsTaken = 0;
	UPROPERTY(Transient)
	TObjectPtr<AActor> ShotCamera;
};
