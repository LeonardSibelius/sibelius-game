// ElsewhereGen.cpp — see header. Pure + headless; mirrors FCarouselSim.

#include "ElsewhereGen.h"
#include "Math/RandomStream.h"

// Weighted pick over a parallel weight array using an already-seeded stream. Returns
// INDEX_NONE only for an empty/zero-weight set. Stable ordering in + stable stream =
// deterministic out (the whole point). Shared with AElsewhereBuilder's scatter so both
// consume EXACTLY one RandRange draw per pick.
int32 FElsewhereGen::WeightedPick(FRandomStream& Rng, const TArray<int32>& Weights)
{
	int32 Total = 0;
	for (int32 W : Weights)
	{
		Total += FMath::Max(0, W);
	}
	if (Total <= 0)
	{
		return INDEX_NONE;
	}

	// RandRange is inclusive; roll in [0, Total-1] and walk the cumulative band.
	int32 Roll = Rng.RandRange(0, Total - 1);
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		Roll -= FMath::Max(0, Weights[i]);
		if (Roll < 0)
		{
			return i;
		}
	}
	return Weights.Num() - 1;   // FP-free, but guard the boundary
}

void FElsewhereGen::BuildDefaultPlaceTypes(TArray<FPlaceTypeDef>& OutPlaces)
{
	OutPlaces.Reset();

	auto MakePlace = [&OutPlaces](FName Id, const FString& Name, FLinearColor Ambient,
		FLinearColor Glow, float Fog, float Light, TArray<FName> Pool)
	{
		FPlaceTypeDef P;
		P.Id = Id;
		P.DisplayName = FText::FromString(Name);
		P.Weight = 10;
		P.AmbientColor = Ambient;
		P.CurioGlowColor = Glow;
		P.FogDensity = Fog;
		P.LightIntensity = Light;
		P.RoomExtent = FVector(800.f, 800.f, 400.f);
		P.PropCountMin = 6;
		P.PropCountMax = 14;
		P.CurioPool = MoveTemp(Pool);
		OutPlaces.Add(MoveTemp(P));
	};

	// MVP starter set (§5). Three distinct moods — drowned, brass-warm, cosmic — so
	// the very first playtest already answers "does it keep surprising me?".
	MakePlace(TEXT("FloodedLibrary"), TEXT("The Flooded Library"),
		FLinearColor(0.02f, 0.06f, 0.10f), FLinearColor(0.4f, 0.7f, 1.0f), 0.04f, 1.4f,
		{ TEXT("DrownedBook"), TEXT("BottledTide"), TEXT("PearlOfPages") });

	MakePlace(TEXT("ClockworkAttic"), TEXT("The Clockwork Attic"),
		FLinearColor(0.10f, 0.07f, 0.03f), FLinearColor(1.0f, 0.8f, 0.4f), 0.025f, 2.2f,
		{ TEXT("BrassGear"), TEXT("InfiniteWatch"), TEXT("DustmoteOrrery") });

	MakePlace(TEXT("StarlitVoid"), TEXT("The Starlit Void"),
		FLinearColor(0.01f, 0.01f, 0.04f), FLinearColor(0.7f, 0.6f, 1.0f), 0.005f, 1.0f,
		{ TEXT("FallenStar"), TEXT("VoidCompass"), TEXT("SingingComet") });

	// A fourth, slightly heavier-rolling personal one (§5 "Inverted Kitchen") — the
	// uncanny home note that makes the feature Leonard's, not generic.
	MakePlace(TEXT("InvertedKitchen"), TEXT("The Inverted Kitchen"),
		FLinearColor(0.06f, 0.05f, 0.05f), FLinearColor(1.0f, 0.5f, 0.3f), 0.015f, 1.8f,
		{ TEXT("UpsideSpoon"), TEXT("RecursiveRecipe"), TEXT("SauceThatDreams") });

	// --- The Winter Forest (SIB Forest) — the showcase PRE-MADE Elsewhere. NOT builder-assembled:
	// TravelLevelName routes the Sauce Door straight to the hand-built L_Elsewhere_Forest, which
	// brings its own PCG scenery + a hand-placed, plan-configured curio. An AI dreamworld (surreal
	// flowers + ferns through snow), not a natural one. Kit/layout fields go unused here.
	{
		FPlaceTypeDef Forest;
		Forest.Id = TEXT("WinterForest");
		Forest.DisplayName = FText::FromString(TEXT("The Winter Forest"));
		Forest.Weight = 10;
		Forest.TravelLevelName = TEXT("L_Elsewhere_Forest");
		Forest.AmbientColor = FLinearColor(0.10f, 0.12f, 0.16f);
		Forest.CurioGlowColor = FLinearColor(0.6f, 0.9f, 1.0f);   // cold aurora glow
		Forest.CurioPool = { TEXT("FrostPetal"), TEXT("DreamingFern"), TEXT("AuroraSeed") };
		OutPlaces.Add(MoveTemp(Forest));
	}

	// --- The Server Cathedral (§5) — the AI's "mind" as a temple. The FIRST dressed
	// place-type (SIB-47 dressing): its kit palette points at the REAL Crebotoly
	// ModularSciFiEnv_K meshes (the cohesive kit chosen for the MVP — matching 4x4m
	// floor/ceiling, 4m wall mids, industrial props). The kit's .uasset bytes are NOT
	// committed (asset policy); installed via Fab -> Add to Project and referenced by
	// path. These same paths are in DT_ElsewherePlaces (the DataTable wins when present;
	// this code default keeps a fresh clone honest). See docs/sib-47-sauce-door-notes.md.
	{
		FPlaceTypeDef Cathedral;
		Cathedral.Id = TEXT("ServerCathedral");
		Cathedral.DisplayName = FText::FromString(TEXT("The Server Cathedral"));
		Cathedral.Weight = 10;
		Cathedral.AmbientColor   = FLinearColor(0.03f, 0.05f, 0.09f);   // cold server-glow blue
		Cathedral.CurioGlowColor = FLinearColor(0.83f, 0.66f, 0.21f);  // warm gold (#D3A836) — the warm focal point vs the cool hall
		Cathedral.FogDensity = 0.03f;
		Cathedral.LightIntensity = 0.9f;                                // dim cool fill; the shafts + curio glow carry the room
		Cathedral.RoomExtent = FVector(1000.f, 1000.f, 500.f);          // a hall, not a closet
		Cathedral.PropCountMin = 3;                                      // sparse — a few hero props, spread out
		Cathedral.PropCountMax = 6;
		Cathedral.CurioPool = { TEXT("CachedPrayer"), TEXT("TheFirstPacket"), TEXT("KernelRelic") };

		// Crebotoly ModularSciFiEnv_K. KitTileSize matches the kit's 4x4m (400cm) modules.
		Cathedral.KitTileSize = 400.f;
		Cathedral.KitWallHeight = 400.f;
		Cathedral.KitMeshScale = 1.f;
		auto Mesh = [](const TCHAR* Path) { return TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Path)); };
		Cathedral.FloorMeshes = {
			Mesh(TEXT("/Game/ModularSciFiEnv_K/Meshes/Floors/SM_Floor_A_4x4m.SM_Floor_A_4x4m")) };
		Cathedral.WallMeshes = {
			Mesh(TEXT("/Game/ModularSciFiEnv_K/Meshes/Walls/SM_Wall_A_Mid_4x4m.SM_Wall_A_Mid_4x4m")) };
		Cathedral.CeilingMeshes = {
			Mesh(TEXT("/Game/ModularSciFiEnv_K/Meshes/Ceilings/SM_Ceiling_A_4x4m.SM_Ceiling_A_4x4m")) };
		// Floor-standing props (legacy PropMeshes path). NOTE: the richer per-seed scatter no
		// longer reads these — the curated _K detail palette lives on the BUILDER's ScatterSet
		// default (AElsewhereBuilder ctor), used for every place that doesn't author its own
		// FPlaceTypeDef::ScatterMeshes. Kept only as the last-resort fallback. (These lamp
		// "_Base" meshes are actually flat ~5cm strip-light plates — bounds-verified — which is
		// why the curated ScatterSet uses machinery/posts instead.)
		Cathedral.PropMeshes = {
			Mesh(TEXT("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AA_Base.SM_Lamp_AA_Base")),
			Mesh(TEXT("/Game/ModularSciFiEnv_K/Meshes/Lamps/SM_Lamp_AB_Base.SM_Lamp_AB_Base")) };

		OutPlaces.Add(MoveTemp(Cathedral));
	}
}

