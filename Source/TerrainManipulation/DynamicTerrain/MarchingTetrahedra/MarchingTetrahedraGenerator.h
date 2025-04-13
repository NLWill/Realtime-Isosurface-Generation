// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"
#include "TerrainManipulation/DataStructs/TArray3D.h"

/**
 *
 */
class TERRAINMANIPULATION_API MarchingTetrahedraGenerator : public UE::Geometry::FMeshShapeGenerator
{
public:
	MarchingTetrahedraGenerator();
	MarchingTetrahedraGenerator(int gridCountX, int gridCountY, int gridCountZ);
	MarchingTetrahedraGenerator(const MarchingTetrahedraGenerator& other);
	~MarchingTetrahedraGenerator();

	// The 3D array of data that informs the shape of the isosurface
	TArray3D<float> dataGrid;

	// The value at which the surface shall be drawn
	float isovalue = 0;

	// Should this generator run in parallel on the GPU?
	bool bGPUCompute = false;

	// Generate the mesh
	virtual UE::Geometry::FMeshShapeGenerator& Generate();

protected:
	/// <summary>
	/// Generate the mesh using compute shaders on the GPU
	/// </summary>
	void GenerateOnGPU();

	/// <summary>
	/// Generate the mesh with linear computation on the CPU
	/// </summary>
	void GenerateOnCPU();

	// Pass GridCells around rather than the whole grid to simplify code
	struct FGridCell
	{
		UE::Geometry::FVector3i indices[8];    // indices of corners of cell
		double values[8];      // field values at corners
	};

	// Pass Tetrahedra around to simplify the GridCell even further
	struct FTetrahedron
	{
		UE::Geometry::FVector3i indices[4];    // indices of corners of cell
		double values[4];      // field values at corners
	};

	/// <summary>
	/// Set the values of the gridCell struct based upon the lowest gridIndex coordinate
	/// </summary>
	/// <param name="gridCell"></param>
	/// <param name="gridIndex"></param>
	void InitialiseGridCell(FGridCell& gridCell, const UE::Geometry::FVector3i& gridIndex);

	/// <summary>
	/// Set the values of the tetrahedron based upon the tetrahedron ID
	/// </summary>
	/// <param name="tetra"></param>
	/// <param name="interpolatedEdges"></param>
	/// <param name="tetraID"></param>
	void InitialiseTetrahedron(FTetrahedron& tetra, const TArray<FVector3d>& interpolatedEdges, const int tetraID);

	/// <summary>
	/// Determine all triangles that will be made from this grid cell
	/// </summary>
	/// <param name="gridCell"></param>
	/// <returns></returns>
	bool TriangulateGridCell(const FGridCell& gridCell);

	/// <summary>
	/// Determine all triangles that will be made from this tetrahedron
	/// </summary>
	/// <param name="tetra"></param>
	/// <returns></returns>
	bool TriangulateTetrahedron(const FTetrahedron& tetra);

	/// <summary>
	/// Calculate the unique index of the cube based upon whether the points fall inside or outside the isovalue
	/// </summary>
	/// <param name="gridCell">An FGridCell struct containing the indexes and values of the 8 vertices in a cube</param>
	/// <returns>The unique identifier describing which vertices are inside/outside the isosurface</returns>
	int CalculateCubeIndex(const FGridCell& gridCell) const;

	/// <summary>
	/// Calculate the unique index of the tetrahedron based upon whether the points fall inside or outside the isovalue
	/// </summary>
	/// <param name="tetra">An FTetrahedron struct containing the indexes and values of the 4 associated vertices</param>
	/// <returns>The unique identifier describing which vertices are inside/outside the isosurface</returns>
	int CalculateTetrahedronIndex(const FTetrahedron& tetra) const;





	const int tetrahedronList[6][4] = {
		{0, 2, 3, 7},
		{0, 6, 2, 7},
		{0, 4, 6, 7},
		{0, 6, 1, 2},
		{0, 1, 6, 4},
		{5, 6, 1, 4}
	};

	/// <summary>
	/// List of edges required for each tetrahedron index. Bit 2^i is used to represent whether edge i is required.
	/// </summary>
	const int tetrahedronEdgeTable[16] =
	{
		0,	13,	19,	30,	38,	43,	53,	56,
		56,	53,	43,	38,	30,	19,	13,	0
	};

	/// <summary>
	/// For each edge id, the vertex ids that connect this edge
	/// </summary>
	const std::pair<int, int> tetrahedronVerticesOnEdge[6] =
	{
		{0, 1}, {1, 2}, {0, 2},
		{0, 3}, {1, 3}, {2, 3}
	};

	/// <summary>
	/// For each tetrahedron index, the edges required to create the valid isosurface. The tables are listed in triangle strips. All are tail-ended by -1 to mark where the necessary triangles finish
	/// </summary>
	const int tetrahedronTriTable[16][5] =
	{
		{-1, -1, -1, -1, -1},
		{0, 3, 2, -1, -1},
		{0, 1, 4, -1, -1},
		{1, 2, 5, -1, -1},
		{1, 4, 2, 3, -1},
		{0, 3, 1, 5, -1},
		{0, 2, 4, 5, -1},
		{3, 5, 4, -1, -1},
		{4, 5, 3, -1, -1},
		{4, 5, 0, 2, -1},
		{1, 5, 0, 3, -1},
		{2, 3, 1, 4, -1},
		{5, 2, 1, -1, -1},
		{4, 1, 0, -1, -1},
		{2, 3, 0, -1, -1},
		{-1, -1, -1, -1, -1},

	};

	/// <summary>
	/// The ordering of the vertices, as defined by Paul Bourke
	/// </summary>
	const UE::Geometry::FVector3i cubeVertexOrder[8] =
	{
		{0, 0, 0},
		{1, 0, 0},
		{1, 1, 0},
		{0, 1, 0},
		{0, 0, 1},
		{1, 0, 1},
		{1, 1, 1},
		{0, 1, 1}
	};

	/// <summary>
	/// List of edges required for each cube index. Bit 2^i is used to represent whether edge i is required.
	/// </summary>
	const int cubeEdgeTable[256] =
	{
		0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
		0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
		0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
		0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
		0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
		0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
		0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
		0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
		0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
		0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
		0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
		0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
		0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
		0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
		0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
		0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
		0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
		0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
		0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
		0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
		0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
		0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
		0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
		0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
		0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
		0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
		0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
		0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
		0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
		0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
		0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
		0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
	};
};
