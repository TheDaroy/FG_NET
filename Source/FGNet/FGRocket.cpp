#include "FGRocket.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Player/FGPlayer.h"

AFGRocket::AFGRocket()
{
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneCompRoot"));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetGenerateOverlapEvents(false);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	SetReplicates(true);
}

void AFGRocket::BeginPlay()
{
	Super::BeginPlay();

	CachedCollisionQueryParams.AddIgnoredActor(this);
	// Owner will be the player that instantiated this.
	CachedCollisionQueryParams.AddIgnoredActor(GetOwner());

	SetRocketVisibility(false);
}

void AFGRocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTimeElapsed -= DeltaTime;
	DistanceMoved += MovementVelocity * DeltaTime;

	FacingRotationStart = FQuat::Slerp(FacingRotationStart.ToOrientationQuat(), FacingRotationCorrection, 0.9f * DeltaTime).Vector();

#if !UE_BUILD_SHIPPING
	if(bDebugDrawCorrection)
	{
		const float ArrowLength = 3000.0f;
		const float ArrowSize = 50.0f;
		DrawDebugDirectionalArrow(GetWorld(), RocketStartLocation, RocketStartLocation + OriginalFacingDirection * ArrowLength, ArrowSize, FColor::Red);
		DrawDebugDirectionalArrow(GetWorld(), RocketStartLocation, RocketStartLocation + FacingRotationStart * ArrowLength, ArrowSize, FColor::Green);
	}
#endif // !UE_BUILD_SHIPPING

	const FVector NewLocation = RocketStartLocation + FacingRotationStart * DistanceMoved;

	SetActorLocation(NewLocation);
	
	FHitResult Hit;
	const FVector StartLoc = NewLocation;
	const FVector EndLoc = StartLoc + FacingRotationStart * 100.0f;
	GetWorld()->LineTraceSingleByChannel(Hit, StartLoc, EndLoc, ECC_Visibility, CachedCollisionQueryParams);

	if (Hit.bBlockingHit)
		Explode(Hit.Actor.Get());

	if (LifeTimeElapsed < 0.0f)
		Explode(nullptr);
}

void AFGRocket::StartMoving(const FVector& Forward, const FVector& InStartLocation)
{
	FacingRotationStart = Forward;
	FacingRotationCorrection = FacingRotationStart.ToOrientationQuat();
	RocketStartLocation = InStartLocation;
	SetActorLocationAndRotation(InStartLocation, Forward.Rotation());
	bIsFree = false;
	SetActorTickEnabled(true);
	SetRocketVisibility(true);
	LifeTimeElapsed = LifeTime;
	DistanceMoved = 0.0f;
	OriginalFacingDirection = FacingRotationStart;
}

void AFGRocket::ApplyCorrection(const FVector& Forward)
{
	FacingRotationCorrection = Forward.ToOrientationQuat();
}

void AFGRocket::Explode(AActor* CollidedActor)
{
	AFGPlayer* HitPlayer = Cast<AFGPlayer>(CollidedActor);
	if (HitPlayer && HitPlayer->IsLocallyControlled())
	{
		HitPlayer->Server_OntHit(this);
	}
	if (Explosion != nullptr)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Explosion, GetActorLocation(), GetActorRotation(), true);
	MakeFree();
}

void AFGRocket::MakeFree()
{
	bIsFree = true;
	SetActorTickEnabled(false);
	SetRocketVisibility(false);
}

void AFGRocket::SetRocketVisibility(bool bVisible)
{
	RootComponent->SetVisibility(bVisible, true);
}

