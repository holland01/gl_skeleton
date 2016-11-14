/*
 ░░░░░░░░░░░░░▄███▄▄▄░░░░░░░ 
 ░░░░░░░░░▄▄▄██▀▀▀▀███▄░░░░░ 
 ░░░░░░░▄▀▀░░░░░░░░░░░▀█░░░░ 
 ░░░░▄▄▀░░░░░░░░░░░░░░░▀█░░░ 
 ░░░█░░░░░▀▄░░▄▀░░░░░░░░█░░░ 
 ░░░▐██▄░░▀▄▀▀▄▀░░▄██▀░▐▌░░░ 
 ░░░█▀█░▀░░░▀▀░░░▀░█▀░░▐▌░░░ 
 ░░░█░░▀▐░░░░░░░░▌▀░░░░░█░░░ 
 ░░░█░░░░░░░░░░░░░░░░░░░█░░░ 
 ░░░█░░▀▄░░░░▄▀░░░░░░░░█░░░ 
 ░░░░█░░░░░░░░░░░▄▄░░░░█░░░░ 
 ░░░░░█▀██▀▀▀▀██▀░░░░░░█░░░░ 
 ░░░░░█░░▀████▀░░░░░░░█░░░░░ ░
 ░░░░░█░░░░░░░░░░░░▄█░░░░░░ 
 ░░░░░░░██░░░░░█▄▄▀▀░█░░░░░░ 
 ░░░░░░░░▀▀█▀▀▀▀░░░░░░█░░░░░ 
 ░░░░░░░░░█░░░░░░░░░░░░█░░░░
 */

