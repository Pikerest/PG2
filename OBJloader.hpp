#pragma once

/*
 * Minimal OBJ loader.
 * Supports the triangle-only OBJ subset used by the project assets and
 * converts it into the shared Vertex/index arrays consumed by Mesh.
 */

#include <vector>
#include <filesystem>
#include <GL/glew.h>

#include "assets.hpp"

// Returns false when the file cannot be opened. Parse problems are reported
// and skipped where possible so partially imperfect assets are easier to debug.
bool loadOBJ(const std::filesystem::path& filename,
	         std::vector <Vertex> & vertices,
	         std::vector <GLuint>& indices);
