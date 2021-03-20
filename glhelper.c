#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "glhelper.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// list of the names of the vertex attributes
static const char* attributes[] = {
    "vPos",
    "vNormal",
    "vTexCoord"
};

FT_Library ft;

int readFile(char* filename, int* size,char **content) {
	FILE* file = fopen(filename, "rb");
    // get file size
	fseek(file, 0, SEEK_END);
	long f_size = ftell(file);
	fseek(file, 0, SEEK_SET);
    // allocate memory to store file
	*content = malloc((f_size + 1) * sizeof(char));
	if(!*content) {
		printf("ERROR: readFile, unable to allocate memory");
		return -1;
	}
    // store file's content
	fread(*content, f_size, 1, file);
	fclose(file);
    // null terminate
    (*content)[f_size] = '\0';
	*size = f_size;
    // debug info
    printf("read file %s, content:\n\033[36m%s\033[0m\n", filename, *content);
	return 0;
}

int loadShader(char* filename, int type, GLuint *shader) {    
    char* source;
    int size;
    // read source file and store content in source
    if(readFile(filename, &size, &source) != 0) {
        return -1;
    }; 
    // because glShaderSource only accepts const char*
    const char* shader_source = source;
    // create, source and compile shader
    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &shader_source, NULL);
    glCompileShader(*shader);
    // retreive debug info
    GLint status;
    GLenum error;
    int logsLength;
    error = glGetError();
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logsLength);
    char log[logsLength+1];
    memset(log, '\0', logsLength+1);
    glGetShaderInfoLog(*shader, logsLength, NULL, log);
    // print debug info
    printf("compiled shader %s, compile status: %i (GL_TRUE: %i), error: %i, logs: [%i]\n%s\n", filename, status, GL_TRUE, error, logsLength, log);
    // free precedentaly allocated memory for source
    free(source);
    return 0;
}

int initProgram(char* fragSourceFilename, char* vertSourceFilename, GLuint *program) {
    // declare shaders
    GLuint vert;
    GLuint frag;
    // load them
    loadShader(vertSourceFilename, GL_VERTEX_SHADER, &vert);
    loadShader(fragSourceFilename, GL_FRAGMENT_SHADER, &frag);
    // attaches the shaders to the program
    glAttachShader(*program, vert);
    glAttachShader(*program, frag);
    // get debug info
    GLint status;
    GLenum error;
    int logsLength;
    glLinkProgram(*program);
    error = glGetError();
    glGetProgramiv(*program, GL_LINK_STATUS, &status);
    glGetProgramiv(*program, GL_INFO_LOG_LENGTH, &logsLength);
    char logs[logsLength+1];
    memset(logs, '\0', logsLength+1);
    glGetProgramInfoLog(*program, logsLength, NULL, logs);
    // print it
    printf("linking status: %i, error: %i, logs: [%i] \n%s\n", status, error, logsLength, logs);
    return 0;
}

void GlhTransformsToMat4(struct GlhTransforms *tsf, mat4 *mat) {
    glm_mat4_identity(*mat);
    // don't ask me why, but that is the correct order
    glm_translate(*mat, tsf->translation);
    glm_rotate_x(*mat, tsf->rotation[0], *mat);
    glm_rotate_y(*mat, tsf->rotation[1], *mat);
    glm_rotate_z(*mat, tsf->rotation[2], *mat);
    glm_scale(*mat, tsf->scale);
}

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)()) {
    int attributesCount = sizeof(attributes) / sizeof(attributes[0]);
    // init shader program
    prg->shaderProgram = glCreateProgram();
    // set attribute location to the indexes of the attributes array
    for(int i = 0; i < attributesCount; i++) {
        glBindAttribLocation(prg->shaderProgram, i, attributes[i]);
    }
    // read, load compile attach and link shaders to program
    initProgram(fragSourceFilename, vertSourceFilename, &prg->shaderProgram);
    // initialze vectors
    vector_init(&prg->uniforms, uniformsCount, sizeof(char*));
    vector_init(&prg->uniformsLocation, uniformsCount, sizeof(GLint));
    vector_init(&prg->attributes, attributesCount, sizeof(char*));
    // push the fixed size arrays' values into the vectors
    vector_push_array(prg->uniforms, uniforms, uniformsCount);
    //* NOTE: the attribute vector is not really needed, as it will always be the 
    //* same as the attributes global array but i choosed to keep it that way if
    //* i ever come arround to implement custom attributes for whatever reasons
    vector_push_array(prg->attributes, attributes, attributesCount);
    // get the uniforms' location
    for(int i = 0; i < uniformsCount; i++) {
        GLint v = glGetUniformLocation(prg->shaderProgram, vector_get(prg->uniforms.data, i, char*));
        vector_push(&prg->uniformsLocation, &v);
    }
    // print here to separate things a bit better and for debug output to be easier to read
    printf("\n");
    // set the setGlobalUniforms function pointer
    prg->setGlobalUniforms = setUniforms;
}

