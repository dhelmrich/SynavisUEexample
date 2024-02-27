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
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

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
  auto Type = Descriptor->GetStringField("type");
  if (Type == "plant")
  {
    Drone->ParseGeometryFromJson(Descriptor);
    // find the spawntarget in the scene


    if (Descriptor->HasField("part"))
    {
      auto Part = Descriptor->GetStringField("part");
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
    result->SetStringField("type","meter");
    // make FString from JSON
    FString OutputString;
    TSharedRef<TJsonWriter<TCHAR, COMPACT>> Writer = TJsonWriterFactory<TCHAR, COMPACT>::Create(&OutputString);
    FJsonSerializer::Serialize(result, Writer);
    // send to drone
    Drone->SendResponse(OutputString);
  }
  else if(Type == "spawnmeter")
  {
    // spawn object of class LightMeter
    auto LightMeter = GetWorld()->SpawnActor<ALightMeter>(ALightMeter::StaticClass());
    // apply possible properties to it
    Drone->ApplyJSONToObject(LightMeter, Descriptor.Get());
    const FString name = LightMeter->GetName();
    Drone->SendResponse(FString::Printf(TEXT("{\"type\":\"spawnmeter\",\"name\":\"%s\"}"), *name));
  }
}

