#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Materials/MaterialRenderProxy.h"

#include "MarchingTetrahedraComputeShader.generated.h"

struct ISOSURFACECOMPUTESHADERS_API FMarchingTetrahedraComputeShaderDispatchParams
{
	TArray<float> dataGridValues;
	FIntVector3 gridPointCount;
	FVector3f gridSizePerCube;
	FVector3f zeroNodeOffset;
	float isovalue;

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
		FMarchingTetrahedraComputeShaderDispatchParams params,
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FMarchingTetrahedraComputeShaderDispatchParams params,
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
			{
				DispatchRenderThread(RHICmdList, params, AsyncCallback);
			});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FMarchingTetrahedraComputeShaderDispatchParams params,
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), params, AsyncCallback);
		}
		else {
			DispatchGameThread(params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarchingTetrahedraComputeShaderLibrary_AsyncExecutionCompleted, const TArray<FVector3f>, outputVertexTriplets);


UCLASS() // Change the _API to match your project
class ISOSURFACECOMPUTESHADERS_API UMarchingTetrahedraComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	// Execute the actual load
	virtual void Activate() override
	{
		if (params.dataGridValues.Num() <= 0 || params.gridSizePerCube.Length() == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("MarchingCubesComputeShaderDispatchParams not completely configured"))
		}

		// Dispatch the compute shader and wait until it completes
		FMarchingTetrahedraComputeShaderInterface::Dispatch(params, [this](TArray<FVector3f> outputVertexTriplets)
			{
				Completed.Broadcast(outputVertexTriplets);
			});
	}

	FOnMarchingTetrahedraComputeShaderLibrary_AsyncExecutionCompleted Completed;

	FMarchingTetrahedraComputeShaderDispatchParams params;
};