void FElsewhereGen::BuildDefaultCurios(TArray<FCurioDef>& OutCurios)
{
	OutCurios.Reset();

	auto MakeCurio = [&OutCurios](FName Id, const FString& Name, EElsewhereRarity Rarity,
		int32 Weight, const FString& Note, const TCHAR* MeshPath = nullptr)
	{
		FCurioDef C;
		C.Id = Id;
		C.DisplayName = FText::FromString(Name);
		C.Rarity = Rarity;
		C.Weight = Weight;
		C.FlavorNote = FText::FromString(Note);
		if (MeshPath)
		{
			C.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		}
		OutCurios.Add(MoveTemp(C));
	};

	// Common are heavy, Rare lighter, Legendary rare — so every place reliably yields
	// a delight and occasionally a "ooh" (§6). Notes are Leonard's hand (light story).
	// --- Flooded Library ---
	MakeCurio(TEXT("DrownedBook"),   TEXT("A Drowned Book"),        EElsewhereRarity::Common,    16, TEXT("Still legible underwater. The ink learned to swim."));
	MakeCurio(TEXT("BottledTide"),   TEXT("A Bottled Tide"),        EElsewhereRarity::Rare,       7, TEXT("Hold it to your ear and the library is still drowning."));
	MakeCurio(TEXT("PearlOfPages"),  TEXT("The Pearl of Pages"),    EElsewhereRarity::Legendary,  2, TEXT("Every book that was ever lost, pressed into one bead."));

	// --- Clockwork Attic ---
	MakeCurio(TEXT("BrassGear"),       TEXT("A Brass Gear"),         EElsewhereRarity::Common,    16, TEXT("It turns nothing now, and turns it perfectly."));
	MakeCurio(TEXT("InfiniteWatch"),   TEXT("The Infinite Watch"),   EElsewhereRarity::Rare,       7, TEXT("Always reads 'soon'."));
	MakeCurio(TEXT("DustmoteOrrery"),  TEXT("A Dustmote Orrery"),    EElsewhereRarity::Legendary,  2, TEXT("The motes orbit a sun I can't see. They keep good time."));

	// --- Starlit Void ---
	MakeCurio(TEXT("FallenStar"),    TEXT("A Fallen Star"),         EElsewhereRarity::Common,    16, TEXT("Cool to the touch. It apologised for the dark."));
	MakeCurio(TEXT("VoidCompass"),   TEXT("The Void Compass"),      EElsewhereRarity::Rare,       7, TEXT("Points to wherever you are not."));
	MakeCurio(TEXT("SingingComet"),  TEXT("A Singing Comet"),       EElsewhereRarity::Legendary,  2, TEXT("One note, held since before there was air to hear it."));

	// --- Inverted Kitchen ---
	MakeCurio(TEXT("UpsideSpoon"),     TEXT("An Upside-Down Spoon"), EElsewhereRarity::Common,    16, TEXT("It stirs the ceiling. The ceiling doesn't mind."));
	MakeCurio(TEXT("RecursiveRecipe"), TEXT("A Recursive Recipe"),   EElsewhereRarity::Rare,       7, TEXT("Step one: prepare the dish from step one."));
	MakeCurio(TEXT("SauceThatDreams"), TEXT("The Sauce That Dreams"),EElsewhereRarity::Legendary,  2, TEXT("A spoonful of the thing behind the door. It's looking back."));

	// --- Server Cathedral --- (No Mesh override: the curio renders as a glowing orb —
	// engine sphere + the emissive M_LampEmiss_MAT tinted to the place's CurioGlowColor,
	// see ACurio::Configure. The kit's lamp parts are flat fixtures, not hero relics.)
	MakeCurio(TEXT("CachedPrayer"),   TEXT("A Cached Prayer"),       EElsewhereRarity::Common,    16, TEXT("Someone asked the machine for grace. It kept the request warm."));
	MakeCurio(TEXT("TheFirstPacket"), TEXT("The First Packet"),      EElsewhereRarity::Rare,       7, TEXT("The very first thing it ever heard. Still unread receipts."));
	MakeCurio(TEXT("KernelRelic"),    TEXT("The Kernel Relic"),      EElsewhereRarity::Legendary,  2, TEXT("Warm to the touch, and it remembers being switched on."));

	// --- Winter Forest (SIB Forest) — surreal blooms through snow (no Mesh override: glowing orb) ---
	MakeCurio(TEXT("FrostPetal"),    TEXT("A Frost Petal"),         EElsewhereRarity::Common,    16, TEXT("A flower that bloomed because the snow asked it to."));
	MakeCurio(TEXT("DreamingFern"),  TEXT("A Dreaming Fern"),       EElsewhereRarity::Rare,       7, TEXT("It unfurls toward a sun only it remembers."));
	MakeCurio(TEXT("AuroraSeed"),    TEXT("An Aurora Seed"),        EElsewhereRarity::Legendary,  2, TEXT("Plant it and the sky grows colours for one night."));
}

