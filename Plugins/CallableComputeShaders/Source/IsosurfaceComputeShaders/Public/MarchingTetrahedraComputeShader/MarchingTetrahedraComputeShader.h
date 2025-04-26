#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"

#include "MarchingTetrahedraComputeShader.generated.h"

struct ISOSURFACECOMPUTESHADERS_API FMarchingTetrahedraComputeShaderDispatchParams
{
	TArray<float> dataGridValues;
	FIntVector3 gridPointCount;
	FVector3f gridSizePerCube;
	FVector3f zeroNodeOffset;
	float isovalue;

	int totalSamples = 10000;
	float Seed;

	FMarchingTetrahedraComputeShaderDispatchParams(const TArray<float>& dataGridValues, FIntVector3 gridPointCount, FVector3f gridSizePerCube, FVector3f zeroNodeOffset, float isovalue) :
		dataGridValues(dataGridValues),
		gridPointCount(gridPointCount),
		gridSizePerCube(gridSizePerCube),
		zeroNodeOffset(zeroNodeOffset),
		isovalue(isovalue)
	{
	}

	FMarchingTetrahedraComputeShaderDispatchParams()
	{
		dataGridValues = TArray<float>();
		gridPointCount = FIntVector3(0, 0, 0);
		gridSizePerCube = FVector3f(0, 0, 0);
		zeroNodeOffset = FVector3f(0, 0, 0);
		isovalue = 0;
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class ISOSURFACECOMPUTESHADERS_API FMarchingTetrahedraComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMarchingTetrahedraComputeShaderDispatchParams Params,
		TFunction<void(int TotalInCircle)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FMarchingTetrahedraComputeShaderDispatchParams Params,
		TFunction<void(int OutputVal)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FMarchingTetrahedraComputeShaderDispatchParams Params,
		TFunction<void(int TotalInCircle)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}else{
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarchingTetrahedraComputeShaderLibrary_AsyncExecutionCompleted, const double, Value);


UCLASS() // Change the _API to match your project
class ISOSURFACECOMPUTESHADERS_API UMarchingTetrahedraComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	// Execute the actual load
	virtual void Activate() override 
	{
		// Dispatch the compute shader and wait until it completes
		FMarchingTetrahedraComputeShaderInterface::Dispatch(Params, [this](int TotalInCircle) {
			// TotalInCircle is set to the result of the compute shader
			// Divide by the total number of samples to get the ratio of samples in the circle
			// We're multiplying by 4 because the simulation is done in quarter-circles
			double FinalPI = ((double) TotalInCircle / (double) Params.totalSamples);

			Completed.Broadcast(FinalPI);
		});
	}
	

	UPROPERTY(BlueprintAssignable)
	FOnMarchingTetrahedraComputeShaderLibrary_AsyncExecutionCompleted Completed;

	FMarchingTetrahedraComputeShaderDispatchParams Params;	
};