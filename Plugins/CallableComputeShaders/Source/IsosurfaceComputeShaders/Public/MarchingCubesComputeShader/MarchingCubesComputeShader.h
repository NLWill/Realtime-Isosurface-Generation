#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialRenderProxy.h"

#include "MarchingCubesComputeShader.generated.h"

struct ISOSURFACECOMPUTESHADERS_API FMarchingCubesComputeShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	TArray<float> dataGridValues;
	FIntVector3 gridPointCount;
	FVector3f gridSizePerCube;
	FVector3f zeroNodeOffset;
	
	

	FMarchingCubesComputeShaderDispatchParams(int groupCountX, int groupCountY, int groupCountZ)
		: X(groupCountX)
		, Y(groupCountY)
		, Z(groupCountZ)
	{
		dataGridValues = TArray<float>();
		gridPointCount = FIntVector3(0, 0, 0);
		gridSizePerCube = FVector3f(100, 100, 100);	// Default to some non-zero size so that is it visible
		zeroNodeOffset = FVector3f(0, 0, 0);
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class ISOSURFACECOMPUTESHADERS_API FMarchingCubesComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMarchingCubesComputeShaderDispatchParams Params,
		TFunction<void(int TotalInCircle)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FMarchingCubesComputeShaderDispatchParams Params,
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
		FMarchingCubesComputeShaderDispatchParams Params,
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



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarchingCubesComputeShaderLibrary_AsyncExecutionCompleted, const double, Value);


UCLASS() // Change the _API to match your project
class ISOSURFACECOMPUTESHADERS_API UMarchingCubesComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	
	// Execute the actual load
	virtual void Activate() override {
		// Create a dispatch parameters struct and set our desired seed
		FMarchingCubesComputeShaderDispatchParams Params(TotalSamples, 1, 1);
		Params.Seed = Seed;

		// Dispatch the compute shader and wait until it completes
		FMarchingCubesComputeShaderInterface::Dispatch(Params, [this](int TotalInCircle) {
			// TotalInCircle is set to the result of the compute shader
			// Divide by the total number of samples to get the ratio of samples in the circle
			// We're multiplying by 4 because the simulation is done in quarter-circles
			double FinalPI = ((double) TotalInCircle / (double) TotalSamples);

			Completed.Broadcast(FinalPI);
		});
	}
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static UMarchingCubesComputeShaderLibrary_AsyncExecution* ExecutePIComputeShader(UObject* WorldContextObject, int TotalSamples, float Seed) {
		UMarchingCubesComputeShaderLibrary_AsyncExecution* Action = NewObject<UMarchingCubesComputeShaderLibrary_AsyncExecution>();
		Action->TotalSamples = TotalSamples;
		Action->Seed = Seed;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}
	

	UPROPERTY(BlueprintAssignable)
	FOnMarchingCubesComputeShaderLibrary_AsyncExecutionCompleted Completed;

	
	float Seed;
	int TotalSamples;
	
};