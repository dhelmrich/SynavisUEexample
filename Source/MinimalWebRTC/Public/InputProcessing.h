// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

    TArray<class ALightMeter*> LightMeters;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Processing")
    FName ObjectName;

    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Input Processing")
    TMap<FString,TObjectPtr<UMaterialInstanceDynamic>> ActorMap;

    UFUNCTION(BlueprintCallable, Category = "Input Processing")
    TArray<float> MeasureLightInfluxOfMesh(AActor* Actor);

protected:
  // Called when the game starts
  virtual void BeginPlay() override;
public:
  // Called every frame
  virtual void Tick(float DeltaTime) override;

};
