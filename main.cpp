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

// NOTE:
// It's likely that initially several iterations of the find min, clear min, replace min cycle will be used.
// You'll need to find a good heuristic for when this can "stop". It could be as soon as there's no possible
// means of appending a cleared column unto another one; at this point, a new stage would be initiated and something
// more granular would be attempted.


// TODO:
// finish up the replace min stage, which is supposed to find a suitable location
// for the column after consolodation: this initially will involve looking for a
// column it can append itself unto - likely one with the most width, least height, at first: the smallest
// width would likely provide little remaining room for other images. The smallest width could be a good
// fallback though.

// Note that, in the current test set of images, the far left column which has the
// cleared column appended to it won't be valid, because its width is _smaller_ than the
// the cleared column's width. The solution to this would be to take the max width
// between the cleared column and the next column with the least amount of images,
// since that's the column which the cleared column will be appended onto.

// The region consolodation phase should then be updated so that the following is performed:
// 1) the append between the cleared column and its next min counter part happens.
// 2) if the top portion of the newly constructed column contains images with a larger width,
//      then flip the column so that the smaller-width images stay on top.
// 3) boundries are computed, and consolodation as necessary is performed to remove wasted space.
//

// TODO:
// Management is becoming fairly crazy, so it's important to add some more abstraction.
// The space_tree_t class should act as a server for insert/shrink/remove, etc.
// operations. Make sure to support operations which are both vertical and horizontal
// in terms of insertion and removal. Other operations such as enabling/disabling
// move and used flags should be transferred over to this class: at the moment
// it makes sense to restrict this functionality to parent regions only.

// BUG: current reimplementation for consolidating columns produces shoddy results. Bad overlapping is the result,
// and the positioning seems pretty off. the mapping between the column indices and the shift partitioning scheme
// is off; there's no guarantee that i is in (right, N) or (left, right)


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
			   (const char* )glewGetErrorString(err));

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
//
// There's 3 major classes of importance:
//
// * atlas_t - used to store the actual atlas-related data
// * gridset_t - a "bitset"-based grid useful for keeping track of regions.
// * place_images1 - performs the major processing/generation of the atlas itself.
//------------------------------------------------------------------------------------

// Bit flag which tells whether or not
// a particular image is rotated by 90 degs.
// we can store about 2 bits of information
// in each coord, since the max atlas dim size
// worth going is 8192. So, we can use about
// four flags total.

// Technically speaking, since atlas
// dims are always powers of two only,
// we'd have 15 bits free, with only
// one bit being used for the size.

// There's no need for that at this time,
// though.
enum {
	ATLAS_COORDS_ROT_90 = 1 << 15
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

	// these two are used during genartion
	std::vector<uint16_t> dims_x;
	std::vector<uint16_t> dims_y;

	// these two aren't used during generation,
	// but need to be written to afterward
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

	bool test_image_bounds_x(size_t x, size_t image) const { return  (x + dims_x[image]) < atlas_width; }

	bool test_image_bounds_y(size_t y, size_t image) const { return (y + dims_y[image]) < atlas_height; }

	void free_texture_data(void)
	{
		if (!!img_tex_handle || !!atlas_tex_handle) {
			GLint curr_bound_tex;
			GL_H( glGetIntegerv(GL_TEXTURE_BINDING_2D, &curr_bound_tex) );

			if ((GLuint) curr_bound_tex == img_tex_handle
				|| (GLuint) curr_bound_tex == atlas_tex_handle) {
				GL_H( glBindTexture(GL_TEXTURE_2D, 0) );
			}

			GL_H( glDeleteTextures(1, &img_tex_handle) );
			GL_H( glDeleteTextures(1, &atlas_tex_handle) );

			img_tex_handle = 0;
			atlas_tex_handle = 0;
		}
	}

	~atlas_t(void)
	{
		free_texture_data();
	}
};

//------------------------------------------------------------------------------------

// a -> start origin
// b -> end origin
struct subregion_t {
	enum {
		SUBREGION_UNSET = 0xFFFF,

		// flags:
		SUBREGION_USED = 1 << 0,
		SUBREGION_MOVING = 1 << 1
	};

	const subregion_t* parent = nullptr;

	uint16_t child_buffer = SUBREGION_UNSET;
	uint16_t image_buffer = 0;

	uint16_t flags = 0;

	uint16_t a_x = 0;
	uint16_t a_y = 0;
	uint16_t b_x = 0;
	uint16_t b_y = 0;

	uint16_t padding = 0;

	template <uint16_t bitflag>
	void set_bitflag(bool flag)
	{
		if (flag) {
			flags |= bitflag;
		} else {
			flags &= ~bitflag;
		}
	}

