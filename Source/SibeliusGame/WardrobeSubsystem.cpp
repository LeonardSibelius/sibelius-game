// WardrobeSubsystem.cpp - see the header.

#include "WardrobeSubsystem.h"

#include "SibeliusGame.h"   // LogSibeliusGame

#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"   // FScreenshotRequest

// Named, not anonymous: the unity-build rule (see PinballTable.cpp).
namespace WardrobeDetail
{
	/** The retired outfits, by mesh name. */
	const TCHAR* const Retired[] = {
		TEXT("MHC_Kaia_Outfits"),   // Kaia's pink dress (Walt, 2026-11-13)
	};

	bool IsRetired(const USkinnedMeshComponent* Mesh)
	{
		const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
		if (!Asset)
		{
			return false;
		}
		const FString Name = Asset->GetName();
		for (const TCHAR* Outfit : Retired)
		{
			if (Name == Outfit)
			{
				return true;
			}
		}
		return false;
	}
}

namespace WardrobeDetail
{
	/** Kaia's body: the skeletal mesh named Body on an actor whose face is hers. */
	USkeletalMeshComponent* KaiaBody(AActor* Actor)
	{
		USkeletalMeshComponent* Body = nullptr;
		bool bKaia = false;
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			const USkinnedAsset* Asset = Mesh ? Mesh->GetSkinnedAsset() : nullptr;
			if (!Asset)
			{
				continue;
			}
			if (Asset->GetName() == TEXT("SKM_MHC_Kaia_FaceMesh"))
			{
				bKaia = true;
			}
			if (Mesh->GetName() == TEXT("Body"))
			{
				Body = Mesh;
			}
		}
		return bKaia ? Body : nullptr;
	}

	/** Balls just inside her shoulders, so the hairline gap between her face and body meshes there
	    shows them and not what is behind her. */
	void BackShoulders(AActor* Actor, const FLinearColor& Color, bool bGlow)
	{
		USkeletalMeshComponent* Body = KaiaBody(Actor);
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bGlow
			? TEXT("/Game/SlotFactory/Materials/M_fate_base.M_fate_base")
			: TEXT("/Game/Go/M_GoSurface.M_GoSurface"));
		if (!Body || !Sphere || !Base)
		{
			return;
		}
		struct FBall { const TCHAR* Bone; FVector Offset; float Radius; };
		// Measured against the monologue's close-up (2026-11-13): bigger and they show through her
		// shoulders; these sit just under the skin along the seam, from the collarbone to the tip.
		const FBall Balls[] = {
			{ TEXT("upperarm_l"), FVector::ZeroVector, 4.2f },
			{ TEXT("clavicle_l"), FVector(9.0, 0.0, 0.0), 3.6f },
			{ TEXT("clavicle_l"), FVector(13.0, 0.0, 0.0), 3.4f },
			{ TEXT("clavicle_l"), FVector(16.0, 0.0, 0.0), 3.2f },
			{ TEXT("clavicle_l"), FVector(14.0, 2.5, 0.0), 3.0f },
			{ TEXT("clavicle_l"), FVector(14.0, -2.5, 0.0), 3.0f },
			{ TEXT("clavicle_l"), FVector(14.0, 0.0, 2.5), 3.0f },
			{ TEXT("clavicle_l"), FVector(14.0, 0.0, -2.5), 3.0f },
			{ TEXT("upperarm_r"), FVector::ZeroVector, 4.2f },
			{ TEXT("clavicle_r"), FVector(-9.0, 0.0, 0.0), 3.6f },
			{ TEXT("clavicle_r"), FVector(-13.0, 0.0, 0.0), 3.4f },
			{ TEXT("clavicle_r"), FVector(-16.0, 0.0, 0.0), 3.2f },
		};
		for (const FBall& Ball : Balls)
		{
			if (Body->GetBoneIndex(Ball.Bone) == INDEX_NONE)
			{
				continue;
			}
			UStaticMeshComponent* Fill = NewObject<UStaticMeshComponent>(Actor);
			Fill->SetStaticMesh(Sphere);
			Fill->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Fill->SetCastShadow(false);
			Fill->SetupAttachment(Body, Ball.Bone);
			Fill->SetRelativeLocation(Ball.Offset);
			Fill->SetRelativeScale3D(FVector(Ball.Radius / 50.0f));
			Fill->RegisterComponent();
			if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Fill))
			{
				M->SetVectorParameterValue(TEXT("Color"), Color);
				M->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
				M->SetScalarParameterValue(TEXT("PatternAmount"), 0.0f);
				M->SetScalarParameterValue(TEXT("Glow"), 3.0f);
				Fill->SetMaterial(0, M);
			}
		}
	}
}

