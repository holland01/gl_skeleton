#ifndef __GL_ATLAS_H__
#define __GL_ATLAS_H__

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

namespace gl_atlas {
        
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
#ifdef GL_ATLAS_MAIN
            g_running = false;
#endif
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
    #define IF_DEBUG(expr) expr
#else
    #define IF_DEBUG(expr) 
#endif

#ifdef DEBUG
    #define GL_H(expr) \
do { \
( expr ); \
exit_on_gl_error(__LINE__, __FUNCTION__, #expr); \
} while (0)
#else
    #define GL_H(expr) expr
#endif
    
#define logf( ... ) logf_impl( __LINE__, __FUNCTION__, __VA_ARGS__ )
    
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
        
        uint16_t width = 1024;
        uint16_t height = 1024;
        
        uint16_t max_width = 0;
        uint16_t max_height = 0;
        
        GLuint img_tex_handle = 0;
        uint32_t num_images = 0;
        
        std::vector<GLuint> layer_tex_handles;
        
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
            if (layer_tex_handles.size() > layer)
                return;
            
            uint32_t alloc_start = layer_tex_handles.size();
            
            layer_tex_handles.resize(layer + 1, 0);
            
            GL_H( glGenTextures((layer + 1) - alloc_start, 
                                &layer_tex_handles[alloc_start]) );
            
            // Fill the layers we just allocated...
            for (size_t i = alloc_start; i < layer_tex_handles.size(); ++i) {
                bind(i);
                
                GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
                GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
                GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
                GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );
                
                alloc_blank_texture(width, height, 0xFF0000FF);
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
            GL_H( glBindTexture(GL_TEXTURE_2D, layer_tex_handles[layer]) );
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
        
        bool test_image_bounds_x(size_t x, size_t image) const { return (x + dims_x[image]) < width; }
        
        bool test_image_bounds_y(size_t y, size_t image) const { return (y + dims_y[image]) < height; }
        
        void free_memory(void)
        {
            {
                GLint curr_bound_tex;
                GL_H( glGetIntegerv(GL_TEXTURE_BINDING_2D, &curr_bound_tex) );
                
                // We unbind if any of this atlas's textures are bound
                // because a glDelete call on a bound item can't be fulfilled
                // until that item is unbound
                
                bool bound = curr_bound_tex && curr_bound_tex == img_tex_handle;
                for (GLuint handle = 0; handle < layer_tex_handles.size() && !bound && curr_bound_tex; ++handle) {
                    bound = curr_bound_tex == layer_tex_handles[handle];
                }
                
                if (bound) {
                    GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
                }
                
                // use only one API call since it's more performant
                {
                    layer_tex_handles.push_back(img_tex_handle);
                    GL_H( glDeleteTextures(layer_tex_handles.size(), &layer_tex_handles[0]) );
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
            
            layer_tex_handles.clear();
            
            filled.clear();
        }
        
        ~atlas_t(void)
        {
            free_memory();
        }
    };
    
    
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
    class gen_layout_bsp
    {
        using atlas_type_t = atlas_t;
        
        atlas_type_t& atlas;
        
        // Only left child's are capable of storing image indices,
        // from the perspective of the child's parent.
        
        // The "lines" (expressed implicitly) will only have
        // positive normals that face either to the right, or upward.
        
        struct node_t;
        
        using node_ptr_t = std::unique_ptr<node_t>;
        
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
        
        std::array<node_ptr_t, maxLayers> roots;
        
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
        gen_layout_bsp(atlas_type_t& atlas_, bool sorted = false)
        :   atlas(atlas_),
        currLayer(0)
        {
            for (node_ptr_t& root: roots) {
                root.reset(new node_t());
            }
            
            bool isMaxLayer = false;
            
            while (!atlas.all_filled() && !isMaxLayer) {
                atlas.maybe_add_layer(currLayer);
                
                atlas.bind(currLayer);
                roots[currLayer]->dims = glm::ivec2(atlas.width, atlas.height);
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
        
        gen_layout_bsp<MAX_ATLAS_LAYERS> placed(atlas, sort);
        
        logf("Total Images: %lu\nArea Accum: %lu",
             atlas.num_images, area_accum);
    }
    
} // namespace gl_atlas

#endif
