// Fill out your copyright notice in the Description page of Project Settings.


#include "TerrainManipulation/DataStructs/TArray2D.h"

template<typename T>
TArray2D<T>::TArray2D()
{
	sizeX = 1;
	sizeY = 1;
	
}

template<typename T>
TArray2D<T>::TArray2D(int32 sizeX, int32 sizeY, T defaultValue)
{
	this->sizeX = sizeX;
	this->sizeY = sizeY;
	data.Init(defaultValue, sizeX * sizeY);
}
