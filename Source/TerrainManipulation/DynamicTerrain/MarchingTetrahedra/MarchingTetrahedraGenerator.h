// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"

/**
 *
 */
class TERRAINMANIPULATION_API MarchingTetrahedraGenerator : public UE::Geometry::FMeshShapeGenerator
{
public:
	MarchingTetrahedraGenerator();
	~MarchingTetrahedraGenerator();

protected:
	/// <summary>
	/// List of edges required for each tetrahedron index. Bit 2^i is used to represent whether edge i is required.
	/// </summary>
	const int edgeTable[16] =
	{
		0,	13,	19,	30,	38,	43,	53,	56,
		56,	53,	43,	38,	30,	19,	13,	0
	};

	/// <summary>
	/// For each edge id, the vertex ids that connect this edge
	/// </summary>
	const std::pair<int, int> verticesOnEdge[6] =
	{
		{0, 1}, {1, 2}, {0, 2},
		{0, 3}, {1, 3}, {2, 3}
	};

	/// <summary>
	/// For each tetrahedron index, the edges required to create the valid isosurface. The tables are listed in triangle strips. All are tail-ended by -1 to mark where the necessary triangles finish
	/// </summary>
	const int triTable[16][5] =
	{
		{-1, -1, -1, -1, -1},
		{0, 3, 2, -1, -1},
		{0, 1, 4, -1, -1},
		{1, 2, 5, -1, -1},
		{1, 4, 3, 2, -1},
		{0, 3, 5, 1, -1},
		{0, 2, 5, 4, -1},
		{3, 5, 4, -1, -1},
		{4, 5, 3, -1, -1},
		{4, 5, 2, 0, -1},
		{1, 5, 3, 0, -1},
		{2, 3, 4, 1, -1},
		{5, 2, 1, -1, -1},
		{4, 1, 0, -1, -1},
		{2, 3, 0, -1, -1},
		{-1, -1, -1, -1, -1},

	};
};
