// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "GameFramework/HUD.h"
#include "Engine/TextureRenderTarget2D.h"

#include "VisionHUD.generated.h"

/**
 * 
 */
UCLASS()
class GROUNDCAMERAFLIGHT_API AVisionHUD : public AHUD
{
	GENERATED_BODY()
	
	AVisionHUD();

	virtual void DrawHUD() override;

	const float LEFTX  = 45.f;
	const float TOPY   = 90.f;
	const float WIDTH  = 256.f;
	const float HEIGHT = 128.f;
	
	const FLinearColor BORDER_COLOR = FLinearColor::Yellow;
	const float BORDER_WIDTH = 2.0f;

	void drawBorder(float lx, float uy, float rx, float by);

	// Access to Vision camera
	UTextureRenderTarget2D* VisionTextureRenderTarget;
	FRenderTarget* VisionRenderTarget;
	TArray<FColor> VisionSurfData;

	// Support for vision algorithms
	int rows;
	int cols;
	uint8_t* imagergb;
};
