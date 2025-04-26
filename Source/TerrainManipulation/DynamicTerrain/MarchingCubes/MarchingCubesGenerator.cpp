// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubesGenerator.h"
#include "IsosurfaceComputeShaders/Public/MarchingCubesComputeShader/MarchingCubesComputeShader.h"

#define DEBUG_MARCHING_CUBES true

#if DEBUG_MARCHING_CUBES
#include <chrono>
#endif

MarchingCubesGenerator::MarchingCubesGenerator()
{
}

MarchingCubesGenerator::~MarchingCubesGenerator()
{
}

TArray<FVector3f> MarchingCubesGenerator::RunAlgorithm(const TArray3D<float> &dataGrid, FVector3f sizeOfCell, FVector3f offsetOfZeroCell, float isovalue)
{
#if DEBUG_MARCHING_CUBES
	// Measure the time taken to perform the algorithm
	auto start = std::chrono::high_resolution_clock::now();
#endif

	// Perform the marching cubes algorithm
	TArray<FVector3f> trianglesArray;
	if (bGPUCompute) 
	{
		trianglesArray = TriangulateGridGPU(dataGrid, sizeOfCell, offsetOfZeroCell, isovalue);
	}
	else 
	{
		std::vector<FVector3f> triangles = TriangulateGrid(dataGrid, sizeOfCell, offsetOfZeroCell, isovalue);

		// Copy the data into a TArray so that it may be used by the procedural mesh
		size_t trianglesSize = triangles.size();
		trianglesArray.Init(FVector3f::Zero(), trianglesSize);
		for (int i = 0; i < trianglesSize; i++) 
		{
			trianglesArray[i] = triangles[i];
		}
	}

#if DEBUG_MARCHING_CUBES
	auto finish = std::chrono::high_resolution_clock::now();

	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
	if (microseconds.count() > 1000 * 1000) {
		double floatingPointMicroseconds = (double)microseconds.count();
		UE_LOG(LogTemp, Display, TEXT("Marching cubes recalculation took %fs"), floatingPointMicroseconds / (1000*1000));
	}
	else if (microseconds.count() > 1000) {
		double floatingPointMicroseconds = (double)microseconds.count();
		UE_LOG(LogTemp, Display, TEXT("Marching cubes recalculation took %fms"), floatingPointMicroseconds / 1000);
	}
	else {
		UE_LOG(LogTemp, Display, TEXT("Marching cubes recalculation took %dµs"), microseconds.count());
	}	
#endif

	return trianglesArray;
}

int MarchingCubesGenerator::CalculateCubeIndex(const GridCell& gridCell, float isovalue)
{
	int cubeIndex = 0;
	for (int i = 0; i < 8; i++)
	{
		if (gridCell.values[i] > isovalue) cubeIndex |= (1 << i);
	}
	return cubeIndex;
}

std::vector<FVector3f> MarchingCubesGenerator::InterpolateVerticesOnEdges(const GridCell& gridCell, float isovalue)
{
	int cubeIndex = CalculateCubeIndex(gridCell, isovalue);
	std::vector<FVector3f> interpolatedVertices;
	// Iterate over the 12 edges of the cube
	// If the surface does cross this edge, find the interpolation point, otherwise write a zero vector
	for (int i = 0; i < 12; i++)
	{
		if (edgeTable[cubeIndex] & 1 << i) 
		{
			std::pair<int, int> vertices = verticesOnEdge[i];
			FVector3f interpolatedPoint = InterpolateEdge(isovalue, gridCell.positions[vertices.first], gridCell.positions[vertices.second], gridCell.values[vertices.first], gridCell.values[vertices.second]);
			interpolatedVertices.push_back(interpolatedPoint);
		}
		else 
		{
			// This edge will not be used in calculations, so pad the list with zero vector
			interpolatedVertices.push_back(FVector3f::ZeroVector);
		}
	}
	return interpolatedVertices;
}

FVector3f MarchingCubesGenerator::InterpolateEdge(float isovalue, FVector3f vertex1, FVector3f vertex2, float value1, float value2)
{
	if (FMath::Abs(value2 - value1) < 1e-5) {
		// There is significant risk of floating point errors and division by zero, so return vertex1
		return vertex1;
	}

	float interpolant = (isovalue - value1) / (value2 - value1);

	return vertex1 + (vertex2 - vertex1) * interpolant;
}

