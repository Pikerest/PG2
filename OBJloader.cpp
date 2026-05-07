#include <string>
#include <algorithm>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "OBJloader.hpp"

bool loadOBJ(const std::filesystem::path& filename, std::vector<Vertex>& vertices, std::vector<GLuint>& indices)
{
	std::cout << "Opening OBJ file: " << filename.string() << std::endl;

	std::vector< glm::vec3 > temp_vertices;
	std::vector< glm::vec2 > temp_uvs;
	std::vector< glm::vec3 > temp_normals;

	vertices.clear();
	indices.clear();

	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "CRITICAL: Could not open file: " << filename << " (Checking in: " << std::filesystem::current_path() << ")" << std::endl;
		return false;
	}

	std::string line;
	int lineNum = 0;
	while (std::getline(file, line)) {
		lineNum++;
		if (line.empty() || line[0] == '#') continue;

		std::stringstream ss(line);
		std::string head;
		ss >> head;

		if (head == "v") {
			glm::vec3 vertex;
			if (!(ss >> vertex.x >> vertex.y >> vertex.z)) {
				std::cerr << "Error parsing vertex at line " << lineNum << std::endl;
				continue;
			}
			temp_vertices.push_back(vertex);
		}
		else if (head == "vt") {
			glm::vec2 uv;
			if (!(ss >> uv.x >> uv.y)) {
				// vt can have 3 components (u, v, w), we only need 2
				ss.clear();
				ss.str(line);
				ss >> head >> uv.x >> uv.y;
			}
			temp_uvs.push_back(uv);
		}
		else if (head == "vn") {
			glm::vec3 normal;
			if (!(ss >> normal.x >> normal.y >> normal.z)) {
				std::cerr << "Error parsing normal at line " << lineNum << std::endl;
				continue;
			}
			temp_normals.push_back(normal);
		}
		else if (head == "f") {
			struct FaceVertex { unsigned int v, t, n; };
			FaceVertex face[3];
			bool hasNormals = true;

			for (int i = 0; i < 3; i++) {
				std::string vertexStr;
				if (!(ss >> vertexStr)) {
					std::cerr << "Error reading face vertex at line " << lineNum << std::endl;
					break;
				}

				unsigned int vIdx = 0, tIdx = 0, nIdx = 0;
				// OBJ faces can be: v, v/t, v//n, v/t/n
				size_t firstSlash = vertexStr.find('/');
				if (firstSlash == std::string::npos) {
					vIdx = std::stoi(vertexStr);
				} else {
					vIdx = std::stoi(vertexStr.substr(0, firstSlash));
					size_t secondSlash = vertexStr.find('/', firstSlash + 1);
					if (secondSlash == std::string::npos) {
						tIdx = std::stoi(vertexStr.substr(firstSlash + 1));
						hasNormals = false;
					} else {
						if (secondSlash > firstSlash + 1) {
							tIdx = std::stoi(vertexStr.substr(firstSlash + 1, secondSlash - firstSlash - 1));
						} else {
							hasNormals = false;
						}
						if (secondSlash + 1 < vertexStr.size()) {
							nIdx = std::stoi(vertexStr.substr(secondSlash + 1));
						} else {
							hasNormals = false;
						}
					}
				}
				face[i] = { vIdx, tIdx, nIdx };
			}

			// Determine face normal for cases without normals
			glm::vec3 faceNormal = glm::vec3(0.0f, 0.0f, 1.0f);
			if (!hasNormals && temp_vertices.size() >= 3) {
				glm::vec3 p0 = temp_vertices[face[0].v - 1];
				glm::vec3 p1 = temp_vertices[face[1].v - 1];
				glm::vec3 p2 = temp_vertices[face[2].v - 1];
				faceNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
			}

			auto compute_fallback_uv = [&](const glm::vec3 &pos, const glm::vec3 &normal) {
				glm::vec3 an = glm::abs(normal);
				if (an.x >= an.y && an.x >= an.z) {
					return glm::vec2((pos.z + 0.5f), (pos.y + 0.5f));
				} else if (an.y >= an.x && an.y >= an.z) {
					return glm::vec2((pos.x + 0.5f), (pos.z + 0.5f));
				} else {
					return glm::vec2((pos.x + 0.5f), (pos.y + 0.5f));
				}
			};

			for (int i = 0; i < 3; i++) {
				Vertex v;
				v.Position = temp_vertices[face[i].v - 1];
				v.Normal = (face[i].n > 0 && face[i].n <= temp_normals.size()) ? temp_normals[face[i].n - 1] : faceNormal;
				v.TexCoords = (face[i].t > 0 && face[i].t <= temp_uvs.size()) ? temp_uvs[face[i].t - 1] : compute_fallback_uv(v.Position, v.Normal);

				auto it = std::find(vertices.begin(), vertices.end(), v);
				if (it == vertices.end()) {
					vertices.push_back(v);
					indices.push_back(vertices.size() - 1);
				} else {
					indices.push_back(static_cast<GLuint>(std::distance(vertices.begin(), it)));
				}
			}
		}
	}

	std::cout << "Successfully loaded: " << filename.string() << " (" << vertices.size() << " vertices, " << indices.size() << " indices)" << std::endl;
	return true;
}
