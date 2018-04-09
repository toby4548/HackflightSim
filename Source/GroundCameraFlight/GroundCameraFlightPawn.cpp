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
    // Update our flight firmware
    hackflight.update();

    // Accumulate elapsed time
    elapsedTime += DeltaSeconds;

    // Compute body-frame roll, pitch, yaw velocities based on differences between motors
    float forces[3];
    forces[0] = motorsToAngularForce(2, 3, 0, 1);
    forces[1] = motorsToAngularForce(1, 3, 0, 2); 
    forces[2] = motorsToAngularForce(1, 2, 0, 3); 

    // Rotate vehicle
    AddActorLocalRotation(DeltaSeconds * FRotator(forces[1], forces[2], forces[0]) * (180 / M_PI));

    // Spin props proportionate to motor values, acumulating their sum 
    float motorSum = 0;
    for (uint8_t k=0; k<4; ++k) {
        FRotator PropRotation(0, motorvals[k]*motordirs[k]*60, 0);
        PropMeshes[k]->AddLocalRotation(PropRotation);
        motorSum += motorvals[k];
    }

    // Get current quaternion
    FQuat q = this->GetActorQuat();

    // Turn quaternion into Euler angles
    // https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles

    // roll (x-axis rotation)
    float sinr = +2.0 * (q.W * q.X + q.Y * q.Z);
    float cosr = +1.0 - 2.0 * (q.X * q.X + q.Y * q.Y);
    float roll = atan2(sinr, cosr);

    // pitch (y-axis rotation)
    float sinp = +2.0 * (q.W * q.Y - q.Z * q.X);
    float pitch = (fabs(sinp) >= 1) ?  copysign(M_PI / 2, sinp) /* use 90 degrees if out of range */ : asin(sinp);

    // yaw (z-axis rotation)
    float siny = +2.0 * (q.W * q.Z + q.X * q.Y);
    float cosy = +1.0 - 2.0 * (q.Y * q.Y + q.Z * q.Z);
    float yaw = atan2(siny, cosy);

    float x = -cos(yaw)*sin(pitch)*sin(roll)-sin(yaw)*cos(roll);
    float y = -sin(yaw)*sin(pitch)*sin(roll)+cos(yaw)*cos(roll);
    float z =  cos(pitch)*sin(roll);

    Debug::printf("%f %f %f", x, y, z);

    PlaneMesh->AddForce(5000*FVector(motorSum/10, 0, motorSum));

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

float AGroundCameraFlightPawn::motorsToAngularForce(int a, int b, int c, int d)
{
    float v = ((motorvals[a] + motorvals[b]) - (motorvals[c] + motorvals[d]));

    return (v<0 ? -1 : +1) * pow(fabs(v), 3);
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

/*
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
*/

