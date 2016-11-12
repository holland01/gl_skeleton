//
//  main.cpp
//  atlasxcode
//
//  Created by holland schutte on 11/11/16.
//  Copyright © 2016 holland schutte. All rights reserved.
//

#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <string>
#include "stb_image.c"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>

#define SHADER(str) "#version 410 core\n"#str"\n"

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
    
    void logf_impl( int line, const char* func, const char* fmt, ... )
    {
        va_list arg;
        
        va_start( arg, fmt );
        fprintf( stdout, "\n[ %s@%i ]: ", func, line );
        vfprintf( stdout, fmt, arg );
        fputs( "\n", stdout );
        va_end( arg );
    }
}

#define GL_H(expr) \
    do { \
        ( expr ); \
        exit_on_gl_error(__LINE__, __FUNCTION__, #expr); \
    } while (0)

#define logf( ... ) logf_impl( __LINE__, __FUNCTION__, __VA_ARGS__ )

extern "C" {
    
    static const char* GLSL_VERTEX_SHADER = SHADER(
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec4 color;
        
        out vec4 vary_Color;
        void main(void) {
            gl_Position = vec4(position, 1.0);
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
    
    static GLuint compile_shader(const char* shader_src, GLenum shader_type)
    {
        GLuint shader;
        GL_H( shader = glCreateShader(shader_type) );
        GL_H( glShaderSource(shader, 1, &shader_src, NULL) );
        GL_H( glCompileShader(shader) );
        
        GLint compile_success;
        GL_H( glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_success) );
        
        if (compile_success == GL_FALSE) {
            GLint info_log_len;
            GL_H( glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_len) );
            
            std::vector<char> log_msg(info_log_len + 1, 0);
            GL_H( glGetShaderInfoLog(shader, (GLsizei)(log_msg.size() - 1),
                                     NULL, &log_msg[0]) );
            
            logf("COMPILE ERROR: %s\n\nSOURCE\n\n---------------\n%s\n--------------",
                 &log_msg[0], shader_src);
            
            return 0;
        }
        
        return shader;
    }
    
    static GLuint link_program(const char* vertex_src, const char* fragment_src)
    {
        GLuint vertex, fragment;
        
        vertex = compile_shader(vertex_src, GL_VERTEX_SHADER);
        if (!vertex)
            goto fail;
        
        fragment = compile_shader(fragment_src, GL_FRAGMENT_SHADER);
        if (!fragment)
            goto fail;
    
        {
            GLuint program;
            GL_H( program = glCreateProgram() );
            
            GL_H( glAttachShader(program, vertex) );
            GL_H( glAttachShader(program, fragment) );
            
            GL_H( glLinkProgram(program) );
            
            GL_H( glDetachShader(program, vertex) );
            GL_H( glDetachShader(program, fragment) );
            
            GL_H( glDeleteShader(vertex) );
            GL_H( glDeleteShader(fragment) );
            
            GLint link_success;
            GL_H( glGetProgramiv(program, GL_LINK_STATUS, &link_success) );
            
            if (link_success == GL_FALSE) {
                GLint info_log_len;
                GL_H( glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_len) );
                
                std::vector<char> log_msg(info_log_len + 1, 0);
                GL_H( glGetProgramInfoLog(program, (GLsizei)(log_msg.size() - 1),
                                         NULL, &log_msg[0]) );
                
                logf("LINK ERROR:\n Program ID: %lu\n Error: %s",
                     program, &log_msg[0]);
                
                goto fail;
            }
            
            return program;
        }
        
    fail:
        g_running = false;
        return 0;
    }
    
    struct vertex_t {
        GLfloat position[3];
        GLfloat st[2];
        uint8_t color[4];
    };
}

int main(int argc, const char * argv[])
{
    load_atlas();
    
    glfwInit();
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL", nullptr, nullptr);
    
    glfwMakeContextCurrent(window);
    
    glewExperimental = true;
    GLenum res = glewInit();
    assert(res == GLEW_OK);
    
    GL_H( glClearColor(0.0f, 0.0f, 0.0f, 1.0f) );
    
    GLuint program = link_program(GLSL_VERTEX_SHADER, GLSL_FRAGMENT_SHADER);
    
    GLuint vao;
    GL_H( glGenVertexArrays(1, &vao) );
    GL_H( glBindVertexArray(vao) );
    
    GLuint vbo;
    GL_H( glGenBuffers(1, &vbo) );
    GL_H( glBindBuffer(GL_ARRAY_BUFFER, vbo) );
    
    struct vertex_t vbo_data[] = {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 255, 0, 0, 255 } },
        { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0, 255, 0, 255 } },
        { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0, 0, 255, 255 } },
        { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 255, 0, 255, 255 } }
    };
    
    GL_H( glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), &vbo_data[0],
                       GL_STATIC_DRAW) );
    
    GL_H( glEnableVertexAttribArray(0) );
    GL_H( glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_data[0]),
                                (GLvoid*) offsetof(vertex_t, position)) );
    
    GL_H( glEnableVertexAttribArray(1) );
    GL_H( glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vbo_data[0]),
                                (GLvoid*) offsetof(vertex_t, color)) );
    
    GL_H( glUseProgram(program) );
    
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS
           && !glfwWindowShouldClose(window)
           && g_running) {
        
        GL_H( glClear(GL_COLOR_BUFFER_BIT) );
        
        GL_H( glDrawArrays(GL_TRIANGLE_STRIP, 0, 4) );
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    GL_H( glUseProgram(0) );
    GL_H( glDeleteProgram(program) );
    
    GL_H( glBindBuffer(GL_ARRAY_BUFFER, 0) );
    GL_H( glDeleteBuffers(1, &vbo) );
    
    GL_H( glBindVertexArray(0) );
    GL_H( glDeleteVertexArrays(1, &vao) );
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
