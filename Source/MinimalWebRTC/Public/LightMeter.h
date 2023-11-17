// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LightMeter.generated.h"

UCLASS()
class MINIMALWEBRTC_API ALightMeter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALightMeter();

	static TSharedPtr<ALightMeter> MeterPoints(UWorld* World, const TArray<FVector>& Points);
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* LightMeasuringReference;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	USceneCaptureComponent2D* LightMeterTarget;

	// light material instance
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
  UMaterialInstanceDynamic* LightMeterMaterialInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UTextureRenderTarget2D* Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MeterResolution = 16;

	// to be set only in render thread
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LightIntensity;

	UFUNCTION(BlueprintCallable)
	void SetMeasureSurfaceSize(float SideLength);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int CounterMax = 50;

	// Side length property between 1 and 100
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1.0f, ClampMax = 100.0f))
	float MeasureSurfaceSideLength = 100.0f;

	// post edit change property for MeasureSurfaceSideLength
#if WITH_EDITOR
   virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY()
	UMaterial* LightMeterMaterial;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
