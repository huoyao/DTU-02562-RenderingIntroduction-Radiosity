#include <math.h>
#include <vector>
#include <float.h>

#include "CGLA/Vec3f.h"
#include "Hemicube.h"
#include "Dataformat.h"
#include "ProgRef.h"

using namespace CGLA;
using namespace std;

extern vector<MyPolygon*> polygons;
extern vector<MyVertex*> vertices;

bool polygon_cmp(MyPolygon* mp1, MyPolygon* mp2)
{ 
  return sqr_length(mp1->unshot_rad) < sqr_length(mp2->unshot_rad);
}

MyPolygon* maxenergy_patch()
{
  return *std::max_element(polygons.begin(), polygons.end(), polygon_cmp);
}

MyPolygon* calcAnalyticalFF()
{
  // Reset all form factors to zero
  for(unsigned int i = 0; i < polygons.size(); i++)
  {
    polygons[i]->formF = 0.0f;
  }
  // PART 5_3_2 Find the patch with maximum energy
  MyPolygon* p = polygons[0];
  p = maxenergy_patch();
  if(p==0) return 0;
  // PART 5_3_3 Calculate the form factors between the maximum patch and all other patches.
  // In this function, do it analytically [B, Sec. 11.2].
  for(unsigned int i = 0; i < polygons.size(); i++)
  {
	  polygons[i]->formF=0;
	  if(polygons[i]==p) continue;
	  CGLA::Vec3f dist = p->center-polygons[i]->center;
	  polygons[i]->formF =
		  (
		  max(dot(normalize(polygons[i]->normal),normalize(dist)),0.0f) *
		  max(dot(normalize(p->normal), -normalize(dist)),0.0f) )
		  / (M_PI * dot(dist, dist));
	  polygons[i]->formF *= p->area;
  }
  // Return the maximum patch
  return p;
}

// PART 5_3_4
bool distributeEnergy(MyPolygon* maxP)
{
  if(maxP == 0)
  	return false;
  // Distribute energy from the maximum patch to all other patches.
  // The energy of the maximum patch is in maxP->unshot_rad (see DataFormat.h).
  for(unsigned int i = 0; i < polygons.size(); i++)
  {
	  if(polygons[i]==maxP) continue;
	  Vec3f value = polygons[i]->formF * maxP->unshot_rad * polygons[i]->diffuse;
	  polygons[i]->rad += value;
	  polygons[i]->unshot_rad += value;
  }
  // Set the unshot radiosity of the maximum patch to zero and return true
  maxP->unshot_rad = CGLA::Vec3f(0);
  return true;
}


//PART 5_3_5
void colorReconstruction()
{
  // Set the colour of all patches by computing their radiances. 
  // (Use nodal averaging to compute the colour of all vertices 
  //  when you get to this part of the exercises.)
	//PART 5_4_0
	for(unsigned int i = 0; i < vertices.size(); i++)
  {
	  vertices[i]->colorcount = 0;
	  vertices[i]->rad = Vec3f(0);
  }
	//END
  for(unsigned int i = 0; i < polygons.size(); i++)
  {
		polygons[i]->color = polygons[i]->rad/(M_PI);
		//PART 5_4_0
		for (int j=0;j<polygons[i]->vertices;j++) {
			vertices[polygons[i]->vertex[j]]->rad += polygons[i]->rad;
			vertices[polygons[i]->vertex[j]]->colorcount++;
		}
		//END
  }
  //PART 5_4_0
  for(unsigned int i = 0; i < vertices.size(); i++)
  {
	  vertices[i]->color = vertices[i]->rad / (vertices[i]->colorcount * M_PI);
  }
  //END
}


void renderPatchIDs()
{
  // Render all polygons in the scene as in displayMyPolygons,
  // but set the colour to the patch index using glColor3ub.
  // Look at the Hemicube::getIndex function to see how the
  // indices are read back.
for(int i=0;i<polygons.size();i++) {				
			if (4==polygons[i]->vertices) 
				glBegin(GL_QUADS);
			else if (3==polygons[i]->vertices)
				glBegin(GL_TRIANGLES);
			else assert(false); // illegal number of vertices
		
			i++;
			GLubyte b = i % 256;
			GLubyte g = ((i-b)/256)%256;
			GLubyte r = (i-b+(g*256))/(256*256);
			i--;
			glColor3ub(r,g,b);
		
			for (int j=0;j<polygons[i]->vertices;j++) {

				Vec3f position = vertices[polygons[i]->vertex[j]]->position;
				glVertex3f(position[0], position[1], position[2]);
			}
		glEnd();
	}
}

MyPolygon* calcFF(Hemicube* hemicube)
{
  
  // Reset all form factors to zero
  for(unsigned int i = 0; i < polygons.size(); i++)
  	polygons[i]->formF = 0; 
  
  // Find the patch with maximum energy
  MyPolygon* p=0;
  p = maxenergy_patch();
  if(p==0) return 0;
  // Compute a normalized up vector for the maximum patch 
  // (use the patch center and one of the patch vertices, for example)
  Vec3f normUp = normalize(p->center-vertices[p->vertex[0]]->position);
  
  	  
	// Render patch IDs to the hemicube and read back the index buffer
	hemicube->renderScene(
		p->center, 
		normUp,
		normalize(p->normal),
		renderPatchIDs);
	hemicube->readIndexBuffer();
	// Compute form factors by stepping through the pixels of the hemicube
	// and calling hemicube->getDeltaFormFactor(...).
	for (int x=0;x<hemicube->rendersize;x++) {
		for (int y=0;y<hemicube->rendersize;y++) {
			unsigned int index = hemicube->getIndex(x,y)-1;
			float deltaformfactor = hemicube->getDeltaFormFactor(x,y);
			if(index>=0 && index<polygons.size())
				polygons[index]->formF += deltaformfactor;
		}
	}
  
  // Return the maximum patch
  return p;
}
