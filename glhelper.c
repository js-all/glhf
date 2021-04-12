#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "glhelper.h"
#include <ft2build.h>
#include FT_FREETYPE_H

// list of the names of the vertex attributes
static const char* attributes[] = {
    "vPos",
    "vNormal",
    "vTexCoord"
};

static char fontGlyphDataMapDefaultKey[9] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, '\0'};

FT_Library ft;

void memset_pattern(void* dest, size_t dest_size, void* pattern, size_t pattern_size) {
    if(pattern_size > dest_size) {
        memcpy(dest, pattern, dest_size);
    } else {
        size_t ptn = pattern_size; // the total size we filled
        size_t lds = dest_size - ptn; // what is left unfilled
        // first exponentially fill dest with bigger and bigger repeating chunks of the pattern
        // after that lds < ptn (because we stop filling for that precise reason)
        memcpy(dest, pattern, pattern_size);
        while(ptn <= lds) {
            memcpy(dest + ptn, dest, ptn);
            lds -= ptn;
            ptn *= 2;
        }
        if(lds > 0) {
            // lds < ptn, therefor, we can just copy from dest to lds and expect to get the full pattern
            // + whats needed to finish dest_size
            memcpy(dest + dest_size - lds, dest, lds);
        }
    }
}

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
    vec3 reverseTransformOrigin;
    glm_vec3_negate_to(tsf->transformsOrigin, reverseTransformOrigin);

    glm_translate(*mat, tsf->translation);
    glm_scale(*mat, tsf->scale);
    glm_translate(*mat, tsf->transformsOrigin);
    glm_rotate_x(*mat, tsf->rotation[0], *mat);
    glm_rotate_y(*mat, tsf->rotation[1], *mat);
    glm_rotate_z(*mat, tsf->rotation[2], *mat);
    glm_translate(*mat, reverseTransformOrigin);
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
    vector_push_array(&prg->uniforms, uniforms, uniformsCount);
    //* NOTE: the attribute vector is not really needed, as it will always be the 
    //* same as the attributes global array but i choosed to keep it that way if
    //* i ever come arround to implement custom attributes for whatever reasons
    vector_push_array(&prg->attributes, attributes, attributesCount);
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
    // mostly for text with slanted fonts because most of the letters
    // will have the same z, and which would end up in a failing depth
    // test with the regular GL_LESS depth function
    glDepthFunc(GL_LEQUAL);
    #undef OPT
}

void GlhFreeContext(struct GlhContext *ctx) {
    vector_free(&ctx->children);
}

