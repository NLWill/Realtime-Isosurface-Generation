#include "CanvasTypes.h"
#include "DynamicMeshBuilder.h"
#include "GlobalShader.h"
#include "IsosurfaceComputeShaders/Public/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.h"
#include "MarchingTetrahedraComputeShader.h"
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


		SHADER_PARAMETER(float, Seed)
		SHADER_PARAMETER(FIntVector3, gridPointCount)
		SHADER_PARAMETER(FVector3f, gridSizePerCube)
		SHADER_PARAMETER(FVector3f, zeroNodeOffset)
		SHADER_PARAMETER(float, isovalue)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int32>, Output)


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

void FMarchingTetrahedraComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMarchingTetrahedraComputeShaderDispatchParams Params, TFunction<void(int OutputVal)> AsyncCallback) {
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

		if (bIsShaderValid) {
			FMarchingTetrahedraComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FMarchingTetrahedraComputeShader::FParameters>();

			PassParameters->Seed = Params.Seed;

			/*
			FRDGBufferDesc outputBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), 1);
			FRDGBufferDesc outputBufferDesc2 = FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1);
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(outputBufferDesc, TEXT("OutputBuffer"));

			PassParameters->Output = GraphBuilder.CreateUAV(OutputBuffer);
			*/

			TArray<int32> outValues = { 0 };
			int32 outputArrayLength = outValues.Num();
			auto vertexBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("NormalsCS_Verticies"), sizeof(int32), outputArrayLength, outValues.GetData(), sizeof(int32) * outputArrayLength, ERDGInitialDataFlags::None);
			auto vertexUAV = GraphBuilder.CreateUAV(vertexBuffer, PF_R32_UINT);
			PassParameters->Output = vertexUAV;

			PassParameters->gridPointCount = Params.gridPointCount;
			PassParameters->gridSizePerCube = Params.gridSizePerCube;
			PassParameters->zeroNodeOffset = Params.zeroNodeOffset;
			PassParameters->isovalue = Params.isovalue;


			//auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			//int totalSamples = Params.totalSamples;
			//int groupSize = NUM_THREADS_MarchingTetrahedraComputeShader_X * NUM_THREADS_MarchingTetrahedraComputeShader_Y * NUM_THREADS_MarchingTetrahedraComputeShader_Z;
			//FIntVector GroupCount(totalSamples * 4 / groupSize, 1, 1);

			// Calculate the number of worker groups required
			int groupCountX = FMath::CeilToInt((float)Params.gridPointCount.X / NUM_THREADS_MarchingTetrahedraComputeShader_X);
			int groupCountY = FMath::CeilToInt((float)Params.gridPointCount.Y / NUM_THREADS_MarchingTetrahedraComputeShader_Y);
			int groupCountZ = FMath::CeilToInt((float)Params.gridPointCount.Z / NUM_THREADS_MarchingTetrahedraComputeShader_Z);
			FIntVector GroupCount(groupCountX, groupCountY, groupCountZ);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ExecuteMarchingTetrahedraComputeShader"), ComputeShader, PassParameters, GroupCount);


			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMarchingTetrahedraComputeShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, vertexBuffer, 0u);

			//TRefCountPtr<FRDGPooledBuffer> pooled_verticies;
			//GraphBuilder.QueueBufferExtraction(vertexBuffer, &pooled_verticies, ERHIAccess::CPURead);

			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) {

					int32* Buffer = (int32*)GPUBufferReadback->Lock(1);
					int32 OutVal = Buffer[0];

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