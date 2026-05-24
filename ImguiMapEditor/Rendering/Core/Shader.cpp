#include "Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace MapEditor {
namespace Rendering {

Shader::Shader(const std::string& vertex_source, const std::string& fragment_source) {
    std::string vs_error, fs_error;
    
    GLuint const vs = compileShader(GL_VERTEX_SHADER, vertex_source, vs_error);
    if (vs == 0) {
        error_ = "Vertex shader: " + vs_error;
        return;
    }
    
    GLuint const fs = compileShader(GL_FRAGMENT_SHADER, fragment_source, fs_error);
    if (fs == 0) {
        glDeleteShader(vs);
        error_ = "Fragment shader: " + fs_error;
        return;
    }
    
    // Link program
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    
    // Check link status
    GLint success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLint length;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length));
        glGetProgramInfoLog(program_, length, nullptr, log.data());
        error_ = "Link error: " + std::string(log.data());
        
        glDeleteProgram(program_);
        program_ = 0;
    }
    
    // Shaders can be deleted after linking
    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    release();
}

Shader::Shader(Shader&& other) noexcept
    : program_(other.program_)
    , error_(std::move(other.error_))
    , uniform_cache_(std::move(other.uniform_cache_))
{
    other.program_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        program_ = other.program_;
        error_ = std::move(other.error_);
        uniform_cache_ = std::move(other.uniform_cache_);
        other.program_ = 0;
    }
    return *this;
}

void Shader::use() const {
    glUseProgram(program_);
}

void Shader::unbind() {
    glUseProgram(0);
}

void Shader::setInt(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) {
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) {
    glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) {
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

GLint Shader::getUniformLocation(const std::string& name) {
    auto it = uniform_cache_.find(name);
    if (it != uniform_cache_.end()) {
        return it->second;
    }
    
    GLint const location = glGetUniformLocation(program_, name.c_str());
    uniform_cache_[name] = location;
    return location;
}

void Shader::release() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniform_cache_.clear();
}

GLuint Shader::compileShader(GLenum type, const std::string& source, std::string& error) {
    GLuint const shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = std::string(log.data());
        
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

} // namespace Rendering
} // namespace MapEditor
