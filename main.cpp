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
#include <queue>
#include <stack>
#include <sstream>
#include <array>
#include <memory>
#include <unordered_map>
#include <utility>
#include <thread>

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

#define SS_INDEX(i) "[" << (i) << "]"

#define MAX_ATLAS_LAYERS 5
#define NUM_ATLAS_LAYERS 4

//------------------------------------------------------------------------------------
// logging, GL error reporting, and an out of place "running" bool...
//------------------------------------------------------------------------------------

static bool g_running = true;

static std::vector<std::string> g_gl_err_msg_cache;

static void exit_on_gl_error(int line, const char* func, const char* expr)
{
    GLenum err = glGetError();

    if (err != GL_NO_ERROR) {
        char msg[256];
        memset(msg, 0, sizeof(msg));

        sprintf(&msg[0], "GL ERROR (%x) in %s@%i [%s]: %s\n", err, func, line, expr,
                (const char* )gluErrorString(err));

        std::string smsg(msg);

        if (std::find(g_gl_err_msg_cache.begin(), g_gl_err_msg_cache.end(), smsg) ==
            g_gl_err_msg_cache.end()) {
            printf("%s", smsg.c_str());
            g_gl_err_msg_cache.push_back(smsg);
        }

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

#ifdef DEBUG
    #define IF_DEBUG(expr) (expr)
#else
    #define IF_DEBUG(expr) 
#endif

#define GL_H(expr) \
do { \
( expr ); \
exit_on_gl_error(__LINE__, __FUNCTION__, #expr); \
} while (0)

#define logf( ... ) logf_impl( __LINE__, __FUNCTION__, __VA_ARGS__ )

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
// atlas generation-specific classes/functions.
//------------------------------------------------------------------------------------

// When we have multiple atlasses, we use layers...
// If a given coord _doesn't_ have either of the two bits 
// set, then it's a part of the first layer, layer 0.

// The layer bits are stored in bits 14 and 15 of the x origin for a given image.

// as you can guess, the maximum amount of supported layers is currently 3.
// an additional 2 layers can easily be supported by using the y-coordinate
// as well, but that's going to add a little more complexity that's not currently worth it.

enum {
    ATLAS_COORDS_LAYER_1 = 1 << 14,
    ATLAS_COORDS_LAYER_2 = 1 << 15,
    ATLAS_COORDS_LAYER_MASK = ATLAS_COORDS_LAYER_1 | ATLAS_COORDS_LAYER_2
};

static void alloc_blank_texture(size_t width, size_t height,
                                uint32_t clear_val);

struct atlas_t {    
    uint8_t desired_bpp = 4;
    uint8_t curr_image = 0;

    uint16_t atlas_width = 1024;
    uint16_t atlas_height = 1024;

    uint16_t max_width = 0;
    uint16_t max_height = 0;

    GLuint img_tex_handle = 0;
    uint32_t num_images = 0;

    std::vector<GLuint> atlas_tex_handles;
    
    std::vector<uint16_t> dims_x;
    std::vector<uint16_t> dims_y;

    std::vector<uint16_t> coords_x;
    std::vector<uint16_t> coords_y;

    std::vector<std::vector<uint8_t>> buffer_table;
    std::vector<std::string> filenames;
    
    std::unordered_map<uint16_t, uint16_t> filled; 
    
    uint16_t origin_x(uint16_t image) const 
    {
        return coords_x[image] & (~ATLAS_COORDS_LAYER_MASK);
    }
    
    uint16_t origin_y(uint16_t image) const 
    {
        return coords_y[image] & (~ATLAS_COORDS_LAYER_MASK);
    }
    
    uint8_t layer(uint16_t image) const
    {        
        auto fetch_coord_layer = [this, &image](uint16_t coord) -> uint16_t {
            switch (coord & ATLAS_COORDS_LAYER_MASK) {
                case 0: return 0;
                case ATLAS_COORDS_LAYER_1: return 1;
                case ATLAS_COORDS_LAYER_2: return 2;
                    
                    // will fire if both bits are set
                default:
                    logf("Layer bits for image %lu are invalid; image is for file %s.\n", 
                         image, 
                         filenames[image].c_str());
                    assert(false);
                    break;
            }
        };
        
        uint16_t ret = fetch_coord_layer(coords_x[image]);
        
        if (ret) {
            ret += fetch_coord_layer(coords_y[image]);
        }
        
        return ret; // for compiler
    }
    
    void maybe_add_layer(uint32_t layer)
    {
        if (atlas_tex_handles.size() > layer)
            return;
       
        uint32_t alloc_start = atlas_tex_handles.size();
        
        atlas_tex_handles.resize(layer + 1, 0);
        
        GL_H( glGenTextures((layer + 1) - alloc_start, 
                            &atlas_tex_handles[alloc_start]) );
        
        // Fill the layers we just allocated...
        for (size_t i = alloc_start; i < atlas_tex_handles.size(); ++i) {
            bind(i);
            
            GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
            GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
            GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
            GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
            
            alloc_blank_texture(atlas_width, atlas_height, 0xFF0000FF);
        }
    }
    
    void set_layer(uint16_t image, uint8_t layer)
    {
        maybe_add_layer(layer);
        
        logf("Switching layer to: %lu", layer);
        
        switch (layer)  {
            case 0:
                coords_x[image] = origin_x(image); // implicit layer bits clear
                coords_y[image] = origin_y(image);
                break;
                
            case 1:
                coords_x[image] |= ATLAS_COORDS_LAYER_1;
                coords_y[image] = origin_y(image);
                break;
            
            case 2:
                coords_x[image] |= ATLAS_COORDS_LAYER_2;
                coords_y[image] = origin_y(image);
                break;
                
            case 3:
                coords_x[image] |= ATLAS_COORDS_LAYER_2;
                coords_y[image] |= ATLAS_COORDS_LAYER_1;
                break;
                
            case 4:
                coords_x[image] |= ATLAS_COORDS_LAYER_2;
                coords_y[image] |= ATLAS_COORDS_LAYER_2;
                break;
            
            default:
                logf("Layer bits for image %lu are invalid; image is for file %s.\n", 
                     image, 
                     filenames[image].c_str());
                assert(false);
                break;
        }
    }
    
    void bind(uint8_t layer) const
    {        
        GL_H( glBindTexture(GL_TEXTURE_2D, atlas_tex_handles[layer]) );
    }

    void bind_image(void) const
    {
        GL_H( glBindTexture(GL_TEXTURE_2D, img_tex_handle) );
    }

    void release(void) const
    {
        GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
    }

    void fill_atlas_image(size_t x, size_t y, size_t image)
    {
        assert(desired_bpp == 4 && "GL RGBA is used...");

        GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                              0,
                              (GLsizei) x,
                              (GLsizei) y,
                              dims_x[image],
                              dims_y[image],
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              &buffer_table[image][0]) );
        
        if (coords_x.size() != num_images)
            coords_x.resize(num_images, 0);
        
        coords_x[image] = (coords_x[image] & ATLAS_COORDS_LAYER_MASK) | ((uint16_t) x);
        
        if (coords_y.size() != num_images)
            coords_y.resize(num_images, 0);
            
        coords_y[image] = (coords_y[image] & ATLAS_COORDS_LAYER_MASK) | ((uint16_t) y);
        
        filled[image] = 1;
    }
    
    void fill_image(size_t image)
    {
        assert(desired_bpp == 4 && "GL RGBA is used...");
        
        GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                              0,
                              0,
                              0,
                              dims_x[image],
                              dims_y[image],
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              &buffer_table[image][0]) );
    }

    
    bool all_filled(void) 
    {
        return filled.size() == num_images;
    }

    void move_image(size_t destx, size_t desty, size_t srcx, size_t srcy, size_t image,
                    uint32_t clear_color)
    {
        assert(desired_bpp == 4 && "clear color is 4 bytes...");

        {
            std::vector<uint32_t> clear_buffer(dims_x[image] * dims_y[image],
                                               clear_color);
            GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                                  0,
                                  (GLsizei) srcx,
                                  (GLsizei) srcy,
                                  dims_x[image],
                                  dims_y[image],
                                  GL_RGBA,
                                  GL_UNSIGNED_BYTE,
                                  &clear_buffer[0]) );
        }

        GL_H( glTexSubImage2D(GL_TEXTURE_2D,
                              0,
                              (GLsizei) destx,
                              (GLsizei) desty,
                              dims_x[image],
                              dims_y[image],
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              &buffer_table[image][0]) );
    }

    bool test_image_bounds_x(size_t x, size_t image) const { return (x + dims_x[image]) < atlas_width; }

    bool test_image_bounds_y(size_t y, size_t image) const { return (y + dims_y[image]) < atlas_height; }

    void free_memory(void)
    {
        {
            GLint curr_bound_tex;
            GL_H( glGetIntegerv(GL_TEXTURE_BINDING_2D, &curr_bound_tex) );

            // We unbind if any of this atlas's textures are bound
            // because a glDelete call on a bound item can't be fulfilled
            // until that item is unbound
            
            bool bound = curr_bound_tex && curr_bound_tex == img_tex_handle;
            for (GLuint handle = 0; handle < atlas_tex_handles.size() && !bound && curr_bound_tex; ++handle) {
                bound = curr_bound_tex == atlas_tex_handles[handle];
            }
            
            if (bound) {
                GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
            }

            // use only one API call since it's more performant
            {
                atlas_tex_handles.push_back(img_tex_handle);
                GL_H( glDeleteTextures(atlas_tex_handles.size(), &atlas_tex_handles[0]) );
            }
        }
        
        img_tex_handle = 0;
        
        curr_image = num_images = 0;
        
        max_width = max_height = 0;
        
        dims_x.clear();
        dims_y.clear();
        coords_x.clear();
        coords_y.clear();
        buffer_table.clear();
        filenames.clear();
        
        atlas_tex_handles.clear();
        
        filled.clear();
    }

    ~atlas_t(void)
    {
        free_memory();
    }
};


