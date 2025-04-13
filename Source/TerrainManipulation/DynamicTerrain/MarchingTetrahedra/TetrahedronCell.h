// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class TERRAINMANIPULATION_API TetrahedronCell
{
public:
	TetrahedronCell();
	TetrahedronCell(TArray<FVector> positions, TArray<float> values);
	~TetrahedronCell();

	TArray<FVector> positions;
	TArray<float> values;
};
