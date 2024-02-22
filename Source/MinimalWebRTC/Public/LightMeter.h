// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PID.h"
#include "LightMeter.generated.h"

USTRUCT()
struct FLightMeterData
{
   GENERATED_BODY()
};

class ULightComponnt;

UCLASS()
class MINIMALWEBRTC_API ALightMeter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALightMeter();
	
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

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callibration")
  double TargetIntensity;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Callibration")
	double Rate = 0.1;

	void AddAdjustmentSetting(float* Setting, double TargetAccuracy);

	UFUNCTION(BlueprintCallable, Category = "Callibration")
	void AddSettingFromName(USceneComponent* Component, FName Name, double TargetAccuracy);
	void RemoveAdjustmentSetting(float* Setting);

	UFUNCTION(BlueprintCallable, Category = "Callibration")
	void SetTargetIntensity(double inTargetIntensity);

	UFUNCTION(BlueprintCallable, Category = "Callibration")
	void Callibrate();
	UFUNCTION(BlueprintCallable, Category = "Callibration")
	void StopCallibrate(bool Failure = false);

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Target")
	int TargetID = -1;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Target")
	int Segment = -1;

	// post edit change property for MeasureSurfaceSideLength
#if WITH_EDITOR
   virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	PID PIDController;

	// Array of pointers to intensity values to be changed
	TArray<float*> LightAdjustmentSetting;
	// Array of when to stop callibrating, in descending order
	// explanation:
	// 0.9 for the first double pointer will stop adjusting this value
	// once the intensity is within 10% of the target intensity
	// After that, the next double pointer will be adjusted
	// these numbers are always adding up to 1.0
	TArray<double> CallibrationTargetAccuracies;
	bool bCallibrating = false;

	float LastImpact = 0.f;

	UPROPERTY()
	UMaterial* LightMeterMaterial;

	int Counter = 0;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	int32 skip = 0;

	TArray<ULightComponent*> Lights;
};
