#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>

// some typedefs missing in very old Windows' GL/gl.h
typedef char GLchar;
#define GL_FRAGMENT_SHADER 					0x8B30
#define GL_VERTEX_SHADER 					0x8B31
#define GL_COMPILE_STATUS 					0x8B81
#define GL_LINK_STATUS 						0x8B82

// OpenGL Function Prototypes (Manually loaded via wglGetProcAddress)

// Shaders
typedef GLuint(WINAPI * PFNGLCREATESHADERPROC) (GLenum type);
typedef void (WINAPI * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (WINAPI * PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef void (WINAPI * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (WINAPI * PFNGLDELETESHADERPROC) (GLuint shader);

// Programs
typedef GLuint(WINAPI * PFNGLCREATEPROGRAMPROC) (void);
typedef void (WINAPI * PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (WINAPI * PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (WINAPI * PFNGLDELETEPROGRAMPROC) (GLuint program);

// Uniforms and Attributes
typedef GLint(WINAPI * PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (WINAPI * PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (WINAPI * PFNGLUNIFORM3FPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (WINAPI * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (WINAPI * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (WINAPI * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (WINAPI * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);

// Global Pointers
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;

PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;

PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLUNIFORM1FPROC glUniform1f = NULL;
PFNGLUNIFORM3FPROC glUniform3f = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;

// Global Variables
HDC 				g_hDC = NULL;
HGLRC 				g_hRC = NULL;
HWND 				g_hWnd = NULL;
HINSTANCE 			g_hInstance = NULL;
GLuint 				g_shaderProgram = 0;
GLint 				g_timeUniformLocation = -1;
GLint       		g_resolutionUniformLocation = -1;
DWORD 				g_startTime = 0;
int         		g_screenWidth = 0;
int         		g_screenHeight = 0;

// GLSL Shader Sources

// Minimal vertex shader: Passes the position directly
const char* const g_vertexShaderSource =
		"#version 120\n"
		"void main() {\n"
		" 	gl_Position = gl_Vertex;\n"
		"}\n";

// Main fragment shader: Julia set with animated C parameter
const char* const g_fragmentShaderSource = 
		"#version 330 core\n"
		"uniform vec3 iResolution; 		// The viewport resolution (width, height, 1.0)\n"
		"uniform float iTime; 			// shader playback time (in seconds)\n"
		"\n "
		"void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
		"{\n"
		"	vec2 p = (2.0*fragCoord-iResolution.xy)/iResolution.y;\n"
		"\n"
		"    float time = 30.0 + 0.2*iTime;\n"
		"    vec2 cc = 1.1*vec2( 0.5*cos(0.1*time) - 0.25*cos(0.2*time), \n"
		"	                    0.5*sin(0.1*time) - 0.25*sin(0.2*time) );\n"
		"\n"
		"	vec4 dmin = vec4(1000.0);\n"
		"    vec2 z = p;\n"
		"    for( int i=0; i<64; i++ )\n"
		"    {\n"
		"        z = cc + vec2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y );\n"
		"\n"
		"		dmin=min(dmin, vec4(abs(0.0+z.y + 0.5*sin(z.x)), \n"
		"							abs(1.0+z.x + 0.5*sin(z.y)), \n"
		"							dot(z,z),\n"
		"						    length( fract(z)-0.5) ) );\n"
		"    }\n"
		"    \n"
		"    vec3 col = vec3( dmin.w );\n"
		"	col = mix( col, vec3(1.00,0.80,0.60),     min(1.0,pow(dmin.x*0.25,0.20)) );\n"
		"    col = mix( col, vec3(0.72,0.70,0.60),     min(1.0,pow(dmin.y*0.50,0.50)) );\n"
		"	col = mix( col, vec3(1.00,1.00,1.00), 1.0-min(1.0,pow(dmin.z*1.00,0.15) ));\n"
		"\n"
		"	col = 1.25*col*col;\n"
		"    col = col*col*(3.0-2.0*col);\n"
		"	\n"
		"    p = fragCoord/iResolution.xy;\n"
		"	col *= 0.5 + 0.5*pow(16.0*p.x*(1.0-p.x)*p.y*(1.0-p.y),0.15);\n"
		"\n"
		"	fragColor = vec4(col,1.0);\n"
		"}"
		"\n"
		"void main()\n"
		"{\n"
		"mainImage(gl_FragColor, gl_FragCoord.xy);\n"
		"}\n";


// Utility Functions

void CheckShaderCompileErrors(GLuint shader, const char* type) {
		GLint success;
		GLchar infoLog[1024];
		if (strcmp(type, "PROGRAM") != 0) {
				glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
				if (!success) {
						glGetShaderInfoLog(shader, 1024, NULL, infoLog);
						MessageBoxA(NULL, infoLog, "Shader Compilation Error", MB_OK | MB_ICONERROR);
						exit(-1);
				}
		} else {
				glGetProgramiv(shader, GL_LINK_STATUS, &success);
				if (!success) {
						glGetProgramInfoLog(shader, 1024, NULL, infoLog);
						MessageBoxA(NULL, infoLog, "Program Linking Error", MB_OK | MB_ICONERROR);
						exit(-1);
				}
		}
}

// Loads the required GLSL function pointers using wglGetProcAddress
BOOL LoadGLSLFunctions() {
		glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
		glAttachShader = (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
		glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");

		glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
		glDeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");

		glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation");
		glUniform1f = (PFNGLUNIFORM1FPROC)wglGetProcAddress("glUniform1f");
		glUniform3f = (PFNGLUNIFORM3FPROC)wglGetProcAddress("glUniform3f"); // Load glUniform3f
		glGetProgramiv = (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
		glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
		glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");
		glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");

		// Check if critical functions were loaded
		if (!glCreateShader || !glCreateProgram || !glUniform3f) { // Check glUniform3f too
				return FALSE;
		}
		return TRUE;
}

// Compiles and links shaders
void InitShaders() {
	// Compile Shaders
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &g_vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	CheckShaderCompileErrors(vertexShader, "VERTEX");

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &g_fragmentShaderSource, NULL); 
	glCompileShader(fragmentShader);
	CheckShaderCompileErrors(fragmentShader, "FRAGMENT");

	// Link Program
	g_shaderProgram = glCreateProgram();
	glAttachShader(g_shaderProgram, vertexShader);
	glAttachShader(g_shaderProgram, fragmentShader);
	glLinkProgram(g_shaderProgram);
	CheckShaderCompileErrors(g_shaderProgram, "PROGRAM");

	// Clean up shader objects
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// Get uniform locations
	glUseProgram(g_shaderProgram);
	g_timeUniformLocation = glGetUniformLocation(g_shaderProgram, "iTime");
	g_resolutionUniformLocation = glGetUniformLocation(g_shaderProgram, "iResolution");
	glUseProgram(0);

	// Set start time for animation
	g_startTime = GetTickCount();
}


// Rendering and Cleanup

void RenderScene() {
	// Calculate elapsed time (in seconds)
	float currentTime = (GetTickCount() - g_startTime) / 1000.0f;

	// Clear the screen (not strictly needed since we draw over everything)
	glClear(GL_COLOR_BUFFER_BIT);

	// Use the shader program
	glUseProgram(g_shaderProgram);

	// Pass the time uniform
	if (g_timeUniformLocation != -1) {
			glUniform1f(g_timeUniformLocation, currentTime);
	}

    // Pass the resolution uniform
    if (g_resolutionUniformLocation != -1) {
        // iResolution is vec3(width, height, 1.0)
        glUniform3f(g_resolutionUniformLocation, (GLfloat)g_screenWidth, (GLfloat)g_screenHeight, 1.0f);
    }

	// Draw a fullscreen quad using two triangles (Immediate Mode for simplicity)
	glBegin(GL_TRIANGLES);
		// Triangle 1
		glVertex2f(-1.0f, -1.0f); // Bottom-left
		glVertex2f( 1.0f, -1.0f); // Bottom-right
		glVertex2f(-1.0f, 1.0f); // Top-left

		// Triangle 2
		glVertex2f( 1.0f, -1.0f); // Bottom-right
		glVertex2f( 1.0f, 1.0f); // Top-right
		glVertex2f(-1.0f, 1.0f); // Top-left
	glEnd();

	// Stop using the program
	glUseProgram(0);

	// Swap buffers to display the rendered image
	SwapBuffers(g_hDC);
}

void CleanUp() {
	if (g_shaderProgram) {
			glDeleteProgram(g_shaderProgram);
	}

	if (g_hRC) {
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(g_hRC);
			g_hRC = NULL;
	}
	if (g_hDC) {
			ReleaseDC(g_hWnd, g_hDC);
			g_hDC = NULL;
	}
	if (g_hWnd) {
			DestroyWindow(g_hWnd);
			g_hWnd = NULL;
	}
	UnregisterClassA("GL_WINDOW_CLASS", g_hInstance);
}

// Win32 Callbacks and Entry Point

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
			case WM_SIZE:
		// The WM_SIZE message is sent when the window size changes.
		if (g_hDC) {
			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;

			// Update the global variables with the new dimensions
			g_screenWidth = width;
			g_screenHeight = height;

			// Update the OpenGL viewport
			glViewport(0, 0, width, height);
		}
		break;
			case WM_KEYDOWN:
					// Exit on ESC key press
					if (wParam == VK_ESCAPE) {
							PostQuitMessage(0);
					}
					break;
			case WM_CLOSE:
					PostQuitMessage(0);
					return 0;
			case WM_DESTROY:
					PostQuitMessage(0);
					return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// The main entry point for a GUI application on Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
		g_hInstance = hInstance;
		WNDCLASSA wc = {0};
		PIXELFORMATDESCRIPTOR pfd = {0};

		// Define Window Class
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hInstance;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszClassName = "GL_WINDOW_CLASS";

		if (!RegisterClassA(&wc)) {
				MessageBoxA(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				return 0;
		}

		// Determine fullscreen size and store it globally
		g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
		g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

		// Create Window (Borderless Fullscreen)
		g_hWnd = CreateWindowExA(
				0,
				"GL_WINDOW_CLASS",
				"Minimal OpenGL Shader",
				WS_POPUP | WS_VISIBLE, // WS_POPUP for no border/caption
				0, 0, g_screenWidth, g_screenHeight, // x, y, width, height
				NULL, NULL, hInstance, NULL
		);

		if (!g_hWnd) {
				MessageBoxA(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				return 0;
		}

		// Get Device Context
		g_hDC = GetDC(g_hWnd);
		if (!g_hDC) {
				MessageBoxA(NULL, "GetDC Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		// Set Pixel Format (Standard GDI/WGL setup for OpenGL 2.1)
		pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 24;
		pfd.iLayerType = PFD_MAIN_PLANE;

		int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
		if (pixelFormat == 0) {
				MessageBoxA(NULL, "ChoosePixelFormat Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		if (!SetPixelFormat(g_hDC, pixelFormat, &pfd)) {
				MessageBoxA(NULL, "SetPixelFormat Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		// Create Rendering Context
		g_hRC = wglCreateContext(g_hDC);
		if (!g_hRC) {
				MessageBoxA(NULL, "wglCreateContext Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		if (!wglMakeCurrent(g_hDC, g_hRC)) {
				MessageBoxA(NULL, "wglMakeCurrent Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		// Load GLSL Functions and Initialize Shaders
		if (!LoadGLSLFunctions()) {
				MessageBoxA(NULL, "Failed to load GLSL functions. Driver too old or not installed?", "Error", MB_ICONEXCLAMATION | MB_OK);
				CleanUp();
				return 0;
		}

		InitShaders();

		// Set viewport and clear color using stored dimensions
		glViewport(0, 0, g_screenWidth, g_screenHeight);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		// Message Loop (Main Game Loop)
		MSG msg;
		BOOL bQuit = FALSE;
		while (!bQuit) {
				// Handle window messages
				if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
						if (msg.message == WM_QUIT) {
								bQuit = TRUE;
						} else {
								TranslateMessage(&msg);
								DispatchMessage(&msg);
						}
				} else {
						// Render the frame when there are no messages to process
						RenderScene();
				}
		}

		// Cleanup and Exit
		CleanUp();
		return (int)msg.wParam;
}
