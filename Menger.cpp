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
        "// Flight speed. Total duration will be 8*flightSpeed. \n"
        "const float flightSpeed = 0.125; \n"
        "const float FOV = 2.; \n"
        "const float Gamma = .75; \n"
        "const vec3 surfaceColor = vec3(.83, .6, .11); \n"
        "const vec3 darkColor = vec3(.329, .078, .5); \n"
        "const float fogDistance = 24.; \n"
        "//const vec3 fogColor = vec3(1., 0., .12); \n"
        "const vec3 fogColor = vec3(.11, .33, .83); \n"
        "const float lightDistance = -.15; \n"
        "const int aoIterations = 5; \n"
        "const float shadowLight = .45; \n"
        " \n"
        "float hash( float n ){ return fract(cos(n)*45758.5453); } \n"
        " \n"
        "vec3 tex3D( sampler2D tex, in vec3 p, in vec3 n ) \n"
        "{ \n"
        "    n = max(n*n, 0.001); // n = max((abs(n) - 0.2)*7., 0.001); // n = max(abs(n), 0.001), etc. \n"
        "    n /= (n.x + n.y + n.z ); \n"
        "    return (texture(tex, p.yz)*n.x + texture(tex, p.zx)*n.y + texture(tex, p.xy)*n.z).xyz; \n"
        "} \n"
        " \n"
        "float sminP( float a, float b, float smoothing ){ \n"
        " \n"
        "    float h = clamp( 0.5+0.5*(b-a)/smoothing, 0.0, 1.0 ); \n"
        "    return mix( b, a, h ) - smoothing*h*(1.0-h); \n"
        "} \n"
        " \n"
        "// This layered Menger sponge is stolen from Shane \n"
        "// https://www.shadertoy.com/view/ldyGWm \n"
        "float map(vec3 q) \n"
        "{ \n"
        "    vec3 p = abs(fract(q/3.)*3. - 1.5); \n"
        "    float d = min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - 1. + .04; \n"
        " \n"
        "    p =  abs(fract(q) - .5); \n"
        "    d = max(d, min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - 1./3. + .05); \n"
        " \n"
        "    p =  abs(fract(q*2.)*.5 - .25); \n"
        "    d = max(d, min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - .5/3. - .015); \n"
        " \n"
        "    p =  abs(fract(q*3./.5)*.5/3. - .5/6.); \n"
        " \n"
        "    return max(d, min(max(p.x, p.y), min(max(p.y, p.z), max(p.x, p.z))) - 1./27. - .015); \n"
        "} \n"
        " \n"
        "float trace(vec3 ro, vec3 rd){ \n"
        " \n"
        "    float t = 0., d; \n"
        "    for(int i=0; i< 48; i++){ \n"
        "        d = map(ro + rd*t); \n"
        "        if (d <.0025*t || t>fogDistance) break; \n"
        "        t += d; \n"
        "    } \n"
        "    return t; \n"
        "} \n"
        " \n"
        "float refTrace(vec3 ro, vec3 rd) \n"
        "{ \n"
        "    float t = 0., d; \n"
        "    for(int i=0; i< 16; i++){ \n"
        "        d = map(ro + rd*t); \n"
        "        if (d <.0025*t || t>fogDistance) break; \n"
        "        t += d; \n"
        "    } \n"
        "    return t; \n"
        "} \n"
        " \n"
        "// Tetrahedral normal (from IQ) \n"
        "vec3 normal(in vec3 p) \n"
        "{ \n"
        "    // Note the slightly increased sampling distance, to alleviate artifacts due to hit point inaccuracies. \n"
        "    vec2 e = vec2(0.003, -0.003); \n"
        "    return normalize(e.xyy * map(p + e.xyy) + e.yyx * map(p + e.yyx) + e.yxy * map(p + e.yxy) + e.xxx * map(p + e.xxx)); \n"
        "} \n"
        " \n"
        "float calculateAO(in vec3 p, in vec3 n) \n"
        "{ \n"
        "    float ao = 0.0, l; \n"
        "    const float falloff = 1.; \n"
        " \n"
        "    const float maxDist = 1.; \n"
        "    for(float i=1.; i<float(aoIterations)+.5; i++){ \n"
        " \n"
        "        l = (i + hash(i))*.5/float(aoIterations)*maxDist; \n"
        "        ao += (l - map( p + n*l ))/ pow(1. + l, falloff); \n"
        "    } \n"
        " \n"
        "    return clamp( 1.-ao/float(aoIterations), 0., 1.); \n"
        "} \n"
        " \n"
        "float softShadow(vec3 ro, vec3 lp, float k) \n"
        "{ \n"
        "    const int maxIterationsShad = 32; \n"
        " \n"
        "    vec3 rd = (lp-ro); \n"
        " \n"
        "    float shade = 1.0; \n"
        "    float dist = 0.002; \n"
        "    float end = max(length(rd), 0.001); \n"
        "    float stepDist = end/float(maxIterationsShad); \n"
        " \n"
        "    rd /= end; \n"
        " \n"
        "    for (int i=0; i < maxIterationsShad; i++){ \n"
        " \n"
        "        float h = map(ro + rd*dist); \n"
        "        shade = min(shade, smoothstep(0.0, 1.0, k*h/dist)); \n"
        "        dist += clamp(h, 0.0001, 0.2); \n"
        " \n"
        "        // Early exit \n"
        "        if (h < 0.00001 || dist > end) break; \n"
        "    } \n"
        " \n"
        "    return min(max(shade, 0.) + shadowLight, 1.0); \n"
        "} \n"
        " \n"
        "vec3 camPathTable[8]; \n"
        "void setCamPath() \n"
        "{ \n"
        "    const float mainCorridor = 2.82*2.; \n"
        " \n"
        "    camPathTable[0] = vec3(0, 0, 0); \n"
        "    camPathTable[1] = vec3(0, 0, mainCorridor); \n"
        "    camPathTable[2] = vec3(0, -mainCorridor, mainCorridor); \n"
        "    camPathTable[3] = vec3(mainCorridor, -mainCorridor, mainCorridor); \n"
        "    camPathTable[4] = vec3(mainCorridor, -mainCorridor, 0); \n"
        "    camPathTable[5] = vec3(0, -mainCorridor, 0); \n"
        "    camPathTable[6] = vec3(-mainCorridor, -mainCorridor, 0); \n"
        "    camPathTable[7] = vec3(-mainCorridor, 0, 0); \n"
        "} \n"
        " \n"
        "/* \n"
        " * http://graphics.cs.ucdavis.edu/education/CAGDNotes/Catmull-Rom-Spline/Catmull-Rom-Spline.html \n"
        " * f(x) = [1, t, t^2, t^3] * M * [P[i-1], P[i], P[i+1], P[i+2]] \n"
        " */ \n"
        "vec3 catmullRomSpline(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t) \n"
        "{ \n"
        "    vec3 c1,c2,c3,c4; \n"
        " \n"
        "    c1 = p1; \n"
        "    c2 = -0.5*p0 + 0.5*p2; \n"
        "    c3 = p0 + -2.5*p1 + 2.0*p2 + -0.5*p3; \n"
        "    c4 = -0.5*p0 + 1.5*p1 + -1.5*p2 + 0.5*p3; \n"
        " \n"
        "    return (((c4*t + c3)*t + c2)*t + c1); \n"
        "} \n"
        " \n"
        "vec3 camPath(float t) \n"
        "{ \n"
        "    const int aNum = 8; \n"
        " \n"
        "    t = fract(t/float(aNum))*float(aNum); \n"
        " \n"
        "    int segNum = int(floor(t)); \n"
        "    float segTime = t - float(segNum); \n"
        " \n"
        "    if (segNum == 0) return catmullRomSpline(camPathTable[aNum-1], camPathTable[0], camPathTable[1], camPathTable[2], segTime); \n"
        "    if (segNum == aNum-2) return catmullRomSpline(camPathTable[aNum-3], camPathTable[aNum-2], camPathTable[aNum-1], camPathTable[0], segTime); \n"
        "    if (segNum == aNum-1) return catmullRomSpline(camPathTable[aNum-2], camPathTable[aNum-1], camPathTable[0], camPathTable[1], segTime); \n"
        " \n"
        "    return catmullRomSpline(camPathTable[int(segNum)-1], camPathTable[int(segNum)], camPathTable[int(segNum)+1], camPathTable[int(segNum)+2], segTime); \n"
        "} \n"
        " \n"
        "void mainImage( out vec4 fragColor, in vec2 fragCoord ) \n"
        "{ \n"
        "    vec2 u = (fragCoord - iResolution.xy*0.5)/iResolution.y; \n"
        "    float speed = iTime * flightSpeed; \n"
        "    setCamPath(); \n"
        " \n"
        "    vec3 ro = camPath(speed); // Camera position \n"
        "    vec3 lk = camPath(speed + .5);  // Look At \n"
        "    vec3 lp = camPath(speed + lightDistance); // Light position. \n"
        " \n"
        "    // Camera vectors \n"
        "    vec3 fwd = normalize(lk-ro); \n"
        "    vec3 rgt = normalize(vec3(fwd.z, 0, -fwd.x)); \n"
        "    vec3 up = (cross(fwd, rgt)); \n"
        " \n"
        "    vec3 rd = normalize(fwd + FOV*(u.x*rgt + u.y*up)); \n"
        " \n"
        "    // Initiate the scene color to black. \n"
        "    vec3 col = vec3(0); \n"
        " \n"
        "    float t = trace(ro, rd); \n"
        " \n"
        "    // Lighting is completely taken from Shane. \n"
        "    if (t < fogDistance) \n"
        "    { \n"
        "        vec3 sp = ro + rd*t; // Surface position. \n"
        "        vec3 sn = normal(sp); // Surface normal. \n"
        "        vec3 ref = reflect(rd, sn); // Reflected ray. \n"
        " \n"
        "        const float ts = 2.; // Texture scale. \n"
        "        vec3 oCol = surfaceColor; \n"
        " \n"
        "        // Darker toned paneling. \n"
        "        vec3 q = abs(mod(sp, 3.) - 1.5); \n"
        "        if (max(max(q.x, q.y), q.z) < 1.063) oCol = oCol*darkColor; \n"
        " \n"
        "        // Bringing out the texture colors a bit. \n"
        "        oCol = smoothstep(0.0, 1.0, oCol); \n"
        " \n"
        "        float sh = softShadow(sp, lp, 16.); // Soft shadows. \n"
        "        float ao = calculateAO(sp, sn); // Self shadows. \n"
        " \n"
        "        vec3 ld = lp - sp; // Light direction. \n"
        "        float lDist = max(length(ld), 0.001); // Light to surface distance. \n"
        "        ld /= lDist; // Normalizing the light direction vector. \n"
        " \n"
        "        float diff = max(dot(ld, sn), 0.); // Diffuse component. \n"
        "        float spec = pow(max(dot(reflect(-ld, sn), -rd), 0.), 12.); // Specular. \n"
        " \n"
        "        float atten = 1.0 / (1.0 + lDist*0.25 + lDist*lDist*.1); // Attenuation. \n"
        " \n"
        "        // Secondary camera light, just to light up the dark areas a bit more. It's here just \n"
        "        // to add a bit of ambience, and its effects are subtle, so its attenuation \n"
        "        // will be rolled into the attenuation above. \n"
        "        diff += max(dot(-rd, sn), 0.)*.45; \n"
        "        spec += pow(max(dot(reflect(rd, sn), -rd), 0.), 12.)*.45; \n"
        " \n"
        "        // Cheap reflections \n"
        "        float rt = refTrace(sp + ref*0.1, ref); // Raymarch from \"sp\" in the reflected direction. \n"
        "        vec3 rsp = sp + ref*rt; // Reflected surface hit point. \n"
        "        vec3 rsn = normal(rsp); // Normal at the reflected surface. \n"
        " \n"
        "        vec3 rCol = surfaceColor; \n"
        "        q = abs(mod(rsp, 3.) - 1.5); \n"
        "        if (max(max(q.x, q.y), q.z)<1.063) rCol = rCol*vec3(.7, .85, 1.); \n"
        "        // Toning down the power of the reflected color, simply because I liked the way it looked more. \n"
        "        rCol = sqrt(rCol); \n"
        "        float rDiff = max(dot(rsn, normalize(lp-rsp)), 0.); // Diffuse at \"rsp\" from the main light. \n"
        "        rDiff += max(dot(rsn, normalize(-rd-rsp)), 0.)*.45; // Diffuse at \"rsp\" from the camera light. \n"
        " \n"
        "        float rlDist = length(lp - rsp); \n"
        "        float rAtten = 1./(1.0 + rlDist*0.25 + rlDist*rlDist*.1); \n"
        "        rCol = min(rCol, 1.)*(rDiff + vec3(.5, .6, .7))*rAtten; // Reflected color. Not accurate, but close enough. \n"
        " \n"
        " \n"
        "        // Combining the elements above to light and color the scene. \n"
        "        col = oCol*(diff + vec3(.5, .6, .7)) + vec3(.5, .7, 1)*spec*2. + rCol*0.25; \n"
        " \n"
        "        // Shading the scene color, clamping, and we're done. \n"
        "        col = min(col*atten*sh*ao, 1.); \n"
        "    } \n"
        " \n"
        "    // Fadeout to a fog \n"
        "    col = mix(col, fogColor, smoothstep(0., fogDistance - 15., t)); \n"
        " \n"
        "    // Last corrections \n"
        "    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / Gamma)); \n"
        "    fragColor = vec4(col, 1.0); \n"
        "} \n"
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
