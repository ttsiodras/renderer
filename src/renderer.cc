/*
 *  renderer - A simple implementation of polygon-based 3D algorithms.
 *  Copyright (C) 2004  Thanassis Tsiodras (ttsiodras@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define DEFINE_VARS

#include "SDL.h"

#include <config.h>

#include <algorithm>
#include <iostream>
#include <memory>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "3d.h"
#include "Screen.h"
#include "Keyboard.h"
#include "Clock.h"
#include "HelpKeys.h"
#include "OnlineHelpKeys.h"

#ifdef _WIN32
#include <sstream>
#include <fstream>
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265f
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

#ifdef USE_TBB
#include "tbb/task_scheduler_init.h"
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::unique_ptr;
using std::min;
using std::max;
using std::string;
using std::stringstream;
using std::ofstream;

typedef enum {
    RENDER_POINTS = 1,
    RENDER_POINTS_FROM_TRIANGLES = 2,
    RENDER_LINES = 3,
    RENDER_AMBIENT = 4,
    RENDER_GOURAUD = 5,
    RENDER_PHONG = 6,
    RENDER_PHONG_SHADOWMAPS = 7,
    RENDER_PHONG_SOFTSHADOWMAPS = 8,
    RENDER_RAYTRACE = 9,
    RENDER_RAYTRACE_ANTIALIAS = 10
} RenderMode;

const char *modes[] = {
    "Point rasterizing via vertices",
    "Point rasterizing via triangles",
    "Line rasterizing",
    "Ambient rasterizing",
    "Gouraud rasterizing",
    "Phong rasterizing",
    "Phong rasterizing with shadow mapping",
    "Phong rasterizing with soft shadow mapping",
    "Raytracing",
    "Raytracing with antialiasing",
};

#define DEGREES_TO_RADIANS(x) ((coord)((x)*M_PI/180.0))

void usage()
{
#ifndef PACKAGE_STRING
#define PACKAGE_STRING "Renderer 2.x"
#endif

    cerr << PACKAGE_STRING;
#ifdef __TIMESTAMP__
    cerr << " (" << __TIMESTAMP__ << ")";
#endif

#ifdef MLAA_ENABLED
    cerr << "\nCompiled with Intel's Morphological Anti-aliasing (MLAA)";
#endif

#ifdef HAVE_GETOPT_H
    cerr << "\nUsage: renderer [OPTIONS] [FILENAME]\n\n";
    cerr << "  -h         this help\n";
#ifdef _WIN32
    cerr << "  -r         print FPS reports to titlebar (every 5 seconds)\n";
    cerr << "  -c <file>  print benchmark results to 'file'\n";
#else
    cerr << "  -r         print FPS reports to stdout (every 5 seconds)\n";
#endif
    cerr << "  -b         benchmark rendering of N frames (default: 100)\n";
    cerr << "  -n N       set number of benchmarking frames\n";
    cerr << "  -w         use two lights\n";
    cerr << "  -m <mode>  rendering mode:\n";
    cerr << "       1 : point mode\n";
    cerr << "       2 : points based on triangles (culling,color)\n";
    cerr << "       3 : triangles, wireframe anti-aliased\n";
    cerr << "       4 : triangles, ambient colors\n";
    cerr << "       5 : triangles, Gouraud shading, ZBuffer\n";
    cerr << "       6 : triangles, per-pixel Phong, ZBuffer\n";
    cerr << "       7 : triangles, per-pixel Phong, ZBuffer, Shadowmaps\n";
    cerr << "       8 : triangles, per-pixel Phong, ZBuffer, Soft shadowmaps\n";
    cerr << "       9 : raytracing, with shadows and reflections\n";
    cerr << "       0 : raytracing, with shadows, reflections and anti-aliasing\n";
#else
    cerr << "\nUsage: renderer [FILENAME]\n\n";
#endif
    exit(0);
}

void ShowHelp(Screen& canvas, Keyboard& keys)
{
    assert(sizeof(helpKeysImage) == HELPW*HELPH*3);
    Uint32 color = SDL_MapRGB(canvas._surface->format, 0, 0, 0);
    SDL_FillRect(canvas._surface, NULL, color);
    unsigned char *pData = helpKeysImage;
    for(int h=0; h<HELPH; h++)
	for(int w=0; w<HELPW; w++) {
	    unsigned char r = 255-*pData++;
	    unsigned char g = 255-*pData++;
	    unsigned char b = 255-*pData++;
	    canvas.DrawPixel(
		(HEIGHT-HELPH)/2 + h,
		(WIDTH-HELPW)/2 + w,
		SDL_MapRGB(canvas._surface->format, r,g,b));
	}
    canvas.ShowScreen();
    keys.poll();
    while((!keys._isH) && (!keys._isAbort))
	keys.poll();
    while((keys._isH) || (keys._isAbort))
	keys.poll();
}

bool g_benchmark = false;
const char *g_filename = NULL;

int main(int argc, char *argv[])
{
#ifdef USE_TBB
    tbb::task_scheduler_init init;
#endif
#ifdef _WIN32
    string reportFile = "";
#endif

    RenderMode mode = RENDER_PHONG_SOFTSHADOWMAPS;
    bool doReports = false;
    bool doBenchmark = false;
    bool useTwoLights = false;
    unsigned benchmarkFrames = 100;

#ifdef HAVE_GETOPT_H
    int c;
    opterr = 0;

    while ((c = getopt (argc, argv, "hbrwn:m:c:")) != -1)
	switch(c) {
	case 'h':
	    usage();
	    break;
	case 'm':
	    if (atoi(optarg) == 0)
		mode = RENDER_RAYTRACE_ANTIALIAS;
	    else
		mode = RenderMode(atoi(optarg));
	    if (mode>RENDER_RAYTRACE_ANTIALIAS) usage();
	    break;
	case 'b':
	    doBenchmark = true;
	    break;
	case 'r':
	    doReports = true;
	    break;
	case 'w':
	    useTwoLights = true;
	    break;
	case 'n':
	    benchmarkFrames = atoi(optarg);
	    break;
#ifdef _WIN32
	case 'c':
	    reportFile = string(optarg);
	    break;
#endif
	case '?':
	    cerr << "No such option (" << (char)optopt << ")\n";
	    usage();
	    break;
	default:
	    break;
    }

    if (optind == argc) {
	usage();
    }
    char *fname = argv[optind];

#else
    if (argc != 2) {
	cerr << "Missing getopt, just pass a filename" << endl;
	exit(0);
    }
    char *fname = argv[1];
#endif

    g_filename = fname;

    // Show "Press H for help" only when not benchmarking.
    // I hate globals, but I hate needlessly polluting interfaces more (see ShowScreen)
    g_benchmark = doBenchmark;

    try {
	bool autoRotate = true;

	Scene scene;
	static Screen canvas(scene);

	coord angle1=0.0f;
	coord angle2=0.0f*M_PI/180.f;
	coord angle3=45.0f*M_PI/180.f;

	scene.load(fname);
	if (g_benchmark && (mode == RENDER_RAYTRACE_ANTIALIAS || mode == RENDER_RAYTRACE)) {
	    // When benchmarking, we dont want the first frame to "suffer" the BVH creation
	    puts("Creating BVH... please wait...");
	    scene.UpdateBoundingVolumeHierarchy(fname);
	}

	coord maxi = Scene::MaxCoordAfterRescale; // this is the

	// A tradeoff...
	// The closer the light is, the higher the chance that the
	// shadow buffer won't "contain" all of the object, and that shadowing
	// will have "clip-errors".
	// The farther away the light is, chances increase that the object
	// will be "contained" in the shadow buffer, but less shadow pixels
	// are actually drawn... the shadow is less accurate...
	// Of course, this distinction applies only to the shadowmapped modes
	// (not raytracing or non-shadowed Gouraud/Phong)
	const coord LightDistanceFactor = 4.0;

	// Just the distance to initially place the camera
	const coord EyeDistanceFactor = 4.0;

	// Add the light that is rotated with the Q/W keys.
	unique_ptr<Light> pLight(
	    new Light(
		LightDistanceFactor*maxi,
		LightDistanceFactor*maxi,
		LightDistanceFactor*maxi));
	scene._lights.push_back(pLight.get());
	pLight->_x = LightDistanceFactor*maxi*cos(angle3);
	pLight->_y = LightDistanceFactor*maxi*sin(angle3);
	pLight->ClearShadowBuffer();

	// Optionally, add a second, static light.
	unique_ptr<Light> pLight2(
	    new Light(
		LightDistanceFactor*maxi,
		-LightDistanceFactor*maxi,
		LightDistanceFactor*maxi));
	if (useTwoLights) {
	    scene._lights.push_back(pLight2.get());
	    pLight2->ClearShadowBuffer();
	}

	Keyboard keys;

	Vector3 eye(maxi*EyeDistanceFactor, 0.0, 0.0);
	Vector3 lookat(eye._x + 1.0f*cos(angle2)*cos(angle1),
		       eye._y + 1.0f*cos(angle2)*sin(angle1),
		       eye._z + 1.0f*sin(angle2));
	Camera sony(eye, lookat);

	Uint32 framesDrawn = 0, previousReport = 0;

	SDL_WM_SetCaption(modes[mode-1], modes[mode-1]);
	canvas.ShowScreen();

	Clock globalTime; // for reporting of FPS every 5 seconds (-r option)

	long msSpentDrawing = 0; // total time (in milliseconds) spent drawing

	// angle to rotate each time navigation keys are pressed
	coord dAngle = DEGREES_TO_RADIANS(0.3f);

	keys.poll();
	pLight->CalculatePositionInCameraSpace(sony);
	pLight->RenderSceneIntoShadowBuffer(scene);
	pLight->CalculateXformFromWorldToLightSpace();

	if (useTwoLights) {
	    pLight2->CalculatePositionInCameraSpace(sony);
	    pLight2->RenderSceneIntoShadowBuffer(scene);
	    pLight2->CalculateXformFromWorldToLightSpace();
	}

	bool dirtyShadowBuffer = true;

	// "Cache-ing" of state, to avoid redraws if all are the same
	Vector3
	    oldEyePosition(1e10,1e10,1e10),
	    oldLookAtPosition(1e10,1e10,1e10),
	    oldLightPosition(1e10,1e10,1e10);
	bool forceRedraw = false;

	while(!keys._isAbort) {
	    if (doBenchmark && framesDrawn==benchmarkFrames)
		break;
	    // Only allow keyboard control if we are not benchmarking
	    if (!doBenchmark) {
		if (keys._isH) {
		    while(keys._isH)
			keys.poll();
		    ShowHelp(canvas, keys);
		    msSpentDrawing = 0;
		    framesDrawn = 0;
		    forceRedraw = true;
		    continue;
		}
		if (keys._isLeft)
		    angle1-=dAngle;
		if (keys._isRight)
		    angle1+=dAngle;
		if (keys._isUp)
		    angle2=min(angle2+dAngle, DEGREES_TO_RADIANS(89.0f));
		if (keys._isDown)
		    angle2=max(angle2-dAngle, DEGREES_TO_RADIANS(-89.0f));
		if (keys._isForward || keys._isBackward) {
		    Vector3 fromEyeToLookat(lookat);
		    fromEyeToLookat -= eye;
		    if (autoRotate)
			fromEyeToLookat *= 0.05f;
		    else
			fromEyeToLookat *= 0.05f*maxi;
		    /*
		    cout << "At " << eye._x << " " << eye._y << " " << eye._z << endl;
		    cout << "Looking at " << lookat._x << " " << lookat._y << " " << lookat._z << endl;
		    cout << "Adding " << fromEyeToLookat._x << " " << fromEyeToLookat._y << " " << fromEyeToLookat._z << endl;
		    */
		    if (keys._isForward)
			eye += fromEyeToLookat;
		    else
			eye -= fromEyeToLookat;
		}
		if (keys._isS || keys._isF || keys._isE || keys._isD) {
		    Vector3 eyeToLookatPoint = lookat;
		    eyeToLookatPoint -= eye;
		    eyeToLookatPoint.normalize();
		    Vector3 zenith(0., 0., 1.);
		    Vector3 rightAxis = cross(eyeToLookatPoint, zenith);
		    rightAxis.normalize();
		    Vector3 upAxis = cross(rightAxis, eyeToLookatPoint);
		    upAxis.normalize();
		    if (keys._isS) { rightAxis *= 0.05f*maxi; eye -= rightAxis; }
		    if (keys._isF) { rightAxis *= 0.05f*maxi; eye += rightAxis; }
		    if (keys._isD) { upAxis *= 0.05f*maxi; eye -= upAxis; }
		    if (keys._isE) { upAxis *= 0.05f*maxi; eye += upAxis; }
		}
		if (keys._isR) {
		    while(keys._isR)
			keys.poll();
		    autoRotate = !autoRotate;
		    if (!autoRotate) {
			Vector3 eyeToAxes = eye;
			eyeToAxes.normalize();
#if defined(__GNUC__)
			// Hideous GCC code generation bug - unless I print them, eyeToAxes are not normalized!
			cout << "Moving to " << eyeToAxes._x << "," <<
			    eyeToAxes._y << "," << eyeToAxes._z << endl;
#endif
			angle2 = asin(-eyeToAxes._z);
			angle1 = (eye._y<0)?acos(eyeToAxes._x/cos(angle2)):-acos(eyeToAxes._x/cos(angle2));
		    } else {
			angle1 = -angle1;
			angle2 = -angle2;
		    }
		}
		if (keys._isLight || keys._isLight2) {
		    if (keys._isLight)
			angle3 += 4*dAngle;
		    else
			angle3 -= 4*dAngle;
		    pLight->_x = LightDistanceFactor*maxi*cos(angle3);
		    pLight->_y = LightDistanceFactor*maxi*sin(angle3);
		    // When we move the light, we really should clean up the shadow buffer
		    // However, this takes a lot of time. When in no-shadows modes (e.g. Gouraud)
		    // why should the user wait for these functions? He shouldn't.
		    // Which is why we use the "dirty..." booleans below.
		    // We set them to true whenever we move the light, and use them
		    // to only compute these shadow-related stuff when needed, and only when needed.
		    dirtyShadowBuffer = true;
		    if (mode == RENDER_PHONG_SHADOWMAPS || mode == RENDER_PHONG_SOFTSHADOWMAPS) {
			pLight->ClearShadowBuffer();
			pLight->RenderSceneIntoShadowBuffer(scene);
			dirtyShadowBuffer = false;
		    } else if (mode == RENDER_RAYTRACE || mode == RENDER_RAYTRACE_ANTIALIAS) {
			pLight->CalculateXformFromWorldToLightSpace();
		    }
		}
		// Change rendering mode, via 0-9 or PgUp/PgDown
		bool newMode = false;
		if (keys._is0 || keys._is1 || keys._is2 || keys._is3 || keys._is4 ||
		    keys._is5 || keys._is6 || keys._is7 || keys._is8 || keys._is9)
		{
		    if (keys._is1) mode = RENDER_POINTS;
		    if (keys._is2) mode = RENDER_POINTS_FROM_TRIANGLES;
		    if (keys._is3) mode = RENDER_LINES;
		    if (keys._is4) mode = RENDER_AMBIENT;
		    if (keys._is5) mode = RENDER_GOURAUD;
		    if (keys._is6) mode = RENDER_PHONG;
		    if (keys._is7) mode = RENDER_PHONG_SHADOWMAPS;
		    if (keys._is8) mode = RENDER_PHONG_SOFTSHADOWMAPS;
		    if (keys._is9) mode = RENDER_RAYTRACE;
		    if (keys._is0) mode = RENDER_RAYTRACE_ANTIALIAS;
		    while (keys._is0 || keys._is1 || keys._is2 || keys._is3 || keys._is4 ||
			   keys._is5 || keys._is6 || keys._is7 || keys._is8 || keys._is9)
			keys.poll();
		    newMode = true;
		}
		if (keys._isPgDown || keys._isPgUp) {
		    Uint32 up = keys._isPgUp;
		    while(keys._isPgDown || keys._isPgUp)
			keys.poll();
		    if (!up)
			mode = mode==RENDER_POINTS?RENDER_RAYTRACE_ANTIALIAS:RenderMode(mode-1);
		    else
			mode = mode==RENDER_RAYTRACE_ANTIALIAS?RENDER_POINTS:RenderMode(mode+1);
		    newMode = true;
		}
		if (newMode) {
		    SDL_WM_SetCaption(modes[mode-1], modes[mode-1]);
		    // Since we just changed mode, make sure we calculate the proper shadow related stuff,
		    // if and only if we have to!    (speed advantage!)
		    // (use the dirty... booleans, see also comment above)
		    if (dirtyShadowBuffer && (mode == RENDER_PHONG_SHADOWMAPS || mode == RENDER_PHONG_SOFTSHADOWMAPS)) {
			pLight->ClearShadowBuffer();
			pLight->RenderSceneIntoShadowBuffer(scene);
			dirtyShadowBuffer = false;
		    }
		    if (mode == RENDER_RAYTRACE || mode == RENDER_RAYTRACE_ANTIALIAS)
			pLight->CalculateXformFromWorldToLightSpace();
		    dAngle = DEGREES_TO_RADIANS(0.3f);
		    msSpentDrawing = 0;
		    framesDrawn = 0;
		    forceRedraw = true;
		    continue;
		}
	    }
	    if (!autoRotate) {
		lookat._x = eye._x - 1.0f*cos(angle2)*cos(angle1);
		lookat._y = eye._y + 1.0f*cos(angle2)*sin(angle1);
		lookat._z = eye._z + 1.0f*sin(angle2);
	    } else {
		angle1-=dAngle;
		lookat._x = 0;
		lookat._y = 0;
		lookat._z = 0;
		coord distance = sqrt(eye._x*eye._x + eye._y*eye._y + eye._z*eye._z);
		eye._x = distance*cos(angle2)*cos(angle1);
		eye._y = distance*cos(angle2)*sin(angle1);
		eye._z = distance*sin(angle2);
	    }

	    sony.set(eye, lookat);

	    if (mode >= RENDER_GOURAUD) {
		pLight->CalculatePositionInCameraSpace(sony);
		if (useTwoLights)
		    pLight2->CalculatePositionInCameraSpace(sony);
	    }
	    if (mode >= RENDER_PHONG_SHADOWMAPS) {
		pLight->CalculateXformFromCameraToLightSpace(sony);
		if (useTwoLights)
		    pLight2->CalculateXformFromCameraToLightSpace(sony);
	    }

	    // Avoid redrawing if possible (saving CPU utilization)
	    if (oldLightPosition != Vector3(*pLight) ||
		oldEyePosition != eye ||
		oldLookAtPosition != lookat ||
		forceRedraw)
	    {
		// Something changed, time to redraw...
		oldLightPosition = Vector3(*pLight);
		oldEyePosition = eye;
		oldLookAtPosition = lookat;
		forceRedraw = false;

		Clock frameRenderTime;
		switch(mode) {
		case RENDER_POINTS:
		    scene.renderPoints(sony, canvas, false); break;
		case RENDER_POINTS_FROM_TRIANGLES:
		    scene.renderPoints(sony, canvas, true); break;
		case RENDER_LINES:
		    scene.renderWireframe(sony, canvas); break;
		case RENDER_AMBIENT:
		    scene.renderAmbient(sony, canvas); break;
		case RENDER_GOURAUD:
		    scene.renderGouraud(sony, canvas); break;
		case RENDER_PHONG:
		    scene.renderPhong(sony, canvas); break;
		case RENDER_PHONG_SHADOWMAPS:
		    scene.renderPhongAndShadowed(sony, canvas); break;
		case RENDER_PHONG_SOFTSHADOWMAPS:
		    scene.renderPhongAndSoftShadowed(sony, canvas); break;
		case RENDER_RAYTRACE:
		case RENDER_RAYTRACE_ANTIALIAS:
		    // Since the raytracing mode is orders of magnitude slower than
		    // all other modes, we use a special "freeze-frame" mode of user control for it.
		    // The frame is rendered, and then the title bar tells the user to hit ESC
		    // to return to soft-shadowmaps.
		    //
		    // We always want this default handling for the raytracing mode. Unless...
		    //
		    // (a) we are benchmarking - in which case we don't care about speed.
		    //     (Indeed, the raytracing mode is the ultimate benchmark, it makes
		    //      me feel like I did 15 years ago, when rendering was *awfully* slow :-)
		    // (b) we intend to view a very simple model that can run at realtime
		    //     speeds even in raytracing mode - we therefore configured with...
		    //         ./configure --disable-brakes
		    //     ...before compiling. This way, HANDLERAYTRACER will not be defined,
		    //     and the raytracing mode is handled just like the other modes.
		    #ifdef HANDLERAYTRACER
		    if (!doBenchmark) {
			Clock raytraceFrameTime;
			if (scene.renderRaytracer(sony, canvas, mode==RENDER_RAYTRACE_ANTIALIAS)) {
			    stringstream msg;
			    if (mode==RENDER_RAYTRACE_ANTIALIAS) msg << "Anti-aliased r"; else msg << "R";
			    msg << "aytracing completed in ";
			    msg << (raytraceFrameTime.readMS()+999)/1000;
			    msg << " seconds - hit ESC to return to soft shadowmapping mode...";
			    SDL_WM_SetCaption(msg.str().c_str(), msg.str().c_str());
			    while(!keys._isAbort) keys.poll();
			    while(keys._isAbort) keys.poll();
			}
			// Do a normal rendering with soft shadow maps
			mode = RENDER_PHONG_SOFTSHADOWMAPS;
			SDL_WM_SetCaption(modes[mode-1], modes[mode-1]);
			msSpentDrawing = 0;
			framesDrawn = 0;
			forceRedraw = true;
			continue;
		    } else {
			scene.renderRaytracer(sony, canvas, mode==RENDER_RAYTRACE_ANTIALIAS);
		    }
		    #else
		    scene.renderRaytracer(sony, canvas, mode==RENDER_RAYTRACE_ANTIALIAS);
		    #endif
		    break;
		} // end of switch rendering mode
		framesDrawn++;
		msSpentDrawing += frameRenderTime.readMS();
	    } // end of if position/light/lookat changed
	    keys.poll();

	    // The more frames per sec, the smaller the dAngle should be
	    // The less frames per sec, the larger  the dAngle should be
	    //   so...
	    // Move dAngle towards 36/framesPerSec in 15 frames (steps)
	    //   for 100fps, dAngle = 0.36
	    //   for 10 fps, dAngle = 3.6
	    // In both cases, we rotate 360 degrees in 10 seconds
	    //dAngle += ((36.0/(framesDrawn/(msSpentDrawing/1000.0)))-dAngle)/15.;
	    //
	    // Changed to 18/framesPerSec (20 seconds needed for a 360 rotation)
	    if (!doBenchmark)
		if (msSpentDrawing)
		    dAngle += (DEGREES_TO_RADIANS(9.0f/(framesDrawn/(msSpentDrawing/1000.0f)))-dAngle)/15.0f;

	    if (doReports && (globalTime.readMS()-previousReport>5000)) {
		previousReport = globalTime.readMS();
		stringstream speed;
		if (msSpentDrawing) {
		    speed << "FPS: " << framesDrawn/(msSpentDrawing/1000.0);
		    #ifdef _WIN32
		    SDL_WM_SetCaption(speed.str().c_str(), speed.str().c_str());
		    #else
		    cout << speed.str().c_str() << endl;
		    #endif
		}
	    }
	}

	if (msSpentDrawing) {
	    #ifdef _WIN32
	    stringstream speed;
	    speed << "Rendering " << framesDrawn << " frames in " << (msSpentDrawing/1000.0) <<
		" seconds. (" << framesDrawn/(msSpentDrawing/1000.0) << " fps)\n";
	    if (reportFile == "")
		MessageBoxA(0, (LPCSTR) speed.str().c_str(), (const char *)"Speed of rendering", MB_OK);
	    else {
		ofstream rpt;
		rpt.open(reportFile.c_str());
		rpt << speed.str().c_str() << endl;
		rpt.close();
	    }
	    #else
	    cout << "Rendering " << framesDrawn << " frames in ";
	    cout << msSpentDrawing/1000.0 << " seconds. (";
	    cout << framesDrawn/(msSpentDrawing/1000.0) << " fps)\n";
	    #endif
	}
    }
    catch(const string& s)
    {
	cerr << s.c_str();
    }
    return 0;
}
