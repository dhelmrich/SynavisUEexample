// Fill out your copyright notice in the Description page of Project Settings.


#include "LightMeter.h"

#include <numeric>

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"


#include "Components/LightComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ImageUtils.h"


// Sets default values
ALightMeter::ALightMeter()
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;
  // create the measurement surface
  LightMeasuringReference = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LightMeasuringReference"));
  LightMeterTarget = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LightMeterTarget"));
  RootComponent = LightMeterTarget;
  LightMeasuringReference->SetupAttachment(RootComponent);
  LightMeasuringReference->AddLocalOffset(FVector(1.0f, 0.0f, 0.0f));

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
  LightMeterTarget->ClipPlaneBase = NearClipPlane;
}

void ALightMeter::SetExposureBias(double Intensity)
{
  LightMeterTarget->PostProcessSettings.AutoExposureBias
    = static_cast<float>(Intensity);
}

float ALightMeter::GetExposureBias()
{
  return LightMeterTarget->PostProcessSettings.AutoExposureBias;
}

void ALightMeter::SetMeasureSurfaceSize(float SideLength)
{
  // plane is 100 units with a pivot in the middle
  LightMeasuringReference->SetWorldScale3D(FVector(SideLength / 100.0f, SideLength / 100.0f, 1.0f));
  // adapt FOVangle accordingly, 100 side length = 50 degrees FOV
  LightMeterTarget->FOVAngle = FMath::Atan(SideLength / DistanceToSurface) * 180.0f / PI;
}


void ALightMeter::StartMeasurementAtObject(TArray<FVector> Points, float inTimePerMeasurement)
{
  MeasurementPoints = Points;
  LightInfluxes.SetNumZeroed(Points.Num());
  if (MeasurementPoints.Num() > 0)
  {
    // retrieve point
    auto point = MeasurementPoints[0];
    CurrentMeasurementIndex = 0;
    CurrentMeasurementAmount = 0;
    AimAtPoint(point);
    this->TimePerMeasurement = inTimePerMeasurement;
    this->TimeSpentMeasuring = this->TimePerMeasurement;
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
  else if (PropertyChangedEvent.GetPropertyName() == "bUseMeasurementSurface")
  {
    CreateOrDestroyMeasurementSurface(bUseMeasurementSurface);
  }
  else if (PropertyChangedEvent.GetPropertyName() == "NearClipPlane")
  {
    LightMeterTarget->ClipPlaneBase = NearClipPlane;
  }
  else if (PropertyChangedEvent.GetPropertyName() == "DistanceToSurface")
  {
    LightMeasuringReference->SetRelativeLocation(FVector(DistanceToSurface, 0.0f, 0.0f));
    SetMeasureSurfaceSize(MeasureSurfaceSideLength);
  }
}
#endif

// Called when the game starts or when spawned
void ALightMeter::BeginPlay()
{
  Super::BeginPlay();
  LightMeterMaterialInstance = UMaterialInstanceDynamic::Create(LightMeterMaterial, this);
  LightMeasuringReference->SetMaterial(0, LightMeterMaterialInstance);

  // create new render target
  Target = NewObject<UTextureRenderTarget2D>();
  LightMeterTarget->TextureTarget = Target;
  Target->InitAutoFormat(MeterResolution, MeterResolution);
  Target->UpdateResourceImmediate(true);
  // set LightMeasuringReference (camera) to render to the new render target
  Target->TargetGamma = 1.0f;

  LightMeterTarget->PostProcessSettings.AutoExposureBias = 5.f;
}

void ALightMeter::AimAtPoint(const UE::Math::TVector<double>& point)
{
  FHitResult Hitres;
  FCollisionQueryParams CollisionParams;
  CollisionParams.AddIgnoredActor(this);
  GetWorld()->LineTraceSingleByChannel(Hitres, point + FVector(0, 0, SurfaceNormalEstimationLength), point - FVector(0, 0, SurfaceNormalEstimationLength), ECC_Visibility, CollisionParams);
  
  if(PrintProgress)
  {
    // debug draw point
    DrawDebugPoint(GetWorld(), point + FVector(0, 0, SurfaceNormalEstimationLength), 10.0f, FColor::Red, false, 20.0f);
    DrawDebugPoint(GetWorld(), point - FVector(0, 0, SurfaceNormalEstimationLength), 10.0f, FColor::Blue, false, 20.0f);
  }
  // get normal
  FVector normal;
  if (!Hitres.bBlockingHit)
  {
    UE_LOG(LogTemp, Error, TEXT("No hit detected at point %d"), CurrentMeasurementIndex);
    normal = FVector(0, 0, 1);
    NumMisses++;
  }
  else
  {
    normal = -Hitres.ImpactNormal;
  }
  auto rotation = UKismetMathLibrary::FindLookAtRotation(point + normal, point);
  // set our position to point + normal*10.0000009536743164
  constexpr float offset = 10.f + std::numeric_limits<float>::epsilon();
  this->SetActorLocation(point + normal * offset
  );
  // set our rotation to the normal
  this->SetActorRotation(normal.Rotation());
}

// Called every frame
void ALightMeter::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);
  if (CounterMax >= 0) ++Counter;