#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
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
        layout(location = 1) in vec2 st;
        layout(location = 2) in vec4 color;
                        
        out vec4 vary_Color;
        out vec2 vary_St;
                            
        void main(void) {
            gl_Position = vec4(position, 1.0);
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
    
    struct vertex_t {
        GLfloat position[3];
        GLfloat st[2];
        uint8_t color[4];
    };
    
    enum {
        COORDS_ROT_90 = 1 << 15
    };
    
    struct atlas_t {
        uint8_t desired_bpp = 4;
        uint8_t curr_image = 0;
        
        uint16_t atlas_width = 2048;
        uint16_t atlas_height = 4096;
        
        uint16_t max_width = 0;
        uint16_t max_height = 0;
        
        GLuint img_tex_handle = 0;
        GLuint atlas_tex_handle = 0;
        uint32_t num_images = 0;
        
        std::vector<uint16_t> dims_x;
        std::vector<uint16_t> dims_y;
        std::vector<uint16_t> coords_x;
        std::vector<uint16_t> coords_y;
        std::vector<std::vector<uint8_t>> buffer_table;
        std::vector<std::string> filenames;
    };
    
    class grid_t
    {
        uint16_t width;
        uint16_t height;
        std::vector<uint8_t> region;
        
    public: 
        grid_t(size_t width_, size_t height_)
            :   width((uint16_t) (width_ & 0xFFFF)),
                height((uint16_t) (height_ & 0xFFFF)),
                region((width_ * height_) >> 3, 0)
        {}
        
        size_t calc_byte(size_t x, size_t y) const
        {
            return (y * (size_t) width + x) >> 3;
        }
        
        size_t calc_shift(size_t x, size_t y) const
        {
            return (y * (size_t) width + x) & 0x7;
        }
        
        bool slot_filled(size_t x, size_t y) const
        {
            return !!(region[calc_byte(x, y)] & (1 << calc_shift(x, y)));
        }
        
        bool subregion_free(size_t a_x, size_t a_y, size_t b_x, size_t b_y) const
        {
            for (size_t y = a_y; y < b_y; ++y) {
                for (size_t x = a_x; x < b_x; ++x) {
                    if (slot_filled(x, y)) {
                        return false;
                    }
                } 
            }
            
            return true;
        }
        
        void fill_subregion(size_t a_x, size_t a_y, size_t b_x, size_t b_y)
        {
            for (size_t y = a_y; y < b_y; ++y) {
                for (size_t x = a_x; x < b_x; ++x) {
                    region[calc_byte(x, y)] |= 1 << calc_shift(x, y);
                } 
            }
        }
    };
    
    static void place_images(atlas_t& atlas)
    {
        
        GL_H( glBindTexture(GL_TEXTURE_2D, atlas.atlas_tex_handle) );
        
        size_t i_x = 0;
        size_t i_y = 0;
        size_t high_y = 0;
        
        grid_t grid(atlas.atlas_width, atlas.atlas_height);
        
        for (size_t image = 0; image < atlas.num_images; ++image) {
            if (grid.subregion_free(i_x, 
                                    i_y, 
                                    i_x + (size_t) atlas.dims_x[image], 
                                    i_y + (size_t) atlas.dims_y[image])) {
                
                if ((size_t) atlas.dims_y[image] > high_y)
                    high_y = (size_t) atlas.dims_y[image];
                
                GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                                      0,
                                      (GLsizei) i_x,
                                      (GLsizei) i_y,
                                      atlas.dims_x[image],
                                      atlas.dims_y[image],
                                      GL_RGBA,
                                      GL_UNSIGNED_BYTE,
                                      &atlas.buffer_table[image][0]) );
                
                grid.fill_subregion(i_x, 
                                    i_y,
                                    i_x + (size_t) atlas.dims_x[image], 
                                    i_y + (size_t) atlas.dims_y[image]);
                
                i_x += (size_t) atlas.dims_x[image];
                i_x &= (size_t) (atlas.atlas_width - 1);
                
                if (i_x == 0) { 
                    i_y += high_y;
                    high_y = 0;
                }
            }
        }
        
        GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
    }
    
    static void alloc_blank_texture(size_t width, size_t height,
                                    uint8_t clear_val)
    {
        std::vector<uint8_t> blank(width * height * 4, clear_val);
        GL_H( glTexImage2D(GL_TEXTURE_2D,
                           0,
                           GL_RGBA8,
                           (GLsizei) width,
                           (GLsizei) height,
                           0,
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           &blank[0]) );
    }
    
    static void upload_curr_image(atlas_t& atlas)
    {
        if (atlas.curr_image >= atlas.num_images)
            atlas.curr_image = 0;
        
        // If the image to be overwritten is larger
        // than the one we're replacing it with,
        // the remaining area will
        // still be occupied by its texels,
        // so we clear the entire buffer first
        alloc_blank_texture(atlas.max_width, atlas.max_height, 255);
        
        GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                              0,
                              0,
                              0,
                              atlas.dims_x[atlas.curr_image],
                              atlas.dims_y[atlas.curr_image],
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              &atlas.buffer_table[atlas.curr_image][0]) );
        
    }
    
    static void convert_rgb_to_rgba(uint8_t* dest, const uint8_t* src, size_t dim_x,
                                    size_t dim_y)
    {
        for (size_t y = 0; y < dim_y; ++y) {
            for (size_t x = 0; x < dim_x; ++x) {
                size_t i = y * dim_x + x;
                dest[i * 4 + 0] = src[i * 3 + 0];
                dest[i * 4 + 1] = src[i * 3 + 1];
                dest[i * 4 + 2] = src[i * 3 + 2];
                dest[i * 4 + 3] = 255;
            }
        }
    }
    
    static uint32_t pack_rgba(uint8_t* rgba)
    {
        return (((uint32_t)rgba[0]) << 0)
            | (((uint32_t)rgba[1]) << 8)
            | (((uint32_t)rgba[2]) << 16)
            | (((uint32_t)rgba[3]) << 24);
    }
    
    static void unpack_rgba(uint8_t* dest, uint32_t src)
    {
        dest[0] = (src >> 0) & 0xFF;
        dest[1] = (src >> 8) & 0xFF;
        dest[2] = (src >> 16) & 0xFF;
        dest[3] = (src >> 24) & 0xFF;
    }
    
    static void swap_rows_rgba(uint8_t* image_data, size_t dim_x, size_t dim_y)
    {
        size_t half_dy = dim_y >> 1;
        for (size_t y = 0; y < half_dy; ++y) {
            for (size_t x = 0; x < dim_x; ++x) {
                size_t top_x = (y * dim_x + x) * 4;
                size_t bot_x = ((dim_y - y - 1) * dim_x + x) * 4;
                uint32_t top = pack_rgba(&image_data[top_x]);
                
                unpack_rgba(&image_data[top_x],
                            pack_rgba(&image_data[bot_x]));

                unpack_rgba(&image_data[bot_x], top);
            }
        }
    }
    
    static atlas_t make_atlas(void)
    {
        DIR* dir = opendir("./textures/gothic_block");
        struct dirent* ent = NULL;
        
        struct atlas_t atlas;
        
        assert(atlas.desired_bpp == 4
               && "Code is only meant to work with textures using desired bpp of 4!");
        
        size_t area_accum = 0;
        
        while (!!(ent = readdir(dir))) {
            char filepath[128];
            memset(filepath, 0, sizeof(filepath));
            strcpy(filepath, "./textures/gothic_block/");
            strcat(filepath, ent->d_name);
        
            int dx, dy, bpp;
            stbi_uc* stbi_buffer = stbi_load(filepath, &dx, &dy, &bpp,
                                             STBI_default);
            
            if (!stbi_buffer) {
                logf("Warning: could not open %s. Skipping.", filepath);
                continue;
            }
            
            if (bpp != atlas.desired_bpp && bpp != 3) {
                logf("Warning: found invalid bpp value of %i for %s. Skipping.",
                     bpp, filepath);
                continue;
            }
            
            atlas.filenames.push_back(std::string(ent->d_name));
            
            std::vector<uint8_t> image_data(dx * dy * atlas.desired_bpp, 0);
            
            if (bpp != atlas.desired_bpp) {
                convert_rgb_to_rgba(&image_data[0], stbi_buffer, dx, dy);
            } else {
                memcpy(&image_data[0], stbi_buffer, dx * dy * atlas.desired_bpp);
            }
            
            if (dx > atlas.max_width)
                atlas.max_width = dx;
            
            if (dy > atlas.max_height)
                atlas.max_height = dy;
            
            area_accum += dx * dy;
            
            atlas.dims_x.push_back(dx);
            atlas.dims_y.push_back(dy);
            
            stbi_image_free(stbi_buffer);
            
            // Reverse image rows, since stb_image treats
            // origin as upper left
            swap_rows_rgba(&image_data[0], dx, dy);
            
            atlas.buffer_table.push_back(std::move(image_data));
            
            atlas.num_images++;
            
        }
        
        closedir(dir);
        
        GL_H( glGenTextures(1, &atlas.img_tex_handle) );
        GL_H( glBindTexture(GL_TEXTURE_2D, atlas.img_tex_handle) );
        
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
        
        upload_curr_image(atlas);
        
        GL_H( glGenTextures(1, &atlas.atlas_tex_handle) );
        GL_H( glBindTexture(GL_TEXTURE_2D, atlas.atlas_tex_handle) );
        
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );

        alloc_blank_texture(atlas.atlas_width, atlas.atlas_height, 0);
        
        GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
        
        place_images(atlas);
        
        logf("Total Images: %lu\nArea Accum: %lu",
             atlas.num_images, area_accum);
        
        return atlas;
    }
}

