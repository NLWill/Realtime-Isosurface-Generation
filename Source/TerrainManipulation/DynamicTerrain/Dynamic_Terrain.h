#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "TerrainManipulation/DataStructs/TArray3D.h"
#include "Dynamic_Terrain.generated.h"

UCLASS()
class TERRAINMANIPULATION_API ADynamic_Terrain : public AActor
{
	GENERATED_BODY()
	
protected:
	//UPROPERTY(EditAnywhere)
	//UMaterialInterface* terrainMaterial;

	// The anchor point for (0,0,0) on the dataGrid (in local coordinates)
	UPROPERTY(EditAnywhere)
	FVector3f bottomLeftAnchor;

	// The anchor point for the furthest point on the dataGrid (in local coordinates)
	UPROPERTY(EditAnywhere)
	FVector3f topRightAnchor;

	// The number of grid points along each axis
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FIntVector gridPointCount;

	// The isovalue at which the surface will be constructed
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float isovalue;

	// The procedural mesh component containing the mesh data that will be rendered in-game
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UDynamicMeshComponent* dynamicMesh;

	UPROPERTY(EditAnywhere)
	bool enableCollision = true;

	UPROPERTY(EditAnywhere)
	bool useMarchingCubes;

public:
	// Sets default values for this actor's properties
	ADynamic_Terrain();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/// <summary>
	/// Recalculate the isosurface given the current state of the dataGrid
	/// </summary>
	UFUNCTION(BlueprintCallable)
	void CalculateMesh();

	/// <summary>
	/// Get the local position coordinate of the given grid indices, scaling based upon the two anchors.
	/// </summary>
	/// <param name="x">Index along X axis</param>
	/// <param name="y">Index along Y axis</param>
	/// <param name="z">Index along Z axis</param>
	/// <returns>Local position relative to the RootComponent</returns>
	FVector3d GetLocalPositionOfGridPoint(int x, int y, int z) const;
	/// <summary>
	/// Get the local position coordinate of the given grid indices, scaling based upon the two anchors.
	/// </summary>
	/// <param name="vertexCoords">Vector of ints for the X,Y,Z indices</param>
	/// <returns>Local position relative to the RootComponent</returns>
	UFUNCTION(BlueprintPure)
	FVector GetLocalPositionOfGridPoint(FIntVector vertexCoords) const;

	/// <summary>
	/// Get the value of the dataGrid at the specified integer coordinates (in grid-space)
	/// </summary>
	/// <param name="vertexCoords">Vector of ints for the X,Y,Z indices</param>
	/// <returns>The value of the scalar field at this vertex</returns>
	UFUNCTION(BlueprintPure)
	float GetValueOfDataGrid(const FIntVector &vertexCoords) const;

	/// <summary>
	/// Add a value to all data points within a radius of a specified point in world-space
	/// </summary>
	/// <param name="centre">The centre of the sphere</param>
	/// <param name="radius">The radius of the sphere</param>
	/// <param name="valueToAdd">The value to be added to all points in the radius</param>
	UFUNCTION(BlueprintCallable)
	void AddToDataGridInRadius(FVector centre, float radius, float valueToAdd);

private:

	/// <summary>
	/// Initialise the data grid to some structured values for testing
	/// </summary>
	void InitialiseDataGrid();

	UE::Geometry::FDynamicMesh3 RegenerateByHand();
	void UpdateDynamicMesh(UE::Geometry::FDynamicMesh3 mesh);

	TArray3D<float> dataGrid;
};