#ifdef READ_USING_IMAGE
  FImageUtils::GetRenderTargetImage(this->Target, Image);
  if(Image.GetNumPixels() > 0)
  {
    FLinearColor Middle = Image.GetOnePixelLinear(Image.GetWidth() / 2, Image.GetHeight() / 2, 0);
    // calculate light intensity
    LightIntensity = (Middle.R + Middle.G + Middle.B) / (300.0) * Sensitivity;
    if (Counter > CounterMax && PrintIntensity)
    {
      Counter = 0;
      UE_LOG(LogTemp, Warning, TEXT("Light intensity: %f"), LightIntensity);
    }
  }
#else
  // enqueue render command to read pixels from render target
  auto Source = Target->GameThread_GetRenderTargetResource();
  CamData.SetNum(Target->SizeX * Target->SizeY);
  FReadSurfaceDataFlags ReadPixelFlags(ERangeCompressionMode::RCM_MinMax);
  ReadPixelFlags.SetLinearToGamma(true);

  if (Source->ReadPixels(CamData, ReadPixelFlags))
  {
    //FColor TopLeft = CamData[0];
    FColor Middle = CamData[CamData.Num() / 2];
    // calculate light intensity
    LightIntensity = (Middle.R + Middle.G + Middle.B) / (3 * 256) * Sensitivity;
    if (Counter > CounterMax && PrintIntensity)
    {
      Counter = 0;
      UE_LOG(LogTemp, Warning, TEXT("Light intensity: %f"), LightIntensity);
    }
  }
#endif
  if (this->TimeSpentMeasuring <= 0.f && CurrentMeasurementIndex >= 0)
  {
    LightInfluxes[CurrentMeasurementIndex] /= CurrentMeasurementAmount;
    CurrentMeasurementAmount = 0;
    if(PrintProgress)
      UE_LOG(LogTemp, Warning, TEXT("Measured point %d/%d: %f"), CurrentMeasurementIndex, MeasurementPoints.Num(), LightInfluxes[CurrentMeasurementIndex]);
    if (++CurrentMeasurementIndex < MeasurementPoints.Num())
    {
      // retrieve point
      auto point = MeasurementPoints[CurrentMeasurementIndex];
      // check the normal
      AimAtPoint(point);
      this->TimeSpentMeasuring = this->TimePerMeasurement;
    }
    else
    {
      if (OnMeasurementFinished != nullptr)
        OnMeasurementFinished(LightInfluxes);
      this->CurrentMeasurementIndex = -1;
    }
  }
  else if (this->TimeSpentMeasuring > 0.f && MeasurementPoints.Num() > 0)
  {
    this->TimeSpentMeasuring -= DeltaTime;
    CurrentMeasurementAmount++;
    LightInfluxes[CurrentMeasurementIndex] += this->LightIntensity;
  }
}

void ALightMeter::OnConstruction(const FTransform& Transform)
{
  Super::OnConstruction(Transform);
  CreateOrDestroyMeasurementSurface(bUseMeasurementSurface);
}

void ALightMeter::CreateOrDestroyMeasurementSurface(bool bCreate)
{
  LightMeasuringReference->SetVisibility(bCreate);
}

