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
#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.c"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/type_ptr.hpp>
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
            printf("GL ERROR (%x) in %s@%i [%s]: %s\n", err, func, line, expr,
                   (const char* )glewGetErrorString(err));
            
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
        
        void bind(void) const
        {
            GL_H( glBindTexture(GL_TEXTURE_2D, atlas_tex_handle) );
        }
        
        void bind_image(void) const
        {
            GL_H( glBindTexture(GL_TEXTURE_2D, img_tex_handle) );
        }
        
        void release(void) const
        {
            GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
        }
        
        void fill_image(size_t x, size_t y, size_t image) const
        {
            GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                                  0,
                                  (GLsizei) x,
                                  (GLsizei) y,
                                  dims_x[image],
                                  dims_y[image],
                                  GL_RGBA,
                                  GL_UNSIGNED_BYTE,
                                  &buffer_table[image][0]) );
        }
        
        bool test_image_bounds_x(size_t x, size_t image) const { return  (x + dims_x[image]) < atlas_width; }
        
        bool test_image_bounds_y(size_t y, size_t image) const { return (y + dims_y[image]) < atlas_height; }
    };
    
    class gridset_t
    {
        uint16_t width;
        uint16_t height;
        std::vector<uint8_t> region;
        
    public: 
        gridset_t(size_t width_, size_t height_)
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
    
    static std::vector<uint16_t> sort_images(const atlas_t& atlas)
    {
        std::vector<uint16_t> sorted(atlas.num_images);
        
        for (size_t i = 0; i < atlas.num_images; ++i) {
            sorted[i] = i;
        }
        
        std::sort(sorted.begin(), sorted.end(), [&atlas](uint16_t a, 
                                                         uint16_t b) -> bool {
            if (atlas.dims_x[a] == atlas.dims_x[b]) {
                return atlas.dims_y[a] < atlas.dims_y[b];
            }
            
            return atlas.dims_x[a] < atlas.dims_x[b];
        });
        
        return sorted;
    }
    
    static void place_images0(atlas_t& atlas)
    {
        size_t i_x = 0;
        size_t i_y = 0;
        size_t high_y = 0;
        
        gridset_t grid(atlas.atlas_width, atlas.atlas_height);
        
        atlas.bind();
        for (size_t image = 0; image < atlas.num_images; ++image) {
            if (grid.subregion_free(i_x, 
                                    i_y, 
                                    i_x + (size_t) atlas.dims_x[image], 
                                    i_y + (size_t) atlas.dims_y[image])) {
                
                if ((size_t) atlas.dims_y[image] > high_y)
                    high_y = (size_t) atlas.dims_y[image];
                
                atlas.fill_image(i_x, i_y, image);
                
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
        atlas.release();
    }
    
    static void place_images1(const atlas_t& atlas) 
    {
        std::vector<uint16_t> sorted = sort_images(atlas);
        
        size_t last_width = (size_t) atlas.dims_x[sorted[0]];
        
        size_t i_x = 0;
        size_t i_y = 0;
        
        atlas.bind();
        
        size_t images_used = 0;
        
        // TODO: Since the base structure is laid out with the following loop,
        // the next step is to eliminate the most dead space and shove 
        // the atlas as far to the direction with the most dead space as 
        // possible: this will allow for a more open space to exist.
        
        // If the dead space which is the most (in either height or width 
        // or total area - which takes priority out of these three measurements
        // still needs to be determined) "dead" or unused contains any images,
        // then try to disperse those images in other empty areas. 
        
        // The dead space should first be looked at in terms of just columns 
        // and rows before moving onto anything more complicated. 
        
        // So, for example, if there's a column with like 2 images and 
        // no other column has less than those two images, then 
        // the next thing to do would be to see if there are any
        // other columns with just two images: the one 
        // with the most width would be the kicker, which you could then 
        // remove the images from and squish the adjacent columns against 
        // each other with, thus closing the dead area.
        
        // The images which had been fetched could then see if they can be 
        // combined, maybe, with the other column containing only two images.
        // Since these images will have larger widths (the ones which were removed)
        // a rotation could be applied to these images to see if they'll fit better
        // (though this isn't guaranteed it will work, since their height when used 
        // as width may still extend into an adjacent column).
        
        // Ultimately this may end up introducing a large number of tests until every image
        // is properly placed into the atlas. There may be some more implicit
        // methods that can be used, though, to improve performance.
        
        // If there's a chunk of unused area which _can't_ hold a given image 
        // without intersecting another image, try rotating it by 90 degrees
        // and then retesting before moving on.
        
        
        
        for (size_t image = 0; image < sorted.size(); ++image) {
            if (last_width != atlas.dims_x[sorted[image]]) {
                i_y = 0;
                i_x += last_width;
                last_width = atlas.dims_x[sorted[image]];
            }
            
            if (!atlas.test_image_bounds_y(i_y, sorted[image])) {
                i_y = 0;
                i_x += last_width;
            }
            
            if (!atlas.test_image_bounds_x(i_x, sorted[image]))
                break;
            
            atlas.fill_image(i_x, i_y, sorted[image]);
            images_used++;
            
            if (last_width == atlas.dims_x[sorted[image]]) {
                i_y += atlas.dims_y[sorted[image]];
            }
        }
        atlas.release();
        
        logf("images used: %llu/%lu", images_used, atlas.num_images);
    }
    
    static void alloc_blank_texture(size_t width, size_t height,
                                    uint32_t clear_val)
    {
        std::vector<uint32_t> blank(width * height, clear_val);
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
        alloc_blank_texture(atlas.max_width, atlas.max_height, 0xFFFFFFFF);
        
        atlas.fill_image(0, 0, atlas.curr_image);
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
        
        atlas.bind_image();
        
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
        
        upload_curr_image(atlas);
        
        GL_H( glGenTextures(1, &atlas.atlas_tex_handle) );
        
        atlas.bind();
        
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
        GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );

        alloc_blank_texture(atlas.atlas_width, atlas.atlas_height, 0xFF0000FF);
        
        atlas.release();
        
        place_images1(atlas);
        
        logf("Total Images: %lu\nArea Accum: %lu",
             atlas.num_images, area_accum);
        
        return atlas;
    }
}

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
        origin.z -= t;
    }
    
    glm::mat4 model_to_view(void)
    {
        view[3] = glm::vec4(-origin, 1.0f);
        return view;
    }
    
    const glm::mat4& view_to_clip(void) const { return projection; }
    
    uint16_t view_width(void) const { return screen_width; }
    
    uint16_t view_height(void) const { return screen_height; }
};

#define KEY_PRESS(key) (glfwGetKey(window, (key)) == GLFW_PRESS)

int main(int argc, const char * argv[])
{
    glfwInit();
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);
 
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
    
    camera.perspective(40.0f, 0.01f, 10.0f);
    camera.walk(-3.0f);
    
    const float CAMERA_STEP = 0.05f;
    
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
        
        if (KEY_PRESS(GLFW_KEY_UP))
            atlas_view = !atlas_view;
        
        if (KEY_PRESS(GLFW_KEY_W)) camera.walk(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_S)) camera.walk(-CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_A)) camera.strafe(-CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_D)) camera.strafe(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_SPACE)) camera.raise(CAMERA_STEP);
        if (KEY_PRESS(GLFW_KEY_LEFT_SHIFT)) camera.raise(-CAMERA_STEP);
        
        if (atlas_view) {
            atlas.bind();
        } else {
            atlas.bind_image();
            
            if (KEY_PRESS(GLFW_KEY_RIGHT)) {
                atlas.curr_image++;
                upload_curr_image(atlas);
                glfwSetWindowTitle(window, atlas.filenames[atlas.curr_image].c_str());
            }
            
            if (KEY_PRESS(GLFW_KEY_LEFT)) {
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