void GlhContextAppendChild(struct GlhContext *ctx, void *child) {
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
    glm_rotate_x(ctx->cachedViewMatrix, rotation[0], ctx->cachedViewMatrix);
    glm_rotate_y(ctx->cachedViewMatrix, rotation[1], ctx->cachedViewMatrix);
    glm_rotate_z(ctx->cachedViewMatrix, rotation[2], ctx->cachedViewMatrix);
    glm_translate(ctx->cachedViewMatrix, translation);
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

// basically a copy paste of GlhRenderObject
void GlhRenderTextObject(struct GlhTextObject *tob, struct GlhContext *ctx) {
    glUseProgram(tob->program->shaderProgram);
    // set the uniforms related to the program
    (*tob->program->setGlobalUniforms)(tob, ctx);
    // bind objects's texture
    glBindTexture(GL_TEXTURE_2D, tob->font->texture);
    // bind VAO
    glBindVertexArray(tob->bufferData.VAO);
    // draw object
    glDrawElements(GL_TRIANGLES, tob->bufferData.vertexCount, GL_UNSIGNED_INT, NULL);
}

void GlhRenderContext(struct GlhContext *ctx) {
    // clear screen and depth buffer (for depth testing)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for(int i = 0; i < ctx->children.size; i++) {
        // get each object and draw it
        void* objPtr = vector_get(ctx->children.data, i, void*);
        // i can do that because the pointer to a struct is also a pointer to its first member
        enum GlhObjectTypes type = *(enum GlhObjectTypes*) objPtr;
        if(type == regular) {
            struct GlhObject* obj = (struct GlhObject*) objPtr;
            GlhRenderObject(obj, ctx);
        } else if(type == text) {
            struct GlhTextObject* tob = (struct GlhTextObject*) objPtr;
            GlhRenderTextObject(tob, ctx);
        }
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
    vector_push_array(&mesh->verticies, verticies, verticiesCount);
    vector_push_array(&mesh->normals, normals, verticiesCount);
    vector_push_array(&mesh->indexes, indices, indicesCount);
    vector_push_array(&mesh->texCoords, texcoords, texcoordsCount);
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
    glm_vec3_copy(scale, tsfm.scale);
    glm_vec3_copy(rotation, tsfm.rotation);
    glm_vec3_copy(translation, tsfm.translation);
    glm_vec3_zero(tsfm.transformsOrigin);
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
    glBufferData(GL_ARRAY_BUFFER, vec->size * components * sizeof(float), array, GL_STATIC_DRAW);
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
 
void GlhFreeFont(struct GlhFont *font) {
    for(int i = 0; i < font->glyphsData.keyVector.size; i++) {
        free(vector_get(font->glyphsData.keyVector.data, i, char*));
    }
    map_free(&font->glyphsData);
    glDeleteTextures(1, &font->texture);
}

void GlhInitFont(struct GlhFont *font, char* ttfFileName, int size, int glyphCount, float packingPrecision) {
    // init the final map which will store all the existing glyph data, plus a single default char
    map_init(&font->glyphsData, sizeof(struct GlhFontGLyphData));
    // load the font and set char size
    FT_Face face;
    if(FT_New_Face(ft, ttfFileName, 0, &face)) {
        printf("ERROR, when loading font face %s\n", ttfFileName);
        return;
    }
    FT_Set_Char_Size(face, 0, size << 6, 96, 96);
    // quick, dirty and easy way to get total glyph count
    int _glyphCount = glyphCount;
    float _packingPrecision = packingPrecision <= 0 ? 0.75 : packingPrecision;
    _packingPrecision = _packingPrecision >= 1 ? 0.999 : _packingPrecision;
    if(glyphCount == -1) {
        _glyphCount = 0;
        FT_UInt ind;
        FT_ULong c;
        c = FT_Get_First_Char(face, &ind);
        while (ind != 0) {
            c = FT_Get_Next_Char(face, c, &ind);
            _glyphCount++;
        }
    }
    // struct that will store all extracted data about chars from the font
    // to avoid re extracting them multiple time when packing (to get the width and height)
    struct tmpGlyphData {
        int w;               // width
        int h;               // height
        int xo;              // x offset (for rendering)
        int yo;              // y offset
        int ad;              // advance (char width with extra steps)
        int ar;              // area
        unsigned long c;     // the char code (unsigned long because thats whats freetype gives us)
        char* px;            // pixels array storing the bitmap texture
    } prePackingGlyphsData[_glyphCount + 1]; // glyphcount +1, to store every glyph, plus the "no glyph" glyph

    // char index in the font
    FT_UInt ci = 0;
    FT_ULong charcode;
    // of all chars
    int totalHeight = 0;
    // -1 is gonna be the no glyph" glyph, then the other will be the chars from the font
    for(int i = -1; i < _glyphCount; i++) {
        FT_Int32 flags = FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT;
        if(i == -1) {
            // -1 should always return no glyph (and therefor a fallback glyph that we want to store)
            // here -1 stand for give me the glyph with char code 18446744073709551615 (casted to unsigned long)
            FT_Load_Char(face, -1, flags);
        } else {
            // we first need the get the char index of the first existing char
            if(i == 0) {
                FT_Get_First_Char(face, &ci);
            }
            // then we load the glyph !
            charcode = FT_Get_Next_Char(face, charcode, &ci);
            FT_Load_Char(face, charcode, flags);
        }

        // extract and store data about the glyph
        FT_Bitmap* bmp = &face->glyph->bitmap;
        prePackingGlyphsData[i+1].w = bmp->width;                           //width
        prePackingGlyphsData[i+1].h = bmp->rows;                            // height
        prePackingGlyphsData[i+1].xo = face->glyph->bitmap_left;            // x offset
        prePackingGlyphsData[i+1].yo = face->glyph->bitmap_top;             // y offset
        prePackingGlyphsData[i+1].ad = face->glyph->advance.x >> 6;         // the advance
        prePackingGlyphsData[i+1].c = i < 0 ? (unsigned long) -1 : charcode;// the character (max value if default)
        prePackingGlyphsData[i+1].px = calloc(bmp->width * bmp->rows, 1);   // allocate the size to store the bitmap
        prePackingGlyphsData[i+1].ar = bmp->width * bmp->rows;              // the area
        totalHeight += bmp->rows;
        // store the bitmap into the px array
        for(int y = 0; y < bmp->rows; y++) {
            for(int x = 0; x < bmp->width; x++) {
                prePackingGlyphsData[i+1].px[y * bmp->width + x] = bmp->buffer[y * bmp->pitch + x];
            }
        }
    }
    // shitty easy to implement buble sort: sort prePackingGlyphsData by area (from largest to smallest)
    // to know if the array is sorted
    int swapCount = 1;
    while(swapCount != 0) {
        swapCount = 0;
        // the array has a length of glyphCount + 1, but because here we swap the current index and the next one
        // we need to not hit the end, or the next one won't exist
        for(int i = 0; i < _glyphCount; i++) {
            if(prePackingGlyphsData[i].ar < prePackingGlyphsData[i + 1].ar) {
                swapCount++;
                // swap
                struct tmpGlyphData tmp = prePackingGlyphsData[i];
                prePackingGlyphsData[i] = prePackingGlyphsData[i + 1];
                prePackingGlyphsData[i + 1] = tmp;
            }
        }
    }
    // here's the pretty bad algorithm i use to pack the glyphs:
    // first set sideLength to totalHeight, as we are sure that it will fit (but probably leave a huge unused space)
    // then, until we cannot pack the glyphs in a square with area of sideLength²:
    //  -> try to pack the glyphs, while running store the packing data in a tmpPacks variable, then if the packing completes,
    //     store that into packs
    //  -> save sideLength in oldSideLength and reduce sideLength so that area is reduced by a percentage
    //  -> try again, until the packing fails, then the last successfull (and therefor smallest (we could find)) packing is in packs
    //     (and its sideLength is in OldSideLength)
    int oldSideLength = 0;
    int sideLength = totalHeight;
    bool packingSuccessfull = true;
    // will simply store the x and y offset of every glyph (with even indexes being the xs and odd indexes being the ys)
    int packs[(_glyphCount + 1) * 2];
    while(packingSuccessfull) {
        // same as packs
        int tmpPacks[(_glyphCount + 1) * 2];
        int packedGlyphsCount = 0;
        // TODO: better the packing algorithm
        // to know where to place the new glyph
        int lastY = 0;
        while(packedGlyphsCount < _glyphCount + 1) {
            // like wise
            int lastX = 0;
            while(packedGlyphsCount < _glyphCount + 1) {
                // if theres not enough horizontal or vertical room to put the glyph
                if(prePackingGlyphsData[packedGlyphsCount].w + lastX >= sideLength) break;
                if(prePackingGlyphsData[packedGlyphsCount].h + lastY >= sideLength) {
                    // to remember the reason we broke out of this while
                    packingSuccessfull = false;
                    break;
                }
                // store the offset
                tmpPacks[packedGlyphsCount*2+0] = lastX;
                tmpPacks[packedGlyphsCount*2+1] = lastY;
                // set the new offset
                lastX += prePackingGlyphsData[packedGlyphsCount].w;
                packedGlyphsCount ++;
            }
            // if the packing failed break now to avoid infinit loop
            if(!packingSuccessfull) break;
            // get max y value of all the packed glyph and set lastY to it
            for(int i = 0; i < packedGlyphsCount; i++) {
                int y = tmpPacks[i*2+1] + prePackingGlyphsData[i].h;
                lastY = lastY < y ? y : lastY;
            }
        }
        if(packingSuccessfull) {
            for(int i = 0; i <= (_glyphCount*2); i++) {
                packs[i] = tmpPacks[i];
            }
            // reduce area
            oldSideLength = sideLength;
            // packing precision is the factor by which we multiply the area
            sideLength = sqrt(_packingPrecision * sideLength * sideLength);
        }
    } 
    // because the reason we broke out of the above while, is that the last packing failed
    // therefor the value stored in the current sideLength is wrong (too small), while
    // the one in oldSideLength is guarented to be the last working one
    sideLength = oldSideLength;
    font->textureSideLength = sideLength;
    // allocate memory to store the font's atlas' pixels
    char* pixels = (char*)calloc(sideLength * sideLength, 1);
    for(int i = 0; i <= _glyphCount; i++) {
        // put the char info in the map
        struct GlhFontGLyphData info = {};
        float invSize = 1.0 / size;
        info.x0 = packs[i*2+0];
        info.y0 = sideLength - 1 - packs[i*2+1] - prePackingGlyphsData[i].h; // all those sidelength - 1 - something is to convert
        info.y1 = sideLength - 1 - packs[i*2+1];            // the glyph's bitmap bottom left origin to the opengl top left origin
        info.x1 = info.x0 + prePackingGlyphsData[i].w;
        info.hgl = (float) prePackingGlyphsData[i].h * invSize; // all the * invSize is to have a good value to work in opengl
        info.wgl = (float) prePackingGlyphsData[i].w * invSize; // because the current ones are way too big and would require insane scaling
        info.advance = (float) prePackingGlyphsData[i].ad * invSize;
        info.x_off = (float) prePackingGlyphsData[i].xo * invSize;
        info.y_off = (float) prePackingGlyphsData[i].yo * invSize;

        // here the malloc is mendatory because only the pointer to the chars is stored in the vectors
        // but that doesn't prevent from dangeling pointers.
        // TODO: make a custom String Vector optimized to store strings and prevent dangeling pointers
        // allocate enough memory to store the ULong char code into a char*
        // + sizeof(char) for the null termination
        char* key = malloc(sizeof(unsigned long) + sizeof(char));
        *(unsigned long*)key = prePackingGlyphsData[i].c; // put the char code in the char*
        *(sizeof(unsigned long) + key) = '\0'; // null terminate 

        // store all the generated glyph infos in the font map
        map_set(&font->glyphsData, key, &info);

        // set atlas' pixels to the char's pixels (correctly offseted)
        for(int y = 0; y < prePackingGlyphsData[i].h; y++) {
            for(int x = 0; x < prePackingGlyphsData[i].w; x++) {
                // tbh even i don't really understand how all this cluster fuck of different images representation works
                // but it does
                pixels[(info.y0 + (prePackingGlyphsData[i].h-1) - y) * sideLength + info.x0 + x] = prePackingGlyphsData[i].px[y * prePackingGlyphsData[i].w + x];
            }
        }

        // free the previously allocated per glyph pixel buffer
        free(prePackingGlyphsData[i].px);
    }

    FT_Done_Face(face);
	FT_Done_FreeType(ft);

    // create opengl texture
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // to be able to use single channel textures
    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // put image data in texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, sideLength, sideLength, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    // generate mipmap to make sampling faster
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // finaly, free out pixels buffer
	free(pixels);
}

void _characterToGlyphData(char c, struct GlhFont *font, struct GlhFontGLyphData *cdata) {
    // converting the char to a correctly formated font char info map key
    // looks complicated but really just is:
    // make char array, store the un altered unsigned long char code data inside
    // and null terminate
    char k[9]; // 9 -> 8 bytes for the unsigned long, + 1 for the nul termination
    *(unsigned long*)k = (unsigned long) c; // cast the char* to an unsined long pointer, to just set it like that
    k[8] = '\0'; // null terminate
    // get the glyph data if it exist, fallback to default if it doesn't
    if(map_has(&font->glyphsData, k)) {
        map_get(&font->glyphsData, k, cdata);
    } else {
        map_get(&font->glyphsData, fontGlyphDataMapDefaultKey, cdata);
    }
}

void _characterToMesh(char c, struct GlhFont *font, float *xoff, float yoff, float *newVerticies, float *newTexCoords, int arrOffset) {
    struct GlhFontGLyphData cdata;
    _characterToGlyphData(c, font, &cdata);
    // to convert from 0 -> textureSideLength texCoords to 0 -> 1
    float invsl = 1.0 / font->textureSideLength;
    float xoffset = *xoff + cdata.x_off;
    float yoffset = yoff + cdata.y_off - cdata.hgl;
    int ao = arrOffset * 3 * 4;
    // simple but not horrible way to set the new verticies
    newVerticies[ao + 0] =       xoffset;       newVerticies[ao +  1] =       yoffset;       newVerticies[ao +  2] = 0;
    newVerticies[ao + 3] = xoffset + cdata.wgl; newVerticies[ao +  4] =       yoffset;       newVerticies[ao +  5] = 0;
    newVerticies[ao + 6] =       xoffset;       newVerticies[ao +  7] = yoffset + cdata.hgl; newVerticies[ao +  8] = 0;
    newVerticies[ao + 9] = xoffset + cdata.wgl; newVerticies[ao + 10] = yoffset + cdata.hgl; newVerticies[ao + 11] = 0;
    ao = arrOffset * 2 * 4;
    newTexCoords[ao + 0] = ((float)cdata.x0) * invsl; newTexCoords[ao + 1] = ((float)cdata.y0) * invsl;
    newTexCoords[ao + 2] = ((float)cdata.x1) * invsl; newTexCoords[ao + 3] = ((float)cdata.y0) * invsl;
    newTexCoords[ao + 4] = ((float)cdata.x0) * invsl; newTexCoords[ao + 5] = ((float)cdata.y1) * invsl;
    newTexCoords[ao + 6] = ((float)cdata.x1) * invsl; newTexCoords[ao + 7] = ((float)cdata.y1) * invsl;
    *xoff += cdata.advance;
}

void GlhTextObjectUpdateMesh(struct GlhTextObject *tob, char* OldString) {
    // until which char can we just keep everything the same
    int charOff = 0;
    int oldLength = OldString == NULL ? 0 : strlen(OldString);
    int newLength = strlen(tob->_text);
    int minLength = (int) glm_min((float) oldLength, (float) newLength);
    int changedLength = 0;
    if(OldString != NULL) {
        for(int i = 0; i < minLength; i++) {
            if (OldString[i] == tob->_text[i]) charOff++;
            else break;
        }
    }
    changedLength = newLength - charOff;
    // remove the changed glyphs' verticies and texcoords
    vector_splice(&tob->verticies, charOff * 3 * 4, -1);
    vector_splice(&tob->texCoords, charOff * 2 * 4, -1);
    // recompute the new ones
    float newVerticies[changedLength * 3 * 4];
    float newTexCoords[changedLength * 2 * 4];
    // the offset where to put the quad
    float xoff = 0;
    // if we reuse data
    if (tob->verticies.size > 0 && charOff > 0) {
        // get the advance of the last reused glyph
        struct GlhFontGLyphData cd;
        _characterToGlyphData(tob->_text[charOff-1], tob->font, &cd);
        // and add it to its x pos into the xoffset
        xoff = vector_get(tob->verticies.data, tob->verticies.size - 6, float) + cd.advance;
    }
    for(int i = 0; i < changedLength; i++) {
        _characterToMesh(tob->_text[charOff + i], tob->font, &xoff, 0, newVerticies, newTexCoords, i);
    }
    // appends changes to verticies and texcoords vector
    vector_push_array(&tob->verticies, newVerticies, changedLength * 3 * 4);
    vector_push_array(&tob->texCoords, newTexCoords, changedLength * 2 * 4);
    // define normalsPattern, to repeat it in the normals arr (because every verticies will have the same normals)
    float normalsPattern[3] = {0.0, 0.0, -1.0};
    float normalsArr[newLength * 3 * 4];
    memset_pattern(normalsArr, sizeof(normalsArr), normalsPattern, sizeof(normalsPattern));
    // compute indices, will allways follow 0, 1, 2, 1, 3, 2 (then 4, 5, 6, 5, 7, 6)
    int indiciesArr[newLength * 6]; // six for two triangles
    int vi = 0; // vertex index
    for(int i = 0; i < newLength * 6; i+=6) {
        indiciesArr[i + 0] = vi + 0; indiciesArr[i + 1] = vi + 1; indiciesArr[i + 2] = vi + 2;
        indiciesArr[i + 3] = vi + 1; indiciesArr[i + 4] = vi + 3; indiciesArr[i + 5] = vi + 2;
        vi += 4;
    }
    // put that data in buffers
    // TODO work in there to cache those buffers and use glBufferSub data if possible for better performances
    // bind VAO now as binding a GL_ELEMENT_ARRAY buffer while the VAO is bound will link it to the VAO
    glBindVertexArray(tob->bufferData.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(normalsArr), normalsArr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tob->bufferData.indexsBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indiciesArr), indiciesArr, GL_DYNAMIC_DRAW);
    // put data in buffers, using glBufferSubData if possible (= if no realloc needed)
    if(oldLength != newLength) {
        glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, tob->verticies.size * tob->verticies.data_size, tob->verticies.data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.tcoordBuffer);
        glBufferData(GL_ARRAY_BUFFER, tob->texCoords.size * tob->texCoords.data_size, tob->texCoords.data, GL_DYNAMIC_DRAW);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.vertexBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * charOff * 3 * 4, sizeof(newVerticies), newVerticies);
        glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.tcoordBuffer);
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * charOff * 2 * 4, sizeof(newTexCoords), newTexCoords);
    }
    // bind attributes to VAO
    setAttribute(tob->bufferData.vertexBuffer, 0, 3);
    setAttribute(tob->bufferData.normalBuffer, 1, 3);
    setAttribute(tob->bufferData.tcoordBuffer, 2, 2);
    // "vertex" here as the number drawn, not the actual one (time 6 for two triangles per quad)
    tob->bufferData.vertexCount = newLength * 6;
    float max_y = vector_get(tob->verticies.data, tob->verticies.size -2, float);
    float max_x = vector_get(tob->verticies.data, tob->verticies.size -3, float);

    tob->transforms.transformsOrigin[0] = max_x * 0.5;
    tob->transforms.transformsOrigin[1] = max_y * 0.5;
}

