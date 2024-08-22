// Fill out your copyright notice in the Description page of Project Settings.


#include "InputProcessing.h"

#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SynavisDrone.h"
#include "WorldSpawner.h"
#include "MaterialShared.h"
#include <SpawnTarget.h>
#include "Shader.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Materials/MaterialRenderProxy.h"
#include "LightMeter.h"
#include "PlantParts.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMeshActor.h"

#include "Components/DirectionalLightComponent.h"


#define COMPACT TCondensedJsonPrintPolicy<TCHAR>

inline void CleanArray(TArray<FVector>& Array)
{
  int lastIndex = Array.Num() - 1;
  while (Array.IsValidIndex(lastIndex) &&
    (Array[lastIndex] == FVector::ZeroVector || Array[lastIndex].ContainsNaN())
    )
  {
    Array.RemoveAt(lastIndex);
    lastIndex--;
  }
}

// Sets default values for this component's properties
AInputProcessing::AInputProcessing()
{
  // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
  // off to improve performance if you don't need them.
  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

  static ConstructorHelpers::FObjectFinder<UMaterial>
    ColorMaterial(TEXT("/Script/Engine.Material'/Game/ColorMaterial.ColorMaterial'"));
  static ConstructorHelpers::FObjectFinder<UMaterial>
    MaizeMaterial(TEXT("/Script/Engine.Material'/Game/MaisBlattTextur_seamless_Mat.MaisBlattTextur_seamless_Mat'"));

  // check if both exist
  if (ColorMaterial.Succeeded() && MaizeMaterial.Succeeded())
  {
    StemBaseMaterial = ColorMaterial.Object;
    LeafBaseMaterial = MaizeMaterial.Object;
  }

  // Find base leaf material master

}

void AInputProcessing::UpdateTime(FString Timecode)
{
  // time in format: YYYY-MM-DDTHH:MM:SS+TZ
  FDateTime Time;
  FDateTime::Parse(Timecode, Time);
  auto month = Time.GetMonth();
  auto day = Time.GetDay();
  auto solartime = Time.GetHour() + Time.GetMinute() / 60.0f + Time.GetSecond() / 3600.0f;
  auto* month_prop = SunSky->GetClass()->FindPropertyByName(TEXT("Month"));
  auto* day_prop = SunSky->GetClass()->FindPropertyByName(TEXT("Day"));
  auto* solartime_prop = SunSky->GetClass()->FindPropertyByName(TEXT("SolarTime"));
  CastField<FIntProperty>(month_prop)->SetPropertyValue_InContainer(SunSky, month);
  CastField<FIntProperty>(day_prop)->SetPropertyValue_InContainer(SunSky, day);
  CastField<FDoubleProperty>(solartime_prop)->SetPropertyValue_InContainer(SunSky, solartime);
}

TArray<float> AInputProcessing::MeasureLightInfluxOfMesh(AActor* Actor)
{
  // retrieve mesh component, if any
  auto MeshComponent = Actor->FindComponentByClass<UMeshComponent>();
  if (!MeshComponent)
  {
    UE_LOG(LogTemp, Error, TEXT("This ACtor does not appear to contain a mesh component."));
  }
  TArray<URuntimeVirtualTexture*> textures = MeshComponent->GetRuntimeVirtualTextures();
  for (URuntimeVirtualTexture* texture : textures)
  {
    // Log all the texture names
    UE_LOG(LogTemp, Warning, TEXT("Texture name: %s"), *(texture->GetName()));
  }
  // dispatch job to render thread
  ENQUEUE_RENDER_COMMAND(MeasureLightInfluxOfMesh)([this, MeshComponent](FRHICommandListImmediate& RHICmdList)
    {
      UMaterialInterface* material = MeshComponent->GetMaterial(0);
      FMaterialRenderProxy* Proxy = material->GetRenderProxy();
      auto* Material = Proxy->GetMaterialNoFallback(ERHIFeatureLevel::SM6);
      if (!Material)
      {
        Material = Proxy->GetMaterialNoFallback(ERHIFeatureLevel::SM5);
      }
      
      if (!Material)
      {
        UE_LOG(LogTemp, Error, TEXT("This Actor does not appear to contain a material."));
      }
      else
      {
        // get render information

      }
    });
  return TArray<float>();
}

