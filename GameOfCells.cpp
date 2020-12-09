//============================================================================
// Author      : Jeremiah Tiongson (40055477)
// Description : 2D Cell Growth simulation assignment for COMP 426.
//============================================================================

#include <windows.h>
#include <thread>
#include <iostream>
#include <vector>
#include <ctime>
#include <string>
#include "glew\glew.h"
#include "freeglut\freeglut.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"
#include "tbb/tick_count.h"
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include <CL/cl2.hpp>

using std::string;
using std::cin;
using std::cout;
using std::vector;
using std::uint32_t;
using std::to_string;

void init(void);
void display(void);
void keyboard(unsigned char, int, int);
void drawString(float x, float y, void* font, const char* string);
void updateMedCell(int x, int y);
void spawnCells();
void spawnCancerCells();
void spawnMedecineCells(int radius, int x, int y);
static int checkCellStatus(int type, int x, int y);
void removeVectorCell(int typeOfCurrentCell, int x_pos, int y_pos);

//Define needed variables

const int font = (int)GLUT_BITMAP_HELVETICA_18;
const uint32_t WIDTH = 500, HEIGHT = 500;
int cell[WIDTH][HEIGHT] = { { } }; //Canvas of pixels (cells) 500 x 500
bool canMedCellRadiate;
#define PI 3.14159265

uint32_t cancerCellLimit = WIDTH * HEIGHT * 0.25;

// Define needed OpenCL variables
// GPU compute device id
cl_device_id gpu_device_id;
// GPU compute context
cl_context gpu_context;
// GPU compute command queue
cl_command_queue gpu_commands;
// GPU compute program
cl_program gpu_program;
// GPU compute kernel
cl_kernel gpu_kernel;

// CPU compute device id
cl_device_id cpu_device_id;
// CPU compute context
cl_context cpu_context;
// CPU compute command queue
cl_command_queue cpu_commands;
// GPU compute program
cl_program cpu_program;
// GPU compute kernel
cl_kernel cpu_kernel;

// Error code returned from api calls
int err;

// Device memory used for the input array
cl_mem readQuad;
// Device memory used for the output array
cl_mem writeQuad;

// Device memory used for the input display array
cl_mem colorQuad;

// Global domain size for our calculation
size_t global;
// Local domain size for our calculation
size_t local;

int windowSize = 500 * 500;

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

vector<Cell> healthy_cells;
vector<Cell> cancer_cells;
vector<Cell> medecine_cells;
vector<uint32_t> previousStateBeforeMed;

struct ParallelMedInjection {

	int radius;
	int x;
	int y;
	
	void operator()(const tbb::blocked_range<int>& range) const {
		for (int i = range.begin(); i != range.end(); ++i)
			spawnMedecineCells(radius, x, y);
	};
};

// Threading
std::thread worker_thread[4];
void workThreadCell(int x1, int y1, int x2, int y2);

//Initializes OpenGL
void init() {
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION); //Initialize matrix state
	glLoadIdentity(); //Load the identity matrix
	gluOrtho2D(-0.5f, WIDTH - 0.5f, -0.5f, HEIGHT - 0.5f);
}

void spawnCancerCells()
{
	srand(time(NULL));

	Cell cancer;
	
	for (int j = 0; j < cancerCellLimit; j++)
	{
		cancer_cells.reserve(cancerCellLimit);
		
		uint32_t x_pos = rand() % WIDTH;
		uint32_t y_pos = rand() % HEIGHT;
		if (cell[x_pos][y_pos] == 0) //If the cell already is of Cancer type, skip it
		{
			j--;
		}
		else
		{
			cancer.x = x_pos;
			cancer.y = y_pos;
			cancer.currentState = 0;
			cancer.previousState = 0;
			cell[x_pos][y_pos] = cancer.currentState; //Otherwise make it a cancer cell
			cancer_cells.push_back(cancer);
		}

		cancer_cells.reserve(cancer_cells.size());
		healthy_cells.reserve(healthy_cells.size() - cancer_cells.size());
	}
}

//Separate the grid in sections for each thread.
void spawnCells() {
	Cell healthy;
	
	//Fill the grid with all healthy cells initially
	for (int i = 0; i < WIDTH; i++) {
		for (int j = 0; j < HEIGHT; j++) {
			healthy.x = i;
			healthy.y = j;
			healthy.currentState = 1;
			healthy.previousState = 1;

			healthy_cells.push_back(healthy);
			cell[i][j] = 1;
		}
	}

	//Spawn cancer cells
	spawnCancerCells();
}

