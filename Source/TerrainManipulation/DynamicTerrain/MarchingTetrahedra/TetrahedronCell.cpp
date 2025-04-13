// Fill out your copyright notice in the Description page of Project Settings.


#include "TetrahedronCell.h"

TetrahedronCell::TetrahedronCell()
{
	positions.Init(FVector::ZeroVector, 4);
	values.Init(0, 4);
}

TetrahedronCell::TetrahedronCell(TArray<FVector> positions, TArray<float> values)
{
	if (positions.Num() != 4 || values.Num() != 4) {
		UE_LOG(LogTemp, Fatal, TEXT("Failed to initialise TetrahedronCell. Length of arrays was not 4"));
	}

	this->positions = positions;
	this->values = values;
}

TetrahedronCell::~TetrahedronCell()
{
}
