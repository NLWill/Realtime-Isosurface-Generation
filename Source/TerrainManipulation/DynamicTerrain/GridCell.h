// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class TERRAINMANIPULATION_API GridCell
{
public:
	GridCell();
	GridCell(TArray<FVector> positions, TArray<float> values);
	~GridCell();

	TArray<FVector> positions;
	TArray<float> values;
};