//Spawn Medecine cells within a 'circle'
void spawnMedecineCells(int radius, int x, int y)
{
	uint32_t medCount = 0;
	
	srand(time(NULL));
	
	for(uint32_t i = 0; i < WIDTH - 5; i++)
	{
		for(uint32_t j = 0; j < HEIGHT - 5; j++)
		{
			uint32_t a = i - x;
			uint32_t b = j - y;

			//The cell at x,y is inside the circle
			if(a*a + b*b <= radius * radius)
			{
				if (rand() % 4 == 0) 
				{
					Cell med;
					med.x = i;
					med.y = j;
					med.currentState = 2;
					
					cell[i][j] = 2;

					medCount++;
				}
			}
		}
	}
}

//Removes cell from the appropriate vector depending on the cell type and position
void removeVectorCell(int typeOfCurrentCell, int x_pos, int y_pos)
{
	//Temp cell for comparison
	Cell cell;
	cell.x = x_pos;
	cell.y = y_pos;

	if (typeOfCurrentCell == 0) //cancer 
	{
		for (int i = 0; i < cancer_cells.size(); i++)
		{
			if (cancer_cells.at(i) == cell)
			{
				cancer_cells.erase(std::remove(cancer_cells.begin(), cancer_cells.end(), cancer_cells.at(i)), cancer_cells.end());
			}
		}
	}
	else if (typeOfCurrentCell == 1) //healthy
	{
		for (int i = 0; i < healthy_cells.size(); i++)
		{
			if (healthy_cells.at(i) == cell)
			{
				healthy_cells.erase(std::remove(healthy_cells.begin(), healthy_cells.end(), healthy_cells.at(i)), healthy_cells.end());
			}
		}
	}
	else //medecine
	{
		for (int i = 0; i < medecine_cells.size(); i++)
		{
			if (medecine_cells.at(i) == cell)
			{
				medecine_cells.erase(std::remove(medecine_cells.begin(), medecine_cells.end(), medecine_cells.at(i)), medecine_cells.end());
			}
		}
	}
}

//Renders string for stats
void drawString(float x, float y, void* font, const char* string)
{
	const char* c;
	glRasterPos2f(x, y);
	for (c = string; *c != '\0'; c++) {
		glutBitmapCharacter(font, *c);
	}
}

//Methods to check whether a point is inside the section of a circle
bool isWithinRadius(int x, int y, int radius)
{
	return x * x + y * y <= radius * radius;
}

bool isClockWise(int x1, int x2, int y1, int y2)
{
	return -x1 * y2 + x1 * x2 > 0;
}

bool insideSection(int x, int y, int originX, int originY, int sectorStartX1, int sectorStartX2, int sectorEndY1, int sectorEndY2, int radius)
{
	int relativeX = x - originX;
	int relativeY = y - originY;

	return !isClockWise(sectorStartX1, sectorStartX2, relativeX, relativeY)
		&& isClockWise(sectorEndY1, sectorEndY2, relativeX, relativeY)
		&& isWithinRadius(relativeX, relativeY, radius * radius);
}

void updateMedCell(int x, int y)
{
	Cell med;
	med.x = x;
	med.y = y;
	cell[x][y] = 2;
	medecine_cells.push_back(med);
}

/** OpenCL Kernel GPU method **/
const char* KernelCPUSource = "\n\
__kernel void UpdateWithCPU(__global int* colorQuad)\n\
{\n\
	Cell healthy;\n\
	for (int i = 0; i < WIDTH; i++) {\n\
		for (int j = 0; j < HEIGHT; j++) {\n\
			healthy.x = i;\n\
			healthy.y = j;\n\
			healthy.currentState = 1;\n\
			healthy.previousState = 1;\n\
			healthy_cells.push_back(healthy);\n\
			cell[i][j] = 1;\n\
		}\n\
	}\n\
spawnCancerCells();\n\
}\n\
\n""";

/** OpenCL Kernel GPU method for updating cells **/

