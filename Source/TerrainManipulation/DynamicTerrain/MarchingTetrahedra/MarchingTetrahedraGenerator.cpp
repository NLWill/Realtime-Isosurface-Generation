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

MarchingTetrahedraGenerator::MarchingTetrahedraGenerator(const MarchingTetrahedraGenerator& other)
{
	dataGrid = TArray3D<float>(other.dataGrid);
}

MarchingTetrahedraGenerator::~MarchingTetrahedraGenerator()
{
}

FMeshShapeGenerator& MarchingTetrahedraGenerator::Generate()
{
	Reset();

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
	return *this;
}

void MarchingTetrahedraGenerator::GenerateOnGPU()
{
	// vector of positions for each GPU worker group
	std::vector<std::vector<FVector3d>> verts;

	// vector of tri ids, consistent with the equivalent vert group
	// tri ids will need to be scaled by the length of previous vert groups to ensure consistency
	std::vector<std::vector<FIndex3i>> tris;

	// Run the algorithm


	// Set the buffer size 
	int totalVertices = 0, totalTriangles = 0, totalUVs = 0, totalNormals = 0;
	for (size_t i = 0; i < verts.size(); i++)
	{
		totalVertices += verts[i].size();
		totalTriangles += tris[i].size();
	}
	SetBufferSizes(totalVertices, totalTriangles, totalUVs, totalNormals);

	// Populate the vertices and triangles arrays
	int numVertices = 0;
	int numTriangles = 0;
	for (size_t i = 0; i < verts.size(); i++)
	{
		for (size_t j = 0; j < verts[i].size(); j++)
		{
			int vertexID = AppendVertex(verts[i][j]);
		}
		for (size_t j = 0; j < tris[i].size(); j++)
		{
			// Offset the triangle indices by the number of vertices from previous GPU worker groups
			// to ensure the tris still point to the correct vertices
			AppendTriangle(tris[i][j].A + numVertices, tris[i][j].B + numVertices, tris[i][j].C + numVertices);
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

			}
		}
	}
}

void MarchingTetrahedraGenerator::InitialiseGridCell(FGridCell& gridCell, const UE::Geometry::FVector3i& gridIndex)
{
	for (int i = 0; i < 8; i++) {
		gridCell.indices[i] = gridIndex + cubeVertexOrder[i];
		gridCell.values[i] = dataGrid.GetElement(gridCell.indices[i].X, gridCell.indices[i].Y, gridCell.indices[i].Z);
	}
}

void MarchingTetrahedraGenerator::InitialiseTetrahedron(FTetrahedron& tetra, const TArray<FVector3d>& interpolatedEdges, const int tetraID)
{
}

bool MarchingTetrahedraGenerator::TriangulateGridCell(const FGridCell& gridCell)
{
	// Firstly determine the cube's unique index based upon which vertices are above/below the isovalue
	int cubeIndex = CalculateCubeIndex(gridCell);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (cubeEdgeTable[cubeIndex] == 0) return false;

	// Interpolate edges


	// Iterate over each tetrahedron
	FTetrahedron tetra;
	for (int i = 0; i < 6; i++)
	{

	}

	return false;
}

bool MarchingTetrahedraGenerator::TriangulateTetrahedron(const FTetrahedron& tetra)
{
	// Firstly determine the tetrahedrons's unique index based upon which vertices are above/below the isovalue
	int tetraIndex = CalculateTetrahedronIndex(tetra);

	// If all vertices are inside or all outside, no triangles need to be constructed, so return false
	if (tetrahedronEdgeTable[tetraIndex] == 0) return false;

	return false;
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
