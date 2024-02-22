// Fill out your copyright notice in the Description page of Project Settings.


#include "PID.h"
#include <algorithm>

PID::PID()
{
}

PID::~PID()
{
}

void PID::SetTarget(float inTarget)
{
  target = inTarget;
  if (Verbose) UE_LOG(LogTemp, Warning, TEXT("PID target set to %f"), target);
}

void PID::Update()
{
  if (!measurement || target == -INFINITY) return;
  measurePoints.Add(*measurement);
  // stop this sample if measurement points are full
  if (measurePoints.Num() >= maxMeasures)
  {
    // we know that light up should make it brighter. so calculate delta to target
    float measurement_avg = std::accumulate(measurePoints.begin(), measurePoints.end(), 0.0) / static_cast<float>(measurePoints.Num());
    float delta = target - measurement_avg;
    if(FMath::Abs(delta) < StopThreshold)
    {
      if(Verbose) UE_LOG(LogTemp, Warning, TEXT("PID reached target %f with delta %f"), target, delta);
      done = true;
      return;
    }
    measurePoints.Empty();
    if(incrementMeasurementLength)
    {
      maxMeasures = static_cast<int64>(maxMeasures * 1.2);
    }
    // if we are in the windup phase, we do not know the impact of the change yet
    if (windup)
    {
      windup = false;
      // increase or decrease purely based on gains and delta
      for (int i = 0; i < ControlPointers.Num(); i++)
      {
        lastChanges[i] = gains[i] * delta * (*ControlPointers[i]);
        if(Verbose) UE_LOG(LogTemp, Warning, TEXT("Changing %f to %f to accomodate delta of %f"), *ControlPointers[i], *ControlPointers[i] + lastChanges[i], delta);
        *ControlPointers[i] += lastChanges[i];
      }
      PostUpdateCallback();
      lastMeasurement = measurement_avg;
    }
    else
    {
      // we have a valid last measurement, so we can calculate the impact
      float impact = measurement_avg - lastMeasurement;
      if(Verbose) UE_LOG(LogTemp, Warning, TEXT("PID impact: %f"), impact);
      // calculate the impact for each controller based on lastChanges
      for (int i = 0; i < ControlPointers.Num(); i++)
      {
        float change = lastChanges[i] / impact;
        // multiply by delta to target to get the change we want
        change *= delta;
        // multiply by gains to get the change we want
        change *= gains[i];
        // save to last change
        lastChanges[i] = change;
        // add to controller
        *ControlPointers[i] += change;
        if(Verbose) UE_LOG(LogTemp, Warning, TEXT("Changed %f to %f to accomodate delta of %f"), *ControlPointers[i] - change, *ControlPointers[i], delta);
      }
      PostUpdateCallback();
    }
  }
}