const char* KernelGPUSource = "\n\
__kernel void UpdateWithGPU(__global int* readQuad, __global int* writeQuad)\n\
{\n\
    /**\n\
    @Desc : Updates each cell state using GPU kernel\n\
    */\n\
canMedCellRadiate = false;\n\
std::uint32_t cancerNeighbours = 0;\n\
std::uint32_t medecineNeighbours = 0;\n\
std::vector<std::uint32_t> medNeighbourX_position;\n\
std::vector<std::uint32_t> medNeighbourY_position;\n\
for (int i = (x - 1); i < (x + 2); i++) {\n if (cell[i][y - 1] == 0)\n\ cancerNeighbours++;\n\
if (cell[i][y - 1] == 2) { \n\
medecineNeighbours++;\n\
medNeighbourX_position.push_back(i)\n\
medNeighbourY_position.push_back(y - 1);\n\
}\n\
if (cell[i][y + 1] == 0)\n\
	erNeighbours++;\n\
if (cell[i][y + 1] == 2) {\n\
		medecineNeighbours++;\n\
		medNeighbourX_position.push_back(i);\n\
		medNeighbourY_position.push_back(y + 1);\n\
	}\n\
}\n\
if (cell[x - 1][y] == 0)\n\
cancerNeighbours++;\n\
\n\
if (cell[x + 1][y] == 0)  \n\
cancerNeighbours++;\n\
if (cell[x - 1][y] == 2) { \n\
	medecineNeighbours++;\n\
	medNeighbourX_position.push_back(x - 1);\n\
	medNeighbourY_position.push_back(y);\n\
}\n\
\n\
if (cell[x + 1][y] == 2) { \n\
	medecineNeighbours++;\n\
	medNeighbourX_position.push_back(x + 1);\n\
	medNeighbourY_position.push_back(y);\n\
}\n\
\n\
if (type == 0) { \n\
	if (medecineNeighbours > 6)\n\
	{\n\
		type = 1;\n\
		removeVectorCell(0, x, y); \n\
\n\
		\n\
		for (int i = 0; i < medNeighbourX_position.size(); i++)\n\
		{\n\
			for (int j = 0; j < medNeighbourY_position.size(); j++)\n\
			{\n\
				Cell healthy;\n\
				healthy.x = medNeighbourX_position.at(i);\n\
				healthy.y = medNeighbourY_position.at(j);\n\
				cell[healthy.x][healthy.y] = 1;\n\
				removeVectorCell(2, i, j); \n\
			}\n\
		}\n\
	}\n\
}\n\
else if (type == 1) \n\
{\n\
	if (cancerNeighbours > 6)\n\
	{\n\
		type = 0; \n\
		removeVectorCell(1, x, y); \n\
	}\n\
}\n\
else \n\
{\n\
	if (rand() % 2 == 0)\n\
return type;\n\
\n\"";

int openCLupdate()
{
	/**
	 @Desc : Helper function for using OpenCL to update cells in parallel. Launches the OpenCL GPU kernel
	 */

	 // Write our initial data set into the read and write array in device memory
	err = clEnqueueWriteBuffer(gpu_commands, readQuad, CL_TRUE, 0, sizeof(int) * windowSize, cell, 0, NULL, NULL);
	err = clEnqueueWriteBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * windowSize, cell, 0, NULL, NULL);
	err = 0;
	err = clSetKernelArg(gpu_kernel, 0, sizeof(cl_mem), &readQuad);
	err |= clSetKernelArg(gpu_kernel, 1, sizeof(cl_mem), &writeQuad);
	err = clGetKernelWorkGroupInfo(gpu_kernel, gpu_device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
	global = windowSize;
	err = clEnqueueNDRangeKernel(gpu_commands, gpu_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
	clFinish(gpu_commands);
	err = clEnqueueReadBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * windowSize, cell, 0, NULL, NULL);

	return err;
}

//Function that updates the view
static void display()
{
	srand(time(NULL));
	glClear(GL_COLOR_BUFFER_BIT); //clears window
	glClearColor(50 / 256.0f, 150 / 256.0f, 200 / 256.0f, 1); //show borders for aesthetic

	GLfloat red, green, blue;

	//Iterate over cells
	for (uint32_t i = 0; i < WIDTH - 5; i++) {
		for (uint32_t j = 0; j < HEIGHT - 5; j++) {

			uint32_t currentCellState = checkCellStatus(cell[i][j], i, j);
			
			switch(currentCellState)
			{
			case 0:
				red = 1.0f;
				green = 0.0f;
				blue = 0.0f;
				cell[i][j] = 0; //set to cancer cell
				break;
			case 1:
				red = 0.0f;
				green = 1.0f;
				blue = 0.0f;
				cell[i][j] = 1; //set to healthy cell
				break;
			case 2:
				if(canMedCellRadiate)
				{
					Cell med;
					
					int direction = rand() % 4;

					switch(direction)
					{
					case 0:
						updateMedCell(i + rand() % 2, j + rand() % 2);
						break;
					case 1:
						updateMedCell(i - rand() % 2, j);
					case 2:
						updateMedCell(i, j + rand() % 2);
					case 3:
						updateMedCell(i + rand() % 2, j);
					}

					//Restore cell to previous state
					if(!previousStateBeforeMed.empty())
					{
						cell[i][j] = previousStateBeforeMed.front(); //start from the front as elements were added as "stack"
						previousStateBeforeMed.erase(previousStateBeforeMed.begin());
					}
				}
					
				red = 1.0f;
				green = 1.0f;
				blue = 0.0f;
				break;
			}
			
			glPointSize(1.0f);
			glColor4f(red, green, blue, 1.0);
			glBegin(GL_POINTS);
			glVertex2f(i, j);
			glEnd();
		}
	}

	std::string health_count = to_string(healthy_cells.size());
	const char* h_c = health_count.c_str();
	std::string cancer_count = to_string(cancer_cells.size());
	const char* c_c = cancer_count.c_str();
	std::string med_count = to_string(medecine_cells.size());
	const char* m_c = med_count.c_str();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glColor3f(0, 0, 0);
	
	// Display the number of each type of cell
	drawString(5, 50, (void*)font, "medecine: ");
	drawString(5, 70, (void*)font, m_c);
	drawString(5, 120, (void*)font, "cancer: ");
	drawString(5, 140, (void*)font, c_c);
	drawString(5, 190, (void*)font, "healthy: ");
	drawString(5, 210, (void*)font, h_c);
	glPopMatrix();
	glutSwapBuffers(); //swaps back buffer to screen buffer
}

