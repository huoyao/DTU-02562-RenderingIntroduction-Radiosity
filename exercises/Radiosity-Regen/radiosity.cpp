// disable warning C4786: symbol greater than 255 character - okay to ignore
#pragma warning(disable: 4786)

#include <iostream>
#include <vector>
#include <string>
#include <GL/glut.h>
#include "Graphics/TrackBall.h"
#include "Loader.h"
#include "Hemicube.h"
#include "Mesher.h"
#include "DataFormat.h"
#include "ProgRef.h"

using namespace std;
using namespace CGLA;
using namespace GFX;
using namespace SG;

#define SPEEDUP		50
#define LIMIT		50

// Radiosity data structures
// ---
vector<MyPolygon*> polygons;  // Polygons = finite elements used in radiosity
							  // computations
vector<MyVertex*> vertices;   // vertices of polygons above

Hemicube *hemicube;           // Hemicube pointer.

Vec3f unshot_rad_limit=Vec3f(LIMIT);
Vec3f unshot_rad=Vec3f(LIMIT+1);

// Parameters loaded from resource file
// ---
float patch_subdiv_size;      // How much do we subdivide polygons to get patch
							  // sizes
float light_subdiv_size;      // How much do we subdivide light sources
int hemicube_size;            // Side length of hemicube face. 
bool use_hemicube;            // Do we use hemicube?
string file_name;             // File name



// Other constants and variables
// ---
const int screen_size = 700;  // Const screen size - MAKE SURE THAT 
							  // screen_size < 1/2 hemicube_size !!!
TrackBall *ball;              // Trackball used for scene navigation
int old_state=GLUT_UP;        // GLUT variable


void displayMyPolygons() {
	for(int i=0;i<polygons.size();i++) {				
			if (4==polygons[i]->vertices) glBegin(GL_QUADS);
			else if (3==polygons[i]->vertices) glBegin(GL_TRIANGLES);
			else assert(false); // illegal number of vertices
		
		glColor3f(polygons[i]->color[0],polygons[i]->color[1],polygons[i]->color[2]);
		for (int j=0;j<polygons[i]->vertices;j++) {
			MyVertex* v = vertices[polygons[i]->vertex[j]];
			glColor3f(v->color[0],v->color[1],v->color[2]);
			Vec3f position = v->position;
			glVertex3f(position[0], position[1], position[2]);
		}
		glEnd();
	}
}



// GLUT callback functions
// --------------------------------------------------
bool done=false;
void display() {

	//just for speedup the rendering
	if(!done){
		if(unshot_rad[0]>unshot_rad_limit[0] && unshot_rad[1]>unshot_rad_limit[2] && unshot_rad[0]>unshot_rad_limit[2])
		{
			for(int i=0; i<SPEEDUP; i++){
				//MyPolygon* maxP = calcAnalyticalFF();
				MyPolygon* maxP = calcFF(hemicube);
				distributeEnergy(maxP);
				colorReconstruction();
			}
	
			unshot_rad=Vec3f(0);
			for(unsigned int i = 0; i < polygons.size(); i++)
				unshot_rad+=polygons[i]->unshot_rad;
			cout<<"Unshot_rad: "<<unshot_rad<<endl;
		}else{
			cout<<"Ok!"<<endl;
			done=true;
		}
	}

	glViewport(0,0,screen_size,screen_size);	
	ball->do_spin();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glColor3f(1.0,0.0,0.0);
	glLoadIdentity();
	ball->set_gl_modelview();
	displayMyPolygons();
	glutSwapBuffers();
	
}


void mouse(int button, int state, int x, int y) {
	if (old_state==GLUT_UP && state==GLUT_DOWN) {
		if (button==GLUT_LEFT_BUTTON) 
			ball->grab_ball(ROTATE_ACTION,Vec2i(x,y));
		else if (button==GLUT_MIDDLE_BUTTON) 
			ball->grab_ball(ZOOM_ACTION,Vec2i(x,y));
		else if (button==GLUT_RIGHT_BUTTON) 
			ball->grab_ball(PAN_ACTION,Vec2i(x,y));
	}
	if (old_state==GLUT_DOWN && state==GLUT_UP)
		ball->release_ball();
	old_state=state;
}

void motion(int x, int y) {
	if (old_state==GLUT_DOWN)
		ball->roll_ball(Vec2i(x,y));
}

void keyboard(unsigned char key, int x, int y) {
  switch(key) {
	case '\033': exit(0); break;
  }
}

void animate() {
  glutPostRedisplay();
}

void visible(int vis)
{
	if (vis == GLUT_VISIBLE)
		glutIdleFunc(animate);
	else
		glutIdleFunc(NULL);
}


int main(int argc, char** argv)
{ 
	CMP::ResourceLoader res("Resource.xml");
	cout << "hemicube_size: " 
			 << (hemicube_size  = res.getInt("hemicube_size")) << endl 
		   << "form_factor_method: " 
			 << (use_hemicube=("hemicube"==res.getString("form_factor_method")))
			 << endl
			 << "patch_subdivision_size: " 
			 << (patch_subdiv_size=res.getFloat("patch_subdivision_size")) << endl
			 << "light_subdivision_size: " 
			 << (light_subdiv_size=res.getFloat("light_subdivision_size")) << endl
		 << "file_name: " 
			 << (file_name=res.getString("file_name")) << endl;

	if(2*hemicube_size > screen_size) 
		cout << "Warning: hemicube * 2 > screen_size " << endl;

	// End of resource loading


	// Initialize GLUT 
	// --------------------------------------------------
	glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
	glutInitWindowSize(screen_size, screen_size);
	glutInit(&argc, argv);
	glutCreateWindow("Radiosity");
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutVisibilityFunc(visible);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);

	// Initialize OpenGL
	// --------------------------------------------------
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-1.0,1.0,-1.0,1.0,1.5,2500.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0.0,0.0,0.0,0.0);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_LIGHTING);
	glDisable(GL_DITHER);
	ball = new TrackBall(Vec3f(0,0,0), screen_size, screen_size, 750, 750);
	
	// Load scene
	vector<MyPolygon*> loaded_polygons;
	vector<MyVertex*> loaded_vertices;
	load_model(file_name, loaded_vertices, loaded_polygons); 

	// Mesh the model
	Mesher::mesh(loaded_vertices, loaded_polygons, vertices, polygons, patch_subdiv_size,light_subdiv_size);

	// Init hemicube
	hemicube = new Hemicube(hemicube_size);

	// code that calculates some initial colors for us to display
	for (int i=0;i<polygons.size();i++) {
		polygons[i]->color = polygons[i]->diffuse;
	}

	// Run
	glutMainLoop();

	return 0;
}

