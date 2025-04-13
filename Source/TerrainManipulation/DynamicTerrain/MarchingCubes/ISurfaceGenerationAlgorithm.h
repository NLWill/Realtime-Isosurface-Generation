// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TerrainManipulation/DataStructs/TArray3D.h"

/**
 * 
 */
class TERRAINMANIPULATION_API ISurfaceGenerationAlgorithm
{
public:
	virtual ~ISurfaceGenerationAlgorithm();

	/// <summary>
	/// Run the algorithm over the data points to create a surface of triangles. For the grid data, values <= 0 are outside the surface and > 0 are inside.
	/// </summary>
	/// <param name="gridData">The data instructing the composition of the surface</param>
	/// <param name="bias">The offset to the grid data to adjust where the surface is being constructed</param>
	/// <returns>An array of vertex IDs required to create the triangles of this surface</returns>
	virtual TArray<FVector> RunAlgorithm(const TArray3D<float>& gridData, FVector sizeOfCell, FVector offsetOfZeroCell, float isovalue = 0) = 0;
};
