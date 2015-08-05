#include "GPUParticleEmitter.h"

GPUParticleEmitter::GPUParticleEmitter() : m_particles(nullptr), m_maxParticles(0),
m_position(-50, 20, -50),
m_drawShader(0),
m_updateShader(0),
m_lastDrawTime(0) {
	m_vao[0] = 0;
	m_vao[1] = 0;
	m_vbo[0] = 0;
	m_vbo[1] = 0;
}
GPUParticleEmitter::~GPUParticleEmitter()
{
	delete[] m_particles;

	glDeleteVertexArrays(2, m_vao);
	glDeleteBuffers(2, m_vbo);

	//delete shaders
	glDeleteProgram(m_drawShader);
	glDeleteProgram(m_updateShader);
}

void GPUParticleEmitter::Initialise(unsigned int a_maxParticles, float a_lifespanMin, float a_lifespanMax,
	float a_velocityMin, float a_velocityMax, float a_startSize, float a_endSize,
	const glm::vec4& a_startColour, const glm::vec4& a_endColour, char* a_vertShaderFile)
{
	//store all the variables that have just been passed in
	m_startColour = a_startColour;
	m_endColour = a_endColour;
	m_startSize = a_startSize;
	m_endSize = a_endSize;
	m_velocityMin = a_velocityMin;
	m_velocityMax = a_velocityMax;
	m_lifespanMin = a_lifespanMin;
	m_lifespanMax = a_lifespanMax;
	m_maxParticles = a_maxParticles;
	m_vertShaderFile = a_vertShaderFile;
	//create the particle array
	m_particles = new GPUParticle[a_maxParticles];

	//set our starting ping-pong buffer
	m_activeBuffer = 0;

	CreateBuffers();
	CreateUpdateShader();
	CreateDrawShader();

}

void GPUParticleEmitter::CreateBuffers()
{
	//Create OpenGL buffers
	glGenVertexArrays(2, m_vao);
	glGenBuffers(2, m_vbo); 

	//setup the first buffer
	glBindVertexArray(m_vao[0]);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, m_maxParticles *
		sizeof(GPUParticle), m_particles, GL_STREAM_DRAW);

	glEnableVertexAttribArray(0);//position
	glEnableVertexAttribArray(1); // velocity
	glEnableVertexAttribArray(2); // lifetime
	glEnableVertexAttribArray(3); // lifespan
	//glEnableVertexAttribArray(4); //texture
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 12);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 24);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 28);

	// setup the second buffer
	glBindVertexArray(m_vao[1]);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, m_maxParticles *
		sizeof(GPUParticle), 0, GL_STREAM_DRAW);

	glEnableVertexAttribArray(0); // position
	glEnableVertexAttribArray(1); // velocity
	glEnableVertexAttribArray(2); // lifetime
	glEnableVertexAttribArray(3); // lifespan
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), 0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 12);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 24);
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE,
		sizeof(GPUParticle), ((char*)0) + 28);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

}
void  GPUParticleEmitter::CreateUpdateShader()
{
	//create shader
	unsigned int vs = LoadShader(GL_VERTEX_SHADER, m_vertShaderFile);
	
	m_updateShader = glCreateProgram();
	glAttachShader(m_updateShader, vs);

	//specify the data that we will stream back
	const char* varyings[] = { "position", "velocity", "lifetime", "lifespan" };
	glTransformFeedbackVaryings(m_updateShader, 4, varyings, GL_INTERLEAVED_ATTRIBS);

	glLinkProgram(m_updateShader);
	
	//remove unneeded handles
	glDeleteShader(vs);

	//binf the shader so that we can set some uniforms that don't change per frame
	glUseProgram(m_updateShader);

	//bind lifetime minimum and maximum
	int loc = glGetUniformLocation(m_updateShader, "lifeMin");
	glUniform1f(loc, m_lifespanMin);
	loc = glGetUniformLocation(m_updateShader, "lifeMax");
	glUniform1f(loc, m_lifespanMax);
}

