// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 *
 */
template <typename T>
class TERRAINMANIPULATION_API TArray3D
{
public:
	TArray3D<T>()
	{
		sizeX = 1;
		sizeY = 1;
		sizeZ = 1;
		data.Init(T(), 1);
	}
	TArray3D<T>(int32 sizeX, int32 sizeY, int32 sizeZ, T defaultValue = T())
	{
		this->sizeX = sizeX;
		this->sizeY = sizeY;
		this->sizeZ = sizeZ;
		data.Init(defaultValue, sizeX * sizeY * sizeZ);
	}
	TArray3D<T>(const TArray3D<T> &other) {
		sizeX = other.sizeX;
		sizeY = other.sizeY;
		sizeZ = other.sizeZ;
		data = other.data;
	}
	~TArray3D<T>() = default;

	/// <summary>
	/// Get the element at the specified (x,y,z) coordinates
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	/// <param name="z"></param>
	/// <returns>The value of the data struct at this coordinate</returns>
	T GetElement(int32 x, int32 y, int32 z) const
	{
		return GetElement(GetArrayIndex(x, y, z));
	}
	/// <summary>
	/// Get the element at the specified (x,y,z) coordinates
	/// </summary>
	/// <param name="indices"></param>
	/// <returns>The value of the data struct at this coordinate</returns>
	T GetElement(FIntVector3 indices) const 
	{
		return GetElement(GetArrayIndex(indices.X, indices.Y, indices.Z));
	}
	/// <summary>
	/// Get an element of the data struct
	/// </summary>
	/// <param name="arrayIndex">The index of the data point in the internal TArray</param>
	/// <returns>The value of the data struct at this index</returns>
	T GetElement(int32 arrayIndex) const 
	{
		return data[arrayIndex];
	}

	TArray<T>& GetRawDataStruct() const 
	{
		return data;
	}

	/// <summary>
	/// Set the value of the element at the specified (x,y,z) coordinates
	/// </summary>
	/// <param name="x"></param>
	/// <param name="y"></param>
	/// <param name="z"></param>
	/// <param name="value">The value to be set</param>
	void SetElement(int32 x, int32 y, int32 z, T value) 
	{
		SetElement(GetArrayIndex(x, y, z), value);
	}
	/// <summary>
	/// Set the value of the element at the specified (x,y,z) coordinates
	/// </summary>
	/// <param name="indices"></param>
	/// <param name="value">The value to be set</param>
	void SetElement(FIntVector3 indices, T value) 
	{
		SetElement(GetArrayIndex(indices.X, indices.Y, indices.Z), value);
	}
	/// <summary>
	/// Set the value of the element at the specified array index
	/// </summary>
	/// <param name="arrayIndex">The index of the data point in the internal TArray</param>
	/// <param name="value">The value to be set</param>
	void SetElement(int32 arrayIndex, T value) 
	{
		data[arrayIndex] = value;
	}

	/// <summary>
	/// Convert 3D coordinates into the single array index using little-endian notation
	/// </summary>
	/// <param name="x">Coordinate 0 of the array</param>
	/// <param name="y">Coordinate 1 of the array</param>
	/// <param name="z">Coordinate 2 of the array</param>
	/// <returns>The index of the internal array for the corresponding element</returns>
	int32 GetArrayIndex(int32 x, int32 y, int32 z) const
	{
		if (x >= sizeX || y >= sizeY || z >= sizeZ) 
		{
			UE_LOG(LogTemp, Fatal, TEXT("Index out of size of the array. Expected index less than size, recieved: (%d, %d, %d)"), x, y, z);
			return -1;
		}

		int32 xComponent = x;
		int32 yComponent = y * sizeX;
		int32 zComponent = z * sizeX * sizeY;
		return zComponent + yComponent + xComponent;
	}
	/// <summary>
	/// Convert internal array reference into grid coordinates
	/// </summary>
	/// <param name="index">The index of the internal array</param>
	/// <returns>A FIntVector3 of the three grid coordinates</returns>
	FIntVector3 GetGridReference(int32 index) const 
	{
		int32 x = index % sizeX;
		int32 y = (index / sizeX) % sizeY;
		int32 z = (index / (sizeX * sizeY)) % sizeZ;
		return FIntVector3(x, y, z);
	}

	int32 GetSize(int32 coordinateAxis) const 
	{
		switch (coordinateAxis) 
		{
		case 0:
			return sizeX;
		case 1:
			return sizeY;
		case 2:
			return sizeZ;
		default:
			return 0;
		}
	}

	bool IsValidIndex(int32 x, int32 y, int32 z) const 
	{
		if (x < 0 || x >= sizeX) return false;
		if (y < 0 || y >= sizeY) return false;
		if (z < 0 || z >= sizeZ) return false;

		return true;
	}

private:

	TArray<T> data;
	int32 sizeX, sizeY, sizeZ;
};
