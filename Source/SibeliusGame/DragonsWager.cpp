#include "DragonsWager.h"
#include "DragonsWagerSaveGame.h"
#include "SibeliusGame.h"
#include "SibeliusSaveIO.h"
#include "SibeliusHUD.h"
#include "TableSounds.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/InputComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace WagerDetail
{
    const TCHAR* Slot = TEXT("DragonsWager");
    const FLinearColor Amber(1.0f, 0.48f, 0.035f);
    const FLinearColor Ruby(1.0f, 0.025f, 0.055f);
    const FLinearColor Emerald(0.025f, 0.85f, 0.3f);
    void Orb(UInstancedStaticMeshComponent* Mesh, const FVector& At, float Size, FLinearColor Color, float Glow = 1)
    {
        const int32 Index = Mesh->AddInstance(FTransform(FRotator::ZeroRotator, At, FVector(Size / 100)), true);
        const TArray<float> Data{Color.R, Color.G, Color.B, Glow};
        Mesh->SetCustomData(Index, Data, true);
    }
}

ADragonsWager::ADragonsWager()
{
    PrimaryActorTick.bCanEverTick = true;
    Tags.Add(TEXT("NoWildRefactor"));
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> KeepPedestal(TEXT("/Game/Fafnir_Keep_Meshingun/Meshes/SM_pedestal.SM_pedestal"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CrownAsset(TEXT("/Game/DragonsWager/SM_WagerCrown.SM_WagerCrown"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> JewelAsset(TEXT("/Game/DragonsWager/SM_WagerGem.SM_WagerGem"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> GoldAsset(TEXT("/Game/DragonsWager/M_WagerGold.M_WagerGold"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> JewelPaint(TEXT("/Game/DragonsWager/M_WagerGem.M_WagerGem"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Plain(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gem(TEXT("/Game/LanternBurst/M_LanternOrb.M_LanternOrb"));
    PlainMaterial = Plain.Object;
    GoldMaterial = GoldAsset.Succeeded() ? GoldAsset.Object.Get() : Plain.Object.Get();
    GemMaterial = Gem.Succeeded() ? Gem.Object.Get() : Plain.Object.Get();
    Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WagerPedestal"));
    Pedestal->SetupAttachment(RootComponent);
    Pedestal->SetStaticMesh(KeepPedestal.Succeeded() ? KeepPedestal.Object.Get() : Cylinder.Object.Get());
    Pedestal->SetRelativeLocation(FVector(0,0,KeepPedestal.Succeeded() ? 0 : 50));
    Pedestal->SetRelativeScale3D(KeepPedestal.Succeeded() ? FVector(0.62) : FVector(0.65,0.65,1));
    Pedestal->SetCollisionProfileName(TEXT("BlockAll"));
    auto Instances = [&](const TCHAR* Name, UStaticMesh* Shape, UMaterialInterface* Mat)
    {
        auto* Part = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Part->SetupAttachment(RootComponent);
        Part->SetStaticMesh(Shape);
        Part->SetMaterial(0, Mat);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(false);
        Part->NumCustomDataFloats = 4;
        return Part;
    };
    Gold = Instances(TEXT("GoldInlay"), Sphere.Object, GoldMaterial);
    Gems = Instances(TEXT("Jewels"), Sphere.Object, GemMaterial);
    Rays = Instances(TEXT("GazeTrace"), Sphere.Object, GemMaterial);
    CrownMesh = Instances(TEXT("CrownPoints"), CrownAsset.Succeeded() ? CrownAsset.Object.Get() : Cone.Object.Get(), GoldMaterial);
    JewelDisplay = Instances(TEXT("FacetedJewels"), JewelAsset.Succeeded() ? JewelAsset.Object.Get() : Sphere.Object.Get(),
        JewelPaint.Succeeded() ? JewelPaint.Object.Get() : GemMaterial.Get());
    Plaque = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Plaque"));
    Plaque->SetupAttachment(RootComponent);
    Plaque->SetRelativeLocation(FVector(0, -26, 147));
    Plaque->SetRelativeRotation(FRotator(0, -90, 0));
    Plaque->SetHorizontalAlignment(EHTA_Center);
    Plaque->SetWorldSize(9);
    Plaque->SetTextRenderColor(FColor(235, 196, 113));
    Plaque->SetText(FText::FromString(TEXT("THE DRAGON'S WAGER\n[E] Steal the crown")));
    // Surveyed against the actual dragon heads in L_IslaKeep (2026-10-28).
    WatchPositions = { FVector(-310,-205,460), FVector(315,-215,460) };
    for (int I=0; I<2; ++I)
    {
        auto* Light = CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("DragonGaze%d"),I));
        Light->SetupAttachment(RootComponent);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetLightColor(WagerDetail::Amber);
        // A warm wash only; the marked ring (UpdateWatch) is what shows the danger.
        // Brighter than this, the unshadowed light blew the dragons out to white.
        Light->SetIntensity(30000);
        Light->SetAttenuationRadius(GazeRange);
        Light->SetInnerConeAngle(12);
        Light->SetOuterConeAngle(GazeHalfAngle);
        Light->SetVolumetricScatteringIntensity(2);
        // The eye sits inside the dragon's head, which swallowed a shadowed light whole,
        // while the sight test (ignoring that dragon) still saw. Unshadowed, the lit
        // area is never smaller than the real danger.
        Light->SetCastShadows(false);
        Light->SetVisibility(false);
        WatchLights.Add(Light);
    }
    // Each jewel sits 90 cm above a traced walkable surface, never inside a stair.
    JewelPositions = {FVector(-600,-50,90), FVector(600,-50,90), FVector(-180,350,304),
        FVector(180,550,402), FVector(-150,750,501), FVector(150,950,590)};
}

void ADragonsWager::EnsureInKeep(UWorld* World)
{
    if (!World || !World->IsGameWorld() || !World->GetMapName().Contains(TEXT("L_IslaKeep"))) return;
    for (TActorIterator<ADragonsWager> It(World); It; ++It) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    World->SpawnActor<ADragonsWager>(FVector(-180,-380,0), FRotator::ZeroRotator, Params);
}

void ADragonsWager::BeginPlay()
{
    Super::BeginPlay();
    ResolveWatchOwners();
    if (const auto* Save = Cast<UDragonsWagerSaveGame>(FSibeliusSaveIO::Load(WagerDetail::Slot)))
    {
        if (Save->Version == 1) { BestScore = FMath::Clamp(Save->BestScore,0,100000); BestRound = FMath::Clamp(Save->BestRound,0,10); }
    }
    DrawPieces();
}

void ADragonsWager::ResolveWatchOwners()
{
    WatchOwners.SetNum(WatchPositions.Num());
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TArray<UStaticMeshComponent*> Parts;
        It->GetComponents(Parts);
        for (const auto* Part : Parts)
        {
            if (!Part->GetStaticMesh() || Part->GetStaticMesh()->GetName() != TEXT("SM_Dragon_a_01")) continue;
            for (int32 I=0; I<WatchPositions.Num(); ++I)
                if (FVector::Dist(It->GetActorLocation(),WatchPositions[I]) < 400) WatchOwners[I] = *It;
        }
    }
}

void ADragonsWager::EndPlay(const EEndPlayReason::Type Reason)
{
    Stop();
    Super::EndPlay(Reason);
}

FText ADragonsWager::GetInteractionPrompt_Implementation() const
{
    if (Rules.State == DragonsWager::Phase::Idle) return FText::FromString(TEXT("[E] The Dragon's Wager - steal the crown"));
    if (Rules.State == DragonsWager::Phase::Returned) return FText::FromString(TEXT("[B] Bank treasure   [T] Risk another round"));
    if (Rules.Crown) return FText::FromString(TEXT("[E] Return the crown"));
    return FText::FromString(TEXT("Find the crown above the stairs. [X] Leave wager"));
}

void ADragonsWager::Interact_Implementation(AActor* Interactor)
{
    auto* Pawn = Cast<APawn>(Interactor);
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > 350) return;
    if (Rules.State == DragonsWager::Phase::Idle) { Start(PC); return; }
    if (PC != Player.Get()) return;
    if (Rules.Return())
    {
        TableSounds::Play(this, ETableSound::Chime1000,0.6f);
        Say(TEXT("Isla: Yours, if you stop now. Or shall we make it interesting?"));
    }
}

void ADragonsWager::Start(APlayerController* PC)
{
    if (!PC || !Cast<ACharacter>(PC->GetPawn())) return;
    Player = PC;
    ResolveWatchOwners(); // streamed actors may have arrived after BeginPlay
    StartTransform = PC->GetPawn()->GetActorTransform();
    Rules.Start(); ResetRound();
    EnableInput(PC);
    InputComponent->bBlockInput = false;
    InputComponent->Priority = 8;
    InputComponent->ClearActionBindings();
    InputComponent->KeyBindings.Reset();
    InputComponent->BindKey(EKeys::K,IE_Pressed,this,&ADragonsWager::Decoy);
    InputComponent->BindKey(EKeys::B,IE_Pressed,this,&ADragonsWager::Bank);
    InputComponent->BindKey(EKeys::T,IE_Pressed,this,&ADragonsWager::Raise);
    InputComponent->BindKey(EKeys::X,IE_Pressed,this,&ADragonsWager::Quit);
    Say(TEXT("Isla: Bring me the crown. Mind the dragons. They are terribly possessive."));
}

void ADragonsWager::ResetRound()
{
    AvailableJewels.Reset();
    for (int32 I=0; I<JewelPositions.Num(); ++I) AvailableJewels.Add(I);
    AttentionUntil = DecoyUntil = 0;
    DrawPieces();
}

void ADragonsWager::ReleaseFreeze()
{
    if (bFrozen && Player.IsValid())
    {
        Player->SetIgnoreMoveInput(false);
        if (Player->PlayerCameraManager) Player->PlayerCameraManager->StopCameraFade();
    }
    for (const FWagerGoldSnapshot& Snapshot : GoldSnapshots)
        if (IsValid(Snapshot.Mesh))
            for (int32 I=0; I<Snapshot.Materials.Num(); ++I) Snapshot.Mesh->SetMaterial(I,Snapshot.Materials[I]);
    GoldSnapshots.Reset();
    bFrozen = false; FrozenPawn.Reset();
}

void ADragonsWager::TurnToGold()
{
    if (!Player.IsValid() || !Player->GetPawn()) return;
    TArray<UMeshComponent*> Parts;
    Player->GetPawn()->GetComponents(Parts);
    for (UMeshComponent* Part : Parts)
    {
        FWagerGoldSnapshot Snapshot;
        Snapshot.Mesh = Part;
        for (int32 I=0; I<Part->GetNumMaterials(); ++I)
        {
            Snapshot.Materials.Add(Part->GetMaterial(I));
            Part->SetMaterial(I,GoldMaterial);
        }
        GoldSnapshots.Add(MoveTemp(Snapshot));
    }
}

bool ADragonsWager::CanPlayInput() const
{
    return Player.IsValid() && Player->GetPawn() && Player->GetViewTarget() == Player->GetPawn()
        && !Player->bShowMouseCursor && !Player->IsPaused();
}

FString ADragonsWager::GetWagerState() const
{
    const TCHAR* Names[] = { TEXT("idle"),TEXT("hunting"),TEXT("returned"),TEXT("caught") };
    int32 Owners=0;
    for (const auto& WatchOwner : WatchOwners) if (WatchOwner.IsValid()) ++Owners;
    return FString::Printf(TEXT("{\"phase\":\"%s\",\"round\":%d,\"purse\":%d,\"jewels\":%d,\"crown\":%s,\"exposure\":%.3f,\"watchOwners\":%d,\"best\":%d,\"frozen\":%s}"),
        Names[static_cast<int>(Rules.State)], Rules.Round, Rules.Purse, Rules.Jewels, Rules.Crown ? TEXT("true") : TEXT("false"),
        Rules.Exposure,Owners,BestScore,bFrozen ? TEXT("true") : TEXT("false"));
}

void ADragonsWager::Stop()
{
    ReleaseFreeze();
    if (Player.IsValid()) DisableInput(Player.Get());
    Rules.Leave(); Player.Reset();
    AvailableJewels.Reset(); AttentionUntil = DecoyUntil = 0;
    for (const auto& Light : WatchLights) Light->SetVisibility(false);
    Rays->ClearInstances();
}

void ADragonsWager::Quit()
{
    if (!CanPlayInput()) return;
    Stop(); DrawPieces();
    Say(TEXT("Isla: Another time, then. Your banked record is safe."));
}

void ADragonsWager::DevKey(const FString& Key)
{
    if (Key == TEXT("K")) Decoy();
    else if (Key == TEXT("B")) Bank();
    else if (Key == TEXT("T")) Raise();
    else if (Key == TEXT("X")) Quit();
}

static FAutoConsoleCommandWithWorldAndArgs GWagerKey(
    TEXT("sib.Wager.Key"),
    TEXT("Presses a Dragon's Wager key (K, B, T or X) for the first player, as the keyboard would."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        // From the editor console the given world is the editor's; the wager lives in PIE.
        if (!(World && World->IsGameWorld()) && GEngine)
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (Context.World() && Context.World()->IsGameWorld()) { World = Context.World(); break; }
        if (!World || Args.Num() == 0) return;
        for (TActorIterator<ADragonsWager> It(World); It; ++It) { It->DevKey(Args[0]); return; }
    }));

void ADragonsWager::Bank()
{
    if (!CanPlayInput()) return;
    const int32 FinishedRound = Rules.Round;
    const int32 Score = Rules.Bank();
    if (Score <= 0) return;
    BestScore = FMath::Max(BestScore,Score); BestRound = FMath::Max(BestRound,FinishedRound);
    SaveRecord();
    Say(FString::Printf(TEXT("Isla: A wise choice. %d treasure banked.%s"),Score,
        bSaveFailed ? TEXT(" Record could not be saved to disk.") : TEXT("")));
    TableSounds::Play(this,ETableSound::KartFinish,0.5f);
    Stop(); DrawPieces();
}

void ADragonsWager::SaveRecord()
{
    auto* Save = NewObject<UDragonsWagerSaveGame>(this);
    Save->BestScore = BestScore; Save->BestRound = BestRound;
    bSaveFailed = !FSibeliusSaveIO::Commit(Save,WagerDetail::Slot);
}

void ADragonsWager::Raise()
{
    if (!CanPlayInput()) return;
    if (Rules.Raise()) { ResetRound(); Say(TEXT("Isla: All of it stays on the table. I do admire your nerve.")); }
}

bool ADragonsWager::ClearPath(const FVector& From, const FVector& To, const AActor* Ignore, const AActor* IgnoreAlso) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonsWagerSight),true,this);
    if (Ignore) Params.AddIgnoredActor(Ignore);
    if (IgnoreAlso) Params.AddIgnoredActor(IgnoreAlso);
    return !GetWorld()->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Params);
}

void ADragonsWager::Decoy()
{
    if (!CanPlayInput()) return;
    if (!Rules.Decoy()) { Say(TEXT("A decoy costs one collected jewel.")); return; }
    FVector Eye; FRotator Aim;
    Player->GetPlayerViewPoint(Eye,Aim);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonsWagerDecoy),true,this);
    Params.AddIgnoredActor(Player->GetPawn());
    const FVector End = Eye + Aim.Vector()*900;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,Eye,End,ECC_Visibility,Params);
    DecoyPosition = bHit ? Hit.ImpactPoint + Hit.ImpactNormal*25 : End;
    DecoyUntil = Clock + 4.5f;
    Say(TEXT("The dragons follow your glittering decoy. Move!"));
    TableSounds::Play(this,ETableSound::Chime100,0.55f,1.3f);
}

