#pragma once

#include "GameFramework/Pawn.h"
#include "FGPlayer.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UFGMovementComponent;
class UStaticMeshComponent;
class USphereComponent;
class UFGPlayerSettings;
class UFGNetDebugWidget;
class AFGRocket;
class AFGPickup;

UCLASS()
class FGNET_API AFGPlayer : public APawn
{
	GENERATED_BODY()

public:
	AFGPlayer();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, Category = Settings)
	UFGPlayerSettings* PlayerSettings = nullptr;

	UFUNCTION(BlueprintPure)
	bool IsBraking() const { return bBrake; }

	UFUNCTION(BlueprintPure)
	int32 GetPing() const;

	UPROPERTY(EditAnywhere, Category = Debug)
	TSubclassOf<UFGNetDebugWidget> DebugMenuClass;

	UFUNCTION(Server, Unreliable)
	void Server_SendLocation(const FVector& LocationToSend);

	void OnPickup(AFGPickup* Pickup);

	void PredictPickedUp(AFGPickup* Pickup);

	UFUNCTION(Server, Reliable)
    void Server_OnPickup(AFGPickup* Pickup);

	UFUNCTION(NetMulticast, Reliable)
	void MultiCast_OnPickup(AFGPickup* Pickup);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_UpdateItemAmount(EFGPickupType PickUpType, int32 ServerAmount);

	//UFUNCTION(Server, Reliable)
	//void Client_OnPickup(AFGPickup* Pickup, bool bConfirmed);
	//UFUNCTION(Client, Reliable)
	//void Client_OnPickupRockets(int32 PickedUpRockets);

	UFUNCTION(Server, Unreliable)
	void Server_SendYaw(float NewYaw);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SendLocation(const FVector& LocationToSend);
	UFUNCTION(NetMulticast, Unreliable)
	void Mulitcast_SendRotation(const FRotator& RotationToSend);
	void ShowDebugMenu();
	void HideDebugMenu();

	UFUNCTION(BlueprintPure)
	int32 GetNumRockets() const { return NumRockets; }

	UFUNCTION(BlueprintImplementableEvent, Category = Player)
    void BP_OnNumRocketsChanged(int32 NewNumRockets);

	int32 GetNumActiveRockets() const;

	void FireRocket();

	void SpawnRockets();

	UFUNCTION(BlueprintImplementableEvent, Category = Player)
	void BP_OnNumHealthChanged(int32 NewHealth);
	
	UPROPERTY(BlueprintReadOnly, Category = Player)
	int32 Health = 100;
	UPROPERTY(BlueprintReadOnly, Category = Player)
	int32 NumRockets = 0;

	UFUNCTION(Server, Reliable, BlueprintCallable)
		void Server_OntHit(AFGRocket* Rocket);
	UFUNCTION(NetMulticast, Reliable)
		void Multicast_OntHit(AFGRocket* Rocket, int32 NewHealth);
private:

	UPROPERTY(EditAnywhere)
	int32 RocketDamage = 10;

	int32 ServerHealth = 100;


	int32 ServerNumRockets = 0;

	UPROPERTY(Replicated, Transient)
	TArray<AFGRocket*> RocketInstances;

	UPROPERTY(EditAnywhere, Category = Weapon)
	TSubclassOf<AFGRocket> RocketClass;

	UPROPERTY(EditAnywhere, Category = Weapon)
	bool bUnlimitedRockets = false;	

	int32 MaxActiveRockets = 3;

	float FireCooldownElapsed = 0.0f;

	FVector GetRocketStartLocation() const;

	AFGRocket* GetFreeRocket() const;

	UFUNCTION(Server, Reliable)
    void Server_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);

	UFUNCTION(NetMulticast, Reliable)
    void Multicast_FireRocket(AFGRocket* NewRocket, const FVector& RocketStartLocation, const FRotator& RocketFacingRotation);


	UFUNCTION(Client, Reliable)
    void Client_RemoveRocket(AFGRocket* RocketToRemove);

	UFUNCTION(BlueprintCallable)
    void Cheat_IncreaseRockets(int32 InNumRockets);
	
	



	
	void Handle_Accelerate(float Value);
	void Handle_Turn(float Value);
	void Handle_BrakePressed();
	void Handle_BrakeReleased();
	

	void Handle_DebugMenuPressed();

	void Handle_FirePressed();

	void CreateDebugWidget();

	UPROPERTY(Transient)
	UFGNetDebugWidget* DebugMenuInstance = nullptr;

	bool bShowDebugMenu = false;

	UPROPERTY(Replicated)
	float ReplicatedYaw = 0.0f;

	UPROPERTY(Replicated)
	FVector ReplicatedLocation;

	float Forward = 0.0f;
	float Turn = 0.0f;

	float MovementVelocity = 0.0f;
	float Yaw = 0.0f;

	bool bBrake = false;

	UPROPERTY(VisibleDefaultsOnly, Category = Collision)
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	USpringArmComponent* SpringArmComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Camera)
	UCameraComponent* CameraComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = Movement)
	UFGMovementComponent* MovementComponent;

	


};