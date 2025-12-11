#define GLM_ENABLE_EXPERIMENTAL
#include <iostream>
#include <algorithm>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include "particleSys.h"
#include "GLSL.h"

using namespace std;

particleSys::particleSys(vec3 source) {

	numP = 100;	
	t = 0.0f; //total time
	h = 0.01f; //time step
	g = vec3(0.0f, 0.01f, 0.0f); //gravity
	start = source;
	theCamera = glm::mat4(1.0);
}

float randomFloat(float l, float h)
{
	float r = rand() / (float) RAND_MAX;
	return (1.0f - r) * l + r * h;
}

void particleSys::gpuSetup() {

  // resive point buffer
  points.resize(numP * 3);
  pointColors.resize(numP * 4);
  cout << "start: " << start.x << " " << start.y << " " <<start.z << endl;
	for (int i=0; i < numP; i++) {
		points[i*3+0] = start.x;
		points[i*3+1] = start.y;
		points[i*3+2] = start.z;

		auto particle = make_shared<Particle>(start);
		particles.push_back(particle);
		particle->load(start);
	}

	//generate the VAO
   glGenVertexArrays(1, &vertArrObj);
   glBindVertexArray(vertArrObj);

   //generate vertex buffer to hand off to OGL - using instancing
   glGenBuffers(1, &vertBuffObj);
   //set the current state to focus on our vertex buffer
   glBindBuffer(GL_ARRAY_BUFFER, vertBuffObj);
   //actually memcopy the data - only do this once
  glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(GLfloat), nullptr, GL_STREAM_DRAW);
   //attribute partical color buffer array
  glGenBuffers(1, &colorBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
  glBufferData(GL_ARRAY_BUFFER, pointColors.size() * sizeof(GLfloat), nullptr, GL_STREAM_DRAW);
   
  assert(glGetError() == GL_NO_ERROR);
   
  glBindVertexArray(0); 
	
}

void particleSys::reSet() {
	for (int i=0; i < numP; i++) {
		particles[i]->load(start);
	}
}


void particleSys::drawMe(std::shared_ptr<Program> prog) {

 	glBindVertexArray(vertArrObj);
	int h_pos = prog->getAttribute("vertPos");
  int c_pos = prog->getAttribute("pColor");

  //bind the position buffer
  glBindBuffer(GL_ARRAY_BUFFER, vertBuffObj);
  GLSL::enableVertexAttribArray(h_pos);
  glVertexAttribPointer(h_pos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribDivisor(h_pos, 1);

  //bind the color buffer
  glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
  GLSL::enableVertexAttribArray(c_pos);
  glVertexAttribPointer(c_pos, 4, GL_FLOAT, GL_FALSE, 0, 0);
  glVertexAttribDivisor(c_pos, 1);

  // Draw the points !
  glDrawArraysInstanced(GL_POINTS, 0, 1, numP);

  // Cleanup
	glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);
}

void particleSys::update() {

  vec3 pos;
  vec3 velocity;
  vec4 col;

  //update the particles, as lifespan changes alpha goes to 0
  for(auto particle : particles) {
        particle->update(t, h, g, start);
  }
  t += h;
 
  // Sort the particles by Z
  //temp->rotate(camRot, vec3(0, 1, 0));
  //be sure that camera matrix is updated prior to this update
  vec3 s, t, sk;
  vec4 p;
  quat r;
  glm::decompose(theCamera, s, r, t, sk, p);
  sorter.C = glm::toMat4(r); 
  sort(particles.begin(), particles.end(), sorter);


  //go through all the particles and update the CPU buffer
   for (int i = 0; i < numP; i++) {
        pos = particles[i]->getPosition();
        col = particles[i]->getColor();

        points[i*3+0] = pos.x;
        points[i*3+1] = pos.y;
        points[i*3+2] = pos.z;
			  //To do - how can you integrate unique colors per particle?
        pointColors[i*4+0] = col.r; 
        pointColors[i*4+1] = col.g; 
        pointColors[i*4+2] = col.b;
        pointColors[i*4+3] = col.a;
  } 

  //update the GPU data
   glBindBuffer(GL_ARRAY_BUFFER, vertBuffObj);
   glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(GLfloat), nullptr, GL_STREAM_DRAW);
   glBufferSubData(GL_ARRAY_BUFFER, 0, points.size()* sizeof(GLfloat), points.data());
   glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
   glBufferData(GL_ARRAY_BUFFER, pointColors.size() * sizeof(GLfloat), nullptr, GL_STREAM_DRAW);
   glBufferSubData(GL_ARRAY_BUFFER, 0, pointColors.size()* sizeof(GLfloat), pointColors.data());
   glBindBuffer(GL_ARRAY_BUFFER, 0);

}
