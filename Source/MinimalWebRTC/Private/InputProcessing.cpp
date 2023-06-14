// Fill out your copyright notice in the Description page of Project Settings.


#include "InputProcessing.h"

#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SynavisDrone.h"
#include "WorldSpawner.h"
#include <SpawnTarget.h>

// Sets default values for this component's properties
UInputProcessing::UInputProcessing()
{
  // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
  // off to improve performance if you don't need them.
  PrimaryComponentTick.bCanEverTick = true;


  // Find base leaf material master

}

// Called when the game starts
void UInputProcessing::BeginPlay()
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
  Drone->ApplicationProcessInput = std::bind(&UInputProcessing::ProcessInput, this, std::placeholders::_1);
}

// Called every frame
void UInputProcessing::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UInputProcessing::ProcessInput(TSharedPtr<FJsonObject> Descriptor)
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
}

