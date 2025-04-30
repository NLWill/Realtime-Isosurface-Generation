// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingTetrahedraGenerator.h"
#include "IsosurfaceComputeShaders/Public/MarchingTetrahedraComputeShader/MarchingTetrahedraComputeShader.h"
#include "SimpleComputeShaders/Public/DemoPiComputeShader/DemoPiComputeShader.h"
#include <vector>


#define DEBUG_MARCHING_TETRA true

#if DEBUG_MARCHING_TETRA
#include <chrono>
#endif

using namespace UE::Geometry;

MarchingTetrahedraGenerator::MarchingTetrahedraGenerator()
{
	dataGrid = TArray3D<float>();
}

MarchingTetrahedraGenerator::MarchingTetrahedraGenerator(int gridCountX, int gridCountY, int gridCountZ)
{
	dataGrid = TArray3D<float>(gridCountX, gridCountY, gridCountZ);
}

MarchingTetrahedraGenerator::MarchingTetrahedraGenerator(TArray3D<float> providedDataGrid)
{
	dataGrid = TArray3D<float>(providedDataGrid);
}

MarchingTetrahedraGenerator::MarchingTetrahedraGenerator(const MarchingTetrahedraGenerator& other)
{
	dataGrid = TArray3D<float>(other.dataGrid);
}

//FDynamicMesh3 MarchingTetrahedraGenerator::Generate()
//{
//#if DEBUG_MARCHING_TETRA
//	// Measure the time taken to perform the algorithm
//	auto start = std::chrono::high_resolution_clock::now();
//#endif
//
//	generatedMesh.Clear();
//
//	// Perform the algorithm to determine the number of triangles
//	if (bGPUCompute)
//	{
//		GenerateOnGPU();
//	}
//	else
//	{
//		GenerateOnCPU();
//	}
//
//#if DEBUG_MARCHING_TETRA
//	auto finish = std::chrono::high_resolution_clock::now();
//
//	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(finish - start);
//	if (microseconds.count() > 1000 * 1000) {
//		double floatingPointMicroseconds = (double)microseconds.count();
//		UE_LOG(LogTemp, Display, TEXT("Marching tetrahedra recalculation took %fs"), floatingPointMicroseconds / (1000 * 1000));
//	}
//	else if (microseconds.count() > 1000) {
//		double floatingPointMicroseconds = (double)microseconds.count();
//		UE_LOG(LogTemp, Display, TEXT("Marching tetrahedra recalculation took %fms"), floatingPointMicroseconds / 1000);
//	}
//	else {
//		UE_LOG(LogTemp, Display, TEXT("Marching tetrahedra recalculation took %dµs"), microseconds.count());
//	}
//
//#endif
//
//	// Return the FDynamicMesh3
//	return generatedMesh;
//}

void MarchingTetrahedraGenerator::GenerateOnGPU(UDynamicMeshComponent* dynamicMesh)
{
	// Run the algorithm
	FIntVector3 gridPointCount(dataGrid.GetSize(0), dataGrid.GetSize(1), dataGrid.GetSize(2));

	FMarchingTetrahedraComputeShaderDispatchParams params(dataGrid.GetRawDataStruct(), gridPointCount, gridCellDimensions, zeroCellOffset, isovalue);
	FMarchingTetrahedraComputeShaderInterface::Dispatch(params, [this, dynamicMesh](TArray<FVector3f> outputVertexTriplets) {
			CreateMeshFromVertexTriplets(outputVertexTriplets);
			dynamicMesh->SetMesh(MoveTemp(generatedMesh));
			dynamicMesh->NotifyMeshUpdated();
		});
}

