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


#define COMPACT TCondensedJsonPrintPolicy<TCHAR>

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
    if (Drone && WorldSpawner)
    {
      break;
    }
  }
  StemMaterial = UMaterialInstanceDynamic::Create(StemBaseMaterial, this, "StemMaterial");
  StemMaterial->SetVectorParameterValue("color", FLinearColor::Green);
  LeafMaterial = UMaterialInstanceDynamic::Create(LeafBaseMaterial, this, "LeafMaterial");
  RootMaterial = UMaterialInstanceDynamic::Create(StemBaseMaterial, this, "RootMaterial");
  RootMaterial->SetVectorParameterValue("color", FLinearColor::White);
  Drone->ApplicationProcessInput = std::bind(&AInputProcessing::ProcessInput, this, std::placeholders::_1);
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
      auto Part = Descriptor->GetStringField(TEXT("part"));
      decltype(LeafTarget) SpawnTarget;
      if (Part == "leaf")
      {
        SpawnTarget = LeafTarget;
      }
      else if (Part == "stem")
      {
        SpawnTarget = StemTarget;
      }
      else if (Part == "root")
      {
        SpawnTarget = RootTarget;
      }
      else
      {
        return;
      }
      // clear the target's mesh sections
      SpawnTarget->ProcMesh->ClearAllMeshSections();
      // get the mesh data from the drone
      SpawnTarget->ProcMesh->CreateMeshSection_LinearColor(0, Drone->Points, Drone->Triangles, Drone->Normals, Drone->UVs, {}, Drone->Tangents, false);
    }
  }
  else if (Type == "do")
  {
    auto Points = GetArrayField<FVector>(Descriptor, "p");
    auto Normals = GetArrayField<FVector>(Descriptor, "n");
    auto Indices = GetArrayField<int32>(Descriptor, "i");
    auto UV = GetArrayField<FVector2D>(Descriptor, "t");
    auto local_index = GetIntFieldOr(Descriptor, "l", 0);
    auto type = GetIntFieldOr(Descriptor, "o", 1);

    // fetch plant from local index
    auto plant = (FieldActors.IsValidIndex(local_index)) ? FieldActors[local_index] : nullptr;
    if (!plant)
    {
      Drone->SendResponse(TEXT("{\"type\":\"error\",\"message\":\"plant not found\"}"));
      return;
    }
    auto ind = plant->AddMesh(Points, Normals, Indices, UV, {}, {}, type);
    auto material_key = FString::Printf(TEXT("%n/%n"), local_index, type);
    auto inst = WorldSpawner->GenerateInstanceFromName(material_key, false);
    plant->Mesh->SetMaterial(ind, inst);
    
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
      LightMeter->SetLightIntensity(sensitivity);
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
    // spawn object of class LightMeter
    auto LightMeter = GetWorld()->SpawnActor<ALightMeter>(ALightMeter::StaticClass());
    // apply possible properties to it
    Drone->ApplyJSONToObject(LightMeter, Descriptor.Get());
    const FString name = LightMeter->GetName();
    Drone->SendResponse(FString::Printf(TEXT("{\"type\":\"spawnmeter\",\"name\":\"%s\"}"), *name));
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
  else if (Type == "placeplant")
  {
    // place plants according to spawn rule
    auto spawn_number = GetIntFieldOr(Descriptor, TEXT("number"), 1);
    auto spawn_rule = GetStringFieldOr(Descriptor, TEXT("rule"), TEXT("square"));
    auto mpi_world_size = GetIntFieldOr(Descriptor, TEXT("mpi_world_size"), 1);
    auto mpi_rank = GetIntFieldOr(Descriptor, TEXT("mpi_rank"), 0);
    auto density = GetDoubleFieldOr(Descriptor, TEXT("density"), 1.0f);

    auto local_count = spawn_number / mpi_world_size;
    auto side_length = (int32)FMath::Floor(FMath::Sqrt((float)spawn_number));

    // find out which partition belongs to us in terms of the MPI world size
    auto start_p = mpi_world_size % (int32)FMath::Sqrt((float)mpi_world_size);
    auto start_q = FMath::Floor(FMath::Sqrt((float)mpi_world_size));
    // partition to indices
    auto start_i = start_p * side_length / FMath::Sqrt((float)mpi_world_size);
    auto start_j = start_q * side_length / FMath::Sqrt((float)mpi_world_size);
    // index to coordinate
    for (auto k = 0; k < local_count; k++)
    {
      auto i = start_i + k % side_length;
      auto j = start_j + k / side_length;
      auto x = i * density;
      auto y = j * density;
      auto z = 0.0f;
      // random rotation
      auto r = FMath::RandRange(0.0f, 360.0f);
      auto plant = GetWorld()->SpawnActor<APlantParts>(APlantParts::StaticClass(), FVector(x, y, z), FRotator(0.0f, r, 0.0f));
    }

    // send response
    Drone->SendResponse(TEXT("{\"type\":\"placeplant\",\"status\":\"ok\"}"));
  }
}

void AInputProcessing::InitializeCalibration()
{
  CallibrationMaterialInstance = WorldSpawner->GenerateInstanceFromName("CallibrationMaterial", true);
  CallibrationTest->GetStaticMeshComponent()->SetMaterial(0, CallibrationMaterialInstance);
}