const FPlaceTypeDef* FElsewhereGen::FindPlace(const TArray<FPlaceTypeDef>& Places, const FName& Id)
{
	return Places.FindByPredicate([&Id](const FPlaceTypeDef& P) { return P.Id == Id; });
}

const FCurioDef* FElsewhereGen::FindCurio(const TArray<FCurioDef>& Curios, const FName& Id)
{
	return Curios.FindByPredicate([&Id](const FCurioDef& C) { return C.Id == Id; });
}

FElsewherePlan FElsewhereGen::RollPlan(
	int32 Seed,
	const TArray<FPlaceTypeDef>& Places,
	const TArray<FCurioDef>& Curios)
{
	FElsewherePlan Plan;
	Plan.Seed = Seed;

	if (Places.Num() == 0)
	{
		return Plan;   // invalid — no content
	}

	FRandomStream Rng(Seed);

	// 1) Pick the place (weighted).
	TArray<int32> PlaceWeights;
	PlaceWeights.Reserve(Places.Num());
	for (const FPlaceTypeDef& P : Places)
	{
		PlaceWeights.Add(P.Weight);
	}
	const int32 PlaceIdx = WeightedPick(Rng, PlaceWeights);
	if (PlaceIdx == INDEX_NONE)
	{
		return Plan;
	}
	const FPlaceTypeDef& Place = Places[PlaceIdx];
	Plan.PlaceTypeId = Place.Id;

	// 2) Pick a curio from THIS place's pool (weighted by the curio's own Weight, so
	//    rarity falls out of the data). Skip ids missing from the registry.
	TArray<FName> PoolIds;
	TArray<int32> PoolWeights;
	for (const FName& CurioId : Place.CurioPool)
	{
		if (const FCurioDef* Def = FindCurio(Curios, CurioId))
		{
			PoolIds.Add(CurioId);
			PoolWeights.Add(Def->Weight);
		}
	}
	const int32 CurioIdx = WeightedPick(Rng, PoolWeights);
	if (CurioIdx == INDEX_NONE)
	{
		return Plan;   // place has no resolvable curios — invalid
	}
	Plan.CurioId = PoolIds[CurioIdx];

	// 3) Derive sub-seeds (still pure functions of Seed) so layout + mood jitter
	//    differ per visit but reproduce exactly for a given seed.
	Plan.LayoutSeed = Rng.RandRange(0, MAX_int32 - 1);
	Plan.MoodSeed   = Rng.RandRange(0, MAX_int32 - 1);

	return Plan;
}