FDynamicMesh3 MarchingTetrahedraGenerator::GenerateOnCPU()
{
	FGridCell gridCell;
	FVector3i gridIndex = FVector3i(0, 0, 0);

	// Iterate over first x, then y, then z
	for (size_t k = 0; k < dataGrid.GetSize(2) - 1; k++)
	{
		gridIndex.Z = k;
		for (size_t j = 0; j < dataGrid.GetSize(1) - 1; j++)
		{
			gridIndex.Y = j;
			for (size_t i = 0; i < dataGrid.GetSize(0) - 1; i++)
			{
				gridIndex.X = i;
				InitialiseGridCell(gridCell, gridIndex);
				bool trianglesAdded = TriangulateGridCell(gridCell);
			}
		}
	}

	return generatedMesh;
}

void MarchingTetrahedraGenerator::InitialiseGridCell(FGridCell& gridCell, const UE::Geometry::FVector3i& gridIndex)
{
	for (int i = 0; i < 8; i++) {
		FVector3i cornerIndex = gridIndex + cubeVertexOrder[i];
		gridCell.positions[i] = FVector3d(cornerIndex.X * gridCellDimensions.X, cornerIndex.Y * gridCellDimensions.Y, cornerIndex.Z * gridCellDimensions.Z);
		gridCell.values[i] = dataGrid.GetElement(gridIndex.X + cubeVertexOrder[i].X, gridIndex.Y + cubeVertexOrder[i].Y, gridIndex.Z + cubeVertexOrder[i].Z);
	}
}

void MarchingTetrahedraGenerator::InitialiseTetrahedron(FTetrahedron& tetra, const FGridCell& gridCell, const int tetrahedronNumber)
{
	for (size_t i = 0; i < 4; i++)
	{
		tetra.cornerIndices[i] = tetrahedronList[tetrahedronNumber][i];
		tetra.values[i] = gridCell.values[tetrahedronList[tetrahedronNumber][i]];
	}
}

bool MarchingTetrahedraGenerator::TriangulateGridCell(const FGridCell& gridCell)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (cubeEdgeTable[cubeIndex] == 0) return false;

	// Interpolate edges
	// Edges are interpolated for the cube to remove duplicate interpolation calculations
	TArray<FVector3d> interpolatedEdgesInCube = InterpolateVerticesOnEdges(gridCell, cubeIndex);

	// Iterate over each tetrahedron
	FTetrahedron tetra{};
	for (int i = 0; i < 6; i++)
	{
		InitialiseTetrahedron(tetra, gridCell, i);
		bool trianglesAdded = TriangulateTetrahedron(tetra, interpolatedEdgesInCube);
	}

	return true;
}

bool MarchingTetrahedraGenerator::TriangulateTetrahedron(const FTetrahedron& tetra, const TArray<FVector3d>& interpolatedEdgesInCube)
{
	// Firstly determine the tetrahedrons's unique index based upon which vertices are above/below the isovalue
	int tetraIndex = CalculateTetrahedronIndex(tetra);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (tetrahedronEdgeTable[tetraIndex] == 0) return false;

	GenerateTrianglesFromTetrahedron(tetra, tetraIndex, interpolatedEdgesInCube);

	return true;
}

int MarchingTetrahedraGenerator::CalculateCubeIndex(const FGridCell& gridCell) const
{
	int cubeIndex = 0;
	for (int i = 0; i < 8; i++)
	{
		if (gridCell.values[i] > isovalue) cubeIndex |= (1 << i);
	}
	return cubeIndex;
}

int MarchingTetrahedraGenerator::CalculateTetrahedronIndex(const FTetrahedron& tetra) const
{
	int tetraIndex = 0;
	for (int i = 0; i < 4; i++)
	{
		if (tetra.values[i] > isovalue) tetraIndex |= (1 << i);
	}
	return tetraIndex;
}

