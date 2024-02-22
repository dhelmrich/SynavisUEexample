// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <numeric>
#include <limits>
#include "CoreMinimal.h"

/**
 * 
 */
class MINIMALWEBRTC_API PID
{
public:
	PID();
	~PID();

	void SetTarget(float inTarget);
	void SetMeasurement(float* inMeasurement) { measurement = inMeasurement; }
	void Update();
	void SetGains(TArray<float> inGains) { gains = inGains; }
	void SetControlPointers(TArray<float*> inControlPointers) { ControlPointers = inControlPointers; }

	void AddControlPoint(float* inControlPoint, float inGain)
	{
	  ControlPointers.Add(inControlPoint);
	  gains.Add(inGain);
		lastChanges.Add(0.0);
	}
	void SetPostUpdateCallback(TFunction<void()> inPostUpdateCallback) { PostUpdateCallback = inPostUpdateCallback; }
	void SetIncrementMeasurementLength(bool inIncrementMeasurementLength) { incrementMeasurementLength = inIncrementMeasurementLength; }
	bool IsFinished() const
	{
		return done;
	}
	void SetVerbose(bool inVerbose) { Verbose = inVerbose; }

protected:
	bool Verbose = true;
	float StopThreshold = 0.001;
	float* measurement = nullptr;
	float target = -INFINITY;
	TArray<float> measurePoints;
	TArray<float*> ControlPointers;
	TArray<float> lastChanges;
	TArray<float> gains;
	TFunction<void()> PostUpdateCallback;
	float lastMeasurement = 0.0;
	bool windup = true;
	bool done = false;
	bool incrementMeasurementLength = false;
	int64 maxMeasures = 50;
	float OverallChange = 0.0;
	int64 NumAdjustments = 0;
	
	void EmptyMeasurePoints() { measurePoints.Empty(); }


};
