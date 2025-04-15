// Fill out your copyright notice in the Description page of Project Settings.


#include "Dynamic_Terrain.h"
#include "MarchingCubes/MarchingCubesGenerator.h"
#include "MarchingTetrahedra/MarchingTetrahedraGenerator.h"
#include "Math/UnrealMathUtility.h"
#include <memory>

// Sets default values
ADynamic_Terrain::ADynamic_Terrain()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	dynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Dynamic Mesh"), false);
	SetRootComponent(dynamicMesh);

	//InitialiseDataGrid();
	//MarchingTetrahedraGenerator marchingTetrahedra = MarchingTetrahedraGenerator(dataGrid);
	//marchingTetrahedra.isovalue = -3.5;
	//FDynamicMesh3 mesh = marchingTetrahedra.Generate();
	//FDynamicMesh3 mesh;
	//mesh.AppendVertex(FVector3d());
	//mesh.AppendVertex(FVector3d(100,0,0));
	//mesh.AppendVertex(FVector3d(100,-100,0));
	//mesh.AppendTriangle(0, 1, 2);
	//mesh.AppendTriangle(2, 1, 0);
	//dynamicMesh->SetMesh(MoveTemp(mesh));

	gridPointCount = FIntVector3(5, 5, 5);
	bottomLeftAnchor = FVector::Zero();
	topRightAnchor = FVector(200, 200, 200);

	isovalue = 0;
}

// Called when the game starts or when spawned
void ADynamic_Terrain::BeginPlay()
{
	Super::BeginPlay();

	InitialiseDataGrid();

	dynamicMesh = Cast<UDynamicMeshComponent>(RootComponent);
	auto mesh = RegenerateByHand();
	UpdateMesh(mesh);

	//CalculateMesh();
}

// Called every frame
void ADynamic_Terrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ADynamic_Terrain::CalculateMesh()
{
	/* Marching Cubes Method
	std::unique_ptr<ISurfaceGenerationAlgorithm> marchingCubes = std::make_unique<MarchingCubesGenerator>();
	double sizeX = (topRightAnchor.X - bottomLeftAnchor.X) / (gridPointCount.X - 1);
	double sizeY = (topRightAnchor.Y - bottomLeftAnchor.Y) / (gridPointCount.Y - 1);
	double sizeZ = (topRightAnchor.Z - bottomLeftAnchor.Z) / (gridPointCount.Z - 1);
	TArray<FVector> vertices = marchingCubes->RunAlgorithm(dataGrid, FVector(sizeX, sizeY, sizeZ), bottomLeftAnchor, isovalue);

	TArray<int32> triangles;
	triangles.Init(0, vertices.Num());
	for (size_t i = 0; i < triangles.Num(); i++)
	{
		triangles[i] = i;
	}

	dynamicMesh->CreateMeshSection(0, vertices, triangles, {}, {}, {}, {}, true);
	*/

	MarchingTetrahedraGenerator marchingTetrahedra = MarchingTetrahedraGenerator(dataGrid);
	
	FDynamicMesh3 mesh = marchingTetrahedra.Generate();

	if (dynamicMesh != nullptr) {
		dynamicMesh->SetMesh(MoveTemp(mesh));
	}
	else {
		UE_LOG(LogTemp, Display, TEXT("dynamicMesh is nullptr"))
	}
}

FVector ADynamic_Terrain::GetLocalPositionOfGridPoint(int x, int y, int z) const
{
	// Get the x,y,z coordinates to scale between 0 (bottom left) and 1 (top right)
	double interpolateX = (double)x / (gridPointCount.X - 1);
	double interpolateY = (double)y / (gridPointCount.Y - 1);
	double interpolateZ = (double)z / (gridPointCount.Z - 1);

	double resultX = bottomLeftAnchor.X + ((topRightAnchor.X - bottomLeftAnchor.X) * interpolateX);
	double resultY = bottomLeftAnchor.Y + ((topRightAnchor.Y - bottomLeftAnchor.Y) * interpolateY);
	double resultZ = bottomLeftAnchor.Z + ((topRightAnchor.Z - bottomLeftAnchor.Z) * interpolateZ);
	return FVector(resultX, resultY, resultZ);
}