TArray<FVector3d> MarchingTetrahedraGenerator::InterpolateVerticesOnEdges(const FGridCell& gridCell, const int cubeIndex)
{
	TArray<FVector3d> interpolatedVertices;
	interpolatedVertices.Init(FVector3d::ZeroVector, 19);
	// Iterate over the 19 edges of the cube (plus diagonals from the tetrahedra)
	// If the surface does cross this edge, find the interpolation point, otherwise write a zero vector
	for (int i = 0; i < 19; i++)
	{
		if (cubeEdgeTable[cubeIndex] & 1 << i)
		{
			std::pair<int, int> vertices = cubeVerticesOnEdge[i];
			FVector3d interpolatedPoint = InterpolateEdge(gridCell.positions[vertices.first], gridCell.positions[vertices.second], gridCell.values[vertices.first], gridCell.values[vertices.second]);
			interpolatedVertices[i] = interpolatedPoint;
		}
		else
		{
			// This edge will not be used in calculations, so pad the list with zero vector
			//interpolatedVertices[i] = FVector3d::ZeroVector;
			// No need as the array is initialised with this anyway
		}
	}
	return interpolatedVertices;
}

FVector3d MarchingTetrahedraGenerator::InterpolateEdge(FVector3d vertex1, FVector3d vertex2, float value1, float value2)
{
	if (FMath::Abs(value2 - value1) < 1e-5) {
		// There is significant risk of floating point errors and division by zero, so return vertex1
		return vertex1;
	}

	float interpolant = (isovalue - value1) / (value2 - value1);

	return vertex1 + (vertex2 - vertex1) * interpolant;
}

void MarchingTetrahedraGenerator::GenerateTrianglesFromTetrahedron(const FTetrahedron& tetra, int tetraIndex, const TArray<FVector3d>& interpolatedEdgesInCube)
{
	// Store the ids of the vertices within the Vertices array
	TArray<int> interpolatedVertexIDs;
	interpolatedVertexIDs.Init(-1, 4);

	// Translate all interpolated edge values from the cube into the tetrahedral space
	TArray<FVector3d> interpolatedEdgesInTetrahedronSpace;
	for (int i = 0; i < 6; i++)
	{
		std::pair<int, int> tetraVertices = tetrahedronVerticesOnEdge[i];
		std::pair<int, int> cubeVertices;
		cubeVertices.first = tetra.cornerIndices[tetraVertices.first];
		cubeVertices.second = tetra.cornerIndices[tetraVertices.second];
		int edgeID = cubeVertexPairToEdge[cubeVertices.first][cubeVertices.second];
		if (edgeID != -1)
		{
			interpolatedEdgesInTetrahedronSpace.Add(interpolatedEdgesInCube[edgeID]);
		}
		else
		{
			// Edge ID should never return -1 as it is a sign of a disconnect between data table definitions
			UE_LOG(LogTemp, Fatal, TEXT("Vertex pair %d and %d do not form a valid edge."), cubeVertices.first, cubeVertices.second)
		}

	}

	// Check at least one triangle is required
	// It should be required, due to tetraIndex checks in the TriangulateTetrahedron function
	if (tetrahedronTriTable[tetraIndex][2] != -1)
	{
		for (int i = 0; i < 3; i++)
		{
			interpolatedVertexIDs[i] = generatedMesh.AppendVertex(interpolatedEdgesInTetrahedronSpace[tetrahedronTriTable[tetraIndex][i]]);
			//interpolatedVertexIDs[i] = generatedMesh.AppendVertex(interpolatedVertexList[tetrahedronTriTable[tetraIndex][i]]);
		}
		generatedMesh.AppendTriangle(interpolatedVertexIDs[0], interpolatedVertexIDs[1], interpolatedVertexIDs[2]);
	}
	else return;

	// Check whether a second triangle is required
	if (tetrahedronTriTable[tetraIndex][3] != -1)
	{
		// Append the final required vertex from the triangle strip
		interpolatedVertexIDs[3] = generatedMesh.AppendVertex(interpolatedEdgesInTetrahedronSpace[tetrahedronTriTable[tetraIndex][3]]);
		// Reverse the direction of the vertices otherwise the triangle will point the wrong way
		generatedMesh.AppendTriangle(interpolatedVertexIDs[3], interpolatedVertexIDs[2], interpolatedVertexIDs[1]);
	}
}

void MarchingTetrahedraGenerator::CreateMeshFromVertexTriplets(const TArray<FVector3f>& vertexTripletList)
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
