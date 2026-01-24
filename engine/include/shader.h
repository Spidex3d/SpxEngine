#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    // Construct and build the shader from file paths
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    // Use / bind the shader program
    void Use() const;

    // convenience uniform setters
    void SetUniformVec3(const char* name, float x, float y, float z) const;
    void SetUniformFloat(const char* name, float value) const;
    void SetUniformInt(const char* name, int value) const; // set an int uniform

    void setVec2(const std::string& name, const glm::vec2 value) const;
    void setVec2(const std::string& name, float x, float y) const;   
    // ------------------------------------------------------------------------
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    // ######################### My Vec4 ###########################################
    void setRGBAVec4(const std::string& name, float r, float g, float b, float a);
    // ------------------------------------------------------------------------
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setVec4(const std::string& name, float x, float y, float z, float w);
    // ------------------------------------------------------------------------
    void setMat2(const std::string& name, const glm::mat2& mat) const;
    // ------------------------------------------------------------------------
    void setMat3(const std::string& name, const glm::mat3& mat) const;  
    // ------------------------------------------------------------------------
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    


    GLuint ID() const { return programID; }

private:
    // private member can only be accessed by member functions and not outside the class
    GLuint programID;
    // Helper functions for shader compilation and file reading
    bool CompileShader(const char* source, GLenum shaderType, GLuint& outShader) const;
    // Read shader source code from file
    std::string ReadFile(const std::string& path) const;
};