//------------------------------------------------------------------------------------
// Sorts images with width ascending, height descending.
// The idea is to produce a grid where every column
// is its own initial width, and each row for that column
// specifically begins at the bottom with the largest
// height placed first. The topmost row of the column
// will contain the image with the smallest height in
// that particular width/column group.
//------------------------------------------------------------------------------------

static std::vector<uint16_t> sort_images(const atlas_t& atlas)
{
    std::vector<uint16_t> sorted(atlas.num_images);

    for (size_t i = 0; i < atlas.num_images; ++i) {
        sorted[i] = i;
    }

    std::sort(sorted.begin(), sorted.end(), [&atlas](uint16_t a, uint16_t b) -> bool {
          if (atlas.dims_x[a] == atlas.dims_x[b]) {
              return atlas.dims_y[a] < atlas.dims_y[b];
          }

          return atlas.dims_x[a] < atlas.dims_x[b];
    });

    return sorted;
}

template <size_t maxLayers>
class gGenLayoutBsp_t
{
    using atlasType_t = atlas_t;
    
    atlasType_t& atlas;

    // Only left child's are capable of storing image indices,
    // from the perspective of the child's parent.
    
    // The "lines" (expressed implicitly) will only have
    // positive normals that face either to the right, or upward.
    
    struct node_t;
    
