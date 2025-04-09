// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
template <typename T>
class TERRAINMANIPULATION_API TArray2D
{
public:
	TArray2D<T>();
	TArray2D<T>(int32 sizeX, int32 sizeY, T defaultValue = T());
	~TArray2D<T>() = default;

private:
	TArray<T> data;
	int32 sizeX, sizeY;
};
