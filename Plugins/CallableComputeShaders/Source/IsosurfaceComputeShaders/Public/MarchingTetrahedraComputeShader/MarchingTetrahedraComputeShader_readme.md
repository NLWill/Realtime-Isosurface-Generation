# MarchingTetrahedraComputeShader usage

## Blueprint

1. Right-click in any blueprint editor and add the node: "CalculatePIComputeShader"
2. Pass in a moderately high non-even number under 256k for the number of samples
3. Enjoy, and get digging into the code

## C++

```cpp
// Params struct used to pass args to our compute shader
FMarchingTetrahedraComputeShaderDispatchParams Params(1, 1, 1);

// Fill in your input parameters here
Params.XYZ = 123;

// These are defined/used in:
// 1. Private/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.cpp under BEGIN_SHADER_PARAMETER_STRUCT
// 2. Public/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.h under FMarchingTetrahedraComputeShaderDispatchParams
// 3. Private/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.cpp under FMarchingTetrahedraComputeShaderInterface::DispatchRenderThread

// Executes the compute shader and blocks until complete. You can place outputs in the params struct
FMarchingTetrahedraComputeShaderInterface::Dispatch(Params);
```

Feel free to delete this file
