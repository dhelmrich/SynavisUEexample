// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PID.h"
#include "Containers/Deque.h"
#include "LightMeter.generated.h"

//#define READ_USING_IMAGE


USTRUCT()
struct FLightMeterData
{
   GENERATED_BODY()
};

class ULightComponnt;

/**
 *en au
 */
UCLASS()
class MINIMALWEBRTC_API ALightMeter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ALightMeter();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bUseMeasurementSurface = false;

	UPROPERTY(EditAnywhere)
	FVector NearClipPlane = FVector(0, 0, 0);

	UPROPERTY(EditAnywhere)
	float DistanceToSurface = 1.0f;
	
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
	double LightIntensity;

	UFUNCTION()
	  void SetExposureBias(double Intensity);

	UFUNCTION(BlueprintCallable)
	float GetExposureBias();

	UFUNCTION(BlueprintCallable)
	void SetMeasureSurfaceSize(float SideLength);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int CounterMax = 50;

	// Side length property between 1 and 100
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1.0f, ClampMax = 100.0f))
	float MeasureSurfaceSideLength = 100.0f;

	UFUNCTION(BlueprintCallable, Category = "Target")
	bool IsIdling()
	{
    return MeasurementPoints.Num() == 0;
	}

  UFUNCTION(BlueprintCallable, Category = "Target")
  void ResetMeasurement()
  {
    MeasurementPoints.Empty();
    LightInfluxes.Empty();
    NumMisses = 0;
  }

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Target")
	int TargetID = -1;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Target")
	int Segment = -1;

  UFUNCTION(BlueprintCallable, Category = "Target")
  void StartMeasurementAtObject(TArray<FVector> Points, float inTimePerMeasurement = 1.f);

	// post edit change property for MeasureSurfaceSideLength
#if WITH_EDITOR
   virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;	
#endif

  TFunction<void(const TArray<float>& )> OnMeasurementFinished{};

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Target")
  bool PrintProgress = true;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Target")
	bool PrintIntensity = false;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Target")
	int NumMisses = 0;

  UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Target")
	float SurfaceNormalEstimationLength = 1.0f;

	UPROPERTY()
	double Sensitivity = 1.0;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
  void AimAtPoint(const TArray<UE::Math::TVector<double>>::ElementType& point);

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMaterial* LightMeterMaterial;

	UPROPERTY()
  TArray<FVector> MeasurementPoints;
	UPROPERTY()
	TArray<float> LightInfluxes;

	UPROPERTY()
	float TimePerMeasurement = 0.1f;
	UPROPERTY()
	float TimeSpentMeasuring = 1.f;
	int CurrentMeasurementIndex = -1;
	int CurrentMeasurementAmount = 0;

	UFUNCTION()
	void CreateOrDestroyMeasurementSurface(bool bCreate);

	int Counter = 0;

#ifdef READ_USING_IMAGE
	FImage Image;
#else
  TArray<FColor> CamData;
#endif

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
  virtual void OnConstruction(const FTransform& Transform) override;

private:
	int32 skip = 0;

};
