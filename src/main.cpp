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
	int headIndex;
	vec3 neckPivot;

	shared_ptr<Shape> sphere;

	int citySize;
	vector<shared_ptr<Shape>> city;

	//global data for ground plane - direct load constant defined CPU data to GPU (not obj)
	GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
	int g_GiboLen;
	//ground VAO
	GLuint GroundVertexArrayID;

	//the image to use as a texture 
	shared_ptr<Texture> textureCement;
	shared_ptr<Texture> textureGodzilla;
	shared_ptr<Texture> textureSkybox;
	shared_ptr<Texture> textureCity;

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

	// light animation data
	float lightTrans = 0;
	float gTrans = -3;
	// hierarchical animation data
	float headTheta = 0;
	float eTheta = 0;
	float hTheta = 0;
	bool gAnimate = true;
	float gPauseStart;
	float gPausedTimeOffset;
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
		if (key == GLFW_KEY_X && action == GLFW_PRESS) {
			if (gAnimate) {
        		// Pause animation
        		gAnimate = false;
				gPauseStart = glfwGetTime();
			} else {
				// Resume animation (need to track current time to continue where it leaves off)
				gAnimate = true;
				gPausedTimeOffset += glfwGetTime() - gPauseStart;
			}
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
		glClearColor(.117f, .4627f, 0.80f, 1.0f);
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
		prog->addAttribute("vertTex");

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
		textureCement = make_shared<Texture>();
  		textureCement->setFilename(resourceDirectory + "/concrete.jpg");
  		textureCement->init();
  		textureCement->setUnit(0);
  		textureCement->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		textureCity = make_shared<Texture>();
  		textureCity->setFilename(resourceDirectory + "/_9.jpg");
  		textureCity->init();
  		textureCity->setUnit(1);
  		textureCity->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);		

		textureGodzilla = make_shared<Texture>();
  		textureGodzilla->setFilename(resourceDirectory + "/reptile.jpg");
  		textureGodzilla->init();
  		textureGodzilla->setUnit(2);
  		textureGodzilla->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

		textureSkybox = make_shared<Texture>();
  		textureSkybox->setFilename(resourceDirectory + "/manhattan-skyline.jpg");
  		textureSkybox->init();
  		textureSkybox->setUnit(3);
  		textureSkybox->setWrapModes(GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT);
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
 		bool rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr, (resourceDirectory + "/cubeWTex.obj").c_str());
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
		headIndex = dummySize - 1;
		// grab min and max to find neck pivot
		vec3 headMin = dummy[headIndex]->min;
		vec3 headMax = dummy[headIndex]->max;
		neckPivot = vec3((headMin.x + headMax.x) * 0.5f, headMin.y,(headMin.z + headMax.z) * 0.5f);

		// gMin.x = sphere->min.x;
		// gMin.y = sphere->min.y;

		// cylinder skybox mesh
		vector<tinyobj::shape_t> TOshapesB;
 		vector<tinyobj::material_t> objMaterialsB;
		//load in the mesh and make the shape(s)
 		rc = tinyobj::LoadObj(TOshapesB, objMaterialsB, errStr, (resourceDirectory + "/cylinder.obj").c_str());
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
 		rc = tinyobj::LoadObj(TOshapesC, objMaterialsC, errStr, (resourceDirectory + "/city.obj").c_str());
		if (!rc) {
			cerr << errStr << endl;
		} else {
			// city is multishaped, need to iterate through every shape
			citySize = TOshapesC.size();
			for(int i = 0; i < citySize; i++){
				shared_ptr<Shape> shapeC = make_shared<Shape>();
				shapeC->createShape(TOshapesC[i]);
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

		float g_groundSize = 40;
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
     	textureCement->bind(curS->getUniform("Texture0"));
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
	void setMaterial(shared_ptr<Program> curS, int i) {

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

		// if in bezier curve animation, set look at point towards dummy
		if(goCamera){
			ViewTrans = glm::lookAt(camPos, vec3(0, 0, -1), vec3(0, 1, 0));
		}

		// send look at matrix to the vertex shader to adjust view
		// returned matrix ^^ apply to view
		View->multMatrix(ViewTrans);

		// Draw the scene
		// draw the city (non textured)

		texProg->bind();
		glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
		glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
		glUniform3f(texProg->getUniform("lightPos"), 2.0 + lightTrans, 2.0, 2.9);
		glUniform1f(texProg->getUniform("MatShine"), 27.9);
		glUniform1i(texProg->getUniform("flip"), 0);
		// texture1 gets assigned to Texture0 in the GPU texture shader
		// texture1->bind(texProg->getUniform("Texture0"));

		//draw big background cylinder
		glUniform1i(texProg->getUniform("flip"), 1);
		textureSkybox->bind(texProg->getUniform("Texture0"));
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(0.0f, -40.0f, 0.0f));
			Model->scale(vec3(40.0, 50.0, 50.0));
			setModel(texProg, Model);
			sphere->draw(texProg);
		Model->popMatrix();
		textureSkybox->unbind();

		// draw the city
		glUniform1i(texProg->getUniform("flip"), 0);
		textureCity->bind(texProg->getUniform("Texture0"));		
		Model->pushMatrix();
			Model->loadIdentity();
			Model->translate(vec3(0.0f, -1.4f, -10.0f));
			Model->rotate(3.14, vec3(0, 1, 0));
			Model->scale(vec3(0.001));
			setModel(texProg, Model);
			for (int i; i < citySize; i++){
				city[i]->draw(texProg);
			}
		Model->popMatrix();
		textureCement->unbind();

		//draw the dummy mesh
		glUniform1i(texProg->getUniform("flip"), 1);
		textureGodzilla->bind(texProg->getUniform("Texture0"));
		// drawHierarchicalModel();
		Model->pushMatrix();
			Model->loadIdentity();
		  	Model->translate(vec3(0, -0.5, 3.0f));
			Model->rotate(-1.57, vec3(1, 0, 0));
			Model->scale(vec3(1, 1, 1));
			for(int i = 0; i < dummySize; i++){
				// if head, animate
				if (i == headIndex){
					Model->pushMatrix();
						Model->translate(-neckPivot);
						Model->rotate(headTheta, vec3(0, 1, 0));
						Model->translate(neckPivot);
						setModel(texProg, Model);
						dummy[headIndex]->draw(texProg);
					Model->popMatrix();
				} else {
					setModel(texProg, Model);
					dummy[i]->draw(texProg);
				}
			}
		Model->popMatrix();
	
		texProg->unbind();

		
		// Animate the head
		// Offset by any paused time
		float currentTime = glfwGetTime() - gPausedTimeOffset;
		float currentSin = sin(currentTime);
		if(gAnimate){
			headTheta = currentSin;
		} 

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
		headTheta = sin(glfwGetTime());
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