void workThreadCell(int initialX, int initialY, int lastX, int lastY)
{	
	for (int i = initialX; i < lastX; i++)
	{
		for (int j = lastY; j < initialY; j++)
		{
			checkCellStatus(cell[i][j], i, j);
		}
	}
}

void update(int value) {
	//Update cells in parallel
	openCLupdate();
	
	glutPostRedisplay();
	glutTimerFunc((1.0f / 30.0f) * 1000.0f, update, 0);
}

void task(int x, int y, GLfloat r, GLfloat g, GLfloat b) {
	spawnCells();
	glutDisplayFunc(display);
	glutTimerFunc(0, update, 0);
}

int main(int argc, char** argv)
{
	int const MAX_PLATFORMS = 10;
	int const MAX_DEVICES = 10;
	char dname[500];
	cl_device_id devices[MAX_DEVICES];
	cl_uint num_devices, entries;
	cl_ulong long_entries;
	int d, platform;
	cl_int err;
	cl_uint num_platforms;
	cl_platform_id platform_id[MAX_PLATFORMS];
	size_t p_size;

	//Query platform info
	err = clGetPlatformIDs(MAX_PLATFORMS, platform_id, &num_platforms);
	if (err != CL_SUCCESS)
	{
		printf("Error: Failed to get clGetPlatformIDs =%d \n", err);
		return 0;

	}
	printf("Found %d platforms \n", num_platforms);

	for (platform = 0; platform < num_platforms; platform++) {
		clGetPlatformInfo(platform_id[platform], CL_PLATFORM_NAME, 500, dname, NULL);
		printf("CL_PLATFORM_NAME = %s\n", dname);
		clGetPlatformInfo(platform_id[platform], CL_PLATFORM_VERSION, 500, dname, NULL);
		printf("CL_PLATFORM_VERSION = %s\n", dname);
		clGetDeviceIDs(platform_id[platform], CL_DEVICE_TYPE_ALL, 10, devices, &num_devices);
		printf("%d devices found\n", num_devices);
	}
	
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutInitWindowPosition(WIDTH, 100);
	glutCreateWindow("2D Cell Growth");
	init();
	spawnCells();
	
	//Assign threads to each quadrant in the grid
	worker_thread[0] = std::thread(workThreadCell, 0, 0, 250, 250);
	worker_thread[1] = std::thread(workThreadCell, 250, 0, 500, 250);
	worker_thread[2] = std::thread(workThreadCell, 0, 250, 250, 500);
	worker_thread[3] = std::thread(workThreadCell, 250, 250, 500, 500);
	
	glutDisplayFunc(display);

	for (int i = 0; i < 4; i++)
		worker_thread[i].join();
	
	glutKeyboardFunc(keyboard);
	glutTimerFunc(0, update, 0);

	glutMainLoop();

	// Cleanup
	clReleaseMemObject(readQuad);
	clReleaseMemObject(writeQuad);
	clReleaseProgram(gpu_program);
	clReleaseProgram(cpu_program);
	clReleaseKernel(gpu_kernel);
	clReleaseKernel(cpu_kernel);
	clReleaseCommandQueue(gpu_commands);
	clReleaseCommandQueue(cpu_commands);
	clReleaseContext(gpu_context);
	clReleaseContext(cpu_context);
	
	return 0;
}