    using nodePtr_t = std::unique_ptr<node_t>;
    
    struct node_t {
        bool region;

        int16_t image;

        glm::ivec2 origin;
        glm::ivec2 dims;

        node_t* leftChild;
        node_t* rightChild;

        node_t(void)
            :   region(false),
                image(-1),
                origin(0, 0), dims(0, 0),
                leftChild(nullptr), rightChild(nullptr)
        {}
        
        ~node_t(void) 
        {
            if (leftChild)
                delete leftChild;
            
            if (rightChild)
                delete rightChild;
        }
    };

    node_t* Insert(node_t* node, uint16_t image)
    {
        if (node->region) {
            node_t* n = Insert(node->leftChild, image);

            if (n) {
                node->leftChild = n;
				return node;
			}

            n = Insert(node->rightChild, image);

			if (n) {
				node->rightChild = n;
                return node;
            } 
		} else {

            if (node->image >= 0)
                return nullptr;

            glm::ivec2 image_dims(atlas.dims_x[image], atlas.dims_y[image]);

            if (node->dims.x < image_dims.x || node->dims.y < image_dims.y)
                return nullptr;

            if (node->dims.x == image_dims.x && node->dims.y == image_dims.y) {
                node->image = image;
                atlas.fill_atlas_image(node->origin.x, node->origin.y, node->image);
                return node;
            }

            node->region = true;

            uint16_t dx = node->dims.x - image_dims.x;
            uint16_t dy = node->dims.y - image_dims.y;
            
            node->leftChild = new node_t();
            node->rightChild = new node_t();
            
            // Is the partition line vertical?
            if (dx > dy) {
                node->leftChild->dims.x = image_dims.x;
                node->leftChild->dims.y = node->dims.y;
                node->leftChild->origin = node->origin;
                
                node->rightChild->dims.x = dx;
                node->rightChild->dims.y = node->dims.y;
                node->rightChild->origin = node->origin;
                
                node->rightChild->origin.x += image_dims.x;
            
            // Nope, it's horizontal
            } else {
                node->leftChild->dims.x = node->dims.x;
                node->leftChild->dims.y = image_dims.y;
                node->leftChild->origin = node->origin;
                
                node->rightChild->dims.x = node->dims.x;
                node->rightChild->dims.y = dy;
                node->rightChild->origin = node->origin;
                
                node->rightChild->origin.y += image_dims.y;
            }

			// The only way we're able to make it here 
            // is if node's dimensions are >= the image's dimensions.
            // If they're exactly equal, then node would have already been
            // set and this call won't happen.
            // Otherwise, the left child's values are set to values
            // which have already been examined for size, or are
            // set to one of the image's dimension values. 
            
            // That said, an assert is good for true piece of mind...
            node->leftChild = Insert(node->leftChild, image);
		
            assert(node->leftChild);
            
            return node;
        }

        return nullptr;
    }
    
