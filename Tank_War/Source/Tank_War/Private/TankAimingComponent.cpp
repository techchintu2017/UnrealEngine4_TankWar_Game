// Copyright EmbraceIT Ltd.

#include "../Tank_War/Public/TankAimingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../Tank_War/Public/TankBarrel.h"
#include "../Tank_War/Public/TankTurret.h"
#include "../Tank_War/Public/Projectile.h"
#include "Tank_War.h"

// We never want assertions triggering in production code, they are for development and debugging only. 
// When the player is playing we need to handle errors.
UTankAimingComponent::UTankAimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTankAimingComponent::BeginPlay()
{
	// So that first first is after initial reload
	Super::BeginPlay();
	LastFireTime = FPlatformTime::Seconds();
}

void UTankAimingComponent::Initialise(UTankBarrel* BarrelToSet, UTankTurret* TurretToSet)
{
	Barrel = BarrelToSet;
	Turret = TurretToSet;
}

// Sets logic for relaoding and firing, every frame asks is it time for relaoding etc
void UTankAimingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// Out Of Ammo
	if (RoundsLeft <= 0)
	{
		FiringState = EFiringState::OutOfAmmo;
	}
	// Reloads
	if ((FPlatformTime::Seconds() - LastFireTime) < ReloadTimeInSeconds)
	{
		FiringState = EFiringState::Reloading;
	}
	// Fires
	else if (IsBarrelMoving())
	{
		FiringState = EFiringState::Aiming;
	}
	// Locks
	else
	{
		FiringState = EFiringState::Locked;
	}
}

// Returns how much ammo we actually have left
int32 UTankAimingComponent::GetRoundsLeft() const
{
	return RoundsLeft;
}

// Simply returns the firing state of the AI tank (The ENUM)
EFiringState UTankAimingComponent::GetFiringState() const
{
	return FiringState;
}

// If forward direction and aim direction are equal then the barrel is not moving 
bool UTankAimingComponent::IsBarrelMoving()
{
	// Only use ensure for things we never expect to happen
	if (!ensure(Barrel)) { return false; }
	auto BarrelForward = Barrel->GetForwardVector();
	return !BarrelForward.Equals(AimDirection, 0.01);
}

void UTankAimingComponent::AimAt(FVector HitLocation)
{
	//we do this to protect the barrel and prevent errors ensure is a macro that explains important problems
	//ensure helps prevent engine crash
	if (!ensure(Barrel)) { return; }
	FVector OutLaunchVelocity;
	// gets the socket from the barrel asset we creeated
	FVector StartLocation = Barrel->GetSocketLocation(FName("Projectile"));
	
	//if we have a solution move to the next code block
	bool bHaveAimSolution = UGameplayStatics::SuggestProjectileVelocity
	(
		this,
		OutLaunchVelocity,
		StartLocation,
		HitLocation,
		LaunchSpeed,
		false,
		0,
		0,
		ESuggestProjVelocityTraceOption::DoNotTrace
	);

	// calls the move barrel function 
	if (bHaveAimSolution)
	{
		AimDirection = OutLaunchVelocity.GetSafeNormal();
		MoveBarrelTowards(AimDirection);
	}
}

// MoveBarrelTowards at paticular aim direction
void UTankAimingComponent::MoveBarrelTowards(FVector TargetAimDirection)
{
	if (!ensure(Barrel) || !ensure(Turret)) { return; }

	// gets the x direction of the barrel to get, rotation gets yaw, pitch, and roll
	auto BarrelRotator = Barrel->GetForwardVector().Rotation();
	auto AimAsRotator = TargetAimDirection.Rotation();
	// Gets the difference of the roation
	auto DeltaRotator = AimAsRotator - BarrelRotator;
	Barrel->Elevate(DeltaRotator.Pitch);
	// Always yaw the shortest way so it doesnt turn the long way around iff Absolut value is less than 180 we go backwards to avoid going the long way
	if (FMath::Abs(DeltaRotator.Yaw) < 180)
	{
		Turret->Rotate(DeltaRotator.Yaw);
	}
	else
	{
		Turret->Rotate(-DeltaRotator.Yaw);
	}
}

// Fires the projectile
void UTankAimingComponent::Fire()
{
	// Spawns a projectile at the socket location on the barrel and introduces the concept of projectile to the computer
	if (FiringState == EFiringState::Locked || FiringState == EFiringState::Aiming)
	{
		if (!ensure(Barrel)) { return; }
		if (!ensure(ProjectileBlueprint)) { return; }
		auto Projectile = GetWorld()->SpawnActor<AProjectile>(
			ProjectileBlueprint,
			Barrel->GetSocketLocation(FName("Projectile")),
			Barrel->GetSocketRotation(FName("Projectile"))
			);

		// Resets last time fired
		Projectile->LaunchProjectile(LaunchSpeed);
		LastFireTime = FPlatformTime::Seconds();
		RoundsLeft--;
	}
}
