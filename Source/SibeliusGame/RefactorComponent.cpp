// RefactorComponent.cpp
//
// SIB-26 — Ch2 Refactor. See header.

#include "RefactorComponent.h"
#include "RefactorableComponent.h"
#include "RefuserController.h"      // wild refactor: only Refusers transmute
#include "Interactable.h"           // interactables keep their jobs (never wild targets)
#include "SibeliusAim.h"            // rays start level with his eyes, even in third person
#include "SibeliusGame.h"
#include "SibeliusHUD.h"            // "REFACTOR NEEDS 10 SAUCE"
#include "ProgressionSubsystem.h"   // the wallet treasure is paid from
#include "DancerAgentComponent.h"   // R on an agent: her stand-in
#include "InteractorComponent.h"    // ...found by the same focus E and F use
#include "Components/InstancedStaticMeshComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"   // a Paragon hero, posed from her
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "UObject/ConstructorHelpers.h"

URefactorComponent::URefactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The hoard, hard-referenced so it cooks via the CDO (the Death_Back lesson) - see the
	// header. GemShader's gems in its six colours, never its plain MI_Gem (the blue again);
	// Dekogon's two chalices - all that is left of it (Walt, 2026-10-13: "keep the dekogon
	// chalices, but drop the gold bar and chests"; its gems and coin went the same day).
	const TCHAR* GemNames[] = { TEXT("SM_Gem_01"), TEXT("SM_Gem_02"), TEXT("SM_Gem_03") };
	const TCHAR* GemColorNames[] = {
		TEXT("MI_Gem_Blue"), TEXT("MI_Gem_Green"), TEXT("MI_Gem_Mixed"),
		TEXT("MI_Gem_Pink"), TEXT("MI_Gem_Red"), TEXT("MI_Gem_White"),
	};
	const TCHAR* ChaliceNames[] = { TEXT("SM_Chalice_01a"), TEXT("SM_Chalice_02b") };
	auto Hoard = []<typename TAsset>(TArray<TObjectPtr<TAsset>>& Into, const TCHAR* Folder,
		TArrayView<const TCHAR* const> Names)
	{
		for (const TCHAR* Name : Names)
		{
			ConstructorHelpers::FObjectFinder<TAsset> Finder(
				*FString::Printf(TEXT("%s/%s.%s"), Folder, Name, Name));
			if (Finder.Succeeded())
			{
				Into.Add(Finder.Object);
			}
		}
	};
	Hoard(Gems, TEXT("/Game/GemShader/Meshes"), GemNames);
	Hoard(GemColors, TEXT("/Game/GemShader/Materials"), GemColorNames);
	Hoard(Chalices, TEXT("/Game/AncientTreasures/Meshes"), ChaliceNames);

	// Who an agent becomes, in turn (UAgentStandIn). Soft paths: each folder is cooked by name
	// in DefaultGame.ini (Echo's is already cooked by APresence).
	auto Shape = [this](const TCHAR* Name, std::initializer_list<const TCHAR*> Paths)
	{
		FAgentShape& S = AgentShapes.AddDefaulted_GetRef();
		S.Name = Name;
		for (const TCHAR* Path : Paths)
		{
			S.Meshes.Add(TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(Path)));
		}
	};
	Shape(TEXT("Manny"), { TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple") });
	Shape(TEXT("Echo"), { TEXT("/Game/EchoContent/Characters/Echo/Meshes/Echo.Echo"),
		TEXT("/Game/EchoContent/Characters/Echo/Meshes/Echo_Hair.Echo_Hair") });
	Shape(TEXT("Aurora"), { TEXT("/Game/ParagonAurora/Characters/Heroes/Aurora/Meshes/Aurora.Aurora") });
	Shape(TEXT("Shinbi"), { TEXT("/Game/ParagonShinbi/Characters/Heroes/Shinbi/Meshes/Shinbi.Shinbi") });
	Shape(TEXT("Yin"), { TEXT("/Game/ParagonYin/Characters/Heroes/Yin/Meshes/Yin.Yin") });
}

// --- UAgentStandIn -----------------------------------------------------------

namespace AgentStandInDetail
{
	// Her five spine bones against a Paragon hero's three: his chest turns as hers does.
	FName SourceBoneFor(FName Target, bool bThreeSpine)
	{
		if (bThreeSpine)
		{
			if (Target == FName(TEXT("spine_02"))) { return FName(TEXT("spine_03")); }
			if (Target == FName(TEXT("spine_03"))) { return FName(TEXT("spine_05")); }
		}
		return Target;
	}

	// The bone that gives a limb segment its direction at rest ("upperarm_l" -> "lowerarm_l"),
	// or None. Only arms, legs and fingers are lined up; hips, spine and head keep his rest.
	FName AlignChild(FName Bone)
	{
		static const TMap<FString, FString> Next = []
		{
			TMap<FString, FString> M;
			M.Add(TEXT("clavicle"), TEXT("upperarm"));
			M.Add(TEXT("upperarm"), TEXT("lowerarm"));
			M.Add(TEXT("lowerarm"), TEXT("hand"));
			M.Add(TEXT("hand"), TEXT("middle_01"));
			M.Add(TEXT("thigh"), TEXT("calf"));
			M.Add(TEXT("calf"), TEXT("foot"));
			M.Add(TEXT("foot"), TEXT("ball"));
			for (const TCHAR* Finger : { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") })
			{
				M.Add(FString::Printf(TEXT("%s_01"), Finger), FString::Printf(TEXT("%s_02"), Finger));
				M.Add(FString::Printf(TEXT("%s_02"), Finger), FString::Printf(TEXT("%s_03"), Finger));
			}
			return M;
		}();
		const FString Name = Bone.ToString();
		if (Name.Len() < 3 || (!Name.EndsWith(TEXT("_l")) && !Name.EndsWith(TEXT("_r"))))
		{
			return NAME_None;
		}
		const FString* Child = Next.Find(Name.LeftChop(2));
		return Child ? FName(*(*Child + Name.Right(2))) : NAME_None;
	}

	// A skeleton's rest pose in component space, parents before children as stored.
	TArray<FTransform> RestPose(const FReferenceSkeleton& Ref)
	{
		const TArray<FTransform>& Local = Ref.GetRefBonePose();
		TArray<FTransform> CS;
		CS.SetNum(Local.Num());
		for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			CS[Bone] = Parent == INDEX_NONE ? Local[Bone] : Local[Bone] * CS[Parent];
		}
		return CS;
	}

	// Followed by name, or retargeted: a skeleton without her upper spine cannot follow.
	bool NeedsRetarget(const USkeletalMesh* Body)
	{
		return Body && Body->GetRefSkeleton().FindBoneIndex(TEXT("spine_05")) == INDEX_NONE;
	}
}

UAgentStandIn::UAgentStandIn()
{
	// Ticks only while retargeting, after her animation has posed her for the frame.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UAgentStandIn::PutOn(USkeletalMeshComponent* Body, TArrayView<USkeletalMesh* const> Meshes, int32 Index,
	const FString& Name)
{
	AActor* Her = GetOwner();
	if (!Her || !Body || Meshes.Num() == 0 || !Meshes[0])
	{
		return;
	}
	Leader = Body;
	ShapeIndex = Index;
	ShapeName = Name;

	// Everything she is drawn with hangs under Body: the face and the grooms on it, the
	// torso, legs and feet. Hidden, not changed; collision is left alone, so R still finds
	// her to move her on.
	TArray<USceneComponent*> Under;
	Body->GetChildrenComponents(/*bIncludeAllDescendants=*/true, Under);
	Under.Add(Body);
	for (USceneComponent* Each : Under)
	{
		UPrimitiveComponent* Drawn = Cast<UPrimitiveComponent>(Each);
		if (Drawn && Drawn->IsVisible())
		{
			Drawn->SetVisibility(false);
			Hidden.Add(Drawn);
		}
	}

	// A hidden body would stop posing, and the stand-in would freeze mid-step.
	SavedTickOption = static_cast<uint8>(Body->VisibilityBasedAnimTickOption);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Not named "Body" or "Face": UDancerAgentComponent finds her meshes by those names.
	const bool bRetarget = AgentStandInDetail::NeedsRetarget(Meshes[0]);
	USkinnedMeshComponent* First = nullptr;
	for (USkeletalMesh* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		const FName PartName = MakeUniqueObjectName(Her, USkinnedMeshComponent::StaticClass(), TEXT("AgentStandIn"));
		USkinnedMeshComponent* Part = nullptr;
		if (bRetarget && !First)
		{
			Posed = NewObject<UPoseableMeshComponent>(Her, PartName);
			Part = Posed;
		}
		else
		{
			Part = NewObject<USkeletalMeshComponent>(Her, PartName);
		}
		Part->SetSkinnedAssetAndUpdate(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetupAttachment(Body);
		Part->RegisterComponent();
		if (Part != Posed)
		{
			// Following her, or (Echo's hair; a hero's extra parts) the retargeted body.
			Part->bUseBoundsFromLeaderPoseComponent = true;   // culled where she is, not at its rest pose
			Part->SetLeaderPoseComponent(bRetarget && First ? First : Body);
		}
		Parts.Add(Part);
		if (!First)
		{
			First = Part;
		}
	}

	if (Posed)
	{
		BuildRetarget();
		AddTickPrerequisiteComponent(Body);
		SetComponentTickEnabled(true);
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s wears %s (%s, %d mesh(es)): %d of her meshes hidden, following %s."),
		*Her->GetName(), *Name, Posed ? TEXT("retargeted") : TEXT("followed by name"), Parts.Num(), Hidden.Num(),
		*Body->GetName());
}

void UAgentStandIn::BuildRetarget()
{
	const FReferenceSkeleton& Src = Leader->GetSkinnedAsset()->GetRefSkeleton();
	const FReferenceSkeleton& Tgt = Posed->GetSkinnedAsset()->GetRefSkeleton();
	const TArray<FTransform> SrcRest = AgentStandInDetail::RestPose(Src);
	RestGlobal = AgentStandInDetail::RestPose(Tgt);
	RestLocal = Tgt.GetRefBonePose();

	const int32 Num = Tgt.GetNum();
	const bool bThreeSpine = Tgt.FindBoneIndex(TEXT("spine_04")) == INDEX_NONE;
	SourceBone.Init(INDEX_NONE, Num);
	RotationOffset.Init(FQuat::Identity, Num);
	bFromHips.Init(false, Num);
	SourceRestLocation.Init(FVector::ZeroVector, Num);
	Global = RestGlobal;

	TArray<FQuat> Align;   // his rest turned to hers, per bone; a bone not lined up takes its parent's
	Align.Init(FQuat::Identity, Num);
	int32 Mapped = 0, Aligned = 0;
	for (int32 Bone = 0; Bone < Num; ++Bone)
	{
		const int32 Parent = Tgt.GetParentIndex(Bone);
		if (Parent != INDEX_NONE)
		{
			Align[Bone] = Align[Parent];
		}
		const FName Name = Tgt.GetBoneName(Bone);
		const int32 Hers = Src.FindBoneIndex(AgentStandInDetail::SourceBoneFor(Name, bThreeSpine));
		if (Hers == INDEX_NONE)
		{
			continue;   // his own: rides its parent at rest
		}
		SourceBone[Bone] = Hers;
		SourceRestLocation[Bone] = SrcRest[Hers].GetLocation();
		bFromHips[Bone] = Parent == INDEX_NONE || Tgt.GetParentIndex(Parent) == INDEX_NONE;   // root, pelvis
		++Mapped;

		// THE SAME REST POSE FIRST: his limb segment turned to point where hers does.
		const FName Child = AgentStandInDetail::AlignChild(Name);
		const int32 HisChild = Child.IsNone() ? INDEX_NONE : Tgt.FindBoneIndex(Child);
		const int32 HerChild = Child.IsNone() ? INDEX_NONE : Src.FindBoneIndex(Child);
		if (HisChild != INDEX_NONE && HerChild != INDEX_NONE)
		{
			const FVector HisDir = (RestGlobal[HisChild].GetLocation() - RestGlobal[Bone].GetLocation()).GetSafeNormal();
			const FVector HerDir = (SrcRest[HerChild].GetLocation() - SrcRest[Hers].GetLocation()).GetSafeNormal();
			if (!HisDir.IsNearlyZero() && !HerDir.IsNearlyZero())
			{
				Align[Bone] = FQuat::FindBetweenNormals(HisDir, HerDir);
				++Aligned;
			}
		}

		// Then hers turning from her rest turns his from that lined-up rest, in component space:
		// his = her now * (her rest)^-1 * align * his rest.
		RotationOffset[Bone] = SrcRest[Hers].GetRotation().Inverse() * Align[Bone] * RestGlobal[Bone].GetRotation();
	}

	// His hips travel as far as hers, scaled by how much higher his stand.
	const int32 HisPelvis = Tgt.FindBoneIndex(TEXT("pelvis"));
	const int32 HerPelvis = Src.FindBoneIndex(TEXT("pelvis"));
	HipScale = 1.f;
	if (HisPelvis != INDEX_NONE && HerPelvis != INDEX_NONE && SrcRest[HerPelvis].GetLocation().Z > 1.0)
	{
		HipScale = float(RestGlobal[HisPelvis].GetLocation().Z / SrcRest[HerPelvis].GetLocation().Z);
	}
	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] retarget: %d of %d bones turn with hers (%d lined up at rest, %s spine), hips x%.2f."),
		Mapped, Num, Aligned, bThreeSpine ? TEXT("three-bone") : TEXT("five-bone"), HipScale);
}

void UAgentStandIn::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Leader || !Posed || !Posed->GetSkinnedAsset() || Posed->BoneSpaceTransforms.Num() != SourceBone.Num())
	{
		return;
	}
	const TArray<FTransform>& Now = Leader->GetComponentSpaceTransforms();
	if (Now.Num() == 0)
	{
		return;
	}
	const FReferenceSkeleton& Tgt = Posed->GetSkinnedAsset()->GetRefSkeleton();
	const int32 Num = SourceBone.Num();
	for (int32 Bone = 0; Bone < Num; ++Bone)
	{
		const int32 Parent = Tgt.GetParentIndex(Bone);
		const int32 Hers = SourceBone[Bone];
		if (Hers == INDEX_NONE || !Now.IsValidIndex(Hers))
		{
			Global[Bone] = Parent == INDEX_NONE ? RestLocal[Bone] : RestLocal[Bone] * Global[Parent];
			continue;
		}
		FTransform& G = Global[Bone];
		G.SetRotation((Now[Hers].GetRotation() * RotationOffset[Bone]).GetNormalized());
		G.SetScale3D(RestGlobal[Bone].GetScale3D());
		if (bFromHips[Bone])
		{
			G.SetLocation(RestGlobal[Bone].GetLocation() + (Now[Hers].GetLocation() - SourceRestLocation[Bone]) * HipScale);
		}
		else
		{
			// His own bone length, along wherever his parent now points.
			G.SetLocation(Parent == INDEX_NONE ? RestLocal[Bone].GetLocation()
				: Global[Parent].TransformPosition(RestLocal[Bone].GetLocation()));
		}
	}
	for (int32 Bone = 0; Bone < Num; ++Bone)
	{
		const int32 Parent = Tgt.GetParentIndex(Bone);
		Posed->BoneSpaceTransforms[Bone] = Parent == INDEX_NONE ? Global[Bone] : Global[Bone].GetRelativeTransform(Global[Parent]);
	}
	Posed->RefreshBoneTransforms();
}