// Called when the game starts
void AInputProcessing::BeginPlay()
{
  Super::BeginPlay();
  // Search for an ASynavisDrone in the world
  for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
  {
    if (ActorItr->IsA(ASynavisDrone::StaticClass()))
    {
      Drone = Cast<ASynavisDrone>(*ActorItr);
    }
    else if (ActorItr->IsA(AWorldSpawner::StaticClass()))
    {
      WorldSpawner = Cast<AWorldSpawner>(*ActorItr);
    }
    else if (ActorItr->IsA(LightMeterClass))
    {
      LightMeters.Add(Cast<ALightMeter>(*ActorItr));
    }
    else if (ActorItr->GetName().Contains(TEXT("SunSky")))
    {
      SunSky = *ActorItr;
    }
  }

  // set zero position to first hit below
  FHitResult Hit;
  FVector Start = this->GetRootComponent()->GetComponentLocation() + FVector(0.0f, 0.0f, 1000.0f);
  FVector End = this->GetRootComponent()->GetComponentLocation() - FVector(0.0f, 0.0f, 1000.0f);
  FCollisionQueryParams CollisionParams;
  CollisionParams.AddIgnoredActor(this);
  GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, CollisionParams);
  ZeroPosition = Hit.ImpactPoint;

  Drone->ApplicationProcessInput = std::bind(&AInputProcessing::ProcessInput, this, std::placeholders::_1);
}

void AInputProcessing::CheckCompletion(TArray<float> LightInfluxes, int Start, int End, ALightMeter* Meter, int LocalID)
{
  for (int i = Start; i < End; ++i)
    this->LightFluxesAggregate[i] = LightInfluxes[i - Start];
  Meter->ResetMeasurement();
  UE_LOG(LogActor, Warning, TEXT("Light meter %s finished measuring %d points"), *Meter->GetName(), End - Start);
  // count down the number of light meters that are busy
  LightMetersBusy.DecrementExchange();
  if (LightMetersBusy.Load() == 0)
  {
    UE_LOG(LogActor, Warning, TEXT("All light meters finished measuring"));
    // send response
    auto response = FString::Printf(TEXT("{\"type\":\"mm\",\"l\":%d,\"i\":\""), LocalID);
    response += FBase64::Encode(
      reinterpret_cast<const uint8*>(LightFluxesAggregate.GetData()),
      LightFluxesAggregate.Num() * sizeof(float)
    );
    response += TEXT("\"}");
    Drone->SendResponse(response);
    LightFluxesAggregate.Empty();
  }
  if (Meter->NumMisses > 0)
  {
    Drone->SendResponse(FString::Printf(TEXT("{\"type\":\"error\", message:\"Light meter %s missed %d measurements\"}"), *Meter->GetName(), Meter->NumMisses));
  }
}

// Called every frame
void AInputProcessing::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

}