void GlhFreeProgram(struct GlhProgram *prg) {
    // free allocated vectors
    vector_free(&prg->attributes);
    vector_free(&prg->uniforms);
    vector_free(&prg->uniformsLocation);
}

void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle) {
    #define OPT(a, b, c) (a == c ? b : a)
    // set versions
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // create window
    ctx->window = glfwCreateWindow(OPT(windowWidth, 640, 0), OPT(windowHeight, 480, 0), OPT(windowTitle, "window", NULL), NULL, NULL);
    // set context
    glfwMakeContextCurrent(ctx->window);
    glewInit();
    glfwSwapInterval(1);
    // set camera data
    ctx->camera.fov = glm_rad(90);
    glm_vec3_zero(ctx->camera.position);
    glm_vec3_zero(ctx->camera.rotation);
    ctx->camera.zNear = 0.1;
    ctx->camera.zFar = 100;
    ctx->camera.perspective = true;
    // init children vector
    vector_init(&ctx->children, 2, sizeof(void*));
    // get window width and height
    int width, height;
    glfwGetWindowSize(ctx->window, &width, &height);
    // set viewport
    // TODO move those kind of lines to some kind of hook to a glfw resize event
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    #undef OPT
}

void GlhFreeContext(struct GlhContext *ctx) {
    vector_free(&ctx->children);
}

void GlhContextAppendChild(struct GlhContext *ctx, struct GlhObject *child) {
    vector_push(&ctx->children, &child);
}

void GlhComputeContextViewMatrix(struct GlhContext *ctx) {
    vec3 translation;
    vec3 rotation;
    // negate the transforms to give the illusion of a moving camera
    // when in reality the whole world is moving
    glm_vec3_negate_to(ctx->camera.position, translation);
    glm_vec3_negate_to(ctx->camera.rotation, rotation);
    glm_mat4_identity(ctx->cachedViewMatrix);
    glm_translate(ctx->cachedViewMatrix, translation);
    glm_rotate_x(ctx->cachedViewMatrix, rotation[0], ctx->cachedViewMatrix);
    glm_rotate_y(ctx->cachedViewMatrix, rotation[1], ctx->cachedViewMatrix);
    glm_rotate_z(ctx->cachedViewMatrix, rotation[2], ctx->cachedViewMatrix);
}

void GlhComputeContextProjectionMatrix(struct GlhContext *ctx) { 
    //TODO add this computation as well to a resize hook
    // get window width and height to compute the aspect
    #define zn ctx->camera.zNear
    #define zf ctx->camera.zFar
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    float aspect = (float) width / (float) height;
    float f = tanf(M_PI_2 - 0.5*ctx->camera.fov);
    mat4 p = {
        {f / aspect, 0, 0, 0},
        {     0,     f, 0, 0},
        {     0,     0, 0, 0},
        {     0,     0, 0, 0}
    };
    // compute projection matrix
    if(ctx->camera.perspective) {
        //* yes, i know cglm already include a perspective function, but it seems to be broken while that works
        float fn = 1.0f / (zn - zf);
        p[2][2] = (zn + zf) * fn;
        p[2][3] = -1;
        p[3][2] = zn * zf * fn * 2; 
        p[3][3] = 0;                       
    } else {
        float fn = 1.0 / (zf - zn);
        p[2][2] = 2 * fn;
        p[2][3] = 0;
        p[3][2] = 2 * zn * fn + 1;
        p[3][3] = 1;
    }
    // dirty way of setting cachedProjectionMatrix to p
    glm_mat4_identity(ctx->cachedProjectionMatrix);
    glm_mat4_mul(ctx->cachedProjectionMatrix, p, ctx->cachedProjectionMatrix);
    #undef zn
    #undef zf
}

void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx) {
    // use objext's shader program
    glUseProgram(obj->program->shaderProgram);
    // set the uniforms related to the program
    (*obj->program->setGlobalUniforms)(obj, ctx);
    // bind objects's texture
    glBindTexture(GL_TEXTURE_2D, obj->texture);
    // bind VAO
    glBindVertexArray(obj->mesh->bufferData.VAO);
    // draw object
    glDrawElements(GL_TRIANGLES, obj->mesh->bufferData.vertexCount, GL_UNSIGNED_INT, NULL);
}

void GlhRenderContext(struct GlhContext *ctx) {
    // clear screen and depth buffer (for depth testing)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for(int i = 0; i < ctx->children.size; i++) {
        // get each object and draw it
        struct GlhObject* obj = vector_get(ctx->children.data, i, struct GlhObject*);
        GlhRenderObject(obj, ctx);
    }
}

