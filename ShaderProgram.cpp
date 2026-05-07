#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ShaderProgram.hpp"
#include "Mesh.hpp" 

ShaderProgram::ShaderProgram(const std::string & vertex_shader_code, const std::string & fragment_shader_code) {
    auto vertex_shader   = compile_shader(vertex_shader_code, GL_VERTEX_SHADER);
    auto fragment_shader = compile_shader(fragment_shader_code, GL_FRAGMENT_SHADER);

    std::vector<GLuint> shader_ids{vertex_shader, fragment_shader};
    ID = link_shader(shader_ids);
}

GLuint ShaderProgram::getUniformLocation(const std::string & name) {
    if (uniform_location_cache.contains(name)) {
        return uniform_location_cache[name];
    }

    auto loc = glGetUniformLocation(ID, name.c_str());
    if (loc == -1) {
        std::cerr << "No uniform with name: " << name << '\n';
    } else {
        uniform_location_cache[name] = loc;
    }
    return loc;
}

GLint ShaderProgram::getAttribLocation(const std::string & name) {
    GLint loc = glGetAttribLocation(ID, name.c_str());
    if (loc == -1) {
        std::cerr << "No vertex attribute with name: " << name << ", or reserved name (starting with gl_)\n";
    }
    return loc;
}

void ShaderProgram::setUniform(const std::string& name, const GLfloat val) {
    glProgramUniform1f(ID, getUniformLocation(name), val);
}

void ShaderProgram::setUniform(const std::string& name, const GLint val) {
    glProgramUniform1i(ID, getUniformLocation(name), val);
}

void ShaderProgram::setUniform(const std::string& name, const glm::vec3 & val) {
    glProgramUniform3fv(ID, getUniformLocation(name), 1, glm::value_ptr(val));
}

void ShaderProgram::setUniform(const std::string& name, const glm::vec4 & val) {
    glProgramUniform4fv(ID, getUniformLocation(name), 1, glm::value_ptr(val));
}

void ShaderProgram::setUniform(const std::string& name, const glm::mat3 & val) {
	glProgramUniformMatrix3fv(ID, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void ShaderProgram::setUniform(const std::string& name, const glm::mat4 & val) {
	glProgramUniformMatrix4fv(ID, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(val));
}

void ShaderProgram::setUniform(const std::string & name, const std::vector<GLint>& val) {
    glProgramUniform1iv(ID, getUniformLocation(name), val.size(), val.data());
}

void ShaderProgram::setUniform(const std::string & name, const std::vector<GLfloat>& val) {
    glProgramUniform1fv(ID, getUniformLocation(name), val.size(), val.data());
}
   
void ShaderProgram::setUniform(const std::string & name, const std::vector<glm::vec3>& val) {
    glProgramUniform3fv(ID, getUniformLocation(name), val.size(), glm::value_ptr(val[0]));
}
    
std::string ShaderProgram::getShaderInfoLog(const GLuint obj) {
    int log_length = 0;
    std::string s;
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        std::vector<char> v(log_length);
        glGetShaderInfoLog(obj, log_length, nullptr, v.data());
        s.assign(v.begin(), v.end());
    }
    return s;
}

std::string ShaderProgram::getProgramInfoLog(const GLuint obj) {
    int log_length = 0;
    std::string s;
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
        std::vector<char> v(log_length);
        glGetProgramInfoLog(obj, log_length, nullptr, v.data());
        s.assign(v.begin(), v.end());
    }
    return s;
}

GLuint ShaderProgram::compile_shader(const std::string & source_code, const GLenum type) {
    char const *src_cstr = source_code.c_str();
    GLuint shader_ID = glCreateShader(type);
    glShaderSource(shader_ID, 1, &src_cstr, nullptr);
    glCompileShader(shader_ID);
    {
        GLint status;
        glGetShaderiv(shader_ID, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            std::cerr << getShaderInfoLog(shader_ID) << std::endl;
            glDeleteShader(shader_ID);
            throw std::runtime_error("Shader compilation failed.");
        } 
    }
    return shader_ID;
}

GLuint ShaderProgram::link_shader(const std::vector<GLuint> shader_ids) {
	GLuint prog_ID = glCreateProgram();

	for (const auto & id : shader_ids)
		glAttachShader(prog_ID, id);

    glBindAttribLocation(prog_ID, Mesh::attribute_location_position, "position");
    glBindAttribLocation(prog_ID, Mesh::attribute_location_normal, "normal");
    glBindAttribLocation(prog_ID, Mesh::attribute_location_texture_coords, "texture_coords");

	glLinkProgram(prog_ID);

    for (const auto& id : shader_ids) {
        glDetachShader(prog_ID, id);
  		glDeleteShader(id);
  	}

	{ 
        GLint status;
        glGetProgramiv(prog_ID, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            std::cerr << "Error linking shader program." << std::endl;
            std::cerr << getProgramInfoLog(prog_ID) << std::endl;
            glDeleteProgram(prog_ID);
            throw std::runtime_error("Shader linking failed.");
        }
	}
	return prog_ID;
}

std::string ShaderProgram::textFileRead(const std::filesystem::path& filepath) {
    return textFileRead_static(filepath);
}

std::string ShaderProgram::textFileRead_static(const std::filesystem::path& filepath) {
	std::ifstream file(filepath);
	if (!file.is_open())
		throw std::runtime_error(std::string("Error opening file: ") + filepath.string());
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