void ADragonsWager::Say(const FString& Line)
{
    UE_LOG(LogSibeliusGame, Log, TEXT("[DragonsWager] %s"), *Line);
    // While a wager runs the status panel carries the line; a toast as well said it twice.
    if (Rules.State == DragonsWager::Phase::Idle) ASibeliusHUD::Toast(this,Line,5);
    else { Notice = Line; NoticeUntil = Clock + 5; }
}

bool ADragonsWager::UpdateWatch(float DeltaSeconds)
{
    bool Seen = false;
    const APawn* Pawn = Player.IsValid() ? Player->GetPawn() : nullptr;
    Rays->ClearInstances();
    for (int32 I=0; I<WatchLights.Num(); ++I)
    {
        USpotLightComponent* Light = WatchLights[I].Get();
        const bool Active = Rules.State == DragonsWager::Phase::Hunting;
        Light->SetVisibility(Active);
        if (!Active || !Pawn || !WatchPositions.IsValidIndex(I)) continue;
        const FVector Eye = WatchPositions[I];
        const AActor* Dragon = WatchOwners.IsValidIndex(I) ? WatchOwners[I].Get() : nullptr;
        // The two sweeps are out of phase, leaving readable windows to cross.
        const float Sweep = Clock * (0.43f + Rules.Round*0.045f) + I*2.3f;
        const float TargetY = 250 + FMath::Cos(Sweep*0.7f)*950;
        FVector Target = FVector(15 + FMath::Sin(Sweep)*650, TargetY,
            100 + FMath::Clamp((TargetY-100)/850,0.0f,1.0f)*500);
        if (Clock < AttentionUntil) Target = AttentionPosition;
        if (Clock < DecoyUntil) Target = DecoyPosition;
        const FRotator Desired = (Target-Eye).Rotation();
        const FRotator Facing = FMath::RInterpConstantTo(Light->GetComponentRotation(),Desired,DeltaSeconds,42+Rules.Round*3);
        Light->SetWorldLocationAndRotation(Eye,Facing);
        Light->SetOuterConeAngle(GazeHalfAngle);
        const FVector Direction = Facing.Vector();
        FVector PlayerEye; FRotator Unused;
        Pawn->GetActorEyesViewPoint(PlayerEye,Unused);
        for (const FVector& Sample : { Pawn->GetActorLocation(), PlayerEye })
        {
            const FVector Offset = Sample-Eye;
            if (Offset.Size() < GazeRange && FVector::DotProduct(Offset.GetSafeNormal(),Direction) > FMath::Cos(FMath::DegreesToRadians(GazeHalfAngle))
                && ClearPath(Eye,Sample,Pawn,Dragon)) Seen = true;
        }
        // Where the gaze lands is marked with the sight test's own traces, so the ringed
        // ground is the danger: a dotted line from the eye, a ring at the cone's edge.
        FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonsWagerBeam),true,this);
        Params.AddIgnoredActor(Pawn);
        if (Dragon) Params.AddIgnoredActor(Dragon);
        auto Land = [&](const FVector& Along, FHitResult& Out)
        {
            return GetWorld()->LineTraceSingleByChannel(Out,Eye,Eye + Along*GazeRange,ECC_Visibility,Params);
        };
        FHitResult Hit;
        const bool bHit = Land(Direction,Hit);
        const float Length = bHit ? Hit.Distance : GazeRange;
        for (float D=120; D<Length; D+=120)
            WagerDetail::Orb(Rays,Eye + Direction*D,8,WagerDetail::Amber,0.6f);
        if (bHit) WagerDetail::Orb(Rays,Hit.ImpactPoint + Hit.ImpactNormal*4,20,WagerDetail::Amber,1);
        FVector Across, Up;
        Direction.FindBestAxisVectors(Across,Up);
        const float Spread = FMath::DegreesToRadians(GazeHalfAngle);
        for (int32 K=0; K<20; ++K)
        {
            const float Turn = K*2*PI/20;
            const FVector Edge = Direction*FMath::Cos(Spread) + (Across*FMath::Cos(Turn) + Up*FMath::Sin(Turn))*FMath::Sin(Spread);
            FHitResult Rim;
            if (Land(Edge,Rim)) WagerDetail::Orb(Rays,Rim.ImpactPoint + Rim.ImpactNormal*4,12,WagerDetail::Amber,1);
        }
    }
    return Seen;
}

