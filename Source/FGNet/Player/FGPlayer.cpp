#include "FGPlayer.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SphereComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerState.h"
#include "../Components/FGMovementComponent.h"
#include "../FGMovementStatics.h"
#include "Net/UnrealNetwork.h"
#include "FGPlayerSettings.h"
#include "../Debug/UI/FGNetDebugWidget.h"
#include "../FGRocket.h"
#include "../FGPickup.h"

AFGPlayer::AFGPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComponent;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	SpringArmComponent->bInheritYaw = false;
	SpringArmComponent->SetupAttachment(CollisionComponent);

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(SpringArmComponent);

	MovementComponent = CreateDefaultSubobject<UFGMovementComponent>(TEXT("MovementComponent"));

	SetReplicatingMovement(false);
}

void AFGPlayer::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent->SetUpdatedComponent(CollisionComponent);

	CreateDebugWidget();
	if (DebugMenuInstance != nullptr)
	{
		DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	SpawnRockets();

	BP_OnNumRocketsChanged(NumRockets);
	BP_OnNumHealthChanged(Health);
}

void AFGPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FireCooldownElapsed -= DeltaTime;

	if (!ensure(PlayerSettings != nullptr))
		return;


	if (IsLocallyControlled())
	{
		const float MaxVelocity = PlayerSettings->MaxVelocity;
		const float Acceleration = PlayerSettings->Acceleration;
		const float Friction = IsBraking() ? PlayerSettings->BrakingFriction : PlayerSettings->Friction;
		const float Alpha = FMath::Clamp(FMath::Abs(MovementVelocity / (PlayerSettings->MaxVelocity * 0.75f)), 0.0f, 1.0f);
		const float TurnSpeed = FMath::InterpEaseOut(0.0f, PlayerSettings->TurnSpeedDefault, Alpha, 5.0f);
		const float TurnDirection = MovementVelocity > 0.0f ? Turn : -Turn;

		Yaw += (TurnDirection * TurnSpeed) * DeltaTime;
		FQuat WantedFacingDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		MovementComponent->SetFacingRotation(WantedFacingDirection, 10.5f);


		FFGFrameMovement FrameMovement = MovementComponent->CreateFrameMovement();

		MovementVelocity += Forward * Acceleration * DeltaTime;
		MovementVelocity = FMath::Clamp(MovementVelocity, -MaxVelocity, MaxVelocity);
		MovementVelocity *= FMath::Pow(Friction, DeltaTime);

		MovementComponent->ApplyGravity();
		FrameMovement.AddDelta(GetActorForwardVector() * MovementVelocity * DeltaTime);
		MovementComponent->Move(FrameMovement);

		Server_SendLocation(GetActorLocation());
		Server_SendYaw(MovementComponent->GetFacingRotation().Yaw);
	}
	else
	{
		const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), ReplicatedLocation, DeltaTime, 1.0f);
		SetActorLocation(NewLocation);
		MovementComponent->SetFacingRotation(FRotator(0.0f, ReplicatedYaw, 0.0f));
		SetActorRotation(MovementComponent->GetFacingRotation());
	}
}

void AFGPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Accelerate"), this, &AFGPlayer::Handle_Accelerate);
	PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AFGPlayer::Handle_Turn);

	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Pressed, this, &AFGPlayer::Handle_BrakePressed);
	PlayerInputComponent->BindAction(TEXT("Brake"), IE_Released, this, &AFGPlayer::Handle_BrakeReleased);

	PlayerInputComponent->BindAction(TEXT("DebugMenu"), IE_Pressed, this, &AFGPlayer::Handle_DebugMenuPressed);

	PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFGPlayer::Handle_FirePressed);
}

int32 AFGPlayer::GetPing() const
{
	if (GetPlayerState())
	{
		return static_cast<int32>(GetPlayerState()->GetPing());
	}

	return 0;
}

void AFGPlayer::SpawnRockets()
{	
	if (HasAuthority() && RocketClass != nullptr)
	{
		const int32 RocketCache = 8;

		for (int32 Index = 0; Index < RocketCache; ++Index)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.ObjectFlags = RF_Transient;
			SpawnParams.Instigator = this;
			SpawnParams.Owner = this;
			AFGRocket* NewRocketInstance = GetWorld()->SpawnActor<AFGRocket>(RocketClass, GetActorLocation(), GetActorRotation(), SpawnParams);
			RocketInstances.Add(NewRocketInstance);
		}
	}
}



#pragma region PickUpStuff

