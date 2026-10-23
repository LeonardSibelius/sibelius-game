// ArcadeTVSubsystem.h
//
// "LEONARD'S ARCADE" ON THE TVs. Walt, 2026-10-23: "can we put the words 'LEONARD'S ARCADE' on
// the TVs to invite the player". When a level begins, every arcade TV (SibArcade::TVTag - the
// living room's and the bedroom's) is switched on: a UWidgetComponent laid on its glass, drawn
// with Unreal's unlit widget pass-through material, so it reads as a lit screen in a dark room
// as well as a bright one. The default text material would not have: it is lit, and the TVs
// stand where the light is low.
//
// Nothing is added to the maps. The TVs are house-pack meshes, and where each one's glass is
// was measured from the mesh itself - its screen section's vertices (ScreenOn, below).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcadeTVSubsystem.generated.h"

UCLASS()
class SIBELIUSGAME_API UArcadeTVSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