void ADragonsWager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Rules.State == DragonsWager::Phase::Idle) return;
    auto* PC = Player.Get();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { Stop(); DrawPieces(); return; }
    // Conversation, menus, or other camera interactions suspend the wager.
    if (PC->GetViewTarget() != Pawn || PC->bShowMouseCursor || PC->IsPaused()) return;
    Clock += DeltaSeconds;
    if (FVector::Dist(Pawn->GetActorLocation(),GetActorLocation()) > 8000) { Quit(); return; }
    const bool Seen = UpdateWatch(DeltaSeconds);
    if (Rules.Tick(DeltaSeconds,Seen))
    {
        PC->SetIgnoreMoveInput(true); bFrozen = true; FrozenPawn = Pawn;
        TurnToGold();
        // First person never sees its own gilded body, so the view itself turns gold.
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->StartCameraFade(0,0.55f,0.35f,FLinearColor(1.0f,0.62f,0.12f),false,true);
        if (auto* Character = Cast<ACharacter>(Pawn)) Character->GetCharacterMovement()->StopMovementImmediately();
        TableSounds::Play(this,ETableSound::LanternOver,0.65f);
        Say(TEXT("Isla: A splendid addition to my collection. Shall we try again?"));
    }
    if (Rules.State == DragonsWager::Phase::Caught && Rules.Recovery <= 0)
    {
        ReleaseFreeze();
        if (!Respawn(Pawn))
        {
            Stop(); DrawPieces();
            Say(TEXT("The starting spot is blocked. Wager ended; you can move freely."));
            return;
        }
        PC->SetControlRotation(Pawn->GetActorRotation());
        Rules.Start(); ResetRound();
    }
    if (Rules.State == DragonsWager::Phase::Hunting)
    {
        const FVector Here = Pawn->GetActorLocation();
        if (!Rules.Crown && FVector::Dist(Here,CrownPosition) < 145 && ClearPath(Here,CrownPosition,Pawn) && Rules.TakeCrown())
        {
            Say(TEXT("The crown is yours. Return to the pedestal and press E."));
            TableSounds::Play(this,ETableSound::Chime1000,0.6f);
        }
        for (int32 I=AvailableJewels.Num()-1; I>=0; --I)
        {
            const FVector At = JewelPositions[AvailableJewels[I]];
            if (FVector::Dist(Here,At) < 105 && ClearPath(Here,At,Pawn) && Rules.TakeJewel())
            {
                AvailableJewels.RemoveAt(I);
                AttentionPosition = Here; AttentionUntil = Clock + 2;
                TableSounds::Play(this,ETableSound::Chime100,0.55f);
            }
        }
        if (Seen && Rules.Grace <= 0 && Clock > WarningSoundAt)
        {
            WarningSoundAt = Clock + 0.45f;
            TableSounds::Play(this,ETableSound::KartBeep,0.3f,1+Rules.Exposure);
        }
    }
    DrawPieces(); ShowStatus();
}