void UAgentStandIn::TakeOff()
{
	SetComponentTickEnabled(false);
	for (USkinnedMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Parts.Reset();
	Posed = nullptr;
	for (UPrimitiveComponent* Drawn : Hidden)
	{
		if (Drawn)
		{
			Drawn->SetVisibility(true);
		}
	}
	Hidden.Reset();
	if (Leader)
	{
		RemoveTickPrerequisiteComponent(Leader);
		Leader->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(SavedTickOption);
	}
	ShapeIndex = INDEX_NONE;
	ShapeName.Reset();
}

// --- URefactorComponent ------------------------------------------------------

void URefactorComponent::BeginPlay()
{
	Super::BeginPlay();

	// A treasure with no simple collision is one R can never hit again, so the original it
	// hides is gone for good. Leave such meshes out, loudly (Tools/Scripts/prepare_hoard.py).
	int32 Bodiless = 0;
	for (TArray<TObjectPtr<UStaticMesh>>* Pile : { &Gems, &Chalices })
	{
		Bodiless += Pile->RemoveAll([](const TObjectPtr<UStaticMesh>& Mesh)
		{
			const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
			return !Body || Body->AggGeom.GetElementCount() == 0;
		});
	}
	if (Bodiless > 0)
	{
		UE_LOG(LogSibeliusGame, Warning, TEXT("[WildRefactor] %d treasure mesh(es) have no simple collision and are left out - run Tools/Scripts/prepare_hoard.py."),
			Bodiless);
	}

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Hoard: %d gems in %d colours + %d chalices, %d sauce each."),
		Gems.Num(), GemColors.Num(), Chalices.Num(), TreasureSauceCost);
}

void URefactorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CurrentTarget = TraceForRefactorable();
}

URefactorableComponent* URefactorComponent::TraceForRefactorable() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return nullptr;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return nullptr;
	}

	const FVector Aim = PC->PlayerCameraManager->GetCameraRotation().Vector();
	// Level with his eyes while the view is him (first or third person); a table's or a
	// close-up's own camera keeps the old ray from that camera, as it always has.
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Start = PC->GetViewTarget() == Pawn ? SibeliusAim::RayStart(Pawn, CamLoc, Aim) : CamLoc;
	const FVector End = Start + Aim * TraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			// R4: only ever return an actor that is actually refactorable.
			return HitActor->FindComponentByClass<URefactorableComponent>();
		}
	}
	return nullptr;
}

namespace
{
	/* OPTED IN BY TAG, and the tag means two things at once: this actor may be
	   transmuted even though it is interactable (see the candidate test further down),
	   and it keeps its labels while transmuted. Both are wanted in exactly the same
	   place -- the legacy machine's stages -- so one tag carries both rather than two
	   that could drift apart.

	   It lives up here rather than in the namespace below because TriggerRefactor is
	   its first caller and C++ will not look ahead. */
	bool KeepsLabelsWhenWild(const AActor* A)
	{
		return A && A->ActorHasTag(TEXT("WildRefactorOK"));
	}
}