    std::array<nodePtr_t, maxLayers> roots;
    
    uint32_t currLayer;
    
    bool Insert(uint16_t image)
    {
        if (atlas.filled.find(image) != atlas.filled.end()) {
            return true;
        }
        
        node_t* res = Insert(roots[currLayer].get(), image);
        
        //! TODO move this if block out of the function,
        // and replace change the !res check to  
        // !Insert(image)
        if (!res && currLayer < (maxLayers - 1)) {
            currLayer++;
            atlas.set_layer(image, currLayer);
        }
        
        return !!res;
    }    
    
public:
    gGenLayoutBsp_t(atlasType_t& atlas_, bool sorted = false)
        :   atlas(atlas_),
            currLayer(0)
    {
        for (nodePtr_t& root: roots) {
            root.reset(new node_t());
        }
                
        bool isMaxLayer = false;
        
        while (!atlas.all_filled() && !isMaxLayer) {
            atlas.maybe_add_layer(currLayer);
            
            atlas.bind(currLayer);
            roots[currLayer]->dims = glm::ivec2(atlas.atlas_width, atlas.atlas_height);
            if (sorted) {
                std::vector<uint16_t> sorted = sort_images(atlas);
                for (uint16_t i = 0; i < sorted.size(); ++i) {
                    if (!Insert(sorted[i]))
                        break;
                }
            } else {
                for (uint16_t i = 0; i < atlas.num_images; ++i) {
                    if (!Insert(i))
                        break;
                }
            }
            isMaxLayer = currLayer == (maxLayers - 1);
            atlas.release();
        }
        
        logf("Fill Result: %i/%i\n", atlas.filled.size(), atlas.num_images);
    }
};