	uint16_t width(void) const { return b_x - a_x; }

	uint16_t height(void) const { return b_y - a_y; }

	uint16_t parent_start_x(void) const
	{
        uint16_t x = 0;
		if (parent) {
			x += parent->start_x();
		}
		return x;
	}

	uint16_t parent_start_y(void) const
	{
        uint16_t y = 0;
		if (parent) {
			y += parent->start_y();
		}
		return y;
	}

	uint16_t start_x(void) const { return parent_start_x() + a_x; }

	uint16_t start_y(void) const { return parent_start_y() + a_y; }

	uint16_t end_x(void) const { return parent_start_x() + b_x; }

	uint16_t end_y(void) const { return parent_start_y() + b_y; }

	bool leaf(void) const { return child_buffer == SUBREGION_UNSET; }

	bool used(void) const { return !leaf() && !!(flags & SUBREGION_USED); }

	bool moving(void) const { return !leaf() && !!(flags & SUBREGION_MOVING); }

	void make_leaf(void) { child_buffer = SUBREGION_UNSET; }

	void set_used(bool flag) { set_bitflag<SUBREGION_USED>(flag); }

	void set_moving(bool flag) { set_bitflag<SUBREGION_MOVING>(flag); }

	void set_start_x(uint16_t a_x)
	{
		uint16_t w = width();
		this->a_x = a_x;
		this->b_x = a_x + w;
	}

	void set_start_y(uint16_t a_y)
	{
		uint16_t h = height();
		this->a_y = a_y;
		this->b_y = a_y + h;
	}
};

using subregion_ptr_t = std::unique_ptr<subregion_t>;
using subregion_cont_t = std::vector<subregion_ptr_t>;

class region_man_t
{
	std::vector<subregion_cont_t> child_data;

	void shrink_to_fit_x(subregion_t* parent)
	{
		uint16_t max_w = 0;
		uint16_t max_h = 0;

		for (size_t i = 0; i < child_data[parent->child_buffer].size(); ++i) {
			if (child_data[parent->child_buffer][i]->width() > max_w) {
				max_w = child_data[parent->child_buffer][i]->width();
			}

			max_h += child_data[parent->child_buffer][i]->height();
		}

		if (parent->height() > max_h) {
			parent->b_y = parent->a_y + max_h;
		}

		if (parent->width() > max_w) {
			parent->b_x = parent->a_x + max_w;
		}
	}

public:
	void push_child_y(subregion_t* parent, subregion_ptr_t child)
	{
		if (parent->leaf()) {
			parent->child_buffer = child_data.size();
            child_data.push_back(subregion_cont_t());
		}

		child->parent = parent;
        child_data[parent->child_buffer].push_back(std::move(child));
	}

	subregion_ptr_t pop_child(subregion_t* parent)
	{
		subregion_ptr_t c = std::move(
			child_data[parent->child_buffer][child_data[parent->child_buffer]
				.size() - 1]
		);

		child_data[parent->child_buffer].pop_back();

		shrink_to_fit_x(parent);

        return c;
	}

	void stack(subregion_t* top, subregion_t* bottom)
	{
		size_t y_offset = (*(child_data[bottom->child_buffer].end() - 1))->b_y;

		subregion_cont_t& top_buffer = child_data[top->child_buffer];

		for (size_t i = 0; i < top_buffer.size(); ++i) {
			subregion_ptr_t child = std::move(top_buffer[i]);
			child->a_y += y_offset;
			child->b_y += y_offset;
			push_child_y(bottom, std::move(child));
		}

		// FIXME: erasing top.child_buffer from child_data
		// will cause a buffer overflow that results in
		// unintended subregions being transferred
		// over to bottom's child buffer. This may be
		// something related to the STL, and it needs to be looked at.
		child_data[top->child_buffer].clear();
	}

	const subregion_cont_t& operator [](uint16_t x) const
	{ return child_data[x]; }
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

	std::sort(sorted.begin(), sorted.end(), [&atlas](uint16_t a, uint16_t b)
		-> bool {
		if (atlas.dims_x[a] == atlas.dims_x[b]) {
			return atlas.dims_y[a] > atlas.dims_y[b];
		}

		return atlas.dims_x[a] < atlas.dims_x[b];
	});

	return sorted;
}

//------------------------------------------------------------------------------------
// gen_layout: the actual algorithm for generating the atlas positions.
//------------------------------------------------------------------------------------


// flags for choosing optional modifications to make - this is
// more or less for testing purposes.
enum {
	GEN_LAYOUT_CLEAR_MIN_WIDTH_COLUMN = 1 << 0,
	GEN_LAYOUT_CONSOLODATE_COLUMNS_HORIZONTALLY = 1 << 1,
	GEN_LAYOUT_REPLACE_MIN_WIDTH_COLUMN = 1 << 2,

