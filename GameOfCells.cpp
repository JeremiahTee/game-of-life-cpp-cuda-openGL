//============================================================================
// Author      : Jeremiah Tiongson (40055477)
// Description : 2D Cell Growth simulation assignment for COMP 426.
//============================================================================

#include <windows.h>
#include <thread>
#include <iostream>
#include <vector>
#include <ctime>

#include "glew\glew.h"
#include "freeglut\freeglut.h"

void init(void);
void display(void);
void keyboard(unsigned char, int, int);
void spawnMedecineCells(int radius, int x, int y);
static int checkCellStatus(int type, int x, int y);

// Define needed variables
#define health	0
#define cancer	1
#define med		2

const int WIDTH = 500, HEIGHT = 500;
int cell[WIDTH][HEIGHT] = { { } };
bool canMedCellRadiate;

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

//Separate the grid in sections for each thread.
void setup(int x, int y, int m) {
	int h = (m * x) + 2;
	int w = (m * y) + 2;
	for (int i = (h - x); i < h; i++) {
		for (int j = (w - y); j < w; j++) {
			//Initialize each pixel randomly
			cell[i][j] = rand() % 2;
		}
	}
}

//Spawn Medecine cells within a 'circle'
void spawnMedecineCells(int radius, int x, int y)
{	
	for(int i = 0; i < WIDTH; i++)
	{
		for(int j = 0; j < HEIGHT; j++)
		{
			int a = i - x;
			int b = j - y;

			//The cell at x,y is inside the "circle"
			if(a*a + b*b <= radius * radius)
			{
				cell[i][j] = 2; 
			}
		}
	}
}

//Function that updates the view
static void display()
{
	glClear(GL_COLOR_BUFFER_BIT); //clears window
	glClearColor(50 / 256.0f, 150 / 256.0f, 200 / 256.0f, 1);

	GLfloat red, green, blue;

	//Iterate over cells
	for (int i = 5; i < (WIDTH - 5); i++) {
		for (int j = 5; j < (HEIGHT - 5); j++) {

			switch(checkCellStatus(cell[i][j], i, j))
			{
			case 0:
				red = 1.0f;
				green = 0.0f;
				blue = 0.0f;
				cell[i][j] = 0;
				break;
			case 1:
				red = 0.0f;
				green = 1.0f;
				blue = 0.0f;
				cell[i][j] = 1;
				break;
			case 2:
				if(canMedCellRadiate)
				{
					int direction = rand() % 4;

					switch(direction)
					{
					case 0:
						cell[i + rand() % 2][j + rand() % 2] = 2;
						break;
					case 1:
						cell[i - rand() % 2][j] = 2;
					case 2:
						cell[i][j + rand() % 2] = 2;
					case 3:
						cell[i + rand() % 2][j] = 2;
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
			glVertex2i(i, j);
			glEnd();
		}
	}
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
	// Assign threads to each quadrant in the grid
	//worker_thread[0] = std::thread(workThreadCell, 0, 0, 250, 250);
	//worker_thread[1] = std::thread(workThreadCell, 250, 0, 500, 250);
	//worker_thread[2] = std::thread(workThreadCell, 0 , 250, 250, 500);
	//worker_thread[3] = std::thread(workThreadCell, 250, 250, 500, 500);

	//for (int i = 0; i < 4; i++)
	//	worker_thread[i].join();
	//
	glutPostRedisplay();
	glutTimerFunc((1.0f / 30.0f) * 1000.0f, update, 0);
}

void task(int x, int y, int m, GLfloat r, GLfloat g, GLfloat b) {
	setup(x, y, m);
	glutDisplayFunc(display);
	glutTimerFunc(0, update, 0);
}


int main(int argc, char** argv)
{
	int x = 500;
	int y = 500;
	int mult = 1;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutInitWindowPosition(x, 100);
	glutCreateWindow("2D Cell Growth");
	init();
	setup(x, y, mult);
	glutDisplayFunc(display);
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
		int x, y;
		std::cout << "Enter an x position for the center of the circle (must be lower than 250): " << std::endl;
		std::cin >> x;
		std::cout << "Enter a y position for the center of the circle (must be lower than 250): " << std::endl;
		std::cin >> y;
		spawnMedecineCells(50, x, y);
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
	int cancerNeighbours = 0;
	int medecineNeighbours = 0;
	std::vector<int> medNeighbourX_position;
	std::vector<int> medNeighbourY_position;

	for (int i = (x - 1); i < (x + 2); i++) {
		if (cell[i][y - 1] == 1) {
			cancerNeighbours++;
		}

		if(cell[i][y - 1] == 2){
			medecineNeighbours++;
			medNeighbourX_position.push_back(i);
			medNeighbourY_position.push_back(y - 1);
		}
		
		if (cell[i][y + 1] == 1) {
			cancerNeighbours++;
		}

		if(cell[i][y + 1] == 2) {
			medecineNeighbours++;
			medNeighbourX_position.push_back(i);
			medNeighbourY_position.push_back(y + 1);
		}
	}
	
	if (cell[x - 1][y] == 1) {
		cancerNeighbours++;
	}
	if (cell[x + 1][y] == 1) {
		cancerNeighbours++;
	}

	if (cell[x - 1][y] == 2) {
		medecineNeighbours++;
		medNeighbourX_position.push_back(x - 1);
		medNeighbourY_position.push_back(y);
	}
	if (cell[x + 1][y] == 2) {
		medecineNeighbours++;
		medNeighbourX_position.push_back(x + 1);
		medNeighbourY_position.push_back(y);
	}

	if (type == 0) { //cancer cell rule
		if(medecineNeighbours > 6)
		{
			type = 1; // cured cell

			//Change all neighbor medecine cell to healthy
			for(int i = 0; i < medNeighbourX_position.size(); i++)
			{
				for(int j = 0; j < medNeighbourY_position.size(); j++)
				{
					cell[medNeighbourX_position.front()][medNeighbourY_position.front()];
				}
			}
		}
	}else if(type == 1) //healthy cell rule
	{
		if(cancerNeighbours > 6)
		{
			type = 0; //cell becomes cancerous
		}
	}else //medecine cell can radiate
	{
		if(rand() % 2 == 0)
			canMedCellRadiate = true;
	}
	
	return type;
}