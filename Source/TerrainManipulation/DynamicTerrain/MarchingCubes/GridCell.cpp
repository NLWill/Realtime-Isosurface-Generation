// Fill out your copyright notice in the Description page of Project Settings.


#include "GridCell.h"

GridCell::GridCell()
{
	positions.Init(FVector3f::ZeroVector, 8);
	values.Init(0, 8);
}

GridCell::GridCell(TArray<FVector3f> positions, TArray<float> values) : positions(positions), values(values)
{
	if (positions.Num() != 8 || values.Num() != 8) {
		UE_LOG(LogTemp, Fatal, TEXT("Failed to initialise GridCell. Length of arrays was not 8"));
	}
}

GridCell::~GridCell()
{
}
