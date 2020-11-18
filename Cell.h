#pragma once

struct Cell
{
	int x;
	int y;
	int currentState; //0: cancer, 1: health, 2: medecine
	int previousState;
	int direction[1][1];
}; bool operator==(const Cell& cellOne, const Cell& cellTwo) //comparison operator to compare cells
{
	return cellOne.x == cellTwo.x && cellOne.y == cellTwo.y;
}

extern void spawnMedecineCellsCUDA(int radius, int x, int y, int cell[500][500], int medCount);