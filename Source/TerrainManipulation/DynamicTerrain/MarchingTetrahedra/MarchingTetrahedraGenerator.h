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

	// The length of the grid cell along each local axis
	FVector3d gridCellDimensions;

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
		FVector3d positions[8];    // positions of corners of cell
		double values[8];      // field values at corners
	};

	// Pass Tetrahedra around to simplify the GridCell even further
	struct FTetrahedron
	{
		FVector3d positions[4];    // positions of corners of cell
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
	/// <param name="gridCell"></param>
	/// <param name="tetraID"></param>
	void InitialiseTetrahedron(FTetrahedron& tetra, const FGridCell& gridCell, const int tetraID);

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


	TArray<FVector3d> InterpolateVerticesOnEdges(const FGridCell& gridCell, const int cubeIndex);
	TArray<FVector3d> InterpolateVerticesOnEdges(const FTetrahedron& tetra, const int tetraIndex);

	FVector3d InterpolateEdge(FVector3d vertex1, FVector3d vertex2, float value1, float value2);

	void GenerateTrianglesFromTetrahedron(int tetraIndex, const TArray<FVector3d>& interpolatedVertexList);


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
	/// For each edge id, the vertex ids that connect this edge, including all diagonals for marching tetrahedra
	/// </summary>
	const std::pair<int, int> cubeVerticesOnEdge[19] =
	{
		{0, 1}, {1, 2}, {2, 3}, {0, 3},
		{4, 5}, {5, 6}, {6, 7}, {4, 7},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
		{1, 4}, {2, 5}, {2, 7}, {3, 4},
		{0, 2}, {4, 6}, {2, 4}
	};

	/// <summary>
	/// List of edges required for each cube index. Bit 2^i is used to represent whether edge i is required.
	/// </summary>
	const int cubeEdgeTable[256] = {
		0x000000, 0x010109, 0x001203, 0x01130A, 0x056406, 0x04650F, 0x057605, 0x04770C,
		0x00880C, 0x018905, 0x009A0F, 0x019B06, 0x05EC0A, 0x04ED03, 0x05FE09, 0x04FF00,
		0x069190, 0x079099, 0x068393, 0x07829A, 0x03F596, 0x02F49F, 0x03E795, 0x02E69C,
		0x06199C, 0x071895, 0x060B9F, 0x070A96, 0x037D9A, 0x027C93, 0x036F99, 0x026E90,
		0x002230, 0x012339, 0x003033, 0x01313A, 0x054636, 0x04473F, 0x055435, 0x04553C, 
		0x00AA3C, 0x01AB35, 0x00B83F, 0x01B936, 0x05CE3A, 0x04CF33, 0x05DC39, 0x04DD30, 
		0x06B3A0, 0x07B2A9, 0x06A1A3, 0x07A0AA, 0x03D7A6, 0x02D6AF, 0x03C5A5, 0x02C4AC, 
		0x063BAC, 0x073AA5, 0x0629AF, 0x0728A6, 0x035FAA, 0x025EA3, 0x034DA9, 0x024CA0, 
		0x020460, 0x030569, 0x021663, 0x03176A, 0x076066, 0x06616F, 0x077265, 0x06736C, 
		0x028C6C, 0x038D65, 0x029E6F, 0x039F66, 0x07E86A, 0x06E963, 0x07FA69, 0x06FB60, 
		0x0495F0, 0x0594F9, 0x0487F3, 0x0586FA, 0x01F1F6, 0x00F0FF, 0x01E3F5, 0x00E2FC, 
		0x041DFC, 0x051CF5, 0x040FFF, 0x050EF6, 0x0179FA, 0x0078F3, 0x016BF9, 0x006AF0, 
		0x022650, 0x032759, 0x023453, 0x03355A, 0x074256, 0x06435F, 0x075055, 0x06515C, 
		0x02AE5C, 0x03AF55, 0x02BC5F, 0x03BD56, 0x07CA5A, 0x06CB53, 0x07D859, 0x06D950, 
		0x04B7C0, 0x05B6C9, 0x04A5C3, 0x05A4CA, 0x01D3C6, 0x00D2CF, 0x01C1C5, 0x00C0CC, 
		0x043FCC, 0x053EC5, 0x042DCF, 0x052CC6, 0x015BCA, 0x005AC3, 0x0149C9, 0x0048C0, 
		0x0048C0, 0x0149C9, 0x005AC3, 0x015BCA, 0x052CC6, 0x042DCF, 0x053EC5, 0x043FCC, 
		0x00C0CC, 0x01C1C5, 0x00D2CF, 0x01D3C6, 0x05A4CA, 0x04A5C3, 0x05B6C9, 0x04B7C0, 
		0x06D950, 0x07D859, 0x06CB53, 0x07CA5A, 0x03BD56, 0x02BC5F, 0x03AF55, 0x02AE5C, 
		0x06515C, 0x075055, 0x06435F, 0x074256, 0x03355A, 0x023453, 0x032759, 0x022650, 
		0x006AF0, 0x016BF9, 0x0078F3, 0x0179FA, 0x050EF6, 0x040FFF, 0x051CF5, 0x041DFC, 
		0x00E2FC, 0x01E3F5, 0x00F0FF, 0x01F1F6, 0x0586FA, 0x0487F3, 0x0594F9, 0x0495F0, 
		0x06FB60, 0x07FA69, 0x06E963, 0x07E86A, 0x039F66, 0x029E6F, 0x038D65, 0x028C6C, 
		0x06736C, 0x077265, 0x06616F, 0x076066, 0x03176A, 0x021663, 0x030569, 0x020460, 
		0x024CA0, 0x034DA9, 0x025EA3, 0x035FAA, 0x0728A6, 0x0629AF, 0x073AA5, 0x063BAC, 
		0x02C4AC, 0x03C5A5, 0x02D6AF, 0x03D7A6, 0x07A0AA, 0x06A1A3, 0x07B2A9, 0x06B3A0, 
		0x04DD30, 0x05DC39, 0x04CF33, 0x05CE3A, 0x01B936, 0x00B83F, 0x01AB35, 0x00AA3C, 
		0x04553C, 0x055435, 0x04473F, 0x054636, 0x01313A, 0x003033, 0x012339, 0x002230, 
		0x026E90, 0x036F99, 0x027C93, 0x037D9A, 0x070A96, 0x060B9F, 0x071895, 0x06199C, 
		0x02E69C, 0x03E795, 0x02F49F, 0x03F596, 0x07829A, 0x068393, 0x079099, 0x069190, 
		0x04FF00, 0x05FE09, 0x04ED03, 0x05EC0A, 0x019B06, 0x009A0F, 0x018905, 0x00880C, 
		0x04770C, 0x057605, 0x04650F, 0x056406, 0x01130A, 0x001203, 0x010109, 0x000000

	};
};
