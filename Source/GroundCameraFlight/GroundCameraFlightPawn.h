// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

// Math support
#define _USE_MATH_DEFINES
#include <math.h>

#include <hackflight.hpp>
using namespace hf;

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Runtime/Engine/Classes/Sound/SoundCue.h"
#include "Components/AudioComponent.h"
#include "GroundCameraFlightPawn.generated.h"

UCLASS(Config=Game)
class AGroundCameraFlightPawn : public APawn, public Board
{
	GENERATED_BODY()

	// StaticMesh component that will be the visuals for our flying pawn 
	UPROPERTY(Category = Mesh, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UStaticMeshComponent* PlaneMesh;

    // Propeller meshes for spinning
	class UStaticMeshComponent* PropMeshes[4];

    // Audio support: see http://bendemott.blogspot.com/2016/10/unreal-4-playing-sound-from-c-with.html

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
        class USoundCue* propellerAudioCue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
        class USoundCue* propellerStartupCue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio", meta = (AllowPrivateAccess = "true"))
        class UAudioComponent* propellerAudioComponent;


public:
	AGroundCameraFlightPawn();

	// Begin AActor overrides
	virtual void BeginPlay() override;
    void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, 
            bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	// End AActor overrides

    virtual void     init(void) override;
    virtual bool     getEulerAngles(float eulerAngles[3]) override;
    virtual bool     getGyroRates(float gyroRates[3]) override;
    virtual uint32_t getMicroseconds() override;
    virtual void     writeMotor(uint8_t index, float value) override;


protected:

	void ThrottleInput(float Val);
	void PitchInput(float Val);
	void RollInput(float Val);
	void YawInput(float Val);

private:

	/** How quickly forward speed changes */
	UPROPERTY(Category=Plane, EditAnywhere)
	float Acceleration;

	/** How quickly pawn can steer */
	UPROPERTY(Category=Plane, EditAnywhere)
	float TurnSpeed;

	/** Max forward speed */
	UPROPERTY(Category = Pitch, EditAnywhere)
	float MaxSpeed;

	/** Min forward speed */
	UPROPERTY(Category=Yaw, EditAnywhere)
	float MinSpeed;

	/** Current forward speed */
	float CurrentForwardSpeed;

	/** Current yaw speed */
	float CurrentYawSpeed;

	/** Current pitch speed */
	float CurrentPitchSpeed;

	/** Current roll speed */
	float CurrentRollSpeed;

    // Support for spinning propellers
    const int8_t motordirs[4] = {+1, -1, -1, +1};
    float motorvals[4];

    // Support for Hackflight::Board::getMicroseconds()
    float elapsedTime;

    // Converts a set of motor values to angular forces in body frame
    float motorsToAngularForce(int a, int b, int c, int d);
        
public:
	/** Returns PlaneMesh subobject **/
	FORCEINLINE class UStaticMeshComponent* GetPlaneMesh() const { return PlaneMesh; }
};
