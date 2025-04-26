#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Materials/MaterialRenderProxy.h"

#include "MarchingCubesComputeShader.generated.h"

struct ISOSURFACECOMPUTESHADERS_API FMarchingCubesComputeShaderDispatchParams
{
	TArray<float> dataGridValues;
	FIntVector3 gridPointCount;
	FVector3f gridSizePerCube;
	FVector3f zeroNodeOffset;
	float isovalue;

	FMarchingCubesComputeShaderDispatchParams(const TArray<float>& dataGridValues, FIntVector3 gridPointCount, FVector3f gridSizePerCube, FVector3f zeroNodeOffset, float isovalue) :
		dataGridValues(dataGridValues),
		gridPointCount(gridPointCount),
		gridSizePerCube(gridSizePerCube),
		zeroNodeOffset(zeroNodeOffset),
		isovalue(isovalue)
	{
	}

	FMarchingCubesComputeShaderDispatchParams()
	{
		dataGridValues = TArray<float>();
		gridPointCount = FIntVector3(0, 0, 0);
		gridSizePerCube = FVector3f(0, 0, 0);
		zeroNodeOffset = FVector3f(0, 0, 0);
		isovalue = 0;
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class ISOSURFACECOMPUTESHADERS_API FMarchingCubesComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FMarchingCubesComputeShaderDispatchParams Params,
		TFunction<void(TArray<FVector3f> vertexTripletList)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FMarchingCubesComputeShaderDispatchParams Params,
		TFunction<void(TArray<FVector3f> vertexTripletList)> AsyncCallback
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
		TFunction<void(TArray<FVector3f> vertexTripletList)> AsyncCallback
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



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarchingCubesComputeShaderLibrary_AsyncExecutionCompleted, const TArray<FVector3f>, outputVertexList);


UCLASS() // Change the _API to match your project
class ISOSURFACECOMPUTESHADERS_API UMarchingCubesComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
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
		FMarchingCubesComputeShaderInterface::Dispatch(params, [this](TArray<FVector3f> vertexTripletList)
			{
				Completed.Broadcast(vertexTripletList);
			});
	}

	FOnMarchingCubesComputeShaderLibrary_AsyncExecutionCompleted Completed;

	FMarchingCubesComputeShaderDispatchParams params;
};