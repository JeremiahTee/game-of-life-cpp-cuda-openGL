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
void parallelTBB();
void forcePallelSetup(int threadid);

//Define needed variables

const int font = (int)GLUT_BITMAP_HELVETICA_18;
const uint32_t WIDTH = 500, HEIGHT = 500;
int cell[WIDTH][HEIGHT] = { { } }; //Canvas of pixels (cells) 500 x 500
bool canMedCellRadiate;

uint32_t cancerCellLimit = WIDTH * HEIGHT * 0.25;

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
					medecine_cells.push_back(med);
					previousStateBeforeMed.push_back(cell[i][j]); //store the previous state before it was made a med cell
					cell[i][j] = 2;

					medCount++;
				}
			}
		}
		//Cap the number of med cells for now
		medecine_cells.reserve(medCount);
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

void moveMedCell(int x, int y)
{
	//Move med cell based on its position within the circle

	//Quadrant 1
	
	//Quadrant 2

	//Quadrant 3

	//Quadrant 4
}

void updateMedCell(int x, int y)
{
	Cell med;
	med.x = x;
	med.y = y;
	cell[x][y] = 2;
	medecine_cells.push_back(med);
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
	
	return 0;
}

void keyboard(unsigned char key, int x, int y)
{
	//Press 'S'  to spawn medecine cells
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

		tbb::task_scheduler_init init;
		
		ParallelMedInjection injection;
		injection.x = x;
		injection.y = y;
		injection.radius = rad;
		
		tbb::parallel_for(tbb::blocked_range<int>(1, 20), injection);
		//spawnMedecineCells(rad, x, y);
		
		break;
	case 'r': //Remove all medecine cells
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