void keyboard(unsigned char key, int x, int y)
{
	tbb::task_scheduler_init init;
	
	//Press 'S'  to spawn medecine cells at user given input
	switch (key)
	{
	case 's':
		int x, y, rad;
		std::cout << "Enter a radius for the circle (recommended 50) " << std::endl;
		std::cin >> rad;
		std::cout << "Enter an x position for the center of the circle (must be lower than 500): " << std::endl;
		std::cin >> x;
		std::cout << "Enter a y position for the center of the circle (must be lower than 500): " << std::endl;
		std::cin >> y;
		
		ParallelMedInjection userInjection;
		userInjection.x = x;
		userInjection.y = y;
		userInjection.radius = rad;
	
		tbb::parallel_for(tbb::blocked_range<int>(1, PI * userInjection.radius * userInjection.radius * 0.25), userInjection);
		
		break;
	//Press 'D' to spawn two group of med cells at (150,150) & (350,350)
	case 'd':
		std::cout << "Spawning two groups of med cells with radius 30 px at (150, 150) and (350, 350) respectively" << std::endl;

		ParallelMedInjection firstInject;
		firstInject.x = 150;
		firstInject.y = 150;
		firstInject.radius = 30;

		ParallelMedInjection secondInject;
		secondInject.x = 350;
		secondInject.y = 350;
		secondInject.radius = 30;
		
		tbb::parallel_for(tbb::blocked_range<int>(1, PI * firstInject.radius * firstInject.radius * 0.25), firstInject);

		tbb::parallel_for(tbb::blocked_range<int>(1, PI * secondInject.radius * secondInject.radius * 0.25), secondInject);
		break;
	}
	//Re-display
	display();
}

//Check status of individual cell and apply the game rules.
static int checkCellStatus(int type, int x, int y) {
	canMedCellRadiate = false;
	std::uint32_t cancerNeighbours = 0;
	std::uint32_t medecineNeighbours = 0;
	std::vector<std::uint32_t> medNeighbourX_position;
	std::vector<std::uint32_t> medNeighbourY_position;

	for (int i = (x - 1); i < (x + 2); i++) {
		if (cell[i][y - 1] == 0) //cell directly below is cancer
			cancerNeighbours++;

		if (cell[i][y - 1] == 2) { //cell directly below is medecine
			medecineNeighbours++;
			medNeighbourX_position.push_back(i);
			medNeighbourY_position.push_back(y - 1);
		}

		if (cell[i][y + 1] == 0) //cell directly above is cancer
			cancerNeighbours++;

		if (cell[i][y + 1] == 2) { //cell directly above is medecine
			medecineNeighbours++;
			medNeighbourX_position.push_back(i);
			medNeighbourY_position.push_back(y + 1);
		}
	}

	if (cell[x - 1][y] == 0)  //cell adjacent to left is cancer
		cancerNeighbours++;

	if (cell[x + 1][y] == 0)  //cell adjacent to the right is cancer
		cancerNeighbours++;

	if (cell[x - 1][y] == 2) { //cell adjacent to the left is medecine
		medecineNeighbours++;
		medNeighbourX_position.push_back(x - 1);
		medNeighbourY_position.push_back(y);
	}

	if (cell[x + 1][y] == 2) { //cell adjacent to the right is medecine
		medecineNeighbours++;
		medNeighbourX_position.push_back(x + 1);
		medNeighbourY_position.push_back(y);
	}

	if (type == 0) { //cancer cell rule
		if (medecineNeighbours > 6)
		{
			type = 1; // it is now a cured cell
			removeVectorCell(0, x, y); //remove cancer cell from the vector since it has been changed to health

			//Change all neighbor med cells to healthy
			for (int i = 0; i < medNeighbourX_position.size(); i++)
			{
				for (int j = 0; j < medNeighbourY_position.size(); j++)
				{
					Cell healthy;
					healthy.x = medNeighbourX_position.at(i);
					healthy.y = medNeighbourY_position.at(j);
					cell[healthy.x][healthy.y] = 1;
					removeVectorCell(2, i, j); //remove med cell from the vector since it has been changed to health
				}
			}
		}
	}
	else if (type == 1) //healthy cell rule
	{
		if (cancerNeighbours > 6)
		{
			type = 0; //cell becomes cancerous
			removeVectorCell(1, x, y); //remove healthy cell from the vector since it has been changed to health
		}
	}
	else //it's a med cell and it can radiate
	{
		if (rand() % 2 == 0) //slow down the "radiation"
			canMedCellRadiate = true;
	}

	return type;
}