/*
 * Program 3 base code - includes modifications to shape and initGeom in preparation to load
 * multi shape objects 
 * CPE 471 Cal Poly Z. Wood + S. Sueda + I. Dunn
 */

#include <iostream>
#include <glad/glad.h>
#include <chrono>
#include <algorithm>

#include "GLSL.h"
#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"
#include "Spline.h"
#include "Bezier.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

// value_ptr for glm
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace glm;

class Application : public EventCallbacks
{

public:

	WindowManager * windowManager = nullptr;

	// Our shader program - use this one for Blinn-Phong
	std::shared_ptr<Program> prog;

	//Our shader program for textures
	std::shared_ptr<Program> texProg;

	// geometry information
	int dummySize;
	vector<shared_ptr<Shape>> dummy;

	shared_ptr<Shape> sphere;

	int citySize;
	vector<shared_ptr<Shape>> city;

	//global data for ground plane - direct load constant defined CPU data to GPU (not obj)
	GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
	int g_GiboLen;
	//ground VAO
	GLuint GroundVertexArrayID;

	//the image to use as a texture (ground)
	shared_ptr<Texture> texture0;
	shared_ptr<Texture> texture1;
	shared_ptr<Texture> texture2;
	shared_ptr<Texture> texture3;

	//global data (larger program should be encapsulated)
	vec3 gMin;
	//camera data
	vec3 camPos = vec3(0,0,4);
	vec3 camRight;
	vec3 gaze;
	float camSpeed = 0.1;
	//cinematic data
	bool goCamera = false;
	float camT = 0.0f;
	glm::vec3 bezA = vec3(0, 1, 5);
	glm::vec3 bezB = vec3(0, 0, 0);
	glm::vec3 bezC = vec3(3, 1, 1);

	//animation data
	float lightTrans = 0;
	float gTrans = -3;
	float sTheta = 0;
	float eTheta = 0;
	float hTheta = 0;
	//camera values (in degrees)
	float phi = 0;
	float theta = 0;

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
		//update global camera translation (using view vector)
		if (key == GLFW_KEY_A && action == GLFW_REPEAT) {
			camPos -= camSpeed * camRight;
		}
		if (key == GLFW_KEY_D && action == GLFW_REPEAT) {
			camPos += camSpeed * camRight;
		}
		//move camera forward and back
		if (key == GLFW_KEY_W && action == GLFW_REPEAT){
			camPos += camSpeed * gaze;
		}
		if (key == GLFW_KEY_S && action == GLFW_REPEAT){
			camPos -= camSpeed * gaze;
		}

		if (key == GLFW_KEY_Q && action == GLFW_REPEAT){
			lightTrans += 0.5;
		}
		if (key == GLFW_KEY_E && action == GLFW_REPEAT){
			lightTrans -= 0.5;
		}
		if (key == GLFW_KEY_G && action == GLFW_RELEASE) {
			goCamera = !goCamera;
		}
		if (key == GLFW_KEY_Z && action == GLFW_PRESS) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		if (key == GLFW_KEY_Z && action == GLFW_RELEASE) {
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods)
	{
		double posX, posY;

		if (action == GLFW_PRESS)
		{
			 glfwGetCursorPos(window, &posX, &posY);
			 cout << "Pos X " << posX <<  " Pos Y " << posY << endl;
		}
	}

	// delta x and y (mouse motion) adjusts pitch and yaw
	void scrollCallback(GLFWwindow* window, double deltaX, double deltaY) {
		//yaw (horizontal camera movement)
		theta += deltaX;
		// pitch (vertical camera movement clamped from -80 to 80 degrees)
		float new_phi = phi + deltaY;
		phi = std::max(-80.0f, std::min(new_phi, 80.0f));
	}

	void resizeCallback(GLFWwindow *window, int width, int height)
	{
		glViewport(0, 0, width, height);
	}

	void init(const std::string& resourceDirectory)
	{
		GLSL::checkVersion();

		// Set background color.
		glClearColor(.72f, .84f, 1.06f, 1.0f);
		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);

		// Initialize the GLSL program that we will use for local shading
		prog = make_shared<Program>();
		prog->setVerbose(true);
		prog->setShaderNames(resourceDirectory + "/simple_vert.glsl", resourceDirectory + "/simple_frag.glsl");
		prog->init();
		prog->addUniform("P");
		prog->addUniform("V");
		prog->addUniform("M");
		prog->addUniform("MatAmb");
		prog->addUniform("MatDif");
		prog->addUniform("MatSpec");
		prog->addUniform("MatShine");
		prog->addUniform("lightPos");
		prog->addAttribute("vertPos");
		prog->addAttribute("vertNor");

