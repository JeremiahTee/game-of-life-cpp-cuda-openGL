
#include <iostream>
#include <vector>
//#include <GL\freeglut.h>
//#include <CL/cl.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>
//#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// OpenGL Graphics include
#include <GL\freeglut.h>
// OpenCL include
#include <CL/opencl.h>

// Define states for cells
enum cell { CANCER, HEALTHY, MEDICINE };

// 2D area of 1024 x 768 cells
const int g_windowWidth = 1024;
const int g_windowHeight = 768;
cell g_quad[g_windowWidth][g_windowHeight];

// Update every 1/30th second
const int g_updateTime = 1.0 / 30.0 * 1000.0;

// At least 25% of cells initialized as cancer cells
const int g_initialCancer = g_windowWidth * g_windowHeight * 0.26;

void* g_font = GLUT_BITMAP_TIMES_ROMAN_24;

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

int g_totalSize = (g_windowWidth * g_windowHeight);

const char* KernelGPUSource = "\n\
__kernel void UpdateWithGPU(__global int* readQuad, __global int* writeQuad)\n\
{\n\
    /**\n\
    @Desc : Updates each cell state using GPU kernel\n\
    @param1 : pointer to read array\n\
    @param2 : pointer to write array\n\
    */\n\
    int width = 1024;\n\
    int height = 768;\n\
    int i = get_global_id(0);\n\
    int x = i / height;\n\
    int y = i % height;\n\
    int CANCER = 0;\n\
    int HEALTHY = 1;\n\
    int MEDICINE = 2;\n\
    if (readQuad[x*height + y] == HEALTHY || readQuad[x*height + y] == CANCER) {\n\
        int _numSurrounded = 0;\n\
        int _before = 0;\n\
        int _after = 0;\n\
        // If a healthy cell is surrounded by >= 6 cancer cells,\n\
        // it becomes a cancer cell\n\
        if (readQuad[x*height + y] == HEALTHY) {\n\
            _before = CANCER;\n\
            _after = CANCER;\n\
        }\n\
        // If a cancer cell is surrounded by >= 6 medicine cells,\n\
        // it becomes a healthy cell\n\
        else if (readQuad[x*height + y] == CANCER) {\n\
            _before = MEDICINE;\n\
            _after = HEALTHY;\n\
        }\n\
        // Check the states of the surrounding cells\n\
        if (x > 0 && y > 0) {\n\
            if (readQuad[(x - 1)*height + (y - 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (y > 0) {\n\
            if (readQuad[x*height + (y - 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (x < (width - 1) && y > 0) {\n\
            if (readQuad[(x + 1)*height + (y - 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (x > 0) {\n\
            if (readQuad[(x - 1)*height + y] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (x < (width - 1)) {\n\
            if (readQuad[(x + 1)*height + y] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (x > 0 && y < (height - 1)) {\n\
            if (readQuad[(x - 1)*height + (y + 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (y < (height - 1)) {\n\
            if (readQuad[x*height + (y + 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        if (x < (width - 1) && y < (height - 1)) {\n\
            if (readQuad[(x + 1)*height + (y + 1)] == _before)\n\
                _numSurrounded++;\n\
        }\n\
        // Change state if surrounded by >= 6 of a certain cell\n\
        if (_numSurrounded >= 6) {\n\
            writeQuad[x*height + y] = _after;\n\
        }\n\
    }\n\
}\n\
\n";