FVector ADynamic_Terrain::GetLocalPositionOfGridPoint(FIntVector vertexCoords) const
{
	return GetLocalPositionOfGridPoint(vertexCoords.X, vertexCoords.Y, vertexCoords.Z);
}

float ADynamic_Terrain::GetValueOfDataGrid(const FIntVector& vertexCoords) const
{
	return dataGrid.GetElement(vertexCoords);
}

void ADynamic_Terrain::AddToDataGridInRadius(FVector centre, float radius, float valueToAdd)
{
	// Translate from world coordinates to local coordinates
	FTransform transform = RootComponent->GetComponentTransform();
	FVector transformedUnscaledGridCoordinates = transform.InverseTransformPosition(centre);

	double gridSizeX = (topRightAnchor.X - bottomLeftAnchor.X) / (gridPointCount.X - 1);
	double gridSizeY = (topRightAnchor.Y - bottomLeftAnchor.Y) / (gridPointCount.Y - 1);
	double gridSizeZ = (topRightAnchor.Z - bottomLeftAnchor.Z) / (gridPointCount.Z - 1);

	// Scale local coordinates into grid coordinates
	FVector scaledGridCoordinates = FVector(transformedUnscaledGridCoordinates.X / gridSizeX, transformedUnscaledGridCoordinates.Y / gridSizeY, transformedUnscaledGridCoordinates.Z / gridSizeZ);

	// Translation into grid coordinates may make the sphere an ellipse if grid scales are not identical, so calculate axes lengths
	double rX = radius / gridSizeX;
	double rY = radius / gridSizeY;
	double rZ = radius / gridSizeZ;

	// Iterate over integers in the rectangle surrounding the ellipse and check if it is inside the radius
	for (int32 x = FMath::CeilToInt32(scaledGridCoordinates.X - rX); x <= FMath::CeilToInt32(scaledGridCoordinates.X + rX); x++) 
	{
		if (x < 0 || x >= dataGrid.GetSize(0)) {
			// Invalid coordinate, skip this row
			continue;
		}
		for (int32 y = FMath::CeilToInt32(scaledGridCoordinates.Y - rY); y <= FMath::CeilToInt32(scaledGridCoordinates.Y + rY); y++)
		{
			if (y < 0 || y >= dataGrid.GetSize(1)) {
				// Invalid coordinate, skip this row
				continue;
			}
			for (int32 z = FMath::CeilToInt32(scaledGridCoordinates.Z - rZ); z <= FMath::CeilToInt32(scaledGridCoordinates.Z + rZ); z++) 
			{
				if (z < 0 || z >= dataGrid.GetSize(2)) {
					// Invalid coordinate, skip this row
					continue;
				}

				double normalisedDistanceFromCentre = FMath::Square((x - scaledGridCoordinates.X) / rX) + FMath::Square((y - scaledGridCoordinates.Y) / rY) + FMath::Square((z - scaledGridCoordinates.Z) / rZ);
				if (normalisedDistanceFromCentre <= 1) 
				{
					// The object is inside the ellipse, so add to the value
					float currentVal = dataGrid.GetElement(x, y, z);
					dataGrid.SetElement(x, y, z, currentVal + valueToAdd);
				}
			}
		}
	}
}

void ADynamic_Terrain::InitialiseDataGrid()
{
	dataGrid = TArray3D<float>(gridPointCount.X, gridPointCount.Y, gridPointCount.Z);

	for (int i = 0; i < gridPointCount.X; i++)
	{
		for (int j = 0; j < gridPointCount.Y; j++)
		{
			for (int k = 0; k < gridPointCount.Z; k++)
			{
				//dataGrid.SetElement(i, j, k, FMath::RandRange(0, 1));
				dataGrid.SetElement(i, j, k, -FMath::Sqrt((double)FMath::Square(i) + FMath::Square(j) + FMath::Square(k)));
			}
		}
	}
}