		// Initialize the GLSL program that we will use for texture mapping
		texProg = make_shared<Program>();
		texProg->setVerbose(true);
		texProg->setShaderNames(resourceDirectory + "/tex_vert.glsl", resourceDirectory + "/tex_frag0.glsl");
		texProg->init();
		texProg->addUniform("P");
		texProg->addUniform("V");
		texProg->addUniform("M");
		texProg->addUniform("flip");
		texProg->addUniform("Texture0");
		texProg->addUniform("lightPos");
		texProg->addUniform("MatShine");
		texProg->addAttribute("vertPos");
		texProg->addAttribute("vertNor");
		texProg->addAttribute("vertTex");

		//read in a load the texture
		//wrap modes: clamps texture coordinates from 0 to 1, so edge pixels are repeated
		texture0 = make_shared<Texture>();
  		texture0->setFilename(resourceDirectory + "/concrete.jpg");
  		texture0->init();
  		texture0->setUnit(0);
  		texture0->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

  		texture1 = make_shared<Texture>();
  		texture1->setFilename(resourceDirectory + "/ship_texture.jpg");
  		texture1->init();
  		texture1->setUnit(1);
  		texture1->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture2 = make_shared<Texture>();
  		texture2->setFilename(resourceDirectory + "/cartoonWood.jpg");
  		texture2->init();
  		texture2->setUnit(2);
  		texture2->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		texture3 = make_shared<Texture>();
  		texture3->setFilename(resourceDirectory + "/manhattan-skyline.jpg");
  		texture3->init();
  		texture3->setUnit(3);
  		texture3->setWrapModes(GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT);
}

	void initGeom(const std::string& resourceDirectory)
	{
		//EXAMPLE set up to read one shape from one obj file - convert to read several
		// Initialize mesh
		// Load geometry
 		// Some obj files contain material information. We'll ignore them for this assignment.
 		
		// dummy mesh
		vector<tinyobj::shape_t> TOshapes;
 		vector<tinyobj::material_t> objMaterials;
 		string errStr;
 		bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/dummy.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			// dummy is multishaped, need to iterate through every shape
			dummySize = TOshapes.size();
			for(int i = 0; i < dummySize; i++){
				shared_ptr<Shape> shape = make_shared<Shape>();
				shape->createShape(TOshapes[i]);
				shape->measure();
				shape->init();
				// Add to dummy shapes vector
				dummy.push_back(shape);
			}
		}
		//read out information stored in the shape about its size - something like this...
		//then do something with that information.....
		// gMin.x = sphere->min.x;
		// gMin.y = sphere->min.y;

		// cylinder skybox mesh
		vector<tinyobj::shape_t> TOshapesB;
 		vector<tinyobj::material_t> objMaterialsB;
		//load in the mesh and make the shape(s)
 		rc = tinyobj::LoadObj(TOshapesB, objMaterialsB, errStr, (resourceDirectory + "/sphereWTex.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {	
			sphere = make_shared<Shape>();
			sphere->createShape(TOshapesB[0]);
			sphere->measure();
			sphere->init();
		}

		// city mesh
		vector<tinyobj::shape_t> TOshapesC;
 		vector<tinyobj::material_t> objMaterialsC;
 		rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/city.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			// city is multishaped, need to iterate through every shape
			citySize = TOshapesC.size();
			for(int i = 0; i < citySize; i++){
				shared_ptr<Shape> shapeC = make_shared<Shape>();
				shapeC->createShape(TOshapes[i]);
				shapeC->measure();
				shapeC->init();
				// Add to city shapes vector
				city.push_back(shapeC);
			}
		}

		//code to load in the ground plane (CPU defined data passed to GPU)
		initGround();
	}

	//directly pass quad for the ground to the GPU
	void initGround() {

		float g_groundSize = 20;
		float g_groundY = -0.25;

  		// A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
		float GrndPos[] = {
			-g_groundSize, g_groundY, -g_groundSize,
			-g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY,  g_groundSize,
			g_groundSize, g_groundY, -g_groundSize
		};

		float GrndNorm[] = {
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0,
			0, 1, 0
		};

		static GLfloat GrndTex[] = {
      		0, 0, // back
      		0, 1,
      		1, 1,
      		1, 0 };

      	unsigned short idx[] = {0, 1, 2, 0, 2, 3};

		//generate the ground VAO
      	glGenVertexArrays(1, &GroundVertexArrayID);
      	glBindVertexArray(GroundVertexArrayID);

      	g_GiboLen = 6;
      	glGenBuffers(1, &GrndBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndNorBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);

      	glGenBuffers(1, &GrndTexBuffObj);
      	glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
      	glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);

      	glGenBuffers(1, &GIndxBuffObj);
     	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
      	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
      }

      //code to draw the ground plane
     void drawGround(shared_ptr<Program> curS) {
     	curS->bind();
     	glBindVertexArray(GroundVertexArrayID);
		// fragment shader SPECIFIC to textures
     	texture0->bind(curS->getUniform("Texture0"));
		//draw the ground plane 
  		SetModel(vec3(0, -1, 0), 0, 0, 1, curS);
  		glEnableVertexAttribArray(0);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
  		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(1);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
  		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

  		glEnableVertexAttribArray(2);
  		glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
  		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

   		// draw!
  		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
  		glDrawElements(GL_TRIANGLES, g_GiboLen, GL_UNSIGNED_SHORT, 0);

  		glDisableVertexAttribArray(0);
  		glDisableVertexAttribArray(1);
  		glDisableVertexAttribArray(2);
  		curS->unbind();
     }

     //helper function to pass material data to the GPU (for both fragment shaders)
	void SetMaterial(shared_ptr<Program> curS, int i) {

    	switch (i) {
    		case 0: // metal ship
    			glUniform3f(curS->getUniform("MatAmb"), 0.03, 0.03, 0.03);
    			glUniform3f(curS->getUniform("MatDif"), 0.2, 0.2, 0.2);
    			glUniform3f(curS->getUniform("MatSpec"), 0.9, 0.9, 0.9);
    			glUniform1f(curS->getUniform("MatShine"), 200.0);
    		break;
    		case 1: // globe (high ambient)
    			glUniform3f(curS->getUniform("MatAmb"), 0.8, 0.8, 0.8);
    			glUniform3f(curS->getUniform("MatDif"), 1, 1, 1);
    			glUniform3f(curS->getUniform("MatSpec"), 0.5, 0.5, 0.5);
    			glUniform1f(curS->getUniform("MatShine"), 60.0);
    		break;
  		}
	}

	/* helper function to set model trasnforms */
  	void SetModel(vec3 trans, float rotY, float rotX, float sc, shared_ptr<Program> curS) {
  		mat4 Trans = glm::translate( glm::mat4(1.0f), trans);
  		mat4 RotX = glm::rotate( glm::mat4(1.0f), rotX, vec3(1, 0, 0));
  		mat4 RotY = glm::rotate( glm::mat4(1.0f), rotY, vec3(0, 1, 0));
  		mat4 ScaleS = glm::scale(glm::mat4(1.0f), vec3(sc));
  		mat4 ctm = Trans*RotX*RotY*ScaleS;
  		glUniformMatrix4fv(curS->getUniform("M"), 1, GL_FALSE, value_ptr(ctm));
  	}

	void setModel(std::shared_ptr<Program> prog, std::shared_ptr<MatrixStack>M) {
		glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, value_ptr(M->topMatrix()));
   	}

	void updateUsingCameraPath(float frametime)  {
   	  if (goCamera) {
    	camT += frametime * 0.15f;
    	if (camT > 1.0f) {
			camT = 1.0f;
		}
		// defaults to linear interpolation
		camPos = Bezier::quadBez(nullptr, bezA, bezB, bezC, camT);
      }
   	}

	void render(float frametime) {
		// Get current frame buffer size.
		int width, height;
		glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
		glViewport(0, 0, width, height);

		// Clear framebuffer.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//Use the matrix stack
		float aspect = width/(float)height;

		// Create the matrix stacks - please leave these alone for now
		auto Projection = make_shared<MatrixStack>();
		auto View = make_shared<MatrixStack>();
		auto Model = make_shared<MatrixStack>();

		// update the cinematic camera position
		updateUsingCameraPath(frametime);

		// Apply perspective projection.
		Projection->pushMatrix();
		Projection->perspective(45.0f, aspect, 0.01f, 100.0f);

		// View is global translation 
		View->pushMatrix();
		View->loadIdentity();
		
		// calculated with radius 5
		float xGaze = 5.0f * glm::cos(glm::radians(phi)) * glm::cos(glm::radians(theta));
		float yGaze = 5.0f * glm::sin(glm::radians(phi));
		float half_pi = glm::half_pi<float>();
		float zGaze = 5.0f * glm::cos(glm::radians(phi)) * glm::sin(glm::radians(theta));
		
		// look at point (center vector) is cameras position(eye) + gaze
		gaze = normalize(vec3(xGaze, yGaze, zGaze));
		glm::mat4 ViewTrans = glm::lookAt(camPos, camPos + gaze, vec3(0,1,0));

		// right of camera in its own coordinate system (camera basis vector u)
		camRight = normalize(cross(gaze,vec3(0, 1, 0)));

		// if in bezier curve animation, set look at point towards globe
		if(goCamera){
			ViewTrans = glm::lookAt(camPos, vec3(0, 0, -1), vec3(0, 1, 0));
		}

		// send look at matrix to the vertex shader to adjust view
		// returned matrix ^^ apply to view
		View->multMatrix(ViewTrans);

		// Draw the scene
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
		glUniform3f(texProg->getUniform("lightPos"), 2.0 + lightTrans, 2.0, 2.9);
		glUniform1f(texProg->getUniform("MatShine"), 27.9);
		glUniform1i(texProg->getUniform("flip"), 0);
		// texture1 gets assigned to Texture0 in the GPU
		texture1->bind(texProg->getUniform("Texture0"));

		// draw the array of ships
		Model->pushMatrix();

		// Scale
		// float dScale = 1.0/(theShip->max.x-theShip->min.x);
		// // space between
		// float sp = 3.0;
		// // offset from center
		// float off = -3.5;
		//   for (int i =0; i < 3; i++) {
		//   	for (int j=0; j < 3; j++) {
		// 	  Model->pushMatrix();
		// 		Model->translate(vec3(off+sp*i, -0.5, off+sp*j));
		// 		Model->scale(vec3(dScale));
		// 		// SetMaterial(texProg, 0);
		// 		glUniform1i(texProg->getUniform("flip"), 0);
		// 		glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
		// 		theShip->draw(texProg);
		// 	  Model->popMatrix();
		// 	}
		//   }
		// Model->popMatrix();

		//draw big background sphere
		glUniform1i(texProg->getUniform("flip"), 1);
		texture3->bind(texProg->getUniform("Texture0"));
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(0.0f, 1.0f, 0.0f));
			Model->scale(vec3(15.0));
			setModel(texProg, Model);
			sphere->draw(texProg);
		Model->popMatrix();

		//draw the dummy mesh
		glUniform1i(texProg->getUniform("flip"), 0);
		texture2->bind(texProg->getUniform("Texture0"));
		Model->pushMatrix();
			Model->loadIdentity();
		  	Model->translate(vec3(0, -0.5, 0));
			Model->scale(vec3(0.01,0.01,0.01));
			setModel(texProg, Model);
			for(int i = 0; i < dummySize; i++){
				dummy[i]->draw(texProg);
			}	
		Model->popMatrix();
		
		texProg->unbind();


		//switch shaders to the texture mapping shader and draw the ground
		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));
		glUniform3f(texProg->getUniform("lightPos"), 2.0 + lightTrans, 2.0, 2.9);
		glUniform1f(texProg->getUniform("MatShine"), 27.9);	
		glUniform1i(texProg->getUniform("flip"), 0);
		
		drawGround(texProg);

		texProg->unbind();
		
		//animation update example
		sTheta = sin(glfwGetTime());
		eTheta = std::max(0.0f, (float)sin(glfwGetTime()));
		hTheta = std::max(0.0f, (float)cos(glfwGetTime()));

		// Pop matrix stacks.
		Projection->popMatrix();
		View->popMatrix();

	}
};

int main(int argc, char *argv[])
{
	// Where the resources are loaded from
	std::string resourceDir = "../resources";

	if (argc >= 2)
	{
		resourceDir = argv[1];
	}

	Application *application = new Application();

	// Your main will always include a similar set up to establish your window
	// and GL context, etc.

	WindowManager *windowManager = new WindowManager();
	windowManager->init(640, 480);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// This is the code that will likely change program to program as you
	// may need to initialize or set up different data and state

	application->init(resourceDir);
	application->initGeom(resourceDir);

	auto lastTime = chrono::high_resolution_clock::now();
	// Loop until the user closes the window.
	while (! glfwWindowShouldClose(windowManager->getHandle()))
	{
		// save current time for next frame
		auto nextLastTime = chrono::high_resolution_clock::now();

		// get time since last frame
		float deltaTime =
			chrono::duration_cast<std::chrono::microseconds>(
				chrono::high_resolution_clock::now() - lastTime)
				.count();
		// convert microseconds (weird) to seconds (less weird)
		deltaTime *= 0.000001;

		// reset lastTime so that we can calculate the deltaTime
		// on the next frame
		lastTime = nextLastTime;

		
		// Render scene.
		application->render(deltaTime);

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
