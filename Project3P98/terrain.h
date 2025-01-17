#ifndef CS3P98_TERRAIN_H	
#define CS3P98_TERRAIN_H

/*
	Terrain Chunk Class - Single Use 
	*DONT USE WITH TERRAIN CHUNK CACHE IN WORLD CLASS*
	@author Tennyson Demchuk | 6190532 | td16qg@brocku.ca
	@date 02.09.2021
*/

#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <time.h>

// represents a single chunk of terrain in a procedurally generated world
class TerrainChunk {
private:
	// Private vars
	const float scale;						// scale of terrain
	const unsigned int width;				// width of cells grid. square grid so height = width
	const unsigned int halfwidth;			
	const unsigned int v_width;				// width of vertices grid
	const unsigned int vertices_stride;
	const unsigned int num_vertices;		// buffer size information
	const unsigned int num_elements;
	const unsigned int num_faces;
	const unsigned int num_face_elements;
	const unsigned int num_indices;
	const unsigned int vertices_size;		// size of vertices and indices arrays in bytes
	const unsigned int indices_size;
	const float ylvl;						// y level of initial horizontal plane

	float*	vertices		= nullptr;
	int*	indices			= nullptr;
	float*	face_normals	= nullptr;
	float*	face_areas		= nullptr;

	unsigned int VAO, VBO, EBO;				// OpenGL buffer object ID's for this instantiation

	// compute vertex array index of x, z coordinate
	inline int indexOf(int x, int z) {
		return vertices_stride * (z * v_width + x);
	}

public:
	// glob vars
	glm::mat4 model = glm::mat4(1.0f);							// model matrix for this object
	glm::mat4 norm = glm::mat4();
	glm::vec3 color = glm::vec3(0.105f, 0.713f, 0.227f);		// base color for this object - grass green

	/*
		Constructor
		Creates a new TerrainChunk object.
		gridsize should be an even number and is defaulted to 16.
		initialYLevel is defaulted to 0.0
	 */
	TerrainChunk(unsigned int gridsize = 16, float cellscale = 1.0f, float initialYLevel = 0.0f) :
		scale(cellscale),
		width(gridsize),
		halfwidth(width / 2),
		v_width(width + 1),
		vertices_stride(6),
		num_vertices((unsigned int)pow(width + 1, 2)),	// number of vertices in grid
		num_elements(num_vertices* vertices_stride),	// num vertices * (3 floats position + 3 floats normal) floats in vertices array
		num_faces((unsigned int)pow(width, 2) * 2),		// num cells in grid * 2 triangles per cell
		num_face_elements(num_faces * 3),				// num faces * 3 components per normal vector
		num_indices(num_faces* 3),						// num triangles in grid * 3 ints to index each triangle vertex
		vertices_size(num_elements * sizeof(float)),
		indices_size(num_indices * sizeof(int)),
		ylvl(initialYLevel)
	{
		// check for points of contention
		if (width % 2 != 0) printf("WARNING! - Terrain chunk width should be a multiple of 2.\n");

		// allocate buffers
		vertices = new float[num_elements];
		indices = new int[num_indices];
		face_normals = new float[num_face_elements];
		face_areas = new float[num_faces];

		// init normal matrix
		computeNormalMatrix();

		/*
			generate vertices
				+Y			OpenGL 3D Coordinate System			+------+	<-- Grid cell separated into triangles.
				|			(Right Hand Rule)					| \	   |		'+' are vertices
				|____ +X										|	 \ |
				 \												+------+
				  \
				  +Z
		*/
		float vx0 = 0 - (scale * width / 2.0f);	// vertex coords on horizontal XZ plane
		float vx = vx0, vz = vx0;
		unsigned int index = 0;
		for (float y = 0; y < v_width; y++) {
			for (float x = 0; x < v_width; x++) {
				vertices[index++] = vx;			// set vertex XYZ vector components
				vertices[index++] = ylvl;
				vertices[index++] = vz;
				vertices[index++] = 0.0f;		// set default normal vector for vertex
				vertices[index++] = 1.0f;
				vertices[index++] = 0.0f;
				vx += scale;
			}
			vx = vx0;
			vz += scale;
		}

		/*
			generate triangle indices (with CCW winding)

				b --- d		<- (a,b,c,d) are cell vertices relative to current cell
				|  \  |
				c --- a
		*/
		index = 0;
		int a, b, c, d;
		for (unsigned int y = 0; y < width; y++) {		// iterate cells
			for (unsigned int x = 0; x < width; x++) {
				// compute vertex indices
				c = y * v_width + x;			// base vertex index
				a = c + 1;
				b = c + v_width;
				d = a + v_width;
				// add indices for triangle 1 and 2
				indices[index++] = a;
				indices[index++] = b;
				indices[index++] = c;
				indices[index++] = a;
				indices[index++] = d;
				indices[index++] = b;
			}
		}

		/*
			generate initial face normals
		*/
		index = 0;
		while (index < num_face_elements) {
			face_normals[index++] = 0.0f;
			face_normals[index++] = 1.0f;
			face_normals[index++] = 0.0f;
		}

		/*
			set initial face surface areas
		*/
		index = 0;
		while (index < num_faces) { face_areas[index++] = (scale * scale) / 2.0f; }

		// init terrain VAO, VBO, EBO
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		uploadVertexData();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);