void AFGPlayer::OnPickup(AFGPickup* Pickup)
{
	if (IsLocallyControlled())
	{
		//Pickup->HidePickup();
		PredictPickedUp(Pickup);
		Server_OnPickup(Pickup);
	}
}
void AFGPlayer::PredictPickedUp(AFGPickup* Pickup)
{
	switch (Pickup->PickupType)
	{
	case EFGPickupType::Health:
	{
		Health += Pickup->NumPickedUp;
		BP_OnNumHealthChanged(Health);
		break;
	}
	case EFGPickupType::Rocket:
	{
		NumRockets += Pickup->NumPickedUp;
		BP_OnNumRocketsChanged(NumRockets);
		break;
	}
	}
}

//void AFGPlayer::Client_OnPickupRockets_Implementation(int32 PickedUpRockets)
//{
//	NumRockets += PickedUpRockets;
//	BP_OnNumRcketsChanged(NumRockets);	
//}
void AFGPlayer::Server_OnPickup_Implementation(AFGPickup* Pickup)
{
	if (!Pickup->IsPickedUp())
	{
		switch (Pickup->PickupType)
		{
		case EFGPickupType::Health:
		{
			ServerHealth += Pickup->NumPickedUp;
			Multicast_UpdateItemAmount(Pickup->PickupType, ServerHealth);
			MultiCast_OnPickup(Pickup);
			break;
		}
		case EFGPickupType::Rocket:
		{
			ServerNumRockets += Pickup->NumPickedUp;
			Multicast_UpdateItemAmount(Pickup->PickupType, ServerNumRockets);
			
			MultiCast_OnPickup(Pickup);
			break;
		}
		}
		if (GetLocalRole() < ROLE_Authority)
		{
			//Client_OnPickup(Pickup, !Pickup->IsPickedUp());
		}
	}
		
}
void AFGPlayer::MultiCast_OnPickup_Implementation(AFGPickup* Pickup)
{
	Pickup->HandlePickup();
}


//void AFGPlayer::Client_OnPickupCheck_Implementation(AFGPickup* Pickup, bool bPickedUp)
//{
//	if (!bPickedUp) // Pickup wasn't confirmed. Revert.
//	{
//		switch (Pickup->PickupType)
//		{
//		case EFGPickupType::Health:
//		{
//			Health -= Pickup->NumPickedUp;
//			BP_OnNumHealthChanged(Health);
//			Pickup->ShowPickup();
//			break;
//		}
//		case EFGPickupType::Rocket:
//		{
//			NumRockets -= Pickup->NumPickedUp;
//			BP_OnNumRocketsChanged(NumRockets);
//			Pickup->ShowPickup();
//			break;
//		}
//		
//	}
//}

void AFGPlayer::Multicast_UpdateItemAmount_Implementation(EFGPickupType PickUpType, int32 ServerAmount)
{
	if (!IsLocallyControlled())
	{
		switch (PickUpType)
		{
		case EFGPickupType::Rocket:
			BP_OnNumRocketsChanged(ServerAmount);
			break;
		case EFGPickupType::Health:
			BP_OnNumHealthChanged(ServerAmount);
			break;
		default:
			break;
		}
	}
}

#pragma endregion






void AFGPlayer::Server_SendYaw_Implementation(float NewYaw)
{
	ReplicatedYaw = NewYaw;
}
void AFGPlayer::ShowDebugMenu()
{
	CreateDebugWidget();

	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Visible);
	DebugMenuInstance->BP_OnShowWdiget();
}
void AFGPlayer::HideDebugMenu()
{
	if (DebugMenuInstance == nullptr)
		return;

	DebugMenuInstance->SetVisibility(ESlateVisibility::Collapsed);
	DebugMenuInstance->BP_OnHideWdiget();
}
void AFGPlayer::Multicast_SendLocation_Implementation(const FVector& LocationToSend)
{
	if (!IsLocallyControlled())
	{
		SetActorLocation(LocationToSend);
	
	}
}
void AFGPlayer::Mulitcast_SendRotation_Implementation(const FRotator& RotationToSend)
{
	if (!IsLocallyControlled())
	{
		SetActorRotation(RotationToSend);

	}
}
void AFGPlayer::Server_SendLocation_Implementation(const FVector& LocationToSend)
{
	ReplicatedLocation = LocationToSend;
}
int32 AFGPlayer::GetNumActiveRockets() const
{
	int32 NumActive = 0;
	for(AFGRocket* Rocket : RocketInstances)
	{
		if(!Rocket->IsFree())
			NumActive++;
	}

	return NumActive;
}
void AFGPlayer::Handle_Accelerate(float Value)
{
	Forward = Value;
}
void AFGPlayer::Handle_Turn(float Value)
{
	Turn = Value;
}
void AFGPlayer::Handle_BrakePressed()
{
	bBrake = true;
}
void AFGPlayer::Handle_BrakeReleased()
{
	bBrake = false;
}
void AFGPlayer::Handle_DebugMenuPressed()
{
	bShowDebugMenu = !bShowDebugMenu;

	if (bShowDebugMenu)
		ShowDebugMenu();
	else
		HideDebugMenu();
}