int UpdateWithOpenCL()
{
    /**
     @Desc : Helper function for using OpenCL to update cells in parallel. Launches the OpenCL GPU kernel
     */

     // Write our initial data set into the read and write array in device memory
    err = clEnqueueWriteBuffer(gpu_commands, readQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
    err = 0;
    err = clSetKernelArg(gpu_kernel, 0, sizeof(cl_mem), &readQuad);
    err |= clSetKernelArg(gpu_kernel, 1, sizeof(cl_mem), &writeQuad);
    err = clGetKernelWorkGroupInfo(gpu_kernel, gpu_device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
    global = g_totalSize;
    err = clEnqueueNDRangeKernel(gpu_commands, gpu_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
    clFinish(gpu_commands);
    err = clEnqueueReadBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
    /* err = clEnqueueWriteBuffer(gpu_commands, readQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
     if (err != CL_SUCCESS) {
         printf("Error: Failed to write to source array!\n");
         printf("gpu commmand :  %d, readQuad :   %d\n",gpu_commands, readQuad);
         printf("err  :  %d, CL_SUCCESS :   %d\n", err+1, CL_SUCCESS);
         printf("sizeof(int) * g_totalSize  :  %d, CL_TRUE :   %d\n", sizeof(int) * g_totalSize, CL_TRUE);
         printf(" g_quad :  %d, clEnqueueWriteBuffer :   %d\n", g_quad, clEnqueueWriteBuffer(gpu_commands, readQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL));
         exit(1);
     }
     err = clEnqueueWriteBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
     if (err != CL_SUCCESS) {
         printf("Error: Failed to write to source array!\n");
         exit(1);
     }

     // Set the arguments to our compute kernel
     err = 0;
     err = clSetKernelArg(gpu_kernel, 0, sizeof(cl_mem), &readQuad);
     err |= clSetKernelArg(gpu_kernel, 1, sizeof(cl_mem), &writeQuad);
     if (err != CL_SUCCESS) {
         printf("Error: Failed to set kernel arguments! %d\n", err);
         exit(1);
     }

     // Get the maximum work group size for executing the kernel on the device
     err = clGetKernelWorkGroupInfo(gpu_kernel, gpu_device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
     if (err != CL_SUCCESS) {
         printf("Error: Failed to retrieve kernel work group info! %d\n", err);
         exit(1);
     }

     // Execute the kernel over the entire range of our 1D (actually 2D stored as 1D)
     // input data set using the maximum number of work group items for this device
     global = g_totalSize;
     err = clEnqueueNDRangeKernel(gpu_commands, gpu_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
     if (err) {
         printf("Error: Failed to execute kernel!\n");
         return EXIT_FAILURE;
     }

     // Wait for the command commands to get serviced before reading back results
     clFinish(gpu_commands);

     // Read back the results from the device to change the output
     err = clEnqueueReadBuffer(gpu_commands, writeQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
     if (err != CL_SUCCESS) {
         printf("Error: Failed to read output array! %d\n", err);
         exit(1);
     }*/

    return err;
}

void Update(int value)
{
    /**
     @Desc : Function that uses OpenCL to update the cells in parallel, and then calls itself (to update again)
     @param1 : unused parameter that is passed by the glutTimerFunc
     */

     // Update cells in parallel
    UpdateWithOpenCL();

    glutPostRedisplay();
    glutTimerFunc(g_updateTime, Update, 0);
}

void RenderBitmapString(float x, float y, void* font, const char* string)
{
    /**
     @Desc : Renders bitmap strings to display text on screen
     @param1 : x position of where text should be displayed
     @param2 : y position of where text should be displayed
     @param3 : font to be used
     @param4 : string text to be displayed on screen
     */

    const char* c;
    glRasterPos2f(x, y);
    for (c = string; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

const char* KernelCPUSource = "\n\
__kernel void UpdateWithCPU(__global int* colorQuad)\n\
{\n\
    /**\n\
    @Desc : Updates each the display using CPU kernel\n\
    @param1 : pointer to color array\n\
    */\n\
    int width = 1024;\n\
    int height = 768;\n\
    int i = get_global_id(0);\n\
    int x = i / height;\n\
    int y = i % height;\n\
    int CANCER = 0;\n\
    int HEALTHY = 1;\n\
    int MEDICINE = 2;\n\
}\n\
\n";

int UpdateDisplayWithOpenCL()
{
    /**
     @Desc : Helper function for using OpenCL to update display in parallel. Launches the OpenCL CPU kernel
     */

     // Write our initial data set into the read array in device memory
    err = clEnqueueWriteBuffer(cpu_commands, colorQuad, CL_TRUE, 0, sizeof(int) * g_totalSize, g_quad, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to write to source array!\n");
        exit(1);
    }

    // Set the arguments to our compute kernel
    err = 0;
    err = clSetKernelArg(cpu_kernel, 0, sizeof(cl_mem), &colorQuad);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to set kernel arguments! %d\n", err);
        exit(1);
    }

    // Get the maximum work group size for executing the kernel on the device
    err = clGetKernelWorkGroupInfo(cpu_kernel, cpu_device_id, CL_KERNEL_WORK_GROUP_SIZE, sizeof(local), &local, NULL);
    if (err != CL_SUCCESS) {
        printf("Error: Failed to retrieve kernel work group info! %d\n", err);
        exit(1);
    }

    // Execute the kernel over the entire range of our 1D (actually 2D stored as 1D)
    // input data set using the maximum number of work group items for this device
    global = g_totalSize;
    err = clEnqueueNDRangeKernel(cpu_commands, cpu_kernel, 1, NULL, &global, &local, 0, NULL, NULL);
    if (err) {
        printf("Error: Failed to execute kernel!\n");
        return EXIT_FAILURE;
    }

    // Wait for the command commands to get serviced before reading back results
    clFinish(cpu_commands);

    return err;
}

void Display()
{
    /**
     @Desc : Displays the cells and text in a window on screen
     */

     //UpdateDisplayWithOpenCL();

     // Display the cells using OpenGL
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, g_windowWidth, g_windowHeight, 0);

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    int _healthyCount = 0;
    int _cancerCount = 0;
    int _medicineCount = 0;
    for (int x = 0; x < g_windowWidth; x++)
    {
        for (int y = 0; y < g_windowHeight; y++)
        {
            if (g_quad[x][y] == HEALTHY)
            {
                // Healthy cells are green
                glColor3f(0, 1, 0);
                _healthyCount++;
            }
            else if (g_quad[x][y] == CANCER)
            {
                // Cancer cells are red
                glColor3f(1, 0, 0);
                _cancerCount++;
            }
            else if (g_quad[x][y] == MEDICINE)
            {
                // Medicine cells are yellow
                glColor3f(1, 1, 0);
                _medicineCount++;
            }
            glVertex2f(x, y);
            glVertex2f(x + 1, y);
            glVertex2f(x + 1, y + 1);
            glVertex2f(x, y + 1);
        }
    }
    glEnd();

    std::string _hCount = std::to_string(static_cast<long long>(_healthyCount));
    const char* _hc = _hCount.c_str();
    std::string _cCount = std::to_string(static_cast<long long>(_cancerCount));
    const char* _cc = _cCount.c_str();
    std::string _mCount = std::to_string(static_cast<long long>(_medicineCount));
    const char* _mc = _mCount.c_str();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(0, 0, 0);
    // Display the number of each type of cell
    RenderBitmapString(0, 30, g_font, "Healthy: ");
    RenderBitmapString(0, 50, g_font, _hc);
    RenderBitmapString(0, 100, g_font, "Cancer: ");
    RenderBitmapString(0, 120, g_font, _cc);
    RenderBitmapString(0, 170, g_font, "Medicine: ");
    RenderBitmapString(0, 190, g_font, _mc);
    glPopMatrix();

    glutSwapBuffers();
}

void Initialize()
{
    /**
     @Desc : Initialization function for glut
     */

    glMatrixMode(GL_PROJECTION);
    glViewport(0, 0, g_windowWidth, g_windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat aspect = (GLfloat)g_windowWidth / g_windowHeight;
    gluPerspective(45, aspect, 0.1f, 10.0f);
    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void MouseClicks(int button, int state, int x, int y)
{
    /**
     @Desc : Function that handles mouse buttons being clicked
     @param1 : mouse button that was clicked
     @param2 : state of button that was clicked
     @param3 : x position of pointer when mouse was clicked
     @param4 : y position of pointer when mouse was clicked
     */

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // If medicine is injected on a cancer cell,
        // the medicine is absorbed and the cell turns into a healthy cell
        if (g_quad[x][y] == CANCER) {
            g_quad[x][y] = HEALTHY;
        }
        // If medicine is injected on a healthy or medicine cell,
        // the medicine is not absorbed and propagates radially outwards by one cell
        else {
            g_quad[x][y] = MEDICINE;
            if (x > 0 && y > 0)
                g_quad[x - 1][y - 1] = MEDICINE;
            if (y > 0)
                g_quad[x][y - 1] = MEDICINE;
            if (x < (g_windowWidth - 1) && y > 0)
                g_quad[x + 1][y - 1] = MEDICINE;
            if (x > 0)
                g_quad[x - 1][y] = MEDICINE;
            if (x < (g_windowWidth - 1))
                g_quad[x + 1][y] = MEDICINE;
            if (x > 0 && y < (g_windowHeight - 1))
                g_quad[x - 1][y + 1] = MEDICINE;
            if (y < (g_windowHeight - 1))
                g_quad[x][y + 1] = MEDICINE;
            if (x < (g_windowWidth - 1) && y < (g_windowHeight - 1))
                g_quad[x + 1][y + 1] = MEDICINE;

            int maxLeft = x - 1;
            int maxRight = g_windowWidth - x;
            int maxTop = y - 1;
            int maxBottom = g_windowHeight - y;
            int maxTopLeft, maxBottomLeft, maxTopRight, maxBottomRight;
            if (maxTop > maxLeft)
                maxTopLeft = maxLeft;
            else
                maxTopLeft = maxTop;
            if (maxLeft > maxBottom)
                maxBottomLeft = maxBottom;
            else
                maxBottomLeft = maxLeft;
            if (maxTop > maxRight)
                maxTopRight = maxRight;
            else
                maxTopRight = maxTop;
            if (maxRight > maxBottom)
                maxBottomRight = maxBottom;
            else
                maxBottomRight = maxRight;



            for (int i = 0; i < maxTopLeft; i++) {
                g_quad[x - i][y - i] = MEDICINE;
            }
            for (int i = 0; i < maxTop; i++) {
                g_quad[x][y - i] = MEDICINE;
            }
            for (int i = 0; i < maxTopRight; i++) {
                g_quad[x + i][y - i] = MEDICINE;
            }


            for (int i = 0; i < maxLeft; i++) {
                g_quad[x - i][y] = MEDICINE;
            }
            for (int i = 0; i < maxRight; i++) {
                g_quad[x + i][y] = MEDICINE;
            }



            for (int i = 0; i < maxBottomLeft; i++) {
                g_quad[x - i][y + i] = MEDICINE;
            }
            for (int i = 0; i < maxBottom; i++) {
                g_quad[x][y + i] = MEDICINE;
            }
            for (int i = 0; i < maxBottomRight; i++) {
                g_quad[x + i][y + i] = MEDICINE;
            }

        }
    }
}

void Keyboard(unsigned char key, int mousePositionX, int mousePositionY)
{
    /**
     @Desc : Function that handles keyboard buttons being pressed
     @param1 : key that was pressed
     @param2 : x position of mouse pointer
     @param3 : y position of mouse pointer
     */

    switch (key)
    {
        // Escape key
    case 27:
        exit(0);
        break;

    default:
        break;
    }
}

int main(int argc, char** argv)
{

    int const MAX_PLATFORMS = 10;
    int const MAX_DEVICES = 10;
    char dname[500];
    cl_device_id devices[MAX_DEVICES];
    cl_uint num_devices, entries;
    cl_ulong long_entries;
    int d, ip;
    cl_int err;
    cl_uint num_platforms;
    cl_platform_id platform_id[MAX_PLATFORMS];
    size_t p_size;

    /* obtain list of platforms available */
    err = clGetPlatformIDs(MAX_PLATFORMS, platform_id, &num_platforms);
    if (err != CL_SUCCESS)
    {
        printf("Error: Failure in clGetPlatformIDs,error code=%d \n", err);
        return 0;

    }
    printf("Found %d platforms \n", num_platforms);

    for (ip = 0; ip < num_platforms; ip++) {
        /* obtain information about platform */
        clGetPlatformInfo(platform_id[ip], CL_PLATFORM_NAME, 500, dname, NULL);
        printf("CL_PLATFORM_NAME = %s\n", dname);
        clGetPlatformInfo(platform_id[ip], CL_PLATFORM_VERSION, 500, dname, NULL);
        printf("CL_PLATFORM_VERSION = %s\n", dname);

        /* obtain list of devices available on platform */
        clGetDeviceIDs(platform_id[ip], CL_DEVICE_TYPE_ALL, 10, devices, &num_devices);
        printf("%d devices found\n", num_devices);

        /* query devices for information */

        for (d = 0; d < num_devices; ++d) {
            clGetDeviceInfo(devices[d], CL_DEVICE_NAME, 500, dname, NULL);
            printf("Device #%d name = %s\n", d, dname);
            clGetDeviceInfo(devices[d], CL_DRIVER_VERSION, 500, dname, NULL);
            printf("\tDriver version = %s\n", dname);
            clGetDeviceInfo(devices[d], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &long_entries, NULL);
            printf("\tGlobal Memory (MB):\t%llu\n", long_entries / 1024 / 1024);
            clGetDeviceInfo(devices[d], CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &long_entries, NULL);
            printf("\tGlobal Memory Cache (MB):\t%llu\n", long_entries / 1024 / 1024);
            clGetDeviceInfo(devices[d], CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &long_entries, NULL);
            printf("\tLocal Memory (KB):\t%llu\n", long_entries / 1024);
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(cl_ulong), &long_entries, NULL);
            printf("\tMax clock (MHz) :\t%llu\n", long_entries);
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &p_size, NULL);
            printf("\tMax Work Group Size:\t%d\n", p_size);
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &entries, NULL);
            printf("\tNumber of parallel compute cores or multiprocessors:\t%d\n", entries);
        }
    }

    // initialize
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(g_windowWidth, g_windowHeight);
    glutCreateWindow("2D Cell Growth Simulation");

    // Initialize all cells as healthy cells
    for (int i = 0; i < 1024; i++)
    {
        for (int j = 0; j < 768; j++)
        {
            g_quad[i][j] = HEALTHY;
        }
    }

    // Initialize random seed
    srand((int)time(NULL));


    // Change at least 25% of cells to cancer cells
    for (int i = 0; i <= g_initialCancer; i++)
    {
        int x = rand() % 1024;
        int y = rand() % 768;
        if (g_quad[x][y] == CANCER)
            i--;
        else
            g_quad[x][y] = CANCER;
    }

    glutDisplayFunc(Display);
    glutIdleFunc(Display);
    glutMouseFunc(MouseClicks);
    glutKeyboardFunc(Keyboard);
    glutTimerFunc(g_updateTime, Update, 0);
    Initialize();

    glutMainLoop();

    // Shutdown and cleanup
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