// Fill out your copyright notice in the Description page of Project Settings.


#include "TerrainManipulation/DynamicTerrain/GridCell.h"

GridCell::GridCell()
{
	positions.Init(FVector::ZeroVector, 8);
	values.Init(0, 8);
}

GridCell::GridCell(TArray<FVector> positions, TArray<float> values)
{
	if (positions.Num() != 8 || values.Num() != 8) {
		UE_LOG(LogTemp, Fatal, TEXT("Failed to initialise GridCell. Length of arrays was not 8"));
	}

	this->positions = positions;
	this->values = values;
}

GridCell::~GridCell()
{
}
