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

void MarchingCubesGenerator::GenerateOnGPU(UDynamicMeshComponent* dynamicMesh)
{
	// Run the algorithm
	FIntVector3 gridPointCount(dataGrid.GetSize(0), dataGrid.GetSize(1), dataGrid.GetSize(2));

	FMarchingCubesComputeShaderDispatchParams params(dataGrid.GetRawDataStruct(), gridPointCount, gridCellDimensions, zeroCellOffset, isovalue);
	FMarchingCubesComputeShaderInterface::Dispatch(params, [this, dynamicMesh](TArray<FVector3f> outputVertexTriplets) {
			CreateMeshFromVertexTriplets(outputVertexTriplets);
			dynamicMesh->SetMesh(MoveTemp(generatedMesh));
			dynamicMesh->NotifyMeshUpdated();
		});
}

UE::Geometry::FDynamicMesh3 MarchingCubesGenerator::GenerateOnCPU()
{
	generatedMesh.Clear();	

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
				double sizeX = gridCellDimensions.X;
				double sizeY = gridCellDimensions.Y;
				double sizeZ = gridCellDimensions.Z;
				TArray<FVector3f> cellPositions;
				cellPositions.Init(zeroCellOffset + FVector3f(i * sizeX, j * sizeY, k * sizeZ), 8);
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
				TriangulateCell(gridCell);
			}
		}
	}

	return generatedMesh;
}

int MarchingCubesGenerator::CalculateCubeIndex(const GridCell& gridCell)
{
	int cubeIndex = 0;
	for (int i = 0; i < 8; i++)
	{
		if (gridCell.values[i] > isovalue) cubeIndex |= (1 << i);
	}
	return cubeIndex;
}

TArray<FVector3f> MarchingCubesGenerator::InterpolateVerticesOnEdges(const GridCell& gridCell)
{
	int cubeIndex = CalculateCubeIndex(gridCell);
	TArray<FVector3f> interpolatedVertices;
	interpolatedVertices.Init(FVector3f::ZeroVector, 12);
	// Iterate over the 12 edges of the cube
	// If the surface does cross this edge, find the interpolation point, otherwise write a zero vector
	for (int i = 0; i < 12; i++)
	{
		if (edgeTable[cubeIndex] & 1 << i) 
		{
			std::pair<int, int> vertices = verticesOnEdge[i];
			FVector3f interpolatedPoint = InterpolateEdge(gridCell.positions[vertices.first], gridCell.positions[vertices.second], gridCell.values[vertices.first], gridCell.values[vertices.second]);
			interpolatedVertices[i] = interpolatedPoint;
		}
		else 
		{
			// This edge will not be used in calculations, so pad the list with zero vector
			interpolatedVertices[i] = FVector3f::ZeroVector;
		}
	}
	return interpolatedVertices;
}

FVector3f MarchingCubesGenerator::InterpolateEdge(FVector3f vertex1, FVector3f vertex2, float value1, float value2)
{
	if (FMath::Abs(value2 - value1) < 1e-5) {
		// There is significant risk of floating point errors and division by zero, so return vertex1
		return vertex1;
	}

	float interpolant = (isovalue - value1) / (value2 - value1);

	return vertex1 + (vertex2 - vertex1) * interpolant;
}

void MarchingCubesGenerator::GenerateTriangles(int cubeIndex, const TArray<FVector3f> &vertexList)
{
	// Each triad of vertices on the triTable represent a valid triangle for this cube index
	// Iterate over the triangles and add them to the total list of needed triangles
	for (int i = 0; triTable[cubeIndex][i] != -1; i+=3)
	{
		auto vert1 = generatedMesh.AppendVertex((FVector3d)vertexList[triTable[cubeIndex][i]]);
		auto vert2 = generatedMesh.AppendVertex((FVector3d)vertexList[triTable[cubeIndex][i+1]]);
		auto vert3 = generatedMesh.AppendVertex((FVector3d)vertexList[triTable[cubeIndex][i+2]]);
		generatedMesh.AppendTriangle(UE::Geometry::FIndex3i(vert1, vert2, vert3));
	}
	return;
}

void MarchingCubesGenerator::TriangulateCell(const GridCell &gridCell)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return an empty vector
	if (edgeTable[cubeIndex] == 0) return;

	// Calculate the position along the edges where the surface will intersect
	auto interpolatedVertices = InterpolateVerticesOnEdges(gridCell);

	// Generate the triangles required for this cube configuration with the interpolated points
	GenerateTriangles(cubeIndex, interpolatedVertices);

	return;
}

void MarchingCubesGenerator::CreateMeshFromVertexTriplets(const TArray<FVector3f>& vertexTripletList)
{
	generatedMesh.Clear();

	for (int i = 0; i < vertexTripletList.Num(); i += 3)
	{
		generatedMesh.AppendVertex((FVector3d)vertexTripletList[i]);
		generatedMesh.AppendVertex((FVector3d)vertexTripletList[i + 1]);
		generatedMesh.AppendVertex((FVector3d)vertexTripletList[i + 2]);

		generatedMesh.AppendTriangle(i, i + 1, i + 2);
	}
}