void AInputProcessing::ProcessInput(TSharedPtr<FJsonObject> Descriptor)
{
  auto Type = Descriptor->GetStringField(TEXT("type"));
  if (Type == "plant")
  {
    Drone->ParseGeometryFromJson(Descriptor);
    // find the spawntarget in the scene
    if (Descriptor->HasField(TEXT("part")))
    {
      auto plant = Cast<APlantParts>(Drone->GetObjectFromJSON(Descriptor));
      auto part = GetIntFieldOr(Descriptor, TEXT("part"), 3);
      plant->AddMesh(Drone->Points, Drone->Normals, Drone->Triangles, Drone->UVs, {}, {}, part);
    }
  }
  else if (Type == "do")
  {
    auto Points = GetArrayField<FVector>(Descriptor, "p");
    auto Normals = GetArrayField<FVector>(Descriptor, "n");
    auto Indices = GetArrayField<int32>(Descriptor, "i");
    auto UV = GetArrayField<FVector2D>(Descriptor, "t");
    auto local_index = GetIntFieldOr(Descriptor, "l", 0);
    auto slot = GetIntFieldOr(Descriptor, "s", -1);
    auto type = GetIntFieldOr(Descriptor, "o", 1);
    CleanArray(Points);
    CleanArray(Normals);

    // fetch plant from local index
    auto plant = (FieldActors.IsValidIndex(local_index)) ? FieldActors[local_index] : nullptr;
    if (!plant)
    {
      Drone->SendResponse(TEXT("{\"type\":\"error\",\"message\":\"plant not found\"}"));
      return;
    }
    auto ind = plant->AddMesh(Points, Normals, Indices, UV, {}, {}, type, slot);
    UE_LOG(LogTemp, Warning, TEXT("Added mesh at index %d containing %d vertices and %d triangles"), ind, Points.Num(), Indices.Num() / 3);
    auto material_key = FString::Printf(TEXT("%d/%d"), local_index, type);
    auto inst = WorldSpawner->GenerateInstanceFromName(material_key, false);
    if (!inst)
    {
      UE_LOG(LogTemp, Error, TEXT("Material instance could not be created!"));
    }
    else
    {
      UE_LOG(LogTemp, Warning, TEXT("Material instance named %s created"), *inst->GetName());
    }
    plant->Mesh->SetMaterial(ind, inst);
  }
  else if (Type == TEXT("reset"))
  {
    auto local_index = GetIntFieldOr(Descriptor, TEXT("l"), 0);
    auto plant = (FieldActors.IsValidIndex(local_index)) ? FieldActors[local_index] : nullptr;
    if (!plant)
    {
      Drone->SendResponse(TEXT("{\"type\":\"error\",\"message\":\"plant not found\"}"));
      return;
    }
    else
    {
      plant->Mesh->ClearAllMeshSections();
    }
  }
  else if (Type == TEXT("t"))
  {
    auto Timecode = Descriptor->GetStringField(TEXT("s"));
    UpdateTime(Timecode);
    if (Descriptor->HasField(TEXT("rad")))
    {
      auto* Sun = this->SunSky->FindComponentByClass<UDirectionalLightComponent>();
      auto* IntensityProp = Sun->GetClass()->FindPropertyByName(TEXT("Intensity"));
      double Intensity = Descriptor->GetNumberField(TEXT("rad"));
      CastField<FFloatProperty>(IntensityProp)->SetPropertyValue_InContainer(Sun, (float)Intensity);
      Sun->PropagateLightingScenarioChange();
      Sun->InvalidateLightingCacheDetailed(true, false);
      Sun->UpdateLightGUIDs();
    }
  }
  else if (Type == "mm")
  {
    auto local_id = Descriptor->GetNumberField(TEXT("l"));
    auto* PlantPart = this->FieldActors[local_id];
    auto Points = GetArrayField<FVector>(Descriptor, "p");
    for (auto& point : Points)
    {
      point += PlantPart->GetActorLocation();
    }
    auto* Meter = *LightMeters.FindByPredicate([](ALightMeter* Meter) { return Meter->IsIdling(); });
    if (!Meter)
    {
      // schedule a task in game thread to retry
      FTimerHandle TimerHandle;
      GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Descriptor]() { ProcessInput(Descriptor); }, MeteringRetryTime, false);
    }
    else
    {
      auto Duration = GetDoubleFieldOr(Descriptor, "d", 0.3);
      Meter->StartMeasurementAtObject(Points, Duration);
      Meter->OnMeasurementFinished = [this, local_id, Meter](const TArray<float>& Intensities)
        {
          auto response = FString::Printf(TEXT("{\"type\":\"mm\",\"l\":%d,\"i\":\""), local_id);
          response += FBase64::Encode(
            reinterpret_cast<const uint8*>(Intensities.GetData()),
            Intensities.Num() * sizeof(float)
          );
          response += TEXT("\"}");
          Drone->SendResponse(response);
          Meter->ResetMeasurement();
        };
    }
  }
  else if (Type == "mms")
  {
    auto Points = GetArrayField<FVector>(Descriptor, "p");
    int local_id = Descriptor->GetNumberField(TEXT("l"));
    auto* PlantPart = this->FieldActors[local_id];
    for (auto& point : Points)
    {
      // rotate point by plant part rotation
      point = PlantPart->GetActorRotation().RotateVector(point);
      point += PlantPart->GetActorLocation();
    }
    // find all idle light meters
    auto Meters = LightMeters.FilterByPredicate([](ALightMeter* Meter) { return Meter->IsIdling(); });
    if (Meters.Num() == 0 || LightFluxesAggregate.Num() > 0)
    {
      // schedule a task in game thread to retry
      FTimerHandle TimerHandle;
      GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this, Descriptor]() { ProcessInput(Descriptor); }, MeteringRetryTime, false);
    }
    else
    {
      // make sure that we do not allocate more meters than we have points
      Meters.SetNum(FMath::Min(Meters.Num(), Points.Num()));
      this->LightFluxesAggregate.SetNumZeroed(Points.Num());
      UE_LOG(LogActor, Warning, TEXT("I am dispatching %d meters to measure %d points for ID %d"), Meters.Num(), Points.Num(), local_id);
      auto Duration = GetDoubleFieldOr(Descriptor, "d", 0.3);
      LightMetersBusy.Store(Meters.Num());
      // distribute points to meters
      for (int i = 0; i < Meters.Num(); ++i)
      {
        auto Meter = Meters[i];
        auto Start = i * Points.Num() / Meters.Num();
        auto End = FMath::Min((i + 1) * Points.Num() / Meters.Num(), Points.Num());
        Meter->OnMeasurementFinished = std::bind(&AInputProcessing::CheckCompletion, this, std::placeholders::_1, Start, End, Meter, local_id);
        //Meter->OnMeasurementFinished = [this, Start, End, Meter, local_id](const TArray<float>& Intensities)
        //  {
        //    this->CheckCompletion(Intensities, Start, End, Meter, local_id);
        //  };
        auto SubPoints = TArray<FVector>(Points.GetData() + Start, End - Start);
        Meter->StartMeasurementAtObject(SubPoints, Duration);
      }
    }
  }
  else if (Type == "lightmeter")
  {
    auto object = Descriptor->GetObjectField(TEXT("object"));
    auto sceneobject = Drone->GetObjectFromJSON(Descriptor);
    auto LightMeter = Cast<ALightMeter>(sceneobject);
    if (!LightMeter)
    {
      Drone->SendResponse(TEXT("{\"type\":\"error\",\"message\":\"object is not a lightmeter\"}"));
      return;
    }
    else if (Descriptor->HasField(TEXT("sensitivity")))
    {
      float sensitivity = Descriptor->GetNumberField(TEXT("sensitivity"));
      LightMeter->SetExposureBias(sensitivity);
    }
    auto response = FString::Printf(
      TEXT("{\"type\":\"lightmeter\",\"name\":\"%s\", position: {\"x\":%f,\"y\":%f,\"z\":%f}}, intensity: %f"),
      *LightMeter->GetName(),
      LightMeter->GetActorLocation().X,
      LightMeter->GetActorLocation().Y,
      LightMeter->GetActorLocation().Z,
      LightMeter->LightIntensity
    );
    Drone->SendResponse(response);
  }
  else if (Type == "lightmeters")
  {
    auto result = MakeShared<FJsonObject>();
    TArray<AActor*> FoundActors;
    // find all light meters in scene
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALightMeter::StaticClass(), FoundActors);
    for (auto* Actor : FoundActors)
    {
      auto LightMeter = Cast<ALightMeter>(Actor);
      // add target, segment, and current LightIntensity to array
      auto value = MakeShared<FJsonObject>();
      value->SetNumberField("t", LightMeter->TargetID);
      value->SetNumberField("s", LightMeter->Segment);
      value->SetNumberField("i", LightMeter->LightIntensity);
      auto name = LightMeter->GetName();
      result->SetObjectField(name, value);
    }
    result->SetStringField("type", "meter");
    // make FString from JSON
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, COMPACT>> Writer = TJsonWriterFactory<TCHAR, COMPACT>::Create(&OutputString);
    FJsonSerializer::Serialize(result, Writer);
    // send to drone
    Drone->SendResponse(OutputString);
  }
  else if (Type == "spawnmeter")
  {
    FString Response = TEXT("{\"type\":\"spawnmeter\",\"name\":\"[");
    int number = GetIntFieldOr(Descriptor, TEXT("number"), 1);
    UE_LOG(LogActor, Warning, TEXT("Spawning %d light meters"), number);
    bool CallibrateOnSpawn = GetBoolFieldOr(Descriptor, TEXT("calibrate"), false);
    bool Fillup = GetBoolFieldOr(Descriptor, TEXT("fillup"), false);
    if(Fillup)
    {
      TArray<AActor*> FoundActors;
      UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALightMeter::StaticClass(), FoundActors);
      // count the number of light meters in scene
      number = FMath::Max(0, number - FoundActors.Num());
    }
    for (int i = 0; i < number; i++)
    {
      // spawn object of class LightMeter
      auto LightMeter = GetWorld()->SpawnActor<ALightMeter>(LightMeterClass);
      // apply possible properties to it
      Drone->ApplyJSONToObject(LightMeter, Descriptor, false);
      const FString name = LightMeter->GetName();
      Response += name;
      if (i != number - 1)
      {
        Response += TEXT(",");
      }
      this->LightMeters.Add(LightMeter);
    }
    if (CallibrateOnSpawn)
    {
      this->InitializeCalibration();
    }
    Response += TEXT("]\"}");
    Drone->SendResponse(Response);
  }
  else if (Type == "meter")
  {
    // check for available light meters
    auto result = MakeShared<FJsonObject>();
    TArray<AActor*> FoundActors;
    // find all light meters in scene
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALightMeter::StaticClass(), FoundActors);
    FString response = TEXT("{\"type\":\"meter\",\"meters\":[");
    for (auto* Actor : FoundActors)
    {
      auto LightMeter = Cast<ALightMeter>(Actor);
      response += FString::Printf(TEXT("%s"), *LightMeter->GetName());
      if (Actor != FoundActors.Last())
      {
        response += TEXT(",");
      }
    }
    response += TEXT("]}");
    Drone->SendResponse(response);
  }
  else if (Type == "calibrate")
  {
    double flux_value = Descriptor->GetNumberField(TEXT("flux"));
    // we assume that this meter is aimed at the sun in some way
    const auto Intensity = ReferenceMeter->LightIntensity / ReferenceMeter->Sensitivity;
    auto* Sun = this->SunSky->FindComponentByClass<UDirectionalLightComponent>();
    const auto NewMultiplier = flux_value / Intensity;
    for (auto* Meter : LightMeters)
    {
      Meter->Sensitivity = NewMultiplier;
    }
  }
  else if (Type == "placeplant")
  {
    // place plants according to spawn rule
    auto spawn_number = GetIntFieldOr(Descriptor, TEXT("number"), 1);
    auto spawn_rule = GetStringFieldOr(Descriptor, TEXT("rule"), TEXT("square"));
    auto mpi_world_size = GetIntFieldOr(Descriptor, TEXT("mpi_world_size"), 1);
    auto mpi_rank = GetIntFieldOr(Descriptor, TEXT("mpi_rank"), 0);
    auto spacing = GetDoubleFieldOr(Descriptor, TEXT("spacing"), 1.0f);

    auto local_count = spawn_number / mpi_world_size;
    auto side_length = (int32)FMath::Floor(FMath::Sqrt((float)spawn_number));
    int rank_per_side = FMath::Sqrt((float)mpi_world_size);
    int local_side_length = side_length / rank_per_side;


    // partition to indices
    auto start_i = mpi_rank % rank_per_side;
    auto start_j = mpi_rank / rank_per_side;
    // index to coordinate
    for (auto k = 0; k < local_count; k++)
    {
      auto i = start_i + k % local_side_length;
      auto j = start_j + k / local_side_length;
      auto x = i * spacing;
      auto y = j * spacing;
      auto z = 0.0f;
      // random rotation
      auto r = FMath::RandRange(0.0f, 360.0f);
      auto plant = GetWorld()->SpawnActor<APlantParts>(PlantPartsClass, FVector(x, y, z) + this->ZeroPosition,
        //FRotator(0.0f, r, 0.0f)
        FRotator::ZeroRotator
      );
      this->FieldActors.Add(plant);
    }

    // send response
    Drone->SendResponse(TEXT("{\"type\":\"placeplant\",\"status\":\"ok\"}"));
  }
  else if (Type == "resetlights")
  {
    for (auto* p : FieldActors)
    {
      p->Mesh->ClearAllMeshSections();
    }
    for (auto* l : LightMeters)
    {
      l->ResetMeasurement();
    }
  }
  else if (Type == "delete")
  {
    FString kind = Descriptor->GetStringField(TEXT("kind"));
    if (kind == TEXT("plant"))
    {
      // delete all plantparts in scene
      for (auto* p : FieldActors)
      {
        p->Destroy();
      }
      FieldActors.Empty();
    }
    else if (kind == TEXT("lightmeter"))
    {
      // delete all lightmeters in scene
      for (auto* l : LightMeters)
      {
        l->Destroy();
      }
      LightMeters.Empty();
    }
    else
    {
      // delete both
      for (auto* p : FieldActors)
      {
        p->Destroy();
      }
      FieldActors.Empty();
      for (auto* l : LightMeters)
      {
        l->Destroy();
      }
      LightMeters.Empty();
    }
  }
}

void AInputProcessing::InitializeCalibration()
{
  if (ReferenceMeter)
  {
    // we assume that this meter is aimed at the sun in some way
    const auto Intensity = ReferenceMeter->LightIntensity / ReferenceMeter->Sensitivity;
    auto* Sun = this->SunSky->FindComponentByClass<UDirectionalLightComponent>();
    auto* IntensityProp = Sun->GetClass()->FindPropertyByName(TEXT("Intensity"));
    const auto* IntensityValue = CastField<FFloatProperty>(IntensityProp)->ContainerPtrToValuePtr<float>(Sun);
    const auto IntensityWatts = *IntensityValue / 0.0079;
    const auto NewMultiplier = IntensityWatts / Intensity;
    for (auto* Meter : LightMeters)
    {
      Meter->Sensitivity = NewMultiplier;
    }
  }
}