bool ADragonsWager::Respawn(APawn* Pawn)
{
    // TeleportTo tests placement; never force a pawn into a newly occupied spot.
    if (Pawn->TeleportTo(StartTransform.GetLocation(),StartTransform.Rotator(),false,false)) return true;
    // Isla wanders the hall. If she is standing on the start, use the keep's arrival.
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        if (Pawn->TeleportTo(It->GetActorLocation(),It->GetActorRotation(),false,false)) return true;
    return false;
}

void ADragonsWager::DrawPieces()
{
    using namespace WagerDetail;
    Gold->ClearInstances(); Gems->ClearInstances(); CrownMesh->ClearInstances(); JewelDisplay->ClearInstances();
    auto Stone = [&](const FVector& At, float Scale, const FLinearColor& Color)
    {
        const int32 Index = JewelDisplay->AddInstance(FTransform(FRotator(0,Clock*24,0),At,FVector(Scale)),true);
        JewelDisplay->SetCustomData(Index,TArray<float>{Color.R,Color.G,Color.B,0.0f},true);
    };
    // A restrained gold filigree rim on the keep's own red-and-gold pedestal.
    for (int32 I=0; I<32; ++I)
    {
        const float A = I*2*PI/32;
        Orb(Gold,GetActorLocation()+FVector(FMath::Cos(A)*23,FMath::Sin(A)*23,104),3,Amber,0);
    }
    const int32 RecordGems = FMath::Min(8,BestScore/100);
    for (int32 I=0; I<RecordGems; ++I)
    {
        const float A=I*2*PI/8;
        Stone(GetActorLocation()+FVector(FMath::Cos(A)*18,FMath::Sin(A)*18,109),0.22f,Emerald);
    }
    Stone(GetActorLocation()+FVector(0,0,121),0.55f,Amber);
    if (Rules.State != DragonsWager::Phase::Idle)
    {
        for (int32 Index : AvailableJewels)
            Stone(JewelPositions[Index]+FVector(0,0,FMath::Sin(Clock*2+Index)*8),0.85f,Index%2 ? Emerald : Ruby);
        if (Clock < DecoyUntil)
            for (int I=0; I<12; ++I)
                Orb(Gems,DecoyPosition+FVector(FMath::Cos(Clock*4+I)*35,FMath::Sin(Clock*4+I)*35,I*4),5,Emerald,0.12f);
    }
    if (!Rules.Crown)
    {
        const FRotator Rotation(0,Clock*12,0);
        CrownMesh->AddInstance(FTransform(Rotation,CrownPosition-FVector(0,0,15),FVector(1)),true);
        for (int I=0; I<8; ++I)
        {
            const float A = I*2*PI/8;
            const FVector At = CrownPosition+Rotation.RotateVector(FVector(FMath::Cos(A)*30,FMath::Sin(A)*30,-9));
            Stone(At,0.18f,I%2 ? Emerald : Ruby);
        }
    }
    if (Rules.State == DragonsWager::Phase::Caught && Player.IsValid() && Player->GetPawn())
    {
        const FVector At = Player->GetPawn()->GetActorLocation();
        for (int I=0; I<24; ++I)
            Orb(Gems,At+FVector(FMath::Cos(I*2.4f)*65,FMath::Sin(I*2.4f)*65,(I-12)*6),4,Amber,0.15f);
    }
    const FString Record = BestScore > 0 ? FString::Printf(TEXT("Best: %d treasure, round %d"),BestScore,BestRound)
        : FString(TEXT("No treasure banked yet"));
    Plaque->SetText(FText::FromString(Rules.State == DragonsWager::Phase::Idle
        ? FString::Printf(TEXT("THE DRAGON'S WAGER\n[E] Steal the crown\n%s"),*Record)
        : FString::Printf(TEXT("THE DRAGON'S WAGER\nRound %d of %d"),Rules.Round,DragonsWager::Rules::MaxRounds)));
}

