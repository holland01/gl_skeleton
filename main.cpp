#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <string>
#include "stb_image.c"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>

#define SHADER(str) "#version 330 core\n"#str"\n"

extern "C" {
    
    static bool g_running = true;
    
    static void exit_on_gl_error(int line, const char* func, const char* expr)
    {
        GLenum err = glGetError();
        
        if (err != GL_NO_ERROR) {
            printf("in %s@%i [%s]: %s\n", func, line, expr,
                   (const char* )glewGetString(err));
            
            g_running = false;
        }
    }
}

#define GL_H(expr) \
    do { \
        ( expr ); \
        exit_on_gl_error(__LINE__, __FUNCTION__, #expr); \
    } while (0)

extern "C" {
    
    static const char* GLSL_VERTEX_SHADER = SHADER(
        layout(location = 0) in vec2 position;
        layout(location = 1) in vec4 color;
        
        out vec4 vary_Color;
        void main(void) {
            gl_Position = vec4(position, 0.0, 1.0);
            vary_Color = color;
        }
    );
    
    static const char* GLSL_FRAGMENT_SHADER = SHADER(
        in vec4 vary_Color;
        out vec4 out_Fragment;
        
        void main(void) {
            out_Fragment = vary_Color;
        }
    );
    
    static GLint compile_shader(const char* shader_src)
    {
        // TODO  
    }
}

int main(int argc, const char * argv[])
{
    glfwInit();
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL", nullptr, nullptr);
    
    glfwMakeContextCurrent(window);
    
    glewExperimental = true;
    GLenum res = glewInit();
    assert(res == GLEW_OK);
    
    GL_H( glClearColor(1.0f, 0.0f, 0.0f, 1.0f) );
    
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS
           && !glfwWindowShouldClose(window)
           && g_running) {
        GL_H( glClear(GL_COLOR_BUFFER_BIT) );
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
