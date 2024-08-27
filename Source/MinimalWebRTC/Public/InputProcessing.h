// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LightMeter.h"
#include "PlantParts.h"
#include "InputProcessing.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPixelStreamingResponseCallbackMinimal, FString, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraDataSwitchCallback, int, Setting);

UENUM(BlueprintType)
enum class EDataCollectionType : uint8
{
  None,
  Count,
  Size,
  Mean,
  Position
};

// forward declaration
class ASpawnTarget;
class APlantParts;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnable))
class MINIMALWEBRTC_API AInputProcessing : public AActor
{
  GENERATED_BODY()

public:
  // Sets default values for this component's properties
  AInputProcessing();

  void ProcessInput(TSharedPtr<FJsonObject> Descriptor);


  UFUNCTION(BlueprintCallable)
  void InitializeCalibration();

  UPROPERTY()
  class ASynavisDrone* Drone;
  UPROPERTY()
  class AWorldSpawner* WorldSpawner;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Visuals")
  class UMaterialInstanceDynamic* CallibrationMaterialInstance;

  UPROPERTY()
  FVector ZeroPosition;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  float MeteringRetryTime = 0.5f;

  UPROPERTY()
  class UMaterial* StemBaseMaterial;
  UPROPERTY()
  class UMaterial* LeafBaseMaterial;

  UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
  class AStaticMeshActor* CallibrationTest;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  EDataCollectionType DataCollectionType = EDataCollectionType::None;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  TObjectPtr<AActor> SunSky;

  UFUNCTION(BlueprintCallable, Category = "Input Processing")
  void UpdateTime(FString Timecode);

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  TArray<APlantParts*> FieldActors;

  UPROPERTY()
  TArray<ALightMeter*> LightMeters;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  APlantParts* BufferGeometry { nullptr};

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  FName ObjectName;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  TSubclassOf<APlantParts> PlantPartsClass = APlantParts::StaticClass();

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  TSubclassOf<ALightMeter> LightMeterClass = ALightMeter::StaticClass();

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Input Processing")
  TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> ActorMap;

  UFUNCTION(BlueprintCallable, Category = "Input Processing")
  TArray<float> MeasureLightInfluxOfMesh(AActor* Actor);

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
  TObjectPtr<ALightMeter> ReferenceMeter = nullptr;

protected:
  // Called when the game starts
  virtual void BeginPlay() override;

  void CheckCompletion(TArray<float> LightInfluxes, int Start, int End, ALightMeter* Meter, int LocalID);
  TArray<float> LightFluxesAggregate;
  TAtomic<int32> LightMetersBusy;

  double RefSolarTime{0.0};

public:
  // Called every frame
  virtual void Tick(float DeltaTime) override;

};
