#define GL_ATLAS_MAIN
#define GL_ATLAS_EXTRAS

static bool g_running = true;

#include "gl_atlas.h"

#include <glm/gtx/string_cast.hpp>

#ifdef GL_ATLAS_EXTRAS

using namespace gl_atlas;


//------------------------------------------------------------------------------------
// shader progs and related subroutines
//------------------------------------------------------------------------------------

static const char* GLSL_VERTEX_SHADER = SHADER(
                                               layout(location = 0) in vec3 position;
                                               layout(location = 1) in vec2 st;
                                               layout(location = 2) in vec4 color;
                                               
                                               uniform mat4 modelView;
                                               uniform mat4 projection;
                                               
                                               out vec4 vary_Color;
                                               out vec2 vary_St;
                                               
                                               void main(void) {
                                                   gl_Position = projection * modelView * vec4(position, 1.0);
                                                   vary_Color = color;
                                                   vary_St = st;
                                               }
                                               );

static const char* GLSL_FRAGMENT_SHADER = SHADER(
                                                 in vec4 vary_Color;
                                                 in vec2 vary_St;
                                                 out vec4 out_Fragment;
                                                 
                                                 uniform sampler2D sampler0;
                                                 
                                                 void main(void) {
                                                     out_Fragment = vary_Color * vec4(texture(sampler0, vary_St).rgb, 1.0);
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

//------------------------------------------------------------------------------------
// typical graphics structures
//
// The camera class is here because it helps with
// looking at specific details in the atlas at a closer or farther
// range than its initial position in view space.
//------------------------------------------------------------------------------------

class camera_t
{
    uint16_t screen_width, screen_height;

    glm::vec3 origin;
    glm::mat4 view;
    glm::mat4 projection;

public:
    camera_t(uint16_t screen_w, uint16_t screen_h)
    :   screen_width(screen_w), screen_height(screen_h),
    origin(0.0f),
    view(1.0f), projection(1.0f)
    {}

    void perspective(float fovy, float znear, float zfar)
    {
        projection = glm::perspective(glm::radians(fovy),
                                      ((float) screen_width) / ((float) screen_height),
                                      znear, zfar);
    }

    void strafe(float t)
    {
        origin.x += t;
    }

    void raise(float t)
    {
        origin.y += t;
    }

    void walk(float t)
    {
        origin.z -= t; // negative z is the forward axis
    }

    glm::mat4 model_to_view(void)
    {
        view[3] = glm::vec4(-origin, 1.0f); // flip for view space translation
        return view;
    }

    const glm::mat4& view_to_clip(void) const { return projection; }

    uint16_t view_width(void) const { return screen_width; }

    uint16_t view_height(void) const { return screen_height; }
};

struct vertex_t {
    GLfloat position[3];
    GLfloat st[2];
    GLubyte color[4];
};

template <class clampType>
static clampType clamp_circ(clampType x, clampType min, clampType max) 
{
    if (x < min) {
        x = max;
    } else if (x >= max) {
        x = min;
    }
    
    return x;
}

//------------------------------------------------------------------------------------

#define KEY_PRESS(key) (glfwGetKey(window, (key)) == GLFW_PRESS)

#define ONE_MILLISECOND 1000000 // in nano seconds

#ifdef GL_ATLAS_MAIN

int main(int argc, const char * argv[])
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
   // glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);

    camera_t camera(640, 480);
    GLFWwindow* window = glfwCreateWindow(camera.view_width(),
                                          camera.view_height(),
                                          "OpenGL",
                                          nullptr, nullptr);

    glfwMakeContextCurrent(window);

    glewExperimental = true;
    GLenum res = glewInit();
    assert(res == GLEW_OK);

    GL_H( glClearColor(0.0f, 0.0f, 0.0f, 1.0f) );

    size_t atlas_view_index = 0;

    GLuint program = link_program(GLSL_VERTEX_SHADER, GLSL_FRAGMENT_SHADER);

    GLuint vao;
    GL_H( glGenVertexArrays(1, &vao) );
    GL_H( glBindVertexArray(vao) );

    GLuint vbo;
    GL_H( glGenBuffers(1, &vbo) );
    GL_H( glBindBuffer(GL_ARRAY_BUFFER, vbo) );

    struct vertex_t vbo_data[] = {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 255, 255, 255, 255 } },
        { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 255, 255, 255, 255 } },
        { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 255, 255, 255, 255 } },
        { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 255, 255, 255, 255 } }
    };

    GL_H( glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), &vbo_data[0],
                       GL_STATIC_DRAW) );

    GL_H( glEnableVertexAttribArray(0) );
    GL_H( glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_data[0]),
                                (GLvoid*) offsetof(vertex_t, position)) );

    GL_H( glEnableVertexAttribArray(1) );
    GL_H( glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_data[0]),
                                (GLvoid*) offsetof(vertex_t, st)) );

    GL_H( glEnableVertexAttribArray(2) );
    GL_H( glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vbo_data[0]),
                                (GLvoid*) offsetof(vertex_t, color)) );

    GL_H( glUseProgram(program) );

    GL_H( glUniform1i(glGetUniformLocation(program, "sampler0"), 0) );

    bool atlas_view = false;

    camera.perspective(40.0f, 0.01f, 10.0f);
    camera.walk(-3.0f);

    const float CAMERA_STEP = 0.05f;
    
    int16_t curr_image = 0;
    
    std::vector<std::string> folders = {{
        "aedm7",
        "assaultQ",
        "base_button",
        "base_ceiling",
        "base_door",
        "base_floor",
        "base_light",
        "base_object",
        "base_support",
        "base_trim",
        "base_wall",
        "bata3dm1",
        "common",
        "ctf",
        "effects",
        "force",
        "gothic_block",
        "gothic_button",
        "gothic_cath",
        "gothic_ceiling",
        "gothic_door",
        "gothic_floor",
        "gothic_light",
        "gothic_trim",
        "gothic_wall",
        "liquids",
        "marsbase",
        "moon",
        "moteof",
        "organics",
        "proto",
        "sfx",
        "skies",
        "skin",
        "stone",
        "tim",
        "water",
        "xlab_doorQ"
    }};
    
    int32_t folder_index = 4;
    
    int16_t display_layer = 0;
    
    std::string path;
    
    std::array<atlas_t, 2> atlasses;
    
    auto lset_images = [&atlasses, &folders, &path, &folder_index](void)
    {
        if (folder_index < 0) {
            folder_index = folders.size() - 1;
        } else if (folder_index >= folders.size()) {
            folder_index = 0;
        }
        
        path = "./textures/" + folders[folder_index];
        
        make_atlas(atlasses[0], path, true);
        //make_atlas(atlasses[1], path, false);
        
        GL_H( glActiveTexture(GL_TEXTURE0) );
        GL_H( glBindTexture(GL_TEXTURE_2D, atlasses[0].img_tex_handle) );
    
        std::stringstream ss;
        uint16_t i = 0;
        for (const std::string& fname: atlasses[0].filenames) {
            ss << SS_INDEX(i) << fname << "\n";
            ++i;
        }
        
        logf("Image Filenames:\n%s", ss.str().c_str());
    };
    
    lset_images();
    
    while (!KEY_PRESS(GLFW_KEY_ESCAPE)
           && !glfwWindowShouldClose(window)
           && g_running) {

        GL_H( glUniformMatrix4fv(glGetUniformLocation(program, "modelView"),
                                 1, GL_FALSE, glm::value_ptr(camera.model_to_view())) );

        GL_H( glUniformMatrix4fv(glGetUniformLocation(program, "projection"),
                                 1, GL_FALSE, glm::value_ptr(camera.view_to_clip())) );

        GL_H( glClear(GL_COLOR_BUFFER_BIT) );
        GL_H( glDrawArrays(GL_TRIANGLE_STRIP, 0, 4) );

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (KEY_PRESS(GLFW_KEY_LEFT_BRACKET)) {
            folder_index--;
            lset_images();
        } 
        else if (KEY_PRESS(GLFW_KEY_RIGHT_BRACKET)) {
            folder_index++;
            lset_images();
        }
        
        if (KEY_PRESS(GLFW_KEY_UP))
            atlas_view = !atlas_view;

        if (KEY_PRESS(GLFW_KEY_W)) camera.walk(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_S)) camera.walk(-CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_A)) camera.strafe(-CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_D)) camera.strafe(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_SPACE)) camera.raise(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_LEFT_SHIFT)) camera.raise(-CAMERA_STEP);
        
        if (KEY_PRESS(GLFW_KEY_M)) {
            display_layer = (display_layer + 1) % atlasses[atlas_view_index].layer_tex_handles.size();
        }
        
        if (KEY_PRESS(GLFW_KEY_N)) {
            display_layer--;
            if (display_layer < 0)
                display_layer = atlasses[atlas_view_index].layer_tex_handles.size() - 1;
        }
        
        if (atlas_view) {
            // mod is there in case we only decide to store one atlas
            // in the array, for whatever reason
            atlasses[atlas_view_index & 0x0].bind(display_layer);

            if (KEY_PRESS(GLFW_KEY_RIGHT)) {
                atlas_view_index ^= 0x1;
            }
            
            std::stringstream ss;
            
            switch (atlas_view_index) {
                case 0:
                    ss << "(sorted)";
                    break;
                case 1:
                    ss << "(unsorted)";
                    break;
                default:
                    ss << "(unknown)";
                    break;
            }
            
            ss << ": " << path << "; layer " << display_layer
            << "; dims " << glm::to_string(glm::ivec2(atlasses[atlas_view_index].widths[display_layer], 
                                                      atlasses[atlas_view_index].heights[display_layer]));
            
            glfwSetWindowTitle(window, ss.str().c_str());

        } else {
            atlasses[0].bind_image();

            if (KEY_PRESS(GLFW_KEY_RIGHT)) {
                curr_image = clamp_circ<int16_t>(curr_image + 1, 0, atlasses[0].num_images);
                clear_image(atlasses[0], curr_image);
                glfwSetWindowTitle(window,
                                   atlasses[0].filenames[curr_image].c_str());
            }

            if (KEY_PRESS(GLFW_KEY_LEFT)) {
                curr_image = clamp_circ<int16_t>(curr_image - 1, 0, atlasses[0].num_images);
                clear_image(atlasses[0], curr_image);
                glfwSetWindowTitle(window,
                                   atlasses[0].filenames[curr_image].c_str());
            }
        }
        
        std::this_thread::sleep_for(std::chrono::nanoseconds(ONE_MILLISECOND * 100));
        
    }

fin:
        
    GL_H( glUseProgram(0) );
    GL_H( glDeleteProgram(program) );

    GL_H( glBindBuffer(GL_ARRAY_BUFFER, 0) );
    GL_H( glDeleteBuffers(1, &vbo) );

    GL_H( glBindVertexArray(0) );
    GL_H( glDeleteVertexArrays(1, &vao) );

    for (atlas_t& atlas: atlasses)
        atlas.free_memory();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#endif // GL_ATLAS_MAIN

#endif // GL_ATLAS_EXTRAS
