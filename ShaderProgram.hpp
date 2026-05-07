#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include <GL/glew.h> 
#include <glm/glm.hpp>

#include "non_copyable.hpp"

class ShaderProgram : private NonCopyable {
public:
    ShaderProgram(void) = delete; 

    ShaderProgram(std::string const & vertex_shader_code, std::string const & fragment_shader_code);
    
    static std::shared_ptr<ShaderProgram> from_files(std::filesystem::path const & VS_file, std::filesystem::path const & FS_file) {
        return std::make_shared<ShaderProgram>(textFileRead_static(VS_file), textFileRead_static(FS_file));
    }

    void use(void) {  
        if (ID == currently_used_ID)
            return;
        glUseProgram(ID);
        currently_used_ID = ID;
    };

    void deactivate(void) { 
        glUseProgram(0); 
        currently_used_ID = 0; 
    };   

    ~ShaderProgram(void) {
        deactivate();
        glDeleteProgram(ID);
        ID = 0;
    }
    
    GLuint getID(void) { return ID; }
    GLint  getAttribLocation(const std::string & name);
    
    void setUniform(const std::string & name, const GLfloat val);      
    void setUniform(const std::string & name, const GLint val);        
    void setUniform(const std::string & name, const glm::vec3 & val);  
    void setUniform(const std::string & name, const glm::vec4 & val);  
    void setUniform(const std::string & name, const glm::mat3 & val);   
    void setUniform(const std::string & name, const glm::mat4 & val);
    void setUniform(const std::string & name, const std::vector<GLint> & val);
    void setUniform(const std::string & name, const std::vector<GLfloat> & val);
    void setUniform(const std::string & name, const std::vector<glm::vec3> & val);

private:
    GLuint ID{0}; 
    inline static GLuint currently_used_ID{0};
    std::unordered_map<std::string, GLuint> uniform_location_cache;

    GLuint getUniformLocation(const std::string & name);

    std::string textFileRead(const std::filesystem::path & filename); 
    static std::string textFileRead_static(const std::filesystem::path & filename); 

    GLuint compile_shader(const std::string & source_code, const GLenum type); 
    std::string getShaderInfoLog(const GLuint obj);    

    GLuint link_shader(const std::vector<GLuint> shader_ids); 
    std::string getProgramInfoLog(const GLuint obj);      
};