void ADragonsWager::ShowStatus()
{
    auto* Hud = Player.IsValid() ? Cast<ASibeliusHUD>(Player->GetHUD()) : nullptr;
    if (!Hud) return;
    FString Goal = Rules.Crown ? TEXT("RETURN TO THE PEDESTAL [E]") : TEXT("STEAL THE CROWN ABOVE THE STAIRS");
    FString Keys = TEXT("[K] Jewel decoy (costs one jewel)   [X] Leave wager");
    if (Rules.State == DragonsWager::Phase::Returned)
    {
        Goal = TEXT("SAFE - BANK OR WAGER EVERYTHING");
        Keys = Rules.Round < DragonsWager::Rules::MaxRounds ? TEXT("[B] Bank treasure   [T] Risk next round   [X] Forfeit") : TEXT("[B] Bank treasure - final round complete");
    }
    if (Rules.State == DragonsWager::Phase::Caught) Goal = TEXT("CAUGHT - TURNING TO GOLD...");
    else if (Rules.Grace > 0) Goal = TEXT("THE DRAGONS ARE WAKING - MOVE INTO COVER");
    else if (Rules.Exposure > 0) Goal = TEXT("THEY SEE YOU - BREAK LINE OF SIGHT!");
    Hud->ShowTablePanel(FString::Printf(TEXT("THE DRAGON'S WAGER | Round %d | At risk: %d | Jewels: %d\n%s"),Rules.Round,Rules.Purse,Rules.Jewels,*Goal),
        Keys,0.25f,Clock < NoticeUntil ? Notice : TEXT("Jewels chime. Stone blocks their gaze. A decoy buys four seconds."),Rules.Exposure/Rules.CatchSeconds());
    Hud->ShowTableMarkers({FTableWorldMarker{Rules.Crown ? GetActorLocation()+FVector(0,0,150) : CrownPosition+FVector(0,0,60),
        Rules.Crown ? TEXT("RETURN CROWN [E]") : TEXT("CROWN"),WagerDetail::Amber}});
}
