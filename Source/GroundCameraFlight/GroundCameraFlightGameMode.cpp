// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroundCameraFlightGameMode.h"
#include "GroundCameraFlightPawn.h"

AGroundCameraFlightGameMode::AGroundCameraFlightGameMode()
{
	// set default pawn class to our flying pawn
	DefaultPawnClass = AGroundCameraFlightPawn::StaticClass();
}