void URefactorComponent::TriggerRefactor()
{
	/* Pre-authored refactorables normally win: the office puzzle objects keep their two
	   hand-built states, never roll for treasure, and cost nothing.

	   THE ONE EXCEPTION IS EARNED. A legacy machine stage carries a refactorable too --
	   that authored edit IS the GRADER fix, the whole first ticket. So while the job is
	   open, R on a stage must repair it and nothing else. Once the machine tags its
	   stages WildRefactorOK (ALegacyMachine does that when ticket 1 closes), R starts
	   turning them to treasure instead, and the line keeps running with a heap of gold
	   standing in it -- because the machine drives the workpiece to each part's world
	   LOCATION and never asks what the part looks like.

	   The tag is all-or-nothing across the five stages on purpose. Gate it per stage --
	   say, only the ones that are behaving -- and the stage that refuses to become a
	   gem is the answer to the puzzle. */
	if (CurrentTarget && !KeepsLabelsWhenWild(CurrentTarget->GetOwner()))
	{
		CurrentTarget->ToggleRefactor();
		return;
	}

	TryWildRefactor();
}

namespace
{
	// R on treasure: destroy it, un-hide the first draft, give a restored
	// Refuser his mind back. Shared by the pawn sweep AND the visibility ray —
	// something slapped flat on the floor is exactly what a thin ray misses
	// (Walt's downed fox, in the animal days, refused to become the couch again).
	/* HIDE THE BODY, NOT EVERY MESH. A machine stage carries four static meshes: the
	   crate, and the three dark backing plates behind its plaque, true name and fault
	   lamp. Hiding all of them would leave the plaque's text floating over nothing.

	   So the body is named explicitly with a WildBody component tag, and only that is
	   hidden. The fallback to the first mesh keeps this working for anything that opts
	   in without tagging a body -- FindComponentByClass is what the transmutation
	   already uses to size and vet a target, so it is the same component either way. */
	void SetWildBodyHidden(AActor* A, bool bHidden)
	{
		if (!A)
		{
			return;
		}
		TArray<UStaticMeshComponent*> Meshes;
		A->GetComponents<UStaticMeshComponent>(Meshes);

		TArray<UStaticMeshComponent*> Bodies;
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M && M->ComponentHasTag(TEXT("WildBody")))
			{
				Bodies.Add(M);
			}
		}
		if (Bodies.Num() == 0 && Meshes.Num() > 0)
		{
			Bodies.Add(Meshes[0]);
		}
		for (UStaticMeshComponent* M : Bodies)
		{
			M->SetHiddenInGame(bHidden);
			M->SetVisibility(!bHidden);
		}
	}

	bool RestoreWildDraft(AActor* TreasureActor)
	{
		if (!TreasureActor)
		{
			return false;
		}
		UWildRefactorState* State = TreasureActor->FindComponentByClass<UWildRefactorState>();
		if (!State)
		{
			return false;
		}
		// One plant out of a painted field: put it back exactly where and how it stood.
		if (UInstancedStaticMeshComponent* Field = State->OriginalInstances)
		{
			// Find it where it was left, not by its old number (see HiddenInstanceTransform).
			const FTransform& Left = State->HiddenInstanceTransform;
			auto IsTheHiddenPlant = [Field, &Left](int32 Index)
			{
				FTransform Now;
				return Field->GetInstanceTransform(Index, Now, /*bWorldSpace=*/true)
					&& Now.GetLocation().Equals(Left.GetLocation(), 1.0)
					&& Now.GetScale3D().GetAbsMax() <= Left.GetScale3D().GetAbsMax() * 2.0;
			};
			int32 Index = State->OriginalInstanceIndex;
			if (!IsTheHiddenPlant(Index))
			{
				Index = INDEX_NONE;
				for (int32 Each = 0; Each < Field->GetInstanceCount(); ++Each)
				{
					if (IsTheHiddenPlant(Each))
					{
						Index = Each;
						break;
					}
				}
			}
			if (Index != INDEX_NONE)
			{
				Field->UpdateInstanceTransform(Index, State->OriginalInstanceTransform,
					/*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
				UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d restored (was %d)."),
					*Field->GetName(), Index, State->OriginalInstanceIndex);
			}
			else
			{
				/* Not in the field: the influencer has it out as a live copy, still shrunk.
				   Plant a new one where the original stood. The shrunk copy goes back into the
				   field when he walks away - an invisible extra, not a missing plant. */
				Field->AddInstance(State->OriginalInstanceTransform, /*bWorldSpace=*/true);
				UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s: the hidden plant was out of the field (a live copy) - planted it again."),
					*Field->GetName());
			}
		}
		if (AActor* Original = State->OriginalActor)
		{
			// Undo it the way it was done, or a stage comes back with its crate still
			// invisible and a plaque hanging in mid-air.
			if (State->bHidMeshesOnly)
			{
				SetWildBodyHidden(Original, false);
			}
			else
			{
				Original->SetActorHiddenInGame(false);
			}
			Original->SetActorEnableCollision(true);
			if (ACharacter* Char = Cast<ACharacter>(Original))
			{
				if (!Char->GetController())
				{
					Char->SpawnDefaultController();
				}
			}
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s restored."), *Original->GetName());
		}
		TreasureActor->Destroy();
		return true;
	}
}