void GPUParticleEmitter::Draw(float time, const glm::mat4& a_cameraTransform,
	const glm::mat4& a_projectionView)
{
	//Update the particles using the transform feedback

	glUseProgram(m_updateShader);

	//bind time information
	int loc = glGetUniformLocation(m_updateShader, "time");
	glUniform1f(loc, time);

	float deltaTime = time - m_lastDrawTime; m_lastDrawTime = time;

	loc = glGetUniformLocation(m_updateShader, "deltaTime");

	glUniform1f(loc, deltaTime);

	//Bind emitter's position
	loc = glGetUniformLocation(m_updateShader, "emitterPosition");
	glUniform3fv(loc, 1, &m_position[0]);

	// disable rasterisation
	glEnable(GL_RASTERIZER_DISCARD);

	// bind the buffer we will update
	glBindVertexArray(m_vao[m_activeBuffer]);

	// work out the "other" buffer
	unsigned int otherBuffer = (m_activeBuffer + 1) % 2;

	// bind the buffer we will update into as points
	// and begin transform feedback
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
		m_vbo[otherBuffer]);
	glBeginTransformFeedback(GL_POINTS);

	glDrawArrays(GL_POINTS, 0, m_maxParticles);

	// disable transform feedback and enable rasterization again
	glEndTransformFeedback();
	glDisable(GL_RASTERIZER_DISCARD);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

	// draw the particles using the Geometry Shader to billboard them
	glUseProgram(m_drawShader);

	loc = glGetUniformLocation(m_drawShader, "projectionView");
	glUniformMatrix4fv(loc, 1, false, &a_projectionView[0][0]);

	loc = glGetUniformLocation(m_drawShader, "cameraTransform");
	glUniformMatrix4fv(loc, 1, false, &a_cameraTransform[0][0]);

	// draw particles in the "other" buffer
	glBindVertexArray(m_vao[otherBuffer]);
	glDrawArrays(GL_POINTS, 0, m_maxParticles);

	// swap for next frame
	m_activeBuffer = otherBuffer;
}

void GPUParticleEmitter::CreateDrawShader() {
	
	unsigned int vs = LoadShader(GL_VERTEX_SHADER, m_vertShaderFile);
	unsigned int gs = LoadShader(GL_GEOMETRY_SHADER, "./res/shaders/GPUParticle.geom");
	unsigned int fs = LoadShader(GL_FRAGMENT_SHADER, "./res/shaders/GPUParticle.frag");

	m_drawShader = glCreateProgram();
	glAttachShader(m_drawShader, vs);
	glAttachShader(m_drawShader, fs);
	glAttachShader(m_drawShader, gs);
	glLinkProgram(m_drawShader);

	// remove unneeded handles
	glDeleteShader(vs);
	glDeleteShader(gs);
	glDeleteShader(fs);

	// bind the shader so that we can set
	// some uniforms that don't change per-frame
	glUseProgram(m_drawShader);

	// bind size information for interpolation that won�t change
	int loc = glGetUniformLocation(m_drawShader, "sizeStart");
	glUniform1f(loc, m_startSize);
	loc = glGetUniformLocation(m_drawShader, "sizeEnd");
	glUniform1f(loc, m_endSize);

	// bind colour information for interpolation that wont change
	loc = glGetUniformLocation(m_drawShader, "colourStart");
	glUniform4fv(loc, 1, &m_startColour[0]);
	loc = glGetUniformLocation(m_drawShader, "colourEnd");
	glUniform4fv(loc, 1, &m_endColour[0]);
}
unsigned int GPUParticleEmitter::LoadShader(unsigned int a_type, const char* a_path)
{
	FILE* file = fopen(a_path, "rb");
	if (file == nullptr)
		return 0;

	//read the shader source
	fseek(file, 0, SEEK_END);
	unsigned int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* source = new char[length + 1];
	memset(source, 0, length + 1);
	fread(source, sizeof(char), length, file);
	fclose(file);

	unsigned int shader = glCreateShader(a_type);
	glShaderSource(shader, 1, &source, 0);
	glCompileShader(shader);
	delete[] source;

	return shader;

}