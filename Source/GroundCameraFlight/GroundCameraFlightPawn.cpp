// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GroundCameraFlightPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

// Main firmware
hf::Hackflight hackflight;

// Controller input
#ifdef _WIN32
#include <receivers/sim/windows.hpp>
#else
#include <receivers/sim/linux.hpp>
#endif
hf::Controller controller;

// Board simulation
#include "HackflightSimBoard.hpp"

// PID tuning
hf::Stabilizer stabilizer = hf::Stabilizer(
	1.0f,      // Level P
	.00001f,    // Gyro cyclic P
	0,			// Gyro cyclic I
	0,			// Gyro cyclic D
	0,			// Gyro yaw P
	0);			// Gyro yaw I


// Pawn methods ---------------------------------------------------

AGroundCameraFlightPawn::AGroundCameraFlightPawn()
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> PlaneMesh;
		FConstructorStatics()
			: PlaneMesh(TEXT("/Game/Flying/Meshes/3DFly.3DFly"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	// Create static mesh component
	PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh0"));
	PlaneMesh->SetStaticMesh(ConstructorStatics.PlaneMesh.Get());	// Set static mesh
	RootComponent = PlaneMesh;

	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->SetupAttachment(RootComponent);	// Attach SpringArm to RootComponent
	SpringArm->TargetArmLength = 160.0f; // The camera follows at this distance behind the character	
	SpringArm->SocketOffset = FVector(0.f,0.f,60.f);
	SpringArm->bEnableCameraLag = false;	// Do not allow camera to lag
	SpringArm->CameraLagSpeed = 15.f;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);	// Attach the camera
	Camera->bUsePawnControlRotation = false; // Don't rotate camera with controller

	// Start Hackflight firmware
	hackflight.init(this, &controller, &stabilizer);

	// Set handling parameters
	Acceleration = 500.f;
	TurnSpeed = 50.f;
	MaxSpeed = 4000.f;
	MinSpeed = 500.f;
	CurrentForwardSpeed = 0.f;//500.f;

    for (uint8_t k=0; k<4; ++k) {
        motors[k] = 0;
    }

    elapsedTime = 0;
}

void AGroundCameraFlightPawn::Tick(float DeltaSeconds)
{
	const FVector LocalMove = FVector(CurrentForwardSpeed * DeltaSeconds, 0.f, 0.f);

	// Move plan forwards (with sweep so we stop when we collide with things)
	AddActorLocalOffset(LocalMove, true);

	// Calculate change in rotation this frame
	FRotator DeltaRotation(0,0,0);
	DeltaRotation.Pitch = CurrentPitchSpeed * DeltaSeconds;
	DeltaRotation.Yaw = CurrentYawSpeed * DeltaSeconds;
	DeltaRotation.Roll = CurrentRollSpeed * DeltaSeconds;

	// Rotate plane
	AddActorLocalRotation(DeltaRotation);

    // Update our flight firmware
    hackflight.update();

    // Steer the ship from Hackflight controller demands
    //ThrottleInput(4*controller.demands.throttle-2);
    //RollInput(controller.demands.roll);
    //PitchInput(controller.demands.pitch);
    //YawInput(-controller.demands.yaw);

    elapsedTime += DeltaSeconds;

    PlaneMesh->AddForce(FVector(0, 0, 20000*(motors[0]+motors[1]+motors[2]+motors[3])/4));

	// Call any parent class Tick implementation
	Super::Tick(DeltaSeconds);
}

void AGroundCameraFlightPawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, 
        bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    // Deflect along the surface when we collide.
	FRotator CurrentRotation = GetActorRotation();
	SetActorRotation(FQuat::Slerp(CurrentRotation.Quaternion(), HitNormal.ToOrientationQuat(), 0.025f));
}


void AGroundCameraFlightPawn::ThrottleInput(float Val)
{
	// Is there any input?
	bool bHasInput = !FMath::IsNearlyEqual(Val, 0.f);
	// If input is not held down, reduce speed
	float CurrentAcc = bHasInput ? (Val * Acceleration) : (-0.5f * Acceleration);
	// Calculate new speed
	float NewForwardSpeed = CurrentForwardSpeed + (GetWorld()->GetDeltaSeconds() * CurrentAcc);
	// Clamp between MinSpeed and MaxSpeed
	CurrentForwardSpeed = FMath::Clamp(NewForwardSpeed, MinSpeed, MaxSpeed);
}

void AGroundCameraFlightPawn::PitchInput(float Val)
{
	// Target pitch speed is based in input
	float TargetPitchSpeed = (Val * TurnSpeed * -1.f);

	// When steering, we decrease pitch slightly
	TargetPitchSpeed += (FMath::Abs(CurrentYawSpeed) * -0.2f);

	// Smoothly interpolate to target pitch speed
	CurrentPitchSpeed = FMath::FInterpTo(CurrentPitchSpeed, TargetPitchSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void AGroundCameraFlightPawn::YawInput(float Val)
{
	// Target yaw speed is based on input
	float TargetYawSpeed = (Val * TurnSpeed);

	// Smoothly interpolate to target yaw speed
	CurrentYawSpeed = FMath::FInterpTo(CurrentYawSpeed, TargetYawSpeed, GetWorld()->GetDeltaSeconds(), 2.f);
}

void AGroundCameraFlightPawn::RollInput(float Val)
{
	CurrentRollSpeed = 20*Val;

}

void AGroundCameraFlightPawn::init(void)
{
}

bool AGroundCameraFlightPawn::getEulerAngles(float eulerAngles[3]) 
{
    eulerAngles[0] = eulerAngles[1] = eulerAngles[2] = 0;
    return true;
}

bool AGroundCameraFlightPawn::getGyroRates(float gyroRates[3]) 
{
    gyroRates[0] = gyroRates[1] = gyroRates[2] = 0;
    return true;
}

uint32_t AGroundCameraFlightPawn::getMicroseconds() 
{
    return (uint32_t)(elapsedTime*1e6);
}

void AGroundCameraFlightPawn::writeMotor(uint8_t index, float value) 
{
    motors[index] = value;
}