bool URefactorComponent::TryWildRefactor()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !PC->PlayerCameraManager || !World)
	{
		return false;   // (an empty hoard is PickTreasure's null, which every path checks)
	}

	// AN AGENT FIRST, found the way F finds her: the interactor's focus here, then the sweep
	// and the ray below. Before the Refusers and the scenery, so a dancer is never gold.
	if (const UInteractorComponent* Interactor = Pawn->FindComponentByClass<UInteractorComponent>())
	{
		if (CycleStandIn(UDancerAgentComponent::FindFor(Interactor->GetFocusedActor())))
		{
			return true;
		}
	}

	const FVector Aim = PC->PlayerCameraManager->GetCameraRotation().Vector();
	// Level with his eyes while the view is him (first or third person); a table's or a
	// close-up's own camera keeps the old ray from that camera, as it always has.
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Start = PC->GetViewTarget() == Pawn ? SibeliusAim::RayStart(Pawn, CamLoc, Aim) : CamLoc;
	const FVector End = Start + Aim * TraceDistance;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	// Refusers first, hunted with the slap's own generous pawn-channel sweep —
	// the thin visibility ray below often misses a moving pawn entirely
	// (capsules don't block visibility), which made R feel broken on Gideon
	// while F worked fine.
	TArray<FHitResult> PawnHits;
	if (World->SweepMultiByChannel(PawnHits, Start, End, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeSphere(45.f), Params))
	{
		for (const FHitResult& PawnHit : PawnHits)
		{
			// Slapped-flat treasure answers the sweep even when the thin ray misses.
			if (RestoreWildDraft(PawnHit.GetActor()))
			{
				return true;
			}
			if (CycleStandIn(UDancerAgentComponent::FindFor(PawnHit.GetActor())))
			{
				return true;
			}
			ACharacter* Victim = Cast<ACharacter>(PawnHit.GetActor());
			if (Victim && Cast<ARefuserController>(Victim->GetController()))
			{
				UMaterialInterface* Color = nullptr;
				UStaticMesh* Treasure = PickTreasure(Color);
				if (!Treasure || !PayForTreasure())
				{
					return false;   // he keeps working, and keeps his mind
				}
				if (AController* VC = Victim->GetController())
				{
					VC->StopMovement();
					VC->UnPossess();
				}
				AActor* Spawned = SpawnTreasureActor(Treasure, Color, Victim->GetComponentsBoundingBox(), RefuserTreasureSize,
					Victim, nullptr);
				if (!Spawned)
				{
					if (!Victim->GetController())
					{
						Victim->SpawnDefaultController();   // nothing spawned — put him back to work
					}
					return false;
				}
				// Same reversible contract as props (Walt: Gideon should come
				// back): hide him behind the treasure, restore re-possesses him.
				UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
				State->RegisterComponent();
				State->OriginalActor = Victim;
				Victim->SetActorHiddenInGame(true);
				Victim->SetActorEnableCollision(false);
				UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] Refuser transmuted."));
				return true;
			}
		}
	}

	// The office is full of INVISIBLE interaction boxes hovering over the
	// furniture (the AI clue terminal over the desk, the corkboard volume,
	// the cauldron zone) — they block the visibility ray so E-prompts work,
	// and they ate R presses aimed at whatever sits underneath. When the ray
	// hits something untransmutable, look past it (a few hops, no more).
	AActor* Target = nullptr;
	for (int32 Hop = 0; Hop < 4; ++Hop)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			return false;
		}
		AActor* HitTarget = Hit.GetActor();
		if (!HitTarget)
		{
			return false;
		}

		// --- R on treasure: destroy it and un-hide the first draft. ---
		if (RestoreWildDraft(HitTarget))
		{
			return true;
		}
		if (CycleStandIn(UDancerAgentComponent::FindFor(HitTarget)))
		{
			return true;
		}

		// (Refusers are handled by the pawn sweep above.)
		if (Cast<ACharacter>(HitTarget))
		{
			return false;   // players, Shinbi, and civilians are not clay
		}

		/* --- One plant out of a painted field: only in a virtual home. ---
		   The whole field is one actor far too big to transmute, so without this the
		   size limit below refuses every plant on Kaia's planet. Logged, because the
		   planet's pack may not be looked at by an AI (its licence): what the ray met is
		   how a failure there gets diagnosed. */
		if (bWildInstances)
		{
			UInstancedStaticMeshComponent* Field = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent());
			UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] ray met %s (%s), component %s (%s), item %d."),
				*HitTarget->GetName(), *HitTarget->GetClass()->GetName(),
				Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("none"),
				Hit.GetComponent() ? *Hit.GetComponent()->GetClass()->GetName() : TEXT("-"), Hit.Item);
			if (Field && Hit.Item != INDEX_NONE && !HitTarget->ActorHasTag(TEXT("NoWildRefactor")))
			{
				return TransmuteInstance(Field, Hit.Item);
			}
		}

		// --- Scenery candidate — stop hopping. ---
		// Anything wearing a static mesh qualifies, INCLUDING dumb Blueprint
		// decor (Walt's desk is BP_WorkingTable_A1): transmutation only hides
		// the original, so nothing breaks. The exceptions keep their jobs:
		// interactables (books, curios, doors, the cauldron, machines) are
		// never wild targets, and the NoWildRefactor tag opts anything out.
		const bool bHasMesh = HitTarget->FindComponentByClass<UStaticMeshComponent>() != nullptr;
		const bool bInteractable = HitTarget->GetClass()->ImplementsInterface(UInteractable::StaticClass());
		const bool bCandidate = (bHasMesh && !bInteractable) || HitTarget->ActorHasTag(TEXT("WildRefactorOK"));
		if (bCandidate && !HitTarget->ActorHasTag(TEXT("NoWildRefactor")))
		{
			Target = HitTarget;
			break;
		}

		Params.AddIgnoredActor(HitTarget);   // look past it
	}
	if (!Target)
	{
		return false;
	}
	UStaticMeshComponent* SMC = Target->FindComponentByClass<UStaticMeshComponent>();
	if (!SMC || !SMC->GetStaticMesh())
	{
		return false;
	}

	const FVector Extent = Target->GetComponentsBoundingBox().GetSize();
	const float LargestSide = Extent.GetMax();
	if (LargestSide > MaxWildTargetSize)
	{
		return false;   // walls, floors, buildings: never treasure
	}

	// Architecture refuses the treatment BY NAME — a window is desk-sized, so
	// the bounds cap can't catch it, and a window that becomes a gem leaves
	// a gem-shaped hole to the outside world (Walt found this immediately).
	// Kit meshes reliably carry these words; furniture doesn't.
	{
		static const TCHAR* ArchitectureWords[] = {
			TEXT("wall"), TEXT("window"), TEXT("door"), TEXT("floor"),
			TEXT("roof"), TEXT("ceiling"), TEXT("fence"), TEXT("stair"),
			TEXT("beam"), TEXT("column"), TEXT("pillar"), TEXT("railing"),
		};
		const FString MeshName = SMC->GetStaticMesh()->GetName().ToLower();
		const FString ActorName = Target->GetName().ToLower();
		for (const TCHAR* Word : ArchitectureWords)
		{
			if (MeshName.Contains(Word) || ActorName.Contains(Word))
			{
				return false;
			}
		}
	}

	// Spawn the treasure where the prop stands, then hide the prop — never
	// modify it, so R on the treasure restores it exactly as it was.
	// Placed by the prop's BOUNDS and the floor under them, never its pivot:
	// actor pivots are unreliable (a belly-pivot animal spawned at a
	// floor-pivot chair ended up underground — invisible AND untraceable,
	// which read as "the chair disappeared completely").
	UMaterialInterface* Color = nullptr;
	UStaticMesh* Treasure = PickTreasure(Color);
	if (!Treasure || !PayForTreasure())
	{
		return false;
	}
	AActor* Spawned = SpawnTreasureActor(Treasure, Color, Target->GetComponentsBoundingBox(), LargestSide, Target, nullptr);
	if (!Spawned)
	{
		return false;
	}
	UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
	State->RegisterComponent();
	State->OriginalActor = Target;
	State->bHidMeshesOnly = KeepsLabelsWhenWild(Target);
	if (State->bHidMeshesOnly)
	{
		SetWildBodyHidden(Target, true);   // the plaque stays; that is the joke
	}
	else
	{
		Target->SetActorHiddenInGame(true);
	}
	Target->SetActorEnableCollision(false);

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s transmuted."), *Target->GetName());
	return true;
}

