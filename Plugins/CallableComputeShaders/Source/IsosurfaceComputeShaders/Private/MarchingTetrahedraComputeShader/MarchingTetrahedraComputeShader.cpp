#include "MarchingTetrahedraComputeShader.h"
#include "CanvasTypes.h"
#include "DynamicMeshBuilder.h"
#include "GlobalShader.h"
#include "IsosurfaceComputeShaders/Public/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.h"
#include "MaterialShader.h"
#include "MeshDrawShaderBindings.h"
#include "MeshPassProcessor.inl"
#include "MeshPassUtils.h"
#include "PixelShaderUtils.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"
#include "StaticMeshResources.h"
#include "UnifiedBuffer.h"

DECLARE_STATS_GROUP(TEXT("MarchingTetrahedraComputeShader"), STATGROUP_MarchingTetrahedraComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("MarchingTetrahedraComputeShader Execute"), STAT_MarchingTetrahedraComputeShader_Execute, STATGROUP_MarchingTetrahedraComputeShader);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class ISOSURFACECOMPUTESHADERS_API FMarchingTetrahedraComputeShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FMarchingTetrahedraComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FMarchingTetrahedraComputeShader, FGlobalShader);


	class FMarchingTetrahedraComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FMarchingTetrahedraComputeShader_Perm_TEST
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)


		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, dataGridValues)
		SHADER_PARAMETER(FIntVector3, gridPointCount)
		SHADER_PARAMETER(FVector3f, gridSizePerCube)
		SHADER_PARAMETER(FVector3f, zeroNodeOffset)
		SHADER_PARAMETER(float, isovalue)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, vertexTripletIndex)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector3f>, outputVertexTriplets)


	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_MarchingTetrahedraComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_MarchingTetrahedraComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_MarchingTetrahedraComputeShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FMarchingTetrahedraComputeShader, "/IsosurfaceComputeShadersShaders/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.usf", "MarchingTetrahedraComputeShader", SF_Compute);

void FMarchingTetrahedraComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMarchingTetrahedraComputeShaderDispatchParams Params, TFunction<void(TArray<FVector3f> outputVertexTriplets)> AsyncCallback) {
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_MarchingTetrahedraComputeShader_Execute);
		DECLARE_GPU_STAT(MarchingTetrahedraComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "MarchingTetrahedraComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, MarchingTetrahedraComputeShader);

		typename FMarchingTetrahedraComputeShader::FPermutationDomain PermutationVector;

		// Add any static permutation options here
		// PermutationVector.Set<FMarchingTetrahedraComputeShader::FMyPermutationName>(12345);

		TShaderMapRef<FMarchingTetrahedraComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid)
		{
			FMarchingTetrahedraComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarchingTetrahedraComputeShader::FParameters>();

			// Create input buffer for dataGridValues
			int32 dataGridValuesLength = Params.dataGridValues.Num();
			auto dataGridBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("DataGridBuffer"), sizeof(float), dataGridValuesLength, Params.dataGridValues.GetData(), sizeof(float) * dataGridValuesLength, ERDGInitialDataFlags::None);
			auto dataGridSRV = GraphBuilder.CreateSRV(dataGridBuffer, PF_R32_FLOAT);
			PassParameters->dataGridValues = dataGridSRV;

			// Create output buffer for number of tris created
			TArray<int32> vertexTripletIndexValues = { 0 };
			int32 vertexTripletIndexLength = vertexTripletIndexValues.Num();
			auto vertexTripletIndexBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("VertexTipletCount"), sizeof(uint32), vertexTripletIndexLength, vertexTripletIndexValues.GetData(), sizeof(uint32) * vertexTripletIndexLength, ERDGInitialDataFlags::None);
			auto vertexTripletIndexUAV = GraphBuilder.CreateUAV(vertexTripletIndexBuffer, PF_R32_UINT);
			PassParameters->vertexTripletIndex = vertexTripletIndexUAV;

			// Create output buffer for the vertices of the tris
			// 12 is the max tris possbile in a single grid cell
			int maxPossibleTriTriplets = 12 * 3 * (Params.gridPointCount.X - 1) * (Params.gridPointCount.Y - 1) * (Params.gridPointCount.Z - 1);
			// This is the maximum theoretically possible tris for any data grid
			TArray<FVector3f> outputVertexList;
			outputVertexList.Init(FVector3f::ZeroVector, maxPossibleTriTriplets);
			auto outputVertexListBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("VertexTipletCount"), sizeof(FVector3f), maxPossibleTriTriplets, outputVertexList.GetData(), sizeof(FVector3f) * maxPossibleTriTriplets, ERDGInitialDataFlags::None);
			auto outputVertexListUAV = GraphBuilder.CreateUAV(outputVertexListBuffer, PF_R32_UINT);
			PassParameters->outputVertexTriplets = outputVertexListUAV;

			// Set the trivial variables
			PassParameters->gridPointCount = Params.gridPointCount;
			PassParameters->gridSizePerCube = Params.gridSizePerCube;
			PassParameters->zeroNodeOffset = Params.zeroNodeOffset;
			PassParameters->isovalue = Params.isovalue;

			// Calculate the number of worker groups required
			int groupCountX = FMath::CeilToInt((float)Params.gridPointCount.X / NUM_THREADS_MarchingTetrahedraComputeShader_X);
			int groupCountY = FMath::CeilToInt((float)Params.gridPointCount.Y / NUM_THREADS_MarchingTetrahedraComputeShader_Y);
			int groupCountZ = FMath::CeilToInt((float)Params.gridPointCount.Z / NUM_THREADS_MarchingTetrahedraComputeShader_Z);
			FIntVector GroupCount(groupCountX, groupCountY, groupCountZ);

			// Add a pass of the compute shader to the render graph
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ExecuteMarchingTetrahedraComputeShader"), ComputeShader, PassParameters, GroupCount);

			// Configure the readback for the data
			FRHIGPUBufferReadback* GPUBufferReadbackTripletCount = new FRHIGPUBufferReadback(TEXT("MarchingTetrahedraComputeShaderOutputVertexTripletCount"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadbackTripletCount, vertexTripletIndexBuffer, sizeof(uint32));
			FRHIGPUBufferReadback* GPUBufferReadbackVertexList = new FRHIGPUBufferReadback(TEXT("MarchingTetrahedraComputeShaderOutputVertexList"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadbackVertexList, outputVertexListBuffer, maxPossibleTriTriplets * sizeof(FVector3f));

			auto RunnerFunc = [GPUBufferReadbackTripletCount, GPUBufferReadbackVertexList, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadbackTripletCount->IsReady() && GPUBufferReadbackVertexList->IsReady()) {

					uint32* Buffer = (uint32*)GPUBufferReadbackTripletCount->Lock(1);
					uint32 OutVal = Buffer[0];
					GPUBufferReadbackTripletCount->Unlock();

					FVector3f* outputVertexList = (FVector3f*)GPUBufferReadbackVertexList->Lock(OutVal * sizeof(FVector3f));
					TArray<FVector3f> outputVertexArray(outputVertexList, OutVal);
					GPUBufferReadbackVertexList->Unlock();
					for (FVector3f& item : outputVertexArray)
					{
						UE_LOG(LogTemp, Display, TEXT("%s"), *item.ToString())
					}

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, outputVertexArray]()
						{
							AsyncCallback(outputVertexArray);
						});

					delete GPUBufferReadbackTripletCount;
					delete GPUBufferReadbackVertexList;
				}
				else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
						{
							RunnerFunc(RunnerFunc);
						});
				}
				};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
				{
					RunnerFunc(RunnerFunc);
				});

		}
		else
		{
#if WITH_EDITOR
			GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
#endif

			// We exit here as we don't want to crash the game if the shader is not found or has an error.

		}
	}

	GraphBuilder.Execute();
}