#include "CanvasTypes.h"
#include "DynamicMeshBuilder.h"
#include "GlobalShader.h"
#include "IsosurfaceComputeShaders/Public/MarchingCubesComputeShader/MarchingCubesComputeShader.h"
#include "MarchingCubesComputeShader.h"
#include "MaterialShader.h"
#include "MeshDrawShaderBindings.h"
#include "MeshPassProcessor.inl"
#include "MeshPassUtils.h"
#include "PixelShaderUtils.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"
#include "StaticMeshResources.h"
#include "UnifiedBuffer.h"

DECLARE_STATS_GROUP(TEXT("MarchingCubesComputeShader"), STATGROUP_MarchingCubesComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("MarchingCubesComputeShader Execute"), STAT_MarchingCubesComputeShader_Execute, STATGROUP_MarchingCubesComputeShader);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class ISOSURFACECOMPUTESHADERS_API FMarchingCubesComputeShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FMarchingCubesComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FMarchingCubesComputeShader, FGlobalShader);


	class FMarchingCubesComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FMarchingCubesComputeShader_Perm_TEST
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

		// Input
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, dataGridValues)
		SHADER_PARAMETER(FIntVector3, gridPointCount)
		SHADER_PARAMETER(FVector3f, gridSizePerCube)
		SHADER_PARAMETER(FVector3f, zeroNodeOffset)

		// Output
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
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_MarchingCubesComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_MarchingCubesComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_MarchingCubesComputeShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FMarchingCubesComputeShader, "/IsosurfaceComputeShadersShaders/MarchingCubesComputeShader/MarchingCubesComputeShader.usf", "MarchingCubesComputeShader", SF_Compute);
/*
void ReferenceScript(FRHICommandListImmediate& RHICmdList, FMarchingCubesComputeShaderDispatchParams Params, TFunction<void(int OutputVal)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_MarchingCubesComputeShader_Execute);
		DECLARE_GPU_STAT(MarchingCubesComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "MarchingCubesComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, MarchingCubesComputeShader);

		typename FMarchingCubesComputeShader::FPermutationDomain PermutationVector;

		// Add any static permutation options here
		// PermutationVector.Set<FMarchingCubesComputeShader::FMyPermutationName>(12345);

		TShaderMapRef<FMarchingCubesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {
			FMarchingCubesComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarchingCubesComputeShader::FParameters>();

			// Create a buffer to contain the output
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
				TEXT("OutputBuffer"));

			// Associate the output buffer to the relevant output list
			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));


			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteMarchingCubesComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});


			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingCubesComputeShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) {

					int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
					int OutVal = Buffer[0];

					GPUBufferReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal]() {
						AsyncCallback(OutVal);
						});

					delete GPUBufferReadback;
				}
				else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
						});
				}
				};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
				});

		}
		else {
#if WITH_EDITOR
			GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
#endif

			// We exit here as we don't want to crash the game if the shader is not found or has an error.

		}
	}

	GraphBuilder.Execute();
}
*/
void FMarchingCubesComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMarchingCubesComputeShaderDispatchParams params, TFunction<void(TArray<FVector3f> vertexTripletList)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	// Get a reference to the relevant compute shader
	SCOPE_CYCLE_COUNTER(STAT_MarchingCubesComputeShader_Execute);
	DECLARE_GPU_STAT(MarchingCubesComputeShader)
	RDG_EVENT_SCOPE(GraphBuilder, "MarchingCubesComputeShader");
	RDG_GPU_STAT_SCOPE(GraphBuilder, MarchingCubesComputeShader);

	typename FMarchingCubesComputeShader::FPermutationDomain PermutationVector;

	// Add any static permutation options here
	// PermutationVector.Set<FMarchingCubesComputeShader::FMyPermutationName>(12345);

	TShaderMapRef<FMarchingCubesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	// Check if the shader is valid before calling
	bool bIsShaderValid = ComputeShader.IsValid();

	if (bIsShaderValid)
	{
		FMarchingCubesComputeShader::FParameters* passParameters = GraphBuilder.AllocParameters<FMarchingCubesComputeShader::FParameters>();

		// Allocate the trivial data structs to the shader parameters
		passParameters->gridPointCount = params.gridPointCount;
		passParameters->gridSizePerCube = params.gridSizePerCube;
		passParameters->zeroNodeOffset = params.zeroNodeOffset;

		// For Buffers, the data needs to be uploaded to the GPU
		// Upload the input data buffer
		const void* rawData = (void*)params.dataGridValues.GetData();
		int numDataGridEntries = params.dataGridValues.Num();
		int entrySize = sizeof(float);

		FRDGBufferRef dataGridBuffer = CreateUploadBuffer(GraphBuilder, TEXT("dataGridValues"), entrySize, numDataGridEntries, rawData, entrySize * numDataGridEntries);

		passParameters->dataGridValues = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(dataGridBuffer));

		// Create an output buffer
		int maxPossibleTris = 5 * 3 * (params.gridPointCount.X - 1) * (params.gridPointCount.Y - 1) * (params.gridPointCount.Z - 1);
		FRDGBufferRef outputVertexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(FVector3f), maxPossibleTris), TEXT("outputVertexTriplets"));

		passParameters->outputVertexTriplets = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(outputVertexBuffer));

		// Create an output buffer for the number of vertex triplets
		FRDGBufferRef outputTriangleCountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("OutputBuffer"));

		passParameters->vertexTripletIndex = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(outputTriangleCountBuffer, EPixelFormat::PF_R32_UINT));

		// Calculate the number of worker groups required
		FIntVector groupSize(8, 8, 8);
		int groupCountX = FMath::CeilToInt((float)params.gridPointCount.X / groupSize.X);
		int groupCountY = FMath::CeilToInt((float)params.gridPointCount.Y / groupSize.Y);
		int groupCountZ = FMath::CeilToInt((float)params.gridPointCount.Z / groupSize.Z);
		FIntVector groupCount(groupCountX, groupCountY, groupCountZ);

		// Add a pass to the shader graph and dispatch it
		GraphBuilder.AddPass(RDG_EVENT_NAME("ExecuteMarchingCubesComputeShader"), passParameters, ERDGPassFlags::AsyncCompute,
			[&passParameters, ComputeShader, groupCount](FRHIComputeCommandList& RHIComputeCmdList)
			{
				FComputeShaderUtils::Dispatch(RHIComputeCmdList, ComputeShader, *passParameters, groupCount);
			});

		// Retrieve the result from the compute shader
		// Get the vertex count
		FRHIGPUBufferReadback* GPUBufferReadbackTriangleCount = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingCubesComputeShaderTriangleCountOutput"));
		AddEnqueueCopyPass(GraphBuilder, GPUBufferReadbackTriangleCount, outputTriangleCountBuffer, 0u);
		// Get the vertex list
		FRHIGPUBufferReadback* GPUBufferReadbackVertexList = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingCubesComputeShaderVertexListOutput"));
		AddEnqueueCopyPass(GraphBuilder, GPUBufferReadbackVertexList, outputVertexBuffer, 0u);

		auto RunnerFunc = [GPUBufferReadbackTriangleCount, GPUBufferReadbackVertexList, AsyncCallback](auto&& RunnerFunc) -> void
			{
				if (GPUBufferReadbackTriangleCount->IsReady() && GPUBufferReadbackVertexList->IsReady())
				{

					int32* vertexTripletCountBuffer = (int32*)GPUBufferReadbackTriangleCount->Lock(1);
					int vertexTripletCount = vertexTripletCountBuffer[0];

					GPUBufferReadbackTriangleCount->Unlock();

					FVector3f* vertexListBuffer = (FVector3f*)GPUBufferReadbackVertexList->Lock(vertexTripletCount * sizeof(FVector3f));
					TArray<FVector3f> vertexList = TArray<FVector3f>(vertexListBuffer, vertexTripletCount);

					GPUBufferReadbackVertexList->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, vertexList]()
						{
							AsyncCallback(vertexList);
						});

					delete GPUBufferReadbackTriangleCount;
					delete GPUBufferReadbackVertexList;
				}
				else {
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
						{
							RunnerFunc(RunnerFunc);
						});
				}
			};

		AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
			RunnerFunc(RunnerFunc);
			});
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("MarchingCubesComputeShader is not valid"))
	}

	// Execute the graph.
	GraphBuilder.Execute();
}