bool URefactorComponent::TransmuteInstance(UInstancedStaticMeshComponent* Field, int32 Index)
{
	FTransform Plant;
	if (!Field || !Field->GetStaticMesh() || !Field->GetInstanceTransform(Index, Plant, /*bWorldSpace=*/true))
	{
		return false;
	}

	// The plant's own bounds, not the field's: one coral tree, not the forest.
	const FBox Box = Field->GetStaticMesh()->GetBoundingBox().TransformBy(Plant);
	const float LargestSide = Box.GetSize().GetMax();
	if (LargestSide > MaxWildTargetSize || LargestSide < 1.f)
	{
		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d is %.0f cm - outside the limits."),
			*Field->GetName(), Index, LargestSide);
		return false;
	}

	// Same placement rule as a prop: on the ground under the plant's own footprint, level.
	// Not at the plant's tilt - foliage leans with the slope, and a treasure copying it
	// went half into the hillside.
	UMaterialInterface* Color = nullptr;
	UStaticMesh* Treasure = PickTreasure(Color);
	if (!Treasure || !PayForTreasure())
	{
		return false;
	}
	AActor* Spawned = SpawnTreasureActor(Treasure, Color, Box, LargestSide, nullptr, Field);
	if (!Spawned)
	{
		return false;
	}

	UWildRefactorState* State = NewObject<UWildRefactorState>(Spawned);
	State->RegisterComponent();
	State->OriginalInstances = Field;
	State->OriginalInstanceIndex = Index;
	State->OriginalInstanceTransform = Plant;

	FTransform Gone = Plant;
	Gone.SetScale3D(FVector(0.001f));
	State->HiddenInstanceTransform = Gone;
	const bool bHidden = Field->UpdateInstanceTransform(Index, Gone, /*bWorldSpace=*/true,
		/*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);

	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s instance %d (%s, %.0f cm) transmuted%s."),
		*Field->GetName(), Index, *Field->GetStaticMesh()->GetName(), LargestSide,
		bHidden ? TEXT("") : TEXT(" - BUT THE PLANT COULD NOT BE HIDDEN"));
	return true;
}

bool URefactorComponent::CycleStandIn(UDancerAgentComponent* Agent)
{
	AActor* Her = Agent ? Agent->GetOwner() : nullptr;
	USkeletalMeshComponent* Body = Agent ? Agent->FindDanceMesh() : nullptr;
	if (!Her || !Body || AgentShapes.Num() == 0)
	{
		return false;   // nobody to turn her into: R behaves as it did before agents
	}
	const FString Name = Agent->AgentName.IsEmpty() ? Her->GetName() : Agent->AgentName;
	UAgentStandIn* Worn = Her->FindComponentByClass<UAgentStandIn>();

	// The next shape that loads. A hero is loaded the first time he is picked (a moment's
	// pause, once); one whose pack is missing is passed over, loudly.
	TArray<USkeletalMesh*, TInlineAllocator<2>> Meshes;
	int32 Next = Worn ? Worn->GetShapeIndex() + 1 : 0;
	for (; Next < AgentShapes.Num(); ++Next)
	{
		Meshes.Reset();
		for (const TSoftObjectPtr<USkeletalMesh>& Soft : AgentShapes[Next].Meshes)
		{
			if (USkeletalMesh* Mesh = Soft.LoadSynchronous())
			{
				Meshes.Add(Mesh);
			}
		}
		if (Meshes.Num() > 0 && Meshes.Num() == AgentShapes[Next].Meshes.Num())
		{
			break;
		}
		UE_LOG(LogSibeliusGame, Warning, TEXT("[WildRefactor] %s did not load - passed over."), *AgentShapes[Next].Name);
	}

	// After the last: herself again, free, like every refactor undone.
	auto BackToHerself = [Worn, &Name]()
	{
		const FString Was = Worn->GetShapeName();
		Worn->TakeOff();
		Worn->DestroyComponent();
		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s is herself again (was %s)."), *Name, *Was);
	};
	if (Next >= AgentShapes.Num())
	{
		if (Worn)
		{
			BackToHerself();
			return true;
		}
		return false;
	}

	// AND WHEN HE CANNOT PAY FOR THE NEXT ONE, herself too: undoing is free, and a Manny he
	// could not afford to move on from would be a Manny for ever.
	const UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (Worn && Progression && TreasureSauceCost > 0 && Progression->GetSauce() < TreasureSauceCost)
	{
		BackToHerself();
		return true;
	}
	if (!PayForTreasure())
	{
		return true;    // the toast says why; the press was for her all the same
	}
	if (Worn)
	{
		Worn->TakeOff();
	}
	else
	{
		Worn = NewObject<UAgentStandIn>(Her);
		Worn->RegisterComponent();
	}
	Worn->PutOn(Body, Meshes, Next, AgentShapes[Next].Name);
	UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s refactored into %s (%d of %d), %d sauce."),
		*Name, *AgentShapes[Next].Name, Next + 1, AgentShapes.Num(), TreasureSauceCost);
	return true;
}

UStaticMesh* URefactorComponent::PickTreasure(UMaterialInterface*& OutColor) const
{
	// A pile first, half each, then one of that pile, so the treasure is half gems and
	// half chalices whatever each pile holds. A gem gets a colour; a chalice stays as made.
	OutColor = nullptr;
	const TArray<TObjectPtr<UStaticMesh>>* Piles[] = { &Gems, &Chalices };
	TArray<int32, TInlineAllocator<2>> Stocked;
	for (int32 Each = 0; Each < int32(UE_ARRAY_COUNT(Piles)); ++Each)
	{
		if (Piles[Each]->Num() > 0)
		{
			Stocked.Add(Each);
		}
	}
	if (Stocked.Num() == 0)
	{
		return nullptr;
	}
	const int32 Kind = Stocked[FMath::RandRange(0, Stocked.Num() - 1)];
	const TArray<TObjectPtr<UStaticMesh>>& Pile = *Piles[Kind];
	if (Piles[Kind] == &Gems && GemColors.Num() > 0)
	{
		OutColor = GemColors[FMath::RandRange(0, GemColors.Num() - 1)].Get();
	}
	return Pile[FMath::RandRange(0, Pile.Num() - 1)].Get();
}

FVector URefactorComponent::GroundBelow(const FBox& Box, const AActor* Replaced, const UPrimitiveComponent* ReplacedField) const
{
	const FVector Middle = Box.GetCenter();
	FVector Ground(Middle.X, Middle.Y, Box.Min.Z);   // the bounds floor, if nothing is found
	UWorld* World = GetWorld();
	if (!World)
	{
		return Ground;
	}

	/* WORLD-STATIC ONLY, from the middle down. The house's invisible interaction boxes block
	   the visibility channel and hover over the furniture (see the hop loop), so a visibility
	   ray from above would stand the treasure on one of them; floors, furniture and the
	   landscape are world-static. Starting at mid-height finds the table under a lamp, not
	   the floor under the table. */
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TreasureGround), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(GetOwner());
	if (Replaced)
	{
		Params.AddIgnoredActor(Replaced);
	}
	if (ReplacedField)
	{
		Params.AddIgnoredComponent(ReplacedField);   // the plant's own field, not just the plant
	}
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Middle, FVector(Middle.X, Middle.Y, Box.Min.Z - 500.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Ground.Z = Hit.ImpactPoint.Z;
	}
	return Ground;
}