std::vector<FVector3f> MarchingCubesGenerator::GenerateTriangles(int cubeIndex, const std::vector<FVector3f> &vertexList)
{
	std::vector<FVector3f> trianglesForThisCube;

	// Each triad of vertices on the triTable represent a valid triangle for this cube index
	// Iterate over the triangles and add them to the total list of needed triangles
	for (int i = 0; triTable[cubeIndex][i] != -1; i+=3)
	{
		trianglesForThisCube.push_back(vertexList[triTable[cubeIndex][i]]);
		trianglesForThisCube.push_back(vertexList[triTable[cubeIndex][i+1]]);
		trianglesForThisCube.push_back(vertexList[triTable[cubeIndex][i+2]]);
	}
	return trianglesForThisCube;
}

std::vector<FVector3f> MarchingCubesGenerator::TriangulateCell(const GridCell &gridCell, float isovalue)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell, isovalue);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return an empty vector
	if (edgeTable[cubeIndex] == 0) return std::vector<FVector3f>();

	// Calculate the position along the edges where the surface will intersect
	auto interpolatedVertices = InterpolateVerticesOnEdges(gridCell, isovalue);

	// Generate the triangles required for this cube configuration with the interpolated points
	std::vector<FVector3f> trianglesForThisCube = GenerateTriangles(cubeIndex, interpolatedVertices);

	return trianglesForThisCube;
}

std::vector<FVector3f> MarchingCubesGenerator::TriangulateGrid(const TArray3D<float>& dataGrid, FVector3f sizeOfCell, FVector3f offsetOfZeroCell, float isovalue)
{
	std::vector<FVector3f> triangles;
	int cellCountX = dataGrid.GetSize(0) - 1;
	int cellCountY = dataGrid.GetSize(1) - 1;
	int cellCountZ = dataGrid.GetSize(2) - 1;

	// Iterate over all cells and set their triangles into their slot in the triangles array
	for (int k = 0; k < cellCountZ; k++)
	{
		for (int j = 0; j < cellCountY; j++)
		{
			for (int i = 0; i < cellCountX; i++)
			{
				// Generate the GridCell struct
				double sizeX = sizeOfCell.X;
				double sizeY = sizeOfCell.Y;
				double sizeZ = sizeOfCell.Z;
				TArray<FVector3f> cellPositions;
				cellPositions.Init(offsetOfZeroCell + FVector3f(i * sizeX, j * sizeY, k * sizeZ), 8);
				cellPositions[1] += FVector3f(sizeX, 0, 0);
				cellPositions[2] += FVector3f(sizeX, sizeY, 0);
				cellPositions[3] += FVector3f(0, sizeY, 0);
				cellPositions[4] += FVector3f(0, 0, sizeZ);
				cellPositions[5] += FVector3f(sizeX, 0, sizeZ);
				cellPositions[6] += FVector3f(sizeX, sizeY, sizeZ);
				cellPositions[7] += FVector3f(0, sizeY, sizeZ);

				TArray<float> cellValues;
				cellValues.Init(0, 8);
				cellValues[0] = dataGrid.GetElement(i, j, k);
				cellValues[1] = dataGrid.GetElement(i + 1, j, k);
				cellValues[2] = dataGrid.GetElement(i + 1, j + 1, k);
				cellValues[3] = dataGrid.GetElement(i, j + 1, k);
				cellValues[4] = dataGrid.GetElement(i, j, k + 1);
				cellValues[5] = dataGrid.GetElement(i + 1, j, k + 1);
				cellValues[6] = dataGrid.GetElement(i + 1, j + 1, k + 1);
				cellValues[7] = dataGrid.GetElement(i, j + 1, k + 1);

				GridCell gridCell = GridCell(cellPositions, cellValues);

				// Calculate the triangles required for this cube
				std::vector<FVector3f> trianglesForThisCube = TriangulateCell(gridCell, isovalue);

				// Append them to the list of triangle vertices
				for (const FVector3f& vertex : trianglesForThisCube)
				{
					triangles.push_back(vertex);
				}
			}
		}
	}

	return triangles;
}

TArray<FVector3f> MarchingCubesGenerator::TriangulateGridGPU(const TArray3D<float>& dataGrid, FVector3f sizeOfCell, FVector3f offsetOfZeroCell, float isovalue) 
{
	FIntVector3 gridPointCount(dataGrid.GetSize(0), dataGrid.GetSize(1), dataGrid.GetSize(2));

	FMarchingCubesComputeShaderDispatchParams computeShaderParams(dataGrid.GetRawDataStruct(), gridPointCount, sizeOfCell, offsetOfZeroCell, isovalue);

	TArray<FVector3f> vertexTripletList;
	FMarchingCubesComputeShaderInterface::Dispatch(computeShaderParams, [&vertexTripletList](TArray<FVector3f> outputVertexTripletList) 
		{
			vertexTripletList = outputVertexTripletList;
		});

	return vertexTripletList;
}
