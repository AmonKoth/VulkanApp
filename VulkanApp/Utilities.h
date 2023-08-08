#pragma once

//Locations (Indices) of Queue Families (if they exists at all)
struct QueueFamilyIndices {
	int graphicsFamily = -1;

	bool isValid()
	{
		return graphicsFamily >= 0;
	}
};