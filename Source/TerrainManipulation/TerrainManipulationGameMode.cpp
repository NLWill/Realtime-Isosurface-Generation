// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerrainManipulationGameMode.h"
#include "TerrainManipulationCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATerrainManipulationGameMode::ATerrainManipulationGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