// internal, used to avoid repeats
void setAttribute(GLuint buffer, GLuint location, int comp) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(location, comp, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
}

void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount) {
    struct GlhMeshBufferData data = {};
    // zeroify bufferData
    mesh->bufferData = data;
    // initialize and set the vectors
    vector_init(&mesh->verticies, verticiesCount, sizeof(vec3));
    vector_init(&mesh->normals, verticiesCount, sizeof(vec3));
    vector_init(&mesh->indexes, indicesCount, sizeof(vec3));
    vector_init(&mesh->texCoords, texcoordsCount, sizeof(vec2));
    vector_push_array(mesh->verticies, verticies, verticiesCount);
    vector_push_array(mesh->normals, normals, verticiesCount);
    vector_push_array(mesh->indexes, indices, indicesCount);
    vector_push_array(mesh->texCoords, texcoords, texcoordsCount);
    // create and bind VAO
    glGenVertexArrays(1, &mesh->bufferData.VAO);
    glBindVertexArray(mesh->bufferData.VAO);
    // generate and fill VBOs
    GlhGenerateMeshBuffers(mesh);
    // set attributes to VAO
    setAttribute(mesh->bufferData.vertexBuffer, 0, 3);
    setAttribute(mesh->bufferData.normalBuffer, 1, 3);
    setAttribute(mesh->bufferData.tcoordBuffer, 2, 2);
    // bind indices buffer to VAO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->bufferData.indexsBuffer);
}

void GlhFreeMesh(struct GlhMesh *mesh) {
    vector_free(&mesh->verticies);
    vector_free(&mesh->normals);
    vector_free(&mesh->indexes);
    vector_free(&mesh->texCoords);
}

void GlhInitObject(struct GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program) {
    // store transforms into GlhTransforms struct
    struct GlhTransforms tsfm = {};
    glm_vec3_dup(scale, tsfm.scale);
    glm_vec3_dup(rotation, tsfm.rotation);
    glm_vec3_dup(translation, tsfm.translation);
    // file GlhObject struct
    obj->type = regular;
    obj->transforms = tsfm;
    obj->mesh = mesh;
    obj->program = program;
    obj->texture = texture;
}

void GlhUpdateObjectModelMatrix(struct GlhObject *obj) {
    GlhTransformsToMat4(&obj->transforms, &obj->cachedModelMatrix);
}
//internal, used to avoid repeating code
void initArrayBuffer(GLuint *buffer, int components, Vector *vec, char* name) {
    // create and bind buffer
    glGenBuffers(1, buffer);
    glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    // declare the array in which the flattened vector's data will be stored to be passed to the buffer
    float array[vec->size * components];
    // flattens the vector (from vec3[x] to float[3 * x] or vec2[x] to flaot[2 * x])
    for(int i = 0; i < (vec->size) * components; i += components) {
        if(components == 2) {
            array[i + 0] = vector_get(vec->data, i / components, vec2)[0];
            array[i + 1] = vector_get(vec->data, i / components, vec2)[1];
        } else {
            array[i + 0] = vector_get(vec->data, i / components, vec3)[0];
            array[i + 1] = vector_get(vec->data, i / components, vec3)[1];
            array[i + 2] = vector_get(vec->data, i / components, vec3)[2];
        }
    }
    // fill buffer with data
    glBufferData(GL_ARRAY_BUFFER, vec->size * components * sizeof(float), &array[0], GL_STATIC_DRAW);
}

void GlhGenerateMeshBuffers(struct GlhMesh *mesh) {
    // prepare data and create, bind and fills buffer with it
    initArrayBuffer(&mesh->bufferData.vertexBuffer, 3, &mesh->verticies, "verticies");
    initArrayBuffer(&mesh->bufferData.normalBuffer, 3, &mesh->normals, "normals");
    initArrayBuffer(&mesh->bufferData.tcoordBuffer, 2, &mesh->texCoords, "texture coordinates");
    // same thing for indices vectors (done like that because the type and buffer type are different (int and GL_ELEMENT_ARRAY here))
    glGenBuffers(1, &mesh->bufferData.indexsBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->bufferData.indexsBuffer);
    int indexes[mesh->indexes.size * 3];
    // flatten vector
    for (int i = 0; i < (mesh->indexes.size) * 3; i += 3) {
        indexes[i + 0] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[0]);
        indexes[i + 1] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[1]);
        indexes[i + 2] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[2]);
    }
    // fill buffer
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexes.size * 3 * sizeof(unsigned int), &indexes[0], GL_STATIC_DRAW);
    // 3 verticies per triangle
    mesh->bufferData.vertexCount = mesh->indexes.size * 3;
}

// empty right now because no manually allocated data is directly linked with objects
// i still recomend calling it when and objects is not needed for later (multiple
// textures stored in vectors maybe)
void GlhFreeObject(struct GlhObject *obj) {}

