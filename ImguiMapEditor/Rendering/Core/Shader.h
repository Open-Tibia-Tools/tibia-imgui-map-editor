#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace MapEditor {
namespace Rendering {

/**
 * RAII wrapper for OpenGL shader program
 * Compiles and links vertex/fragment shaders
 */
class Shader {
public:
    Shader() = default;
    
    /**
     * Create shader from source code
     * @param vertex_source GLSL vertex shader source
     * @param fragment_source GLSL fragment shader source
     */
    Shader(const std::string& vertex_source, const std::string& fragment_source);
    
    ~Shader();
    
    // Move-only
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    // Use this shader program
    void use() const;
    static void unbind() ;
    
    // Uniform setters
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);
    
    // Accessors
    GLuint id() const { return program_; }
    bool isValid() const { return program_ != 0; }
    
    // Get compilation/link errors
    const std::string& getError() const { return error_; }

private:
    GLint getUniformLocation(const std::string& name);
    void release();
    
    static GLuint compileShader(GLenum type, const std::string& source, std::string& error);
    
    GLuint program_ = 0;
    std::string error_;
    std::unordered_map<std::string, GLint> uniform_cache_;
};

} // namespace Rendering
} // namespace MapEditor
