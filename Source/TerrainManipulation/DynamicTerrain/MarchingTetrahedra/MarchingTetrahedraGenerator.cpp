// Fill out your copyright notice in the Description page of Project Settings.


#include "MarchingTetrahedraGenerator.h"
#include <vector>

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

FDynamicMesh3 MarchingTetrahedraGenerator::Generate()
{
	generatedMesh.Clear();

	// Perform the algorithm to determine the number of triangles
	if (bGPUCompute) 
	{
		GenerateOnGPU();
	}
	else 
	{
		GenerateOnCPU();
	}	

	// A generator should always return a reference to itself
	return generatedMesh;
}

void MarchingTetrahedraGenerator::GenerateOnGPU()
{
	// vector of positions for each GPU worker group
	std::vector<std::vector<FVector3d>> verts;

	// vector of tri ids, consistent with the equivalent vert group
	// tri ids will need to be scaled by the length of previous vert groups to ensure consistency
	std::vector<std::vector<FIndex3i>> tris;

	// Run the algorithm



	// Populate the vertices and triangles arrays
	int numVertices = 0;
	int numTriangles = 0;
	for (size_t i = 0; i < verts.size(); i++)
	{
		for (size_t j = 0; j < verts[i].size(); j++)
		{
			int vertexID = generatedMesh.AppendVertex(verts[i][j]);
		}
		for (size_t j = 0; j < tris[i].size(); j++)
		{
			// Offset the triangle indices by the number of vertices from previous GPU worker groups
			// to ensure the tris still point to the correct vertices
			generatedMesh.AppendTriangle(tris[i][j].A + numVertices, tris[i][j].B + numVertices, tris[i][j].C + numVertices);
		}

		numVertices += verts[i].size();
		numTriangles += tris[i].size();
	}
}

void MarchingTetrahedraGenerator::GenerateOnCPU()
{
	std::vector<FVector> verts;
	std::vector<FIndex3i> tris;

	FGridCell gridCell;
	FVector3i gridIndex = FVector3i(0,0,0);
	
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
}

void MarchingTetrahedraGenerator::InitialiseGridCell(FGridCell& gridCell, const UE::Geometry::FVector3i& gridIndex)
{
	for (int i = 0; i < 8; i++) {
		FVector3i cornerIndex = gridIndex + cubeVertexOrder[i];
		gridCell.positions[i] = FVector3d(cornerIndex.X * gridCellDimensions.X, cornerIndex.Y * gridCellDimensions.Y, cornerIndex.Z * gridCellDimensions.Z);
		gridCell.values[i] = dataGrid.GetElement(gridIndex.X + cubeVertexOrder[i].X, gridIndex.Y + cubeVertexOrder[i].Y, gridIndex.Z + cubeVertexOrder[i].Z);
	}
}

void MarchingTetrahedraGenerator::InitialiseTetrahedron(FTetrahedron& tetra, const FGridCell& gridCell, const int tetraID)
{
	for (size_t i = 0; i < 4; i++)
	{
		tetra.positions[i] = gridCell.positions[tetrahedronList[tetraID][i]];
		tetra.values[i] = gridCell.values[tetrahedronList[tetraID][i]];
	}
}

bool MarchingTetrahedraGenerator::TriangulateGridCell(const FGridCell& gridCell)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (cubeEdgeTable[cubeIndex] == 0) return false;

	// Interpolate edges
	// Less efficient, but delay until tetrahedral level for simplicity
	//TArray<FVector3d> interpolatedEdges = InterpolateVerticesOnEdges(gridCell, cubeIndex);

	// Iterate over each tetrahedron
	FTetrahedron tetra{};
	for (int i = 0; i < 6; i++)
	{
		InitialiseTetrahedron(tetra, gridCell, i);
		bool trianglesAdded = TriangulateTetrahedron(tetra);
	}

	return true;
}

bool MarchingTetrahedraGenerator::TriangulateTetrahedron(const FTetrahedron& tetra)
{
	// Firstly determine the tetrahedrons's unique index based upon which vertices are above/below the isovalue
	int tetraIndex = CalculateTetrahedronIndex(tetra);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (tetrahedronEdgeTable[tetraIndex] == 0) return false;

	// Interpolate edges
	TArray<FVector3d> interpolatedEdges = InterpolateVerticesOnEdges(tetra, tetraIndex);

	GenerateTrianglesFromTetrahedron(tetraIndex, interpolatedEdges);

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

TArray<FVector3d> MarchingTetrahedraGenerator::InterpolateVerticesOnEdges(const FTetrahedron& tetra, const int tetraIndex)
{
	TArray<FVector3d> interpolatedVertices;
	interpolatedVertices.Init(FVector3d::ZeroVector, 6);
	// Iterate over the 6 edges of the tetrahedron
	// If the surface does cross this edge, find the interpolation point, otherwise write a zero vector
	for (int i = 0; i < 6; i++)
	{
		if (tetrahedronEdgeTable[tetraIndex] & 1 << i)
		{
			std::pair<int, int> vertices = tetrahedronVerticesOnEdge[i];
			FVector3d interpolatedPoint = InterpolateEdge(tetra.positions[vertices.first], tetra.positions[vertices.second], tetra.values[vertices.first], tetra.values[vertices.second]);
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
		UE_LOG(LogTemp, Display, TEXT("Rounding Risk, defaulting to vertex 1"))
		return vertex1;
	}

	float interpolant = (isovalue - value1) / (value2 - value1);
	UE_LOG(LogTemp, Display, TEXT("Interpolant = %f"), interpolant)

	return vertex1 + (vertex2 - vertex1) * interpolant;
}

void MarchingTetrahedraGenerator::GenerateTrianglesFromTetrahedron(int tetraIndex, const TArray<FVector3d>& interpolatedVertexList)
{
	// Store the ids of the vertices within the Vertices array
	TArray<int> interpolatedVertexIDs;
	interpolatedVertexIDs.Init(-1, 4);


	// Check at least one triangle is required
	if (tetrahedronTriTable[tetraIndex][2] != -1)
	{
		for (int i = 0; i < 3; i++)
		{
			UE_LOG(LogTemp, Display, TEXT("Appending Vertex: %s"), *(interpolatedVertexList[tetrahedronTriTable[tetraIndex][i]]).ToString());
			interpolatedVertexIDs[i] = generatedMesh.AppendVertex(interpolatedVertexList[tetrahedronTriTable[tetraIndex][i]]);
		}
		generatedMesh.AppendTriangle(interpolatedVertexIDs[0], interpolatedVertexIDs[1], interpolatedVertexIDs[2]);
	}
	else return;

	// Check whether a second triangle is required
	if (tetrahedronTriTable[tetraIndex][3] != -1) 
	{
		// Append the final required vertex from the triangle strip
		interpolatedVertexIDs[3] = generatedMesh.AppendVertex(interpolatedVertexList[tetrahedronTriTable[tetraIndex][3]]);
		// Reverse the direction of the vertices otherwise the triangle will point the wrong way
		generatedMesh.AppendTriangle(interpolatedVertexIDs[3], interpolatedVertexIDs[2], interpolatedVertexIDs[1]);
	}
}