void GlhInitFreeType() {
    if(FT_Init_FreeType(&ft)) {
        printf("ERROR, couldon't init freetype\n");
    }
}

void GlhFreeFreeType() {
    FT_Done_FreeType(ft);
}

void GlhInitFont(struct GlhFont *font, char* ttfFileName, int size, int glyphCount) {
    map_init(&font->glyphsData, sizeof(struct GlhFontGLyphData));
    FT_Face face;
    if(FT_New_Face(ft, ttfFileName, 0, &face)) {
        printf("ERROR, when loading font face %s\n", ttfFileName);
        return;
    }
    FT_Set_Char_Size(face, 0, size << 6, 96, 96);
    int max_dim = (1 + (face->size->metrics.height >> 6)) * ceilf(sqrtf(glyphCount));
	int tex_width = 1;
	while(tex_width < max_dim) tex_width <<= 1;
	int tex_height = tex_width;
    char* pixels = (char*)calloc(tex_width * tex_height, 1);
    int pen_y = 0, pen_x = 0;

    for(int i = 0; i < glyphCount; ++i){
		FT_Load_Char(face, i, FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT);
		FT_Bitmap* bmp = &face->glyph->bitmap;

		if(pen_x + bmp->width >= tex_width){
			pen_x = 0;
			pen_y += ((face->size->metrics.height >> 6) + 1);
		}

		for(int row = 0; row < bmp->rows; ++row){
			for(int col = 0; col < bmp->width; ++col){
				int x = pen_x + col;
				int y = pen_y + row;
				pixels[y * tex_width + x] = bmp->buffer[row * bmp->pitch + col];
			}
		}

		// this is stuff you'd need when rendering individual glyphs out of the atlas
        struct GlhFontGLyphData info;
		info.x0 = pen_x;
		info.y0 = pen_y;
		info.x1 = pen_x + bmp->width;
		info.y1 = pen_y + bmp->rows;

		info.x_off   = face->glyph->bitmap_left;
		info.y_off   = face->glyph->bitmap_top;
		info.advance = face->glyph->advance.x >> 6;
        char c = (char) i;

        map_set(&font->glyphsData, &c, &info);

		pen_x += bmp->width + 1;
	}

	FT_Done_FreeType(ft);

	// write png

	char* png_data = (char*)calloc(tex_width * tex_height * 4, 1);
	for(int i = 0; i < (tex_width * tex_height); ++i){
		png_data[i * 4 + 0] |= pixels[i];
		png_data[i * 4 + 1] |= pixels[i];
		png_data[i * 4 + 2] |= pixels[i];
		png_data[i * 4 + 3] = 0xff;
	}

    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // put image data in texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, png_data);
    // generate mipmap to make sampling faster
    glGenerateMipmap(GL_TEXTURE_2D);

	free(png_data);
	free(pixels);
}

void GlhTextObjectUpdateMesh(struct GlhTextObject *tob, char* OldString) {
    if(OldString == NULL) {
        glGenBuffers(1, &tob->vertBuffer);
        glGenBuffers(1, &tob->uvsBuffer);


    }
}

void GlhTextObjectSetText(struct GlhTextObject *tob, char* string) {
    char* oldState = tob->_text;
    tob->_text = string;
    GlhTextObjectUpdateMesh(tob, oldState);
}

char* GlhTextObjectGetText(struct GlhTextObject *tob) {
    return tob->_text;
}

// transforms can be NULL
void GlhInitTextObject(struct GlhTextObject *tob, char* string, vec4 color, struct GlhTransforms *tsf) {
    tob->type = text;
    if(tsf != NULL) {
        tob->transforms = *tsf;
    } else {
        glm_vec3_copy(GLM_VEC3_ZERO, tob->transforms.rotation);
        glm_vec3_copy(GLM_VEC3_ZERO, tob->transforms.translation);
        glm_vec3_copy(GLM_VEC3_ONE, tob->transforms.scale);
    }
    vector_init(&tob->verticies, 60, sizeof(float));
    vector_init(&tob->uvs, 40, sizeof(float));

    glm_vec4_copy(color, tob->color);
    tob->_text = NULL;
    GlhTextObjectSetText(tob, string);
}

int loadTexture(GLuint *texture, char* filename, bool alpha) {
    // create, bind texture and set parameters
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // read file and get width height and channels
    int width, height, nrChannels;
    // flip image because opengl use bottom left corner origin instead of top left
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if(data) {
        GLenum format = alpha ? GL_RGBA : GL_RGB;
        // put image data in texture
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        // generate mipmap to make sampling faster
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        printf("unable to load image\n");
        return -1;
    }
    // free image data
    stbi_image_free(data);
    return 0;
}