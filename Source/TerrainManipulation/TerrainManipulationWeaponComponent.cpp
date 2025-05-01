// Copyright Epic Games, Inc. All Rights Reserved.


#include "TerrainManipulationWeaponComponent.h"
#include "TerrainManipulationCharacter.h"
#include "TerrainManipulationProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

#include "DynamicTerrain/Dynamic_Terrain.h"

// Sets default values for this component's properties
UTerrainManipulationWeaponComponent::UTerrainManipulationWeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}


void UTerrainManipulationWeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	if (firingType == EFiringType::FT_Projectile) 
	{
		if (ProjectileClass != nullptr)
		{
			UWorld* const World = GetWorld();
			if (World != nullptr)
			{
				APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
				const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
				// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
				const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

				//Set Spawn Collision Handling Override
				FActorSpawnParameters ActorSpawnParams;
				ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

				// Spawn the projectile at the muzzle
				World->SpawnActor<ATerrainManipulationProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);
			}
		}
	}
	else if (firingType == EFiringType::FT_Raycast) 
	{
		APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
		const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);

		// You can use this to customize various properties about the trace
		FCollisionQueryParams Params;
		// Ignore the player's pawn
		Params.AddIgnoredActor(Character);
		Params.bDebugQuery = true;

		// The hit result gets populated by the line trace
		FHitResult Hit;

		// Raycast out from the camera, only collide with pawns (they are on the ECC_Pawn collision channel)
		FVector Start = PlayerController->PlayerCameraManager->GetCameraLocation();
		float raycastLength = 1000.0f;
		FVector End = Start + (PlayerController->PlayerCameraManager->GetCameraRotation().Vector() * raycastLength);
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECollisionChannel::ECC_Visibility, Params);

		if (bHit) {
			UE_LOG(LogTemp, Display, TEXT("Hit object"));
			Hit.GetActor()->NotifyHit(Hit.GetComponent(), GetOwner(), this, false, Hit.ImpactPoint, Hit.ImpactNormal, FVector::ZeroVector, Hit);
			/*ADynamic_Terrain* dynamicTerrain = Cast<ADynamic_Terrain>(Hit.GetActor());
			if (dynamicTerrain) {
				UE_LOG(LogTemp, Display, TEXT("Successfully cast to dynamic terrain"));
				dynamicTerrain->AddToDataGridInRadius(Hit.ImpactPoint, 100, -1);
				dynamicTerrain->CalculateMesh();
			}*/
		}
		else {
			UE_LOG(LogTemp, Display, TEXT("Miss object"))
		}
	}
	
	
	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}
	
	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

bool UTerrainManipulationWeaponComponent::AttachWeapon(ATerrainManipulationCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no weapon component yet
	if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UTerrainManipulationWeaponComponent>())
	{
		return false;
	}

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTerrainManipulationWeaponComponent::Fire);
		}
	}

	return true;
}

void UTerrainManipulationWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ensure we have a character owner
	if (Character != nullptr)
	{
		// remove the input mapping context from the Player Controller
		if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(FireMappingContext);
			}
		}
	}

	// maintain the EndPlay call chain
	Super::EndPlay(EndPlayReason);
}