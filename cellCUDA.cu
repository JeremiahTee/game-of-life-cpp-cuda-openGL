
#include "cuda_runtime.h"
#include <iostream>

typedef int cell_arr[500];

__global__
void kernel_computeMedecineCells(int radius, int x, int y, cell_arr* cell, int medCount)
{
	for (uint32_t i = 0; i < 500 - 5; i++)
	{
		for (uint32_t j = 0; j < 500 - 5; j++)
		{
			uint32_t a = i - x;
			uint32_t b = j - y;

			//The cell at x,y is inside the circle
			if (a * a + b * b <= radius * radius)
			{
				cell[i][j] = 2;
			}
		}
	}
}

void spawnMedecineCellsCUDA(int radius, int x, int y, int cell[500][500], int medCount)
{
	cell_arr* d_cell;
	size_t size = 500 * 500 * sizeof(int);
	
	//Allocate memory on device
	cudaMalloc(&d_cell, size);

	//Copy cell array from host to the device
	cudaMemcpy(d_cell, cell, size, cudaMemcpyHostToDevice);

	//Run kernel
	kernel_computeMedecineCells << <1,1 >> >(radius, x, y, d_cell, medCount);
	
	// Wait on GPU before accessing host
	cudaDeviceSynchronize();

	//Copy cell array from device to the host
	cudaMemcpy(cell, d_cell, size, cudaMemcpyDeviceToHost);

	//Free allocated memory
	cudaFree(d_cell);
}