UE::Geometry::FDynamicMesh3 ADynamic_Terrain::RegenerateByHand()
{
	UE::Geometry::FDynamicMesh3 mesh;
	mesh.EnableAttributes();

	UE::Geometry::FDynamicMeshNormalOverlay* normalsOverlay =
		mesh.Attributes()->PrimaryNormals();

	const TArray<FVector3d> vertices{ FVector3d(-1, -1, -1), FVector3d(1, -1, -1),
							  FVector3d(-1, -1, 1),  FVector3d(1, -1, 1),
							  FVector3d(-1, 1, -1),  FVector3d(1, 1, -1),
							  FVector3d(-1, 1, 1),   FVector3d(1, 1, 1) };

	const TArray<FVector3f> normals{ FVector3f(0, -1, 0), FVector3f(0, 1, 0),
							 FVector3f(-1, 0, 0), FVector3f(1, 0, 0),
							 FVector3f(0, 0, -1), FVector3f(0, 0, 1) };

	int32 id;
	UE::Geometry::EMeshResult result;

	for (auto vertex : vertices) {
		mesh.AppendVertex(vertex * 100);
	}

	for (auto normal : normals) {
		normalsOverlay->AppendElement(normal);
		normalsOverlay->AppendElement(normal);
		normalsOverlay->AppendElement(normal);
	}

	// Face 0, 1. Normal 0
	id = mesh.AppendTriangle(0, 1, 2);
	result = normalsOverlay->SetTriangle(id, { 0, 1, 2 });

	id = mesh.AppendTriangle(3, 1, 2);
	result = normalsOverlay->SetTriangle(id, { 0, 1, 2 });

	// Face 2, 3. Normal 1
	id = mesh.AppendTriangle(4, 5, 6);
	result = normalsOverlay->SetTriangle(id, { 1 * 3, 1 * 3 + 1, 1 * 3 + 2 });

	id = mesh.AppendTriangle(7, 5, 6);
	result = normalsOverlay->SetTriangle(id, { 1 * 3, 1 * 3 + 1, 1 * 3 + 2 });

	// Face 4, 5. Normal 2
	id = mesh.AppendTriangle(4, 6, 0);
	result = normalsOverlay->SetTriangle(id, { 2 * 3, 2 * 3 + 1, 2 * 3 + 2 });

	id = mesh.AppendTriangle(2, 6, 0);
	result = normalsOverlay->SetTriangle(id, { 2 * 3, 2 * 3 + 1, 2 * 3 + 2 });

	// Face 6, 7. Normal 3
	id = mesh.AppendTriangle(1, 5, 3);
	result = normalsOverlay->SetTriangle(id, { 3 * 3, 3 * 3 + 1, 3 * 3 + 2 });

	id = mesh.AppendTriangle(7, 5, 3);
	result = normalsOverlay->SetTriangle(id, { 3 * 3, 3 * 3 + 1, 3 * 3 + 2 });

	// Face 8, 9. Normal 4
	id = mesh.AppendTriangle(0, 1, 4);
	result = normalsOverlay->SetTriangle(id, { 4 * 3, 4 * 3 + 1, 4 * 3 + 2 });

	id = mesh.AppendTriangle(5, 1, 4);
	result = normalsOverlay->SetTriangle(id, { 4 * 3, 4 * 3 + 1, 4 * 3 + 2 });

	// Face 10, 11. Normal 5
	id = mesh.AppendTriangle(2, 3, 6);
	result = normalsOverlay->SetTriangle(id, { 5 * 3, 5 * 3 + 1, 5 * 3 + 2 });

	id = mesh.AppendTriangle(7, 3, 6);
	result = normalsOverlay->SetTriangle(id, { 5 * 3, 5 * 3 + 1, 5 * 3 + 2 });

	// Why is this false?
	const auto validityResult =
		mesh.CheckValidity({}, UE::Geometry::EValidityCheckFailMode::ReturnOnly);

	return mesh;
}

void ADynamic_Terrain::UpdateMesh(UE::Geometry::FDynamicMesh3 mesh)
{
	auto root = Cast<UDynamicMeshComponent>(GetRootComponent());

	if (root) {
		*(root->GetMesh()) = mesh;
		root->NotifyMeshUpdated();
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("No Mesh Component"));
	}
}