	GEN_LAYOUT_ALL_STAGES = GEN_LAYOUT_CLEAR_MIN_WIDTH_COLUMN
		| GEN_LAYOUT_CONSOLODATE_COLUMNS_HORIZONTALLY
		| GEN_LAYOUT_REPLACE_MIN_WIDTH_COLUMN
};

//--------------
//    NOTE
//--------------
// An important property of the generation mechanism
// is that the coordinates used are
// based on actual texel locations which would exist within
// the atlas, at least as far as the GL client API implies.

// This means that, for subregion_t instances, the b_x and b_y members
// represent ending coordinates in the same sense that a NULL terminator
// represents the end of a c-string. So, the last (x, y) coordinate pair for an image
// will actually be (b_x - 1, b_y - 1), due to zero indexing.

class gen_layout
{
	std::vector<uint16_t> sorted;
	std::vector<subregion_ptr_t> subregions;

	const atlas_t& atlas;

	region_man_t tree;

	// Lay out as many images as possible using the sorted indices.
	// If we get to a point where a column's height is too tall
	// (in the sense that it exceeds our atlas height), we attempt to
	// take the remaining heights to be placeed within the group
	// and generate a separate adjacent column with them.
	// We also keep track of our placement using the "gridset"
	void place_sorted_images(void)
	{
		size_t last_width = (size_t) atlas.dims_x[sorted[0]];
		size_t images_used = 0;
		size_t i_x = 0;
		size_t i_y = 0;

		size_t col = -1;

		auto add_column = [&col, &i_x, &last_width, this](void) -> void
		{
			col++;
			subregions.push_back(subregion_ptr_t(new subregion_t()));
            subregions[col]->a_x = i_x;
            subregions[col]->b_x = i_x + last_width;
		};

		add_column();

		for (uint16_t sorted_img_index: sorted) {
			if (last_width != atlas.dims_x[sorted_img_index]) {
				i_y = 0;
				i_x += last_width;
				last_width = atlas.dims_x[sorted_img_index];
				add_column();
			}

			if (!atlas.test_image_bounds_y(i_y, sorted_img_index)) {
				i_y = 0;
				i_x += last_width;
				add_column();
			}

			// Images which aren't used are guaranteed to not
			// exceed the height of the atlas vertically; this simplifies
			// generation at later stages because we don't have to worry about
			// vertical positioning being out of bounds when making adjustments
			// to the initial layout constructed from this function.

			subregion_ptr_t child(new subregion_t());

			// Let the x space be relative
			child->a_x = (uint16_t) 0;
			child->a_y = (uint16_t) i_y;
			child->b_x = (uint16_t) atlas.dims_x[sorted_img_index];
			child->b_y = (uint16_t) i_y + atlas.dims_y[sorted_img_index];

			child->image_buffer = sorted_img_index;

			if (atlas.test_image_bounds_x(i_x, sorted_img_index)) {

                uint16_t b_y = child->b_y;
                tree.push_child_y(subregions[col].get(), std::move(child));

                subregions[col]->b_y = b_y;
				subregions[col]->set_used(true);

				images_used++;
			}

			if (last_width == atlas.dims_x[sorted_img_index]) {
				i_y += atlas.dims_y[sorted_img_index];
			}
		}

		logf("images used: %llu/%lu", images_used, atlas.num_images);
	}


	uint16_t get_smallest_column(void)
	{
		uint16_t min_index = 0;
		uint16_t min_count = atlas.num_images;

		size_t i = 0;
		for (const subregion_ptr_t& parent: subregions) {
			if (parent->used()
			&& tree[parent->child_buffer].size() < min_count) {
				min_count = tree[parent->child_buffer].size();
				min_index = i;
			}

			++i;
		}

		return min_index;
	}

