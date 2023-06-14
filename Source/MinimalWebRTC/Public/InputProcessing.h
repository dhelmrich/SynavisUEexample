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

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MINIMALWEBRTC_API UInputProcessing : public UActorComponent
{
  GENERATED_BODY()

public:
  // Sets default values for this component's properties
  UInputProcessing();

  void ProcessInput(TSharedPtr<FJsonObject> Descriptor);

  UPROPERTY()
    class ASynavisDrone* Drone;
  UPROPERTY()
    class AWorldSpawner* WorldSpawner;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    TObjectPtr<ASpawnTarget> LeafTarget;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    TObjectPtr<ASpawnTarget> RootTarget;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    TObjectPtr<ASpawnTarget> StemTarget;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    class UMaterialInstanceDynamic* StemMaterial;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    class UMaterialInstanceDynamic* LeafMaterial;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Visuals")
    class UMaterialInstanceDynamic* RootMaterial;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Input Processing")
    EDataCollectionType DataCollectionType = EDataCollectionType::None;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Input Processing")
    TArray<AActor*> RandomActors;

  UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Input Processing")
    FName ObjectName;

protected:
  // Called when the game starts
  virtual void BeginPlay() override;



public:
  // Called every frame
  virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

};
