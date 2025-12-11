//
// sueda - geometry edits Z. Wood
// 3/16
//

#include <iostream>
#include "Particle.h"
#include "GLSL.h"
#include "MatrixStack.h"
#include "Program.h"
#include "Texture.h"


float randFloat(float l, float h)
{
	float r = rand() / (float) RAND_MAX;
	return (1.0f - r) * l + r * h;
}

Particle::Particle(vec3 start) :
	charge(1.0f),
	m(1.0f),
	d(0.0f),
	x(start),
	v(0.0f, 0.0f, 0.0f),
	lifespan(randFloat(2.0f, 7.0f)),
	tEnd(0.0f),
	scale(1.0f),
	color(1.0f, 1.0f, 1.0f, 1.0f)
{
}

Particle::~Particle()
{
}

// void Particle::load(vec3 start)
// {
// 	float offsetT = randFloat(0.0f, 2.0f);
// 	rebirth(-offsetT, start);
// }

void Particle::load(vec3 start)
{
    // Random lifespan BEFORE rebirth
    lifespan = randFloat(2.0f, 7.0f);

    // Random age: how long the particle has already been alive
    float age = randFloat(0.0f, lifespan);

    // Birth in the past
    float birthTime = -age;

    rebirth(birthTime, start);

    // Fast forward particle to its life position
    update(0.0f, age, vec3(0,0,0), start);
}

/* all particles born at the origin */
void Particle::rebirth(float t, vec3 start)
{
	charge = randFloat(0.0f, 1.0f) < 0.5 ? -1.0f : 1.0f;	
	m = 1.0f;
  	d = randFloat(0.0f, 0.02f);
	x = start;
	v.x = randFloat(-0.2f, 0.2f);
	v.y = randFloat(0.3f, 1.0f);
	v.z = randFloat(-0.2f, 0.2f);
	lifespan = randFloat(2.0f, 7.0f); 
	tEnd = t + lifespan;
	scale = randFloat(0.2, 1.0f);
   	color.r = 0.0f;
   	color.g = 0.0f;
   	color.b = 0.0f;
	color.a = 0.0f;
}

// t current time, h is displacement, x position, g gravity, start is starting position
void Particle::update(float t, float h, const vec3 &g, const vec3 start)
{
	if(t > tEnd) {
		rebirth(t, start);
	}

	//euler implementation (learned this in physics)
	v += g*h;
	x += v*h;

	//have colors gradually turn smokey grey
	color.r += 0.001f;
   	color.g += 0.001f;
   	color.b += 0.001f;

	//To do - how do you want to update the forces?
	color.a = (tEnd-t)/lifespan;
}