	void consolidate_x(void)
	{
		uint16_t clear = get_smallest_column();

		subregions[clear]->set_used(false);

		uint16_t next_clear = get_smallest_column();

		subregions[next_clear]->set_used(false);

		uint16_t top, bot;

		if (subregions[next_clear]->width() > subregions[clear]->width()) {
			top = clear;
			bot = next_clear;
		} else {
			top = next_clear;
			bot = clear;
		}

		tree.stack(subregions[top].get(), subregions[bot].get());

		uint16_t left, right;

		if (subregions[next_clear]->a_x < subregions[clear]->a_x) {
			right = clear;
			left = next_clear;
		} else {
			right = next_clear;
			left = clear;
		}

		// shift everything in the range (a, b) to the left
		// by one width slot
		auto shift_range = [this](size_t a, size_t b) -> size_t
		{
			size_t accum_x = 0;
			for (size_t i = a + 1; i < b; ++i) {
				if (subregions[i]->used()) {
					size_t w = subregions[i]->width();
					subregions[i]->a_x = subregions[a]->a_x + accum_x;
					subregions[i]->b_x = subregions[i]->a_x + w;
					accum_x += w;
				}
			}
			return accum_x;
		};

		// shift everything between left cleared and right cleared regions
		shift_range(left, right);

		// shift all regions after farthest cleared region
		size_t far_dist = shift_range(right, subregions.size());

		subregions[top]->make_leaf();

		subregions[bot]->set_used(true);

		// reposition new region
		size_t w = subregions[bot]->width();
		subregions[bot]->a_x = subregions[right]->a_x + far_dist;
		subregions[bot]->b_x = subregions[bot]->a_x + w;

		if (subregions[bot]->b_x >= atlas.atlas_width
			|| subregions[bot]->b_y >= atlas.atlas_height) {
			// TODO: create a new, separate atlas which chunks
			// off this data.
			subregions[bot]->set_used(false);
		}
	}

	void upload_used_images(void)
	{
		atlas.bind();
        for (const subregion_ptr_t& r: subregions) {
            if (r->used()) {
                for (const subregion_ptr_t& c: tree[r->child_buffer]) {
                    atlas.fill_image(c->start_x(), c->start_y(), c->image_buffer);
				}
			}
		}
		atlas.release();
	}

	void print_subregions(void)
	{
		static size_t print_count = 0;

		std::stringstream ss;

		ss  << "\n-------------SUBREGIONS"
			<< SS_INDEX(print_count++)
			<< "-------------\n";

		size_t i = 0;
		for (const subregion_ptr_t& s: subregions) {
			ss  << "\t" << SS_INDEX(i)
				<< "{ used: " << std::to_string(s->flags)
				<< ", a_x: " << s->start_x()
				<< ", a_y: " << s->start_y()
                << ", b_x: " << s->b_x
                << ", b_y: " << s->b_y
				<< " }\n";
			i++;
		}

		logf("%s", ss.str().c_str());
	}

public:
	gen_layout(const atlas_t& atlas_, uint16_t flags = GEN_LAYOUT_ALL_STAGES)
    :   sorted(sort_images(atlas_)),
		atlas(atlas_)
	{
		place_sorted_images();

		if (!!(flags & GEN_LAYOUT_CONSOLODATE_COLUMNS_HORIZONTALLY))
			consolidate_x();

		upload_used_images();
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

	atlas.fill_image(0, 0, atlas.curr_image);
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

static atlas_t make_atlas(std::string dirpath,
						  uint16_t gen_layout_flags = GEN_LAYOUT_ALL_STAGES)
{
	if (*(dirpath.end()) != '/')
		dirpath.append(1, '/');

	DIR* dir = opendir(dirpath.c_str());
	struct dirent* ent = NULL;

	struct atlas_t atlas;

	if (!dir) {
		logf("Could not open %s", dirpath.c_str());
		g_running = false;
		return atlas;
	}

	assert(atlas.desired_bpp == 4
		   && "Code is only meant to work with textures using desired bpp of 4!");

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

	GL_H( glGenTextures(1, &atlas.atlas_tex_handle) );

	atlas.bind();

	GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
	GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR) );
	GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE) );
	GL_H( glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE) );

	alloc_blank_texture(atlas.atlas_width, atlas.atlas_height, 0xFF0000FF);

	atlas.release();

	gen_layout placed(atlas, gen_layout_flags);

	logf("Total Images: %lu\nArea Accum: %lu",
		 atlas.num_images, area_accum);

	return atlas;
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

	size_t atlas_view_index = 0;
	std::array<atlas_t, 2> atlasses = {{
		make_atlas("./textures/base_wall"),
		make_atlas("./textures/base_wall", 0)
	}};

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
	GL_H( glBindTexture(GL_TEXTURE_2D, atlasses[0].img_tex_handle) );

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
			atlasses[atlas_view_index].bind();

			if (KEY_PRESS(GLFW_KEY_RIGHT)) {
				atlas_view_index ^= 0x1;
			}

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
	}

	GL_H( glUseProgram(0) );
	GL_H( glDeleteProgram(program) );

	GL_H( glBindBuffer(GL_ARRAY_BUFFER, 0) );
	GL_H( glDeleteBuffers(1, &vbo) );

	GL_H( glBindVertexArray(0) );
	GL_H( glDeleteVertexArrays(1, &vao) );

	for (atlas_t& atlas: atlasses)
		atlas.free_texture_data();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
