#include <windows.h>
#include <thread>
#include <iostream>
#include "glew\glew.h"
#include "freeglut\freeglut.h"

GLfloat r = 0.0f, g = 0.0f, b = 0.0f;
const int WIDTH = 1024, HEIGHT = 768;

bool cell[WIDTH][HEIGHT] = { {false} };

void init() {
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	gluOrtho2D(-0.5f, WIDTH - 0.5f, -0.5f, HEIGHT - 0.5f);
}

//Seperate the grid in sections for each thread.
void setup(int x, int y, int m) {
	int h = (m * x) + 2;
	int w = (m * y) + 2;
	for (int i = (h - x); i < h; i++) {
		for (int j = (w - y); j < w; j++) {
			//Initialize each pixel with an arbitry alive/dead value.
			cell[i][j] = (rand() % 2);
		}
	}
}

void changeColor(GLfloat red, GLfloat green, GLfloat blue) {
	r = red;
	g = green;
	b = blue;
}

//Check status of individual cell and apply the game rules.
static boolean checkStatus(bool status, int x, int y) {
	int liveNeighbours = 0;

	for (int i = (x - 1); i < (x + 2); i++) {
		if (cell[i][y - 1] == true) {
			liveNeighbours++;
		}
		if (cell[i][y + 1] == true) {
			liveNeighbours++;
		}
	}
	if (cell[x - 1][y] == true) {
		liveNeighbours++;
	}
	if (cell[x + 1][y] == true) {
		liveNeighbours++;
	}

	if (status == true) {
		if (liveNeighbours < 2) {
			status = false;
		}
		else if (liveNeighbours == 2 || liveNeighbours == 3) {
			status = true;
		}
		else if (liveNeighbours > 3) {
			status = false;
		}
	}
	else {
		if (liveNeighbours == 3) {
			status = true;
		}
	}
	return status;
}

//Display individual pixels.
static void display()
{
	glClear(GL_COLOR_BUFFER_BIT);

	GLfloat red, green, blue;

	for (int i = 5; i < (WIDTH - 5); i++) {
		for (int j = 5; j < (HEIGHT - 5); j++) {
			//Check the updated status of the current cell.
			//If alive, fill in pixel with color. If dead, color value is 0.
			bool alive = checkStatus(cell[i][j], i, j);
			if (alive == true) {
				red = r;
				green = g;
				blue = b;
				cell[i][j] = true;
			}
			else {
				red = 0.0f;
				green = 0.0f;
				blue = 0.0f;
				cell[i][j] = false;
			}
			glPointSize(1.0f);
			glColor3f(red, green, blue);
			glBegin(GL_POINTS);
			glVertex2i(i, j);
			glEnd();
		}
	}
	glutSwapBuffers();
}

void update(int value) {
	glutPostRedisplay();
	glutTimerFunc(10.00, update, 0);
}

void task(int x, int y, int m, GLfloat r, GLfloat g, GLfloat b) {
	setup(x, y, m);
	glutDisplayFunc(display);
	glutTimerFunc(0, update, 0);
	changeColor(r, g, b);
}

int main(int argc, char** argv)
{

	int x = 200;
	int y = 150;
	int mult = 1;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutCreateWindow("Demo OPEN GL");
	init();
	setup(x, y, mult);
	glutDisplayFunc(display);
	glutTimerFunc(0, update, 0);
	changeColor(0.0f, 1.0f, 0.0f);

	glutMainLoop();
	return 0;
}