void GlhUpdateTextObjectModelMatrix(struct GlhTextObject *tob) {
    GlhTransformsToMat4(&tob->transforms, &tob->cachedModelMatrix);
}

void GlhTextObjectSetText(struct GlhTextObject *tob, char* string) {
    char oldState[strlen(tob->_text) + 1];
    strcpy(oldState, tob->_text);
    int strl = strlen(string) + 1;
    if(strl > tob->_textAllocated) {
        tob->_text = realloc(tob->_text, strl);
    }
    strcpy(tob->_text, string);
    GlhTextObjectUpdateMesh(tob, oldState);
}

char* GlhTextObjectGetText(struct GlhTextObject *tob) {
    return tob->_text;
}

// transforms can be NULL
void GlhInitTextObject(struct GlhTextObject *tob, char* string, struct GlhFont *font, struct GlhProgram *prg, vec4 color, struct GlhTransforms *tsf) {
    tob->type = text;
    tob->font = font;
    tob->program = prg;
    if(tsf != NULL) {
        tob->transforms = *tsf;
    } else {
        glm_vec3_copy(GLM_VEC3_ZERO, tob->transforms.rotation);
        glm_vec3_copy(GLM_VEC3_ZERO, tob->transforms.translation);
        glm_vec3_copy(GLM_VEC3_ONE, tob->transforms.scale);
        glm_vec3_zero(tob->transforms.transformsOrigin);
    }
    vector_init(&tob->verticies, 60, sizeof(float));
    vector_init(&tob->texCoords, 40, sizeof(float));

    glm_vec4_copy(color, tob->color);
    tob->_text = malloc(tob->_textAllocated = strlen(string) + 1);

    glGenVertexArrays(1, &tob->bufferData.VAO);
    glGenBuffers(1, &tob->bufferData.vertexBuffer);
    glGenBuffers(1, &tob->bufferData.tcoordBuffer);
    glGenBuffers(1, &tob->bufferData.normalBuffer);
    glGenBuffers(1, &tob->bufferData.indexsBuffer);

    GlhTextObjectSetText(tob, string);
}

