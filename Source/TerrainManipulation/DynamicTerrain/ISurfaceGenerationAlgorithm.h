// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TerrainManipulation/DataStructs/TArray3D.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Components/DynamicMeshComponent.h"

/**
 * 
 */
class TERRAINMANIPULATION_API ISurfaceGenerationAlgorithm
{
public:
	virtual ~ISurfaceGenerationAlgorithm();

	/// <summary>
	/// Generate the mesh using compute shaders on the GPU
	/// </summary>
	/// <param name="dynamicMesh">The DynamicMeshComponent that will receive the new mesh</param>
	virtual void GenerateOnGPU(UDynamicMeshComponent* dynamicMesh) = 0;

	/// <summary>
	/// Generate the mesh with linear computation on the CPU
	/// </summary>
	virtual UE::Geometry::FDynamicMesh3 GenerateOnCPU() = 0;

	// The 3D array of data that informs the shape of the isosurface
	TArray3D<float> dataGrid;

	// The value at which the surface shall be drawn
	float isovalue = 0;

	// The length of the grid cell along each local axis
	FVector3f gridCellDimensions;

	// The position of the (0,0,0) vertex in relation to the UDynamicMeshComponent
	FVector3f zeroCellOffset;

	// The mesh that shall be returned after the algorithm is complete
	UE::Geometry::FDynamicMesh3 generatedMesh = UE::Geometry::FDynamicMesh3::FDynamicMesh3();
};
