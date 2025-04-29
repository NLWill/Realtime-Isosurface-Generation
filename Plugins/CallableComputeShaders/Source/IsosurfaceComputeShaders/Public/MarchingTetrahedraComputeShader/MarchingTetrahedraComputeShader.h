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
		FMarchingTetrahedraComputeShaderDispatchParams Params,
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FMarchingTetrahedraComputeShaderDispatchParams Params,
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
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
		TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}
		else {
			DispatchGameThread(Params, AsyncCallback);
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
		// Dispatch the compute shader and wait until it completes
		FMarchingTetrahedraComputeShaderInterface::Dispatch(Params, [this](TArray<FVector3f> outputVertexTriplets)
			{
				Completed.Broadcast(outputVertexTriplets);
				/*if (Completed.IsBound()) {
					Completed.Execute(outputVertexTriplets);
				}
				else {
					UE_LOG(LogTemp, Display, TEXT("Why is this not bound any more"))
				}*/
			});
	}


	UPROPERTY(BlueprintAssignable)
	FOnMarchingTetrahedraComputeShaderLibrary_AsyncExecutionCompleted Completed;

	FMarchingTetrahedraComputeShaderDispatchParams Params;
};