void GlhFreeTextObject(struct GlhTextObject *tob) {
    vector_free(&tob->verticies);
    vector_free(&tob->texCoords);
    free(tob->_text);
}

void GlhInitComputeShader(struct GlhComputeShader *cs, char* filename) {
    GLuint shader;
    loadShader(filename, GL_COMPUTE_SHADER, &shader);
    cs->program = glCreateProgram();
    glAttachShader(cs->program, shader);
    glLinkProgram(cs->program);
}

void GlhRunComputeShader(struct GlhComputeShader *cs, GLuint inputTexture, GLuint outputTexture, GLenum sizedInFormat, GLenum sizedOutFormat, int workGroupsWidth, int workGroupsHeight) {
    glUseProgram(cs->program);
    glBindImageTexture(1, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, sizedOutFormat);
    glBindImageTexture(0, inputTexture, 0, GL_FALSE, 0, GL_READ_ONLY, sizedInFormat);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glDispatchCompute(workGroupsWidth, workGroupsHeight, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

int loadTexture(GLuint *texture, char* filename, bool alpha) {
    // create, bind texture and set parameters
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // read file and get width height and channels
    int width, height, nrChannels;
    // flip image because opengl use bottom left corner origin instead of top left
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if(data) {
        GLenum format = alpha ? GL_RGBA : GL_RGB;
        GLenum sizedFormat = alpha ? GL_RGBA8 : GL_RGB8;
        // put image data in texture
        glTexImage2D(GL_TEXTURE_2D, 0, sizedFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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

void createSingleColorTexture(GLuint *texture, float r, float g, float b) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    char red = (char) ((int) (r * 255));
    char green = (char) ((int) (g * 255));
    char blue = (char) ((int) (b * 255));
    char data[4] = {red, green, blue, 0xff};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void saveImage(char* filepath, GLFWwindow* w) {
    int width, height;
    glfwGetFramebufferSize(w, &width, &height);
    GLsizei nrChannels = 3;
    GLsizei stride = nrChannels * width;
    stride += (stride % 4) ? (4 - stride % 4) : 0;
    GLsizei bufferSize = stride * height;
    char* buffer = malloc(bufferSize);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filepath, width, height, nrChannels, buffer, stride);
}
void createEmptySizedTexture(GLuint *texture, int width, int height, GLenum sizedFormat, GLenum format, GLenum type) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, sizedFormat, width, height, 0, format, type, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
}