bool UWardrobeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Game worlds only: the editor's own view of a level keeps showing what the assets hold.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UWardrobeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	int32 Hidden = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Hidden += Undress(*It) ? 1 : 0;
	}
	SpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UWardrobeSubsystem::HandleSpawned));
	BoundWorld = &InWorld;

	// -WardrobeShots: pictures at 5, 10 and 15 seconds, to check a scene's clothes without the
	// editor (a cutscene has no other way to be looked at offscreen).
	if (FParse::Param(FCommandLine::Get(), TEXT("WardrobeShots")))
	{
		for (int32 I = 1; I <= 3; ++I)
		{
			FTimerHandle Handle;
			const FString Name = FString::Printf(TEXT("Wardrobe_%s_%02d.png"), *InWorld.GetMapName(), I * 5);
			InWorld.GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Name]()
			{
				FScreenshotRequest::RequestScreenshot(Name, /*bShowUI=*/false, /*bAddFilenameSuffix=*/false);
			}), I * 5.0f, false);
		}
	}
	if (Hidden > 0)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Wardrobe] %d retired outfit(s) hidden in %s."), Hidden, *InWorld.GetMapName());
	}
}

void UWardrobeSubsystem::Deinitialize()
{
	if (UWorld* World = BoundWorld.Get())
	{
		World->RemoveOnActorSpawnedHandler(SpawnedHandle);
	}
	SpawnedHandle.Reset();
	Super::Deinitialize();
}

void UWardrobeSubsystem::HandleSpawned(AActor* Actor)
{
	if (Undress(Actor))
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[Wardrobe] retired outfit hidden on %s."), *Actor->GetName());
	}
}

bool UWardrobeSubsystem::Undress(AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}
	bool bHid = false;
	// -WardrobeHide=Hair,Face: a test switch that hides components by name, to find which part
	// of a character something belongs to (2026-11-13, a strap on Kaia's shoulder).
	FString HideList;
	if (FParse::Value(FCommandLine::Get(), TEXT("WardrobeHide="), HideList, /*bShouldStopOnSeparator=*/false))
	{
		TArray<FString> Names;
		HideList.ParseIntoArray(Names, TEXT(","));
		TArray<UPrimitiveComponent*> Parts;
		Actor->GetComponents<UPrimitiveComponent>(Parts);
		for (UPrimitiveComponent* Part : Parts)
		{
			if (Part && Names.Contains(Part->GetName()))
			{
				Part->SetVisibility(false, false);   // that part alone: its children stay
				Part->SetHiddenInGame(true, false);
			}
		}
	}
	TArray<USkinnedMeshComponent*> Meshes;
	Actor->GetComponents<USkinnedMeshComponent>(Meshes);
	for (USkinnedMeshComponent* Mesh : Meshes)
	{
		if (WardrobeDetail::IsRetired(Mesh))
		{
			Mesh->SetVisibility(false, /*bPropagateToChildren=*/true);
			Mesh->SetHiddenInGame(true, /*bPropagateToChildren=*/true);
			bHid = true;
		}
	}
	// -WardrobeBackingTest: bright green balls inside Kaia's shoulders, to see the seam gap fill.
	if (FParse::Param(FCommandLine::Get(), TEXT("WardrobeBackingTest")))
	{
		WardrobeDetail::BackShoulders(Actor, FLinearColor(0.0f, 1.0f, 0.0f), true);
	}
	// -WardrobeForceLOD=1: every skinned mesh at one level of detail (1 = the finest), to test a seam.
	int32 ForceLOD = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("WardrobeForceLOD="), ForceLOD) && ForceLOD > 0)
	{
		TArray<USkinnedMeshComponent*> Skins;
		Actor->GetComponents<USkinnedMeshComponent>(Skins);
		for (USkinnedMeshComponent* Skin : Skins)
		{
			Skin->SetForcedLOD(ForceLOD);
			UE_LOG(LogSibeliusGame, Display, TEXT("[Wardrobe] test: %s forced to LOD %d (was predicting %d)."), *Skin->GetName(), ForceLOD - 1, Skin->GetPredictedLODLevel());
		}
	}
	// -WardrobeHideSlot=Face.eyelashes_shader_shader;Body.x: blank material slots by name (test).
	FString SlotList;
	if (FParse::Value(FCommandLine::Get(), TEXT("WardrobeHideSlot="), SlotList, /*bShouldStopOnSeparator=*/false))
	{
		UMaterialInterface* Hide = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MetaHumans/Common/Materials/M_Hide.M_Hide"));
		TArray<FString> Pairs;
		SlotList.ParseIntoArray(Pairs, TEXT(";"));
		TArray<UMeshComponent*> MeshParts;
		Actor->GetComponents<UMeshComponent>(MeshParts);
		for (const FString& Pair : Pairs)
		{
			FString Comp, Slot;
			if (!Pair.Split(TEXT("."), &Comp, &Slot))
			{
				continue;
			}
			for (UMeshComponent* Part : MeshParts)
			{
				if (Part && Hide && Part->GetName() == Comp)
				{
					const int32 Index = Part->GetMaterialIndex(FName(*Slot));
					if (Index != INDEX_NONE)
					{
						Part->SetMaterial(Index, Hide);
						UE_LOG(LogSibeliusGame, Display, TEXT("[Wardrobe] test: %s slot %s hidden."), *Comp, *Slot);
					}
				}
			}
		}
	}
	return bHid;
}
