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

	void SetTarget(double inTarget);
	void SetMeasurement(double* inMeasurement) { measurement = inMeasurement; }
	void Update();
	void SetGains(TArray<double> inGains) { gains = inGains; }
	void SetControlPointers(TArray<double*> inControlPointers) { ControlPointers = inControlPointers; }

	void AddControlPoint(double* inControlPoint, double inGain)
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
	double StopThreshold = 0.001;
	double* measurement = nullptr;
	double target = -INFINITY;
	TArray<double> measurePoints;
	TArray<double*> ControlPointers;
	TArray<double> lastChanges;
	TArray<double> gains;
	TFunction<void()> PostUpdateCallback;
	double lastMeasurement = 0.0;
	bool windup = true;
	bool done = false;
	bool incrementMeasurementLength = false;
	int64 maxMeasures = 50;
	double OverallChange = 0.0;
	int64 NumAdjustments = 0;
	
	void EmptyMeasurePoints() { measurePoints.Empty(); }


};