bool URefactorComponent::PayForTreasure() const
{
	if (TreasureSauceCost <= 0)
	{
		return true;
	}
	UProgressionSubsystem* Progression = UProgressionSubsystem::Get(this);
	if (!Progression)
	{
		return false;   // headless / no save: no wallet, so no treasure
	}
	// TrySpendSauce IS THE CHECK (the coffee cup's rule): one call, one answer.
	// The HUD's sauce counter shows the spend on its own.
	if (!Progression->TrySpendSauce(TreasureSauceCost))
	{
		FString Line = TooPoorLine;
		Line.ReplaceInline(TEXT("{0}"), *FString::FromInt(TreasureSauceCost));
		Line.ReplaceInline(TEXT("{1}"), *FString::FromInt(Progression->GetSauce()));
		ASibeliusHUD::Toast(this, Line, 3.0f, SibeliusToast::Warn);
		return false;
	}
	return true;
}

AActor* URefactorComponent::SpawnTreasureActor(UStaticMesh* TreasureMesh, UMaterialInterface* Color, const FBox& Space, float MatchSize,
	const AActor* Replaced, const UPrimitiveComponent* ReplacedField)
{
	UWorld* World = GetWorld();
	if (!World || !TreasureMesh)
	{
		return nullptr;
	}
	FVector Ground = GroundBelow(Space, Replaced, ReplacedField);
	const FBoxSphereBounds Bounds = TreasureMesh->GetBounds();
	const FVector Extent = Bounds.BoxExtent;   // every treasure stands as made (see Gems)

	const AActor* Me = GetOwner();
	auto FlatToMe = [Me](const FVector& From)
	{
		FVector V = Me ? Me->GetActorLocation() - From : FVector::ForwardVector;
		V.Z = 0.f;
		return V;
	};
	FVector ToMe = FlatToMe(Ground);
	float Distance = ToMe.Size();

	// Uniform-scale the treasure so it roughly fills the space of what it replaced (a
	// desk-wide gem, a Gideon-sized chalice). The ceiling is high because the chalices are
	// hand-sized: one a few centimetres across has to reach a coral tree's ten metres on
	// Kaia's planet. But never AROUND him: its footprint stops PlayerClearance short
	// of him - shrinking down to half size, then stepping back away from him for the rest.
	const float Fit = FMath::Max(MatchSize, MinTreasureSize) / FMath::Max(Extent.GetMax() * 2.f, 1.f);
	float Scale = Fit;
	const float Reach = FVector2D(Extent.X, Extent.Y).Size();   // footprint radius at scale 1
	if (Me && Reach > UE_KINDA_SMALL_NUMBER)
	{
		Scale = FMath::Clamp((Distance - PlayerClearance) / Reach, Fit * 0.5f, Fit);
		const float Needed = Scale * Reach + PlayerClearance;
		if (Distance < Needed)
		{
			// Straight away from him; standing on its middle, the way he is facing.
			const FVector Away = Distance > 1.f ? -ToMe / Distance : Me->GetActorForwardVector().GetSafeNormal2D();
			const FVector Step = Away * (Needed - Distance);
			Ground = GroundBelow(Space.ShiftBy(FVector(Step.X, Step.Y, 0.f)), Replaced, ReplacedField);
			ToMe = FlatToMe(Ground);
			Distance = ToMe.Size();
		}
	}
	Scale = FMath::Clamp(Scale, 0.05f, 400.f);

	// BROAD FACE TO HIM: turn it so its thin side points at where he stands (a chalice's
	// handles go to the sides, not at him).
	const float Yaw = FMath::Atan2(ToMe.Y, ToMe.X) + (Extent.Y < Extent.X ? UE_HALF_PI : 0.f);
	const FQuat Rotation(FVector::ZAxisVector, Yaw);

	// Its bounds, not its pivot, go where they should: centred over Ground, bottom on it.
	// Actor pivots are unreliable (belly-pivot animals sank underground and were lost; the
	// gems' pivots are at the girdle, well above their point).
	const FVector Offset = Rotation.RotateVector(Bounds.Origin) * Scale;   // pivot to bounds centre
	const FVector Location(Ground.X - Offset.X, Ground.Y - Offset.Y, Ground.Z + Extent.Z * Scale - Offset.Z);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation.Rotator(), SpawnParams);
	if (Actor)
	{
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		Actor->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(TreasureMesh);
		if (Color)
		{
			Comp->SetMaterial(0, Color);
		}
		Actor->SetActorScale3D(FVector(Scale));

		// Every treasure must block the R-trace (ECC_Visibility), or it can never
		// be restored — an untraceable treasure over a hidden prop is a softlock.
		// (Only a mesh with simple collision can: BeginPlay keeps the others out.)
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Comp->SetCollisionResponseToAllChannels(ECR_Block);
		Comp->SetCanEverAffectNavigation(false);

		UE_LOG(LogSibeliusGame, Display, TEXT("[WildRefactor] %s%s%s, %.0f cm tall and %.0f across, %.0f cm from him."),
			*TreasureMesh->GetName(), Color ? TEXT(" in ") : TEXT(""), Color ? *Color->GetName() : TEXT(""),
			Extent.Z * 2.f * Scale, FMath::Max(Extent.X, Extent.Y) * 2.f * Scale, Distance);
	}
	return Actor;
}
