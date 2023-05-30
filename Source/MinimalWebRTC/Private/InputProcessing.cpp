// Fill out your copyright notice in the Description page of Project Settings.


#include "InputProcessing.h"
#include "Materials/MaterialInstanceDynamic.h"

// Sets default values for this component's properties
UInputProcessing::UInputProcessing()
{
  // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
  // off to improve performance if you don't need them.
  PrimaryComponentTick.bCanEverTick = true;

  // ...
}

void UInputProcessing::SendMessage(FString Message)
{
  FString Data = FString(reinterpret_cast<TCHAR*>(TCHAR_TO_ANSI(*Message)));
  OnPixelStreamingResponse.Broadcast(Data);
}

// Called when the game starts
void UInputProcessing::BeginPlay()
{
  Super::BeginPlay();

}

// Called every frame
void UInputProcessing::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
  FString Data;
  switch (DataCollectionType)
  {
  case EDataCollectionType::None:
    break;
  case EDataCollectionType::Count:
  {
    int Count = RandomActors.Num();
    Data = FString::Printf(TEXT("{\"type\":\"data\",\"count\":%d}"), Count);

  }
  break;
  case EDataCollectionType::Size:
    Data = FString::Printf(TEXT("{\"type\":\"data\",\"size\":%d}"), 0);
    break;
  case EDataCollectionType::Mean:
    Data = FString::Printf(TEXT("{\"type\":\"data\",\"Mean\":%d}"), 0);
    break;
  case EDataCollectionType::Position:
    Data = FString::Printf(TEXT("{\"type\":\"data\",\"position\":{\"x\":%f,\"y\":%f,\"z\":%f}}"), 0.f, 0.f, 0.f);
    break;
  }
  if (Data.Len() > 0)
  {
    SendMessage(Data);
  }
}

void UInputProcessing::ProcessInput(FString Descriptor)
{
  auto* input_pointer = *Descriptor;
  auto* ansi = reinterpret_cast<const char*>(input_pointer);
  FString Message = FString(UTF8_TO_TCHAR(ansi));
  Message.ReplaceCharInline('\n', ' ');
  Message.ReplaceCharInline('\r', ' ');
  UE_LOG(LogTemp, Warning, TEXT("Received %s"), *Message);

  TSharedPtr<FJsonObject> Jason = MakeShareable(new FJsonObject());
  TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Descriptor);

  FJsonSerializer::Deserialize(Reader, Jason);

  if (Jason->HasField("type"))
  {
    FString Type = Jason->GetStringField("type");
    if (Type == "console")
    {
      if (Jason->HasField("command"))
      {
        FString Command = Jason->GetStringField("command");
        UE_LOG(LogTemp, Warning, TEXT("Console command %s"), *Command);
        auto* Controller = GetWorld()->GetFirstPlayerController();
        if (Controller)
        {
          Controller->ConsoleCommand(Command);
        }
      }
    }
    else if (Type == "geometry")
    {
      TArray<FVector> Points, Normals;
      TArray<int> Triangles;
      FBase64 Base64;
      // pre-allocate the data destination
      TArray<uint8> Dest;
      // determine the maximum size of the data
      uint64_t MaxSize = 0;
      for (auto Field : Jason->Values)
      {
        if (Field.Key == "type")
          continue;
        const auto& Value = Field.Value;
        if (Value->Type == EJson::String)
        {
          auto Source = Value->AsString();
          if (Base64.GetDecodedDataSize(Source) > MaxSize)
            MaxSize = Source.Len();
        }
      }
      // allocate the destination buffer
      Dest.SetNumUninitialized(MaxSize);
      // get the json property for the points
      auto points = Jason->GetStringField("points");
      // decode the base64 string
      Base64.Decode(points, Dest);
      // copy the data into the points array
      Points.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
      FMemory::Memcpy(Points.GetData(), Dest.GetData(), Dest.Num());
      auto normals = Jason->GetStringField("normals");
      Dest.Reset();
      Base64.Decode(normals, Dest);
      Normals.SetNumUninitialized(Dest.Num() / sizeof(FVector), true);
      FMemory::Memcpy(Normals.GetData(), Dest.GetData(), Dest.Num());
      auto triangles = Jason->GetStringField("triangles");
      Dest.Reset();
      Base64.Decode(triangles, Dest);
      Triangles.SetNumUninitialized(Dest.Num() / sizeof(int), true);
      FMemory::Memcpy(Triangles.GetData(), Dest.GetData(), Dest.Num());
    }
    else if (Type == "info")
    {
      if (Jason->HasField("frametime"))
      {
        const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"frametime\":%f}"), GetWorld()->GetDeltaSeconds());
        OnPixelStreamingResponse.Broadcast(Response);
      }
      else if (Jason->HasField("memory"))
      {
        const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"memory\":%d}"), FPlatformMemory::GetStats().TotalPhysical);
        OnPixelStreamingResponse.Broadcast(Response);
      }
      else if (Jason->HasField("fps"))
      {
        const FString Response = FString::Printf(TEXT("{\"type\":\"info\",\"fps\":%d}"), static_cast<uint32_t>(FPlatformTime::ToMilliseconds(FPlatformTime::Cycles64())));
        OnPixelStreamingResponse.Broadcast(Response);
      }
      else if (Jason->HasField("object"))
      {
        FString RequestedObjectName = Jason->GetStringField("object");
        TArray<AActor*> FoundActors;

      }
    }
    else if (Type == "data")
    {
      if (Jason->HasField("channel"))
      {
        // we were signalled that we should send data each frame on the data channel
        FString ChannelName = Jason->GetStringField("channel");
        ObjectName = FName(*ChannelName);
        FString CalculationKind = Jason->GetStringField("kind");
        if(CalculationKind == "count")
        {
          DataCollectionType = EDataCollectionType::Count;
        }
        else if (CalculationKind == "size")
        {
          DataCollectionType = EDataCollectionType::Size;
        }
        else if (CalculationKind == "mean")
        {
          DataCollectionType = EDataCollectionType::Mean;
        }
        else if (CalculationKind == "position")
        {
          DataCollectionType = EDataCollectionType::Position;
        }
        else
        {
          DataCollectionType = EDataCollectionType::None;
        }
      }
      else if (Jason->HasField("track"))
      {
        // we were signalled that we should send data each frame on the video track
        int TrackId = Jason->GetIntegerField("track");
        OnCameraDataRequest.Broadcast(TrackId);
      }
    }
  }
}