		/*
		Vertex Attrib Pointers
		- Enable vertex attrib array by passing in it's unique index to glEnableVertexAttribArray
		- create vertex attrib pointers with:
			glVertexAttribPointer(uint index, int size, GLenum type, bool normalized, int stride, const void* ptr)
			- index = unique index of vertex attribute to be modified
			- size	= # components per vertex attributute (ie. dimensionality of vertex data, can be 1,2,3, or 4)
			- type	= data type of each vector components, usually GL_FLOAT but can also be GL_DOUBLE, GL_INT, GL_BYTE, etc...
			- normalized = whether fixed-point data values should be normalized (GL_TRUE) or converted directly as fixed point vals when accessed
			- stride = The byte offset between consecutive generic vertex attributes. If stride is 0, the vertex attributes are interpreted as tightly packed
			- pointer = offset of the first component of the first vertex attribute in the array in the data store of the buffer currently bound to GL_ARRAY_BUFFER, initially 0.
	*/
		glEnableVertexAttribArray(0);			// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertices_stride * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);			// normal attribute
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertices_stride * sizeof(float), (void*)(3 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);		// unbind VBO (legal but not necessary)
	}

	// destructor - cleanup
	~TerrainChunk() {
		delete[] vertices;
		delete[] indices;
		delete[] face_normals;
		delete[] face_areas;
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
	}

	// augments each vertices height (y component) by an amount given by a sinusoidal function
	void applySinusoidalHeightmap() {
		int index = 1;
		for (unsigned int y = 0; y < v_width; y++) {
			for (unsigned int x = 0; x < v_width; x++) {
				vertices[index] += (float)cos(0.7 * (double)x * scale);
				index += vertices_stride;
			}
		}
	}

	// augments each vertices height (y component) by a random amount
	void applyRandomHeightmap() {
		srand((unsigned int)time(0));								// seed rng and randomly modify terrain
		for (unsigned int i = 1; i < num_elements; i += vertices_stride) {
			vertices[i] = ((float)(rand() % 101) - 50) / 130.0f;
		}
	}

	// compute smoothed vertex normals from surrounding polygon face normals weighted by face angle and surface area
	// BROKEN - DONT USE
	// https://www.bytehazard.com/articles/vertnorm.html
	// smoothed normal = normalize(sum(theta_i * triangle_surface_area_i * triangle_face_normal_i))
	// theta is the angle between the bounding vectors that form each triangle incident with V
	// surface area is the surface area of each triangle face
	// face normal is the normal vector of each triangle face
	/*
		    v2  v3
			+---+
			|\  |\			<-- Upper row triangle faces that are adjacent to vertex V
			|  \|  \
		 v1 +---V---+ v4
			 \  |\  |		<-- Lower row triangle faces that are adjacent to vertex V
			   \|  \|   
				+---+
				v6  v5
	*/
	void computeAngleWeightedSmoothNormals() {
		int x, z;
		int v_index = 0, V_index = 0, flo_index = 0, fhi_index = 0, fn_index = 0;	// vertex, face low, face high, face normal
		glm::vec3 norm;
		glm::vec3 V, A, B, VA, VB;
		glm::vec3 v1, v2, v3, v4, v5, v6;
		for (z = 0; z < (signed int)v_width; z++) {			// iterate vertices
			for (x = 0; x < (signed int)v_width; x++) {
				v_index = indexOf(x, z-1);		// get adjacent and central vertices
				v6 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index += vertices_stride;
				v5 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index += vertices_stride * v_width;
				v4 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index -= vertices_stride;
				V_index = v_index;
				V = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index -= vertices_stride;
				v1 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index += vertices_stride * v_width;
				v2 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);
				v_index += vertices_stride;
				v3 = glm::vec3(vertices[v_index], vertices[v_index + 1], vertices[v_index + 2]);

				v1 = glm::normalize(v1 - V);
				v2 = glm::normalize(v2 - V);
				v3 = glm::normalize(v3 - V);
				v4 = glm::normalize(v4 - V);
				v5 = glm::normalize(v5 - V);
				v6 = glm::normalize(v6 - V);

				// compute base face indices (ie. face ID, remember face_normals have 3 components per face)
				// each vertex has 6 faces connected to it (unless vertex is a border vertex)
				flo_index = 2 * ((z - 1) * width + (x - 1));		// partial lower row triangle 1
				fhi_index = flo_index + 4 * width;				// upper row triangle 1
				flo_index++;										// increment to get lower row triangle 1 index
				norm = glm::vec3(0.0f, 0.0f, 0.0f);

				// compute smoothed normal
				fn_index = 3 * flo_index;		// normal array has 3 components per normal
				norm += acos(glm::dot(v1, v6)) * face_areas[flo_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));
				norm += acos(glm::dot(v6, v5)) * face_areas[flo_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));
				norm += acos(glm::dot(v5, v4)) * face_areas[flo_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));

				fn_index = 3 * fhi_index;
				norm += acos(glm::dot(v1, v6)) * face_areas[fhi_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));
				norm += acos(glm::dot(v1, v6)) * face_areas[fhi_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));
				norm += acos(glm::dot(v1, v6)) * face_areas[fhi_index++] * glm::normalize(glm::vec3(face_normals[fn_index++], face_normals[fn_index++], face_normals[fn_index++]));
				
				norm = glm::normalize(norm);

				// store smoothed normal
				vertices[V_index + 3] = norm.x;
				vertices[V_index + 4] = norm.y;
				vertices[V_index + 5] = norm.z;
			}
		}
	}

	// uses surrounding vertex heights to efficiently compute vertex normal approximations
	void computeSmoothNormalsApproximation() {	// compute smoothed normals from surrounding vertex heights
		int x, z;								// from 'compute_normal' - https://github.com/itoral/gl-terrain-demo/blob/master/src/ter-terrain.cpp#L129
		int index, j;
		float l, r, u, d;
		glm::vec3 norm;
		for (z = 0; z < (signed int)v_width; z++) {			// for each inner vertex in terrain mesh (excluding border vertices), compute height of surrounding terrain and adjust normal
			for (x = 0; x < (signed int)v_width; x++) {
				index = indexOf(x, z);
				j = index - 5;
				l = (j > -1 && j < (signed int)v_width ? vertices[j] : 0.0f);
				j = index + 7;
				r = (j > -1 && j < (signed int)v_width ? vertices[j] : 0.0f);
				j = index + (vertices_stride * v_width) + 1;
				d = (j > -1 && j < (signed int)v_width ? vertices[j] : 0.0f);
				j = index - (vertices_stride * v_width) + 1;
				u = (j > -1 && j < (signed int)v_width ? vertices[j] : 0.0f);

				norm = glm::normalize(glm::vec3(l - r, 2.0f, d - u));
				vertices[index + 3] = norm.x;
				vertices[index + 4] = norm.y;
				vertices[index + 5] = norm.z;
			}
		}
	}

	// iterates faces and updates normals
	// BROKEN
	void computeFaceNormals() {
		/*
			triangle indices (with CCW winding)

				b --- d		<- (a,b,c,d) are cell vertices relative to current cell
				|  \  |
				c --- a
		*/
		int n_index = 0, a_index = 0;
		int zShift = width + 1;					// # indices that represent an ip/down shift on the z axis
		int a, b, c, d;
		glm::vec3 A, B, C, D, AB, AC, AD, norm;
		for (unsigned int z = 0; z < width; z++) {		// iterate cells
			for (unsigned int x = 0; x < width; x++) {
				// compute vertex indices
				c = indexOf(x, z);				// base index in vertices array
				a = c + vertices_stride;
				b = c + zShift;
				d = a + zShift;

				// get vectors
				A = glm::vec3(vertices[a], vertices[a + 1], vertices[a + 2]);
				B = glm::vec3(vertices[b], vertices[b + 1], vertices[b + 2]);
				C = glm::vec3(vertices[c], vertices[c + 1], vertices[c + 2]);
				D = glm::vec3(vertices[d], vertices[d + 1], vertices[d + 2]);
				AB = B - A;
				AC = C - A;
				AD = D - A;

				norm = glm::normalize(glm::cross(AB, AC));		// compute normal and area for triangle 1
				face_areas[a_index++] = glm::length(norm) / 2.0f;
				face_normals[n_index++] = norm.x;
				face_normals[n_index++] = norm.y;
				face_normals[n_index++] = norm.z;
				norm = glm::normalize(glm::cross(AD, AB));		// compute normal and area for triangle 2
				face_areas[a_index++] = glm::length(norm) / 2.0f;
				face_normals[n_index++] = norm.x;
				face_normals[n_index++] = norm.y;
				face_normals[n_index++] = norm.z;
			}
		}
	}

	// returns unit width of terrain chunk grid
	unsigned int width() { return width; }

	// returns half unit width of terrain chunk grid
	unsigned int halfwidth() { return halfwidth; }

	// Computes the normal matrix for this objects current model matrix
	// this should be called whenever the model matrix is modified
	void computeNormalMatrix() {
		norm = glm::mat3(glm::transpose(glm::inverse(model)));
	}

	// Binds this objects VAO for drawing
	// Must be called at least once prior to drawing
	void bindVAO() {
		glBindVertexArray(VAO);
	}

	// uploads vertex data to graphics card
	void uploadVertexData() {
		bindVAO();
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices_size, vertices, GL_STATIC_DRAW);		// upload data to graphics card
	}

	// uploads this objects norm matrix to shader program
	// must be called at least once before drawing
	void uploadNormalMatrix(Shader& shader) {
		shader.setMat3("normalMatrix", norm);
	}

	// uploads this objects model matrix to shader program
	// must be called at least once before drawing
	// should be called whenever model matrixis changed
	void uploadModel(Shader& shader) {
		shader.setMat4("model", model);
	}

	// uploads this objects color vector to shader program
	// must be called at least once before drawing
	void uploadColor(Shader& shader) {
		shader.setVec3("objcolor", color.x, color.y, color.z);
	}

	/*
		Draws terrain mesh to the screen
	*/
	void draw() {
		glDrawElements(GL_TRIANGLES, indices_size, GL_UNSIGNED_INT, 0);
	}
};

#endif