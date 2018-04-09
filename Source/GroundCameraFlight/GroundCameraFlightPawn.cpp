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

// Debugging

static FColor TEXT_COLOR = FColor::Yellow;
static float  TEXT_SCALE = 2.f;

void hf::Board::outbuf(char * buf)
{
	if (GEngine) {

		// 0 = overwrite; 5.0f = arbitrary time to display
		GEngine->AddOnScreenDebugMessage(0, 5.0f, TEXT_COLOR, FString(buf), true, FVector2D(TEXT_SCALE,TEXT_SCALE));
	}

}

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

	// Start Hackflight firmware
	hackflight.init(this, &controller, &stabilizer);

	// Set handling parameters
	Acceleration = 500.f;
	TurnSpeed = 50.f;
	MaxSpeed = 4000.f;
	MinSpeed = 500.f;
	CurrentForwardSpeed = 0.f;//500.f;

    // Initialize the motor-spin values
    for (uint8_t k=0; k<4; ++k) {
        motorvals[k] = 0;
    }

    // Initialize elapsed time
    elapsedTime = 0;

	// Load our Sound Cue for the propeller sound we created in the editor... 
	// note your path may be different depending
	// on where you store the asset on disk.
	static ConstructorHelpers::FObjectFinder<USoundCue> propellerCue(TEXT("'/Game/Flying/Audio/MotorSoundCue'"));
	
	// Store a reference to the Cue asset - we'll need it later.
	propellerAudioCue = propellerCue.Object;

	// Create an audio component, the audio component wraps the Cue, 
	// and allows us to ineract with it, and its parameters from code.
	propellerAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("PropellerAudioComp"));

	// I don't want the sound playing the moment it's created.
	propellerAudioComponent->bAutoActivate = false;

	// I want the sound to follow the pawn around, so I attach it to the Pawns root.
	propellerAudioComponent->SetupAttachment(GetRootComponent());
}

void AGroundCameraFlightPawn::PostInitializeComponents()
{
	if (propellerAudioCue->IsValidLowLevelFast()) {
		propellerAudioComponent->SetSound(propellerAudioCue);
	}

    // Grab the static prop mesh components by name, storing them for use in Tick()
    TArray<UStaticMeshComponent *> staticComponents;
    this->GetComponents<UStaticMeshComponent>(staticComponents);
    for (int i = 0; i < staticComponents.Num(); i++) {
        if (staticComponents[i]) {
            UStaticMeshComponent* child = staticComponents[i];
            if (child->GetName() == "Prop1") PropMeshes[0] = child;
            if (child->GetName() == "Prop2") PropMeshes[1] = child;
            if (child->GetName() == "Prop3") PropMeshes[2] = child;
            if (child->GetName() == "Prop4") PropMeshes[3] = child;
        }
	}

	Super::PostInitializeComponents();
}

void AGroundCameraFlightPawn::BeginPlay()
{
    // Start playing the sound.  Note that because the Cue Asset is set to loop the sound,
    // once we start playing the sound, it will play continiously...
    propellerAudioComponent->Play();

    Super::BeginPlay();
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

    elapsedTime += DeltaSeconds;

    PlaneMesh->AddForce(FVector(0, 0, 20000*(motorvals[0]+motorvals[1]+motorvals[2]+motorvals[3])/4));

    // Spin props proportionate to motor values, acumulating their sum for sound play
    float motorSum = 0;
    for (uint8_t k=0; k<4; ++k) {
        FRotator PropRotation(0, motorvals[k]*motordirs[k]*60, 0);
        PropMeshes[k]->AddLocalRotation(PropRotation);
        motorSum += motorvals[k];
    }

   // Modulate the pitch and voume of the propeller sound
    propellerAudioComponent->SetFloatParameter(FName("pitch"), motorSum / 4);
    propellerAudioComponent->SetFloatParameter(FName("volume"), motorSum / 4);

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
    motorvals[index] = value;
}