//------------------------------------------------------------------------------------
// minor texture utils
//------------------------------------------------------------------------------------
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

    atlas.fill_image(atlas.curr_image);
}

//------------------------------------------------------------------------------------
// pixel manipulations
//------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------
// this should be the actual atlas_t ctor, but for now it works.
//------------------------------------------------------------------------------------

void make_atlas(atlas_t& atlas, std::string dirpath, bool sort)
{
    if (*(dirpath.end()) != '/')
        dirpath.append(1, '/');

    DIR* dir = opendir(dirpath.c_str());
    struct dirent* ent = NULL;

    if (!dir) {
        logf("Could not open %s", dirpath.c_str());
        g_running = false;
        return;
    }

    assert(atlas.desired_bpp == 4
           && "Code is only meant to work with textures using desired bpp of 4!");

    atlas.free_memory();
    
    size_t area_accum = 0;

    while (!!(ent = readdir(dir))) {
        std::string filepath(dirpath);
        filepath.append(ent->d_name);

        int dx, dy, bpp;
        stbi_uc* stbi_buffer = stbi_load(filepath.c_str(), &dx, &dy, &bpp,
                                         STBI_default);

        if (!stbi_buffer) {
            logf("Warning: could not open %s. Skipping.", filepath.c_str());
            continue;
        }

        if (bpp != atlas.desired_bpp && bpp != 3) {
            logf("Warning: found invalid bpp value of %i for %s. Skipping.",
                 bpp, filepath.c_str());
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

    atlas.release();

    gGenLayoutBsp_t<MAX_ATLAS_LAYERS> placed(atlas, sort);
    
    logf("Total Images: %lu\nArea Accum: %lu",
         atlas.num_images, area_accum);
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

//------------------------------------------------------------------------------------

#define KEY_PRESS(key) (glfwGetKey(window, (key)) == GLFW_PRESS)

#define ONE_MILLISECOND 1000000 // in nano seconds

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
        if (folder_index < 0)
            folder_index = folders.size() - 1;
        else if (folder_index >= folders.size())
            folder_index = 0;
        
        path = "./textures/" + folders[folder_index];
        
        make_atlas(atlasses[0], path, true);
        make_atlas(atlasses[1], path, false);
        
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
            display_layer = (display_layer + 1) % atlasses[atlas_view_index].atlas_tex_handles.size();
        }
        
        if (KEY_PRESS(GLFW_KEY_N)) {
            display_layer--;
            if (display_layer < 0)
                display_layer = atlasses[atlas_view_index].atlas_tex_handles.size() - 1;
        }
        
        if (atlas_view) {
            // mod is there in case we only decide to store one atlas
            // in the array, for whatever reason
            atlasses[atlas_view_index].bind(display_layer);

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
            
            ss << ": " << path << "; layer " << display_layer;
            
            glfwSetWindowTitle(window, ss.str().c_str());

        } else {
            atlasses[0].bind_image();

            if (KEY_PRESS(GLFW_KEY_RIGHT)) {
                atlasses[0].curr_image++;
                upload_curr_image(atlasses[0]);
                glfwSetWindowTitle(window,
                                   atlasses[0].filenames[atlasses[0].curr_image].c_str());
            }

            if (KEY_PRESS(GLFW_KEY_LEFT)) {
                atlasses[0].curr_image--;
                upload_curr_image(atlasses[0]);
                glfwSetWindowTitle(window,
                                   atlasses[0].filenames[atlasses[0].curr_image].c_str());
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
