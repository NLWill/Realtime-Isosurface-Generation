// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingCubesGenerator.h"

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

TArray<FVector> MarchingCubesGenerator::RunAlgorithm(const TArray3D<float> &dataGrid, FVector sizeOfCell, FVector offsetOfZeroCell, float isovalue)
{
#if DEBUG_MARCHING_CUBES
	// Measure the time taken to perform the algorithm
	auto start = std::chrono::high_resolution_clock::now();
#endif

	// Perform the marching cubes algorithm
	std::vector<FVector> triangles = TriangulateGrid(dataGrid, sizeOfCell, offsetOfZeroCell, isovalue);

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

	// Copy the data into a TArray so that it may be used by the procedural mesh
	TArray<FVector> trianglesArray;
	size_t trianglesSize = triangles.size();
	trianglesArray.Init(FVector::Zero(), trianglesSize);
	for (int i = 0; i < trianglesSize; i++) {
		trianglesArray[i] = triangles[i];
	}

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

std::vector<FVector> MarchingCubesGenerator::InterpolateVerticesOnEdges(const GridCell& gridCell, float isovalue)
{
	int cubeIndex = CalculateCubeIndex(gridCell, isovalue);
	std::vector<FVector> interpolatedVertices;
	// Iterate over the 12 edges of the cube
	// If the surface does cross this edge, find the interpolation point, otherwise write a zero vector
	for (int i = 0; i < 12; i++)
	{
		if (edgeTable[cubeIndex] & 1 << i) 
		{
			std::pair<int, int> vertices = verticesOnEdge[i];
			FVector interpolatedPoint = InterpolateEdge(isovalue, gridCell.positions[vertices.first], gridCell.positions[vertices.second], gridCell.values[vertices.first], gridCell.values[vertices.second]);
			interpolatedVertices.push_back(interpolatedPoint);
		}
		else 
		{
			// This edge will not be used in calculations, so pad the list with zero vector
			interpolatedVertices.push_back(FVector::ZeroVector);
		}
	}
	return interpolatedVertices;
}

FVector MarchingCubesGenerator::InterpolateEdge(float isovalue, FVector vertex1, FVector vertex2, float value1, float value2)
{
	if (FMath::Abs(value2 - value1) < 1e-5) {
		// There is significant risk of floating point errors and division by zero, so return vertex1
		return vertex1;
	}

	float interpolant = (isovalue - value1) / (value2 - value1);

	return vertex1 + (vertex2 - vertex1) * interpolant;
}

std::vector<FVector> MarchingCubesGenerator::GenerateTriangles(int cubeIndex, const std::vector<FVector> &vertexList)
{
	std::vector<FVector> trianglesForThisCube;

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

std::vector<FVector> MarchingCubesGenerator::TriangulateCell(const GridCell &gridCell, float isovalue)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell, isovalue);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return an empty vector
	if (edgeTable[cubeIndex] == 0) return std::vector<FVector>();

	// Calculate the position along the edges where the surface will intersect
	auto interpolatedVertices = InterpolateVerticesOnEdges(gridCell, isovalue);

	// Generate the triangles required for this cube configuration with the interpolated points
	std::vector<FVector> trianglesForThisCube = GenerateTriangles(cubeIndex, interpolatedVertices);

	return trianglesForThisCube;
}

std::vector<FVector> MarchingCubesGenerator::TriangulateGrid(const TArray3D<float>& dataGrid, FVector sizeOfCell, FVector offsetOfZeroCell, float isovalue)
{
	std::vector<FVector> triangles;
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
				TArray<FVector> cellPositions;
				cellPositions.Init(offsetOfZeroCell + FVector(i * sizeX, j * sizeY, k * sizeZ), 8);
				cellPositions[1] += FVector(sizeX, 0, 0);
				cellPositions[2] += FVector(sizeX, sizeY, 0);
				cellPositions[3] += FVector(0, sizeY, 0);
				cellPositions[4] += FVector(0, 0, sizeZ);
				cellPositions[5] += FVector(sizeX, 0, sizeZ);
				cellPositions[6] += FVector(sizeX, sizeY, sizeZ);
				cellPositions[7] += FVector(0, sizeY, sizeZ);

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
				std::vector<FVector> trianglesForThisCube = TriangulateCell(gridCell, isovalue);

				// Append them to the list of triangle vertices
				for (const FVector& vertex : trianglesForThisCube)
				{
					triangles.push_back(vertex);
				}
			}
		}
	}

	return triangles;
}