int main(int argc, const char * argv[])
{
    glfwInit();
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(640, 480, "OpenGL", nullptr, nullptr);
    
    glfwMakeContextCurrent(window);
    
    glewExperimental = true;
    GLenum res = glewInit();
    assert(res == GLEW_OK);
    
    GL_H( glClearColor(0.0f, 0.0f, 0.0f, 1.0f) );
    
    struct atlas_t atlas = make_atlas();
    
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
    
    GL_H( glActiveTexture(GL_TEXTURE0) );
    GL_H( glBindTexture(GL_TEXTURE_2D, atlas.img_tex_handle) );
    
    GL_H( glUniform1i(glGetUniformLocation(program, "sampler0"), 0) );
    
    bool atlas_view = false;
    
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS
           && !glfwWindowShouldClose(window)
           && g_running) {
        
        GL_H( glClear(GL_COLOR_BUFFER_BIT) );
        
        GL_H( glDrawArrays(GL_TRIANGLE_STRIP, 0, 4) );
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            atlas_view = !atlas_view;
        
        if (atlas_view) {
            GL_H( glBindTexture(GL_TEXTURE_2D, atlas.atlas_tex_handle) );
        } else {
            GL_H( glBindTexture(GL_TEXTURE_2D, atlas.img_tex_handle));
            
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                atlas.curr_image++;
                upload_curr_image(atlas);
                glfwSetWindowTitle(window, atlas.filenames[atlas.curr_image].c_str());
            }
            
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                atlas.curr_image--;
                upload_curr_image(atlas);
                glfwSetWindowTitle(window, atlas.filenames[atlas.curr_image].c_str());
            }
        }
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