#pragma region FireRocket

void AFGPlayer::Handle_FirePressed()
{
	FireRocket();
}
void AFGPlayer::FireRocket()
{
	if (FireCooldownElapsed > 0.0f)
		return;

	if (NumRockets <= 0 && !bUnlimitedRockets)
		return;

	if(GetNumActiveRockets() >= MaxActiveRockets)
		return;

	AFGRocket* NewRocket = GetFreeRocket();

	if (!ensure(NewRocket != nullptr))
		return;

	FireCooldownElapsed = PlayerSettings->FireCooldown;

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (HasAuthority())
		{
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
			BP_OnNumRocketsChanged(NumRockets);
		}
		else
		{
			NumRockets--;
			NewRocket->StartMoving(GetActorForwardVector(), GetRocketStartLocation());
			Server_FireRocket(NewRocket, GetRocketStartLocation(), GetActorRotation());
			BP_OnNumRocketsChanged(NumRockets);
		}
	}
}
void AFGPlayer::Server_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& FacingRotation)
{
	if ((ServerNumRockets - 1) < 0 && !bUnlimitedRockets)
	{
		Client_RemoveRocket(NewRocket);
	}
	else
	{
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(FacingRotation.Yaw, GetActorForwardVector().Rotation().Yaw) * 0.5f;
		const FRotator NewFacingRotation = FacingRotation + FRotator(0.0f, DeltaYaw, 0.0f);
		ServerNumRockets--;
		Multicast_UpdateItemAmount(EFGPickupType::Rocket,ServerNumRockets);
		Multicast_FireRocket(NewRocket, RocketStartLocation, NewFacingRotation);
	}
}
void AFGPlayer::Multicast_FireRocket_Implementation(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& FacingRotation)
{
	if (!ensure(NewRocket != nullptr))
		return;

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		NewRocket->ApplyCorrection(FacingRotation.Vector());
	}
	else
	{
		NumRockets--;
		
		NewRocket->StartMoving(FacingRotation.Vector(), RocketStartLocation);
	}
}
void AFGPlayer::Client_RemoveRocket_Implementation(AFGRocket* RocketToRemove)
{
	RocketToRemove->MakeFree();
}
#pragma endregion
#pragma region HitByRocket

void AFGPlayer::Server_OntHit_Implementation(AFGRocket* Rocket)
{
	ServerHealth -= RocketDamage;
	Multicast_OntHit(Rocket, ServerHealth);
}

void AFGPlayer::Multicast_OntHit_Implementation(AFGRocket* Rocket, int32 NewHealth)
{
	Health = NewHealth;
	BP_OnNumHealthChanged(Health);
}
#pragma endregion






void AFGPlayer::Cheat_IncreaseRockets(int32 InNumRockets)
{
	if (IsLocallyControlled())
		NumRockets += InNumRockets;
}

void AFGPlayer::CreateDebugWidget()
{
	if (DebugMenuClass == nullptr)
		return;

	if (!IsLocallyControlled())
		return;

	if (DebugMenuInstance == nullptr)
	{
		DebugMenuInstance = CreateWidget<UFGNetDebugWidget>(GetWorld(), DebugMenuClass);
		DebugMenuInstance->AddToViewport();
	}
}

AFGRocket* AFGPlayer::GetFreeRocket() const
{
	for (AFGRocket* Rocket : RocketInstances)
	{
		if (Rocket == nullptr)
			continue;

		if (Rocket->IsFree())
			return Rocket;
	}

	return nullptr;
}

FVector AFGPlayer::GetRocketStartLocation() const
{
	const FVector StartLoc = GetActorLocation() + GetActorForwardVector() * 100.0f;
	return StartLoc;
}


void AFGPlayer::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFGPlayer, ReplicatedYaw);
	DOREPLIFETIME(AFGPlayer, ReplicatedLocation);
	DOREPLIFETIME(AFGPlayer, RocketInstances);
	
	
}
