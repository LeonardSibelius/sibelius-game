// Copyright Epic Games, Inc. All Rights Reserved.

#include "SibeliusGameGameMode.h"
#include "SibeliusHUD.h"

ASibeliusGameGameMode::ASibeliusGameGameMode()
{
	// Always-on center reticle (a BP GameMode derived from this inherits it unless it
	// overrides HUDClass).
	HUDClass = ASibeliusHUD::StaticClass();
}
