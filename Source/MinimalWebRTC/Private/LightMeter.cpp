// Fill out your copyright notice in the Description page of Project Settings.


#include "LightMeter.h"

#include <numeric>

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"


#include "Components/LightComponent.h"

// Sets default values
ALightMeter::ALightMeter()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;

  LightMeasuringReference = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LightMeasuringReference"));
  RootComponent = LightMeasuringReference;
  LightMeterTarget = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LightMeterTarget"));
  LightMeterTarget->AddLocalOffset(FVector(0.0f, 0.0f, 100.0f));
  LightMeterTarget->SetupAttachment(RootComponent);

  // fetch /Script/Engine.StaticMesh'/Engine/BasicShapes/Plane.Plane'
  static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
  if (PlaneMesh.Succeeded())
  {
    LightMeasuringReference->SetStaticMesh(PlaneMesh.Object);
  }

  // fetch material
  static ConstructorHelpers::FObjectFinder<UMaterial> LightMeterMaterialRefObjectFinder(TEXT("/Script/Engine.Material'/Game/ColorCardMaterial.ColorCardMaterial'"));
  if (LightMeterMaterialRefObjectFinder.Succeeded())
  {
    LightMeterMaterial = LightMeterMaterialRefObjectFinder.Object;
  }

  // set near clip plane of camera to no distance
  LightMeterTarget->ClipPlaneBase = FVector(0.0, 0.0, 0.0);
  PIDController.SetMeasurement(&LightIntensity);
  PIDController.SetIncrementMeasurementLength(false);
}

void ALightMeter::SetMeasureSurfaceSize(float SideLength)
{
  // plane is 100 units with a pivot in the middle
  LightMeasuringReference->SetWorldScale3D(FVector(SideLength / 100.0f, SideLength / 100.0f, 1.0f));
  // adapt FOVangle accordingly, 100 side length = 50 degrees FOV
  LightMeterTarget->FOVAngle = FMath::Atan(SideLength / 100.0f) * 180.0f / PI;
}

void ALightMeter::AddAdjustmentSetting(float* Setting, double TargetAccuracy)
{
  PIDController.AddControlPoint(Setting, Rate);
}

void ALightMeter::AddSettingFromName(USceneComponent* Component, FName Name, double TargetAccuracy)
{
  auto prop = Component->GetClass()->FindPropertyByName(Name);
  auto* val = prop->ContainerPtrToValuePtr<float>(Component);
  if (val == nullptr)
  {
    UE_LOG(LogTemp, Error, TEXT("Could not find property %s in component %s"), *Name.ToString(), *Component->GetName());
    return;
  }
  PIDController.AddControlPoint(val, TargetAccuracy);
}

void ALightMeter::RemoveAdjustmentSetting(float* Setting)
{
  int index = LightAdjustmentSetting.Find(Setting);
  if (index >= 0)
  {
    LightAdjustmentSetting.RemoveAt(index);
    CallibrationTargetAccuracies.RemoveAt(index);
  }
}

void ALightMeter::SetTargetIntensity(double inTargetIntensity)
{
  TargetIntensity = inTargetIntensity;
  PIDController.SetTarget(TargetIntensity);
}

void ALightMeter::Callibrate()
{
  bCallibrating = true;
}

void ALightMeter::StopCallibrate(bool Failure)
{
  bCallibrating = false;
  if (Failure)
  {
    LightAdjustmentSetting.Empty();
    CallibrationTargetAccuracies.Empty();
  }
}

#if WITH_EDITOR
void ALightMeter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
  Super::PostEditChangeProperty(PropertyChangedEvent);
  if (PropertyChangedEvent.GetPropertyName() == "MeasureSurfaceSideLength")
  {
    SetMeasureSurfaceSize(MeasureSurfaceSideLength);
  }
}
#endif

// Called when the game starts or when spawned
void ALightMeter::BeginPlay()
{
  Super::BeginPlay();
  PIDController.SetTarget(TargetIntensity);
  LightMeterMaterialInstance = UMaterialInstanceDynamic::Create(LightMeterMaterial, this);
  LightMeasuringReference->SetMaterial(0, LightMeterMaterialInstance);

  // create new render target
  Target = NewObject<UTextureRenderTarget2D>();
  LightMeterTarget->TextureTarget = Target;
  Target->InitAutoFormat(MeterResolution, MeterResolution);
  Target->UpdateResourceImmediate(true);
  // set LightMeasuringReference (camera) to render to the new render target
  Target->TargetGamma = 1.0f;



  // search scene for all ULightComponents
  TArray<AActor*> FoundActors;
  // get all world actors
  UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
  for (auto* Actor : FoundActors)
  {
    // get all components of actor
    TArray<UActorComponent*> FoundComponents;
    Actor->GetComponents(FoundComponents);
    for (auto* Component : FoundComponents)
    {
      // check if component is a light component
      if (ULightComponent* LightComponent = Cast<ULightComponent>(Component))
      {
        // add light component to array
        Lights.Add(LightComponent);
      }
    }
  }

  PIDController.SetPostUpdateCallback([this]()
    {
      for (auto L : Lights)
      {
        L->UpdateColorAndBrightness();
      }
      skip = 20;
    });
}

// Called every frame
void ALightMeter::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  if (CounterMax >= 0) ++Counter;
  // enqueue render command to read pixels from render target
  auto Source = Target->GameThread_GetRenderTargetResource();
  TArray<FColor> CamData;
  FReadSurfaceDataFlags ReadPixelFlags(ERangeCompressionMode::RCM_MinMax);
  ReadPixelFlags.SetLinearToGamma(true);
  if (Source->ReadPixels(CamData, ReadPixelFlags))
  {
    FColor TopLeft = CamData[0];
    FColor Middle = CamData[CamData.Num() / 2];

    // calculate light intensity
    LightIntensity = (Middle.R + Middle.G + Middle.B) / (3.0f * 255.f);
    if (Counter > CounterMax)
    {
      Counter = 0;
      UE_LOG(LogTemp, Warning, TEXT("Light intensity: %f"), LightIntensity);
    }
  }
  if (bCallibrating && skip == 0)
  {
    PIDController.Update();
    if (PIDController.IsFinished())
    {
      StopCallibrate(false);
    }
  }
  else if (skip > 0)
  {
    skip--;
  }
}

