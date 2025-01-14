#include <freetype2/freetype/fttypes.h>
#include <limits.h>
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
// set the margin that will be applied to every character in the atlas of a font to avoid
// having a character rendering a thin line of pixels of its neighbour because of float precision.
// (to understand better, just look at the generated atlas texture with this value set to 2 and 10)
static const int CharMarginSize = 2;

static char fontGlyphDataMapDefaultKey[9] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, '\0'};

GlhGlobalShaders GlobalShaders;

bool globalShadersReady;

FT_Library ft;

unsigned int OpenGLObjectLabelID = 0;

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

// set an opengl object label to something like `LABEL_00026` (adding a unique identifier to avoid duplicates label)
void set_opengl_label(GLenum identifier, GLuint name, char* label) {
    int suffixlesLabelLength = strlen(label);
    int labelLength = suffixlesLabelLength + 1 + 5 + 1;
    char newLabel[labelLength];
    sprintf(newLabel, "%s_%05u", label, OpenGLObjectLabelID++);
    glObjectLabel(identifier, name, labelLength, newLabel);
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
    set_opengl_label(GL_SHADER, *shader, "SHADER");
    glShaderSource(*shader, 1, &shader_source, NULL);
    glCompileShader(*shader);
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

    glLinkProgram(*program);
    return 0;
}

void GlhTransformsToMat4(GlhTransforms *tsf, mat4 *mat) {
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

GlhTransforms GlhGetIdentityTransform() {
    GlhTransforms tsf = {};
    tsf.scale[0] = 1;
    tsf.scale[1] = 1;
    tsf.scale[2] = 1;
    return tsf;
}

void GlhInitProgram(GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)()) {
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
    set_opengl_label(GL_PROGRAM, prg->shaderProgram, "SHADER_PROGRAM");
    // set the setGlobalUniforms function pointer
    prg->setGlobalUniforms = setUniforms;
}

void __GS_glyphs_uniform(GlhTextObject *obj, GlhContext *ctx) {
    mat4 mvp, mv;
    glm_mat4_mul(ctx->cachedViewMatrix, obj->cachedModelMatrix, mv);
    glm_mat4_mul(ctx->cachedProjectionMatrix, mv, mvp);
    glUniformMatrix4fv(vector_get(obj->glyphProgram->uniformsLocation.data, 0, GLint), 1, GL_FALSE,(float*) mvp);
}

void __GS_text_uniform(GlhTextObject *obj, GlhContext *ctx) {
    mat4 mvp, mv;
    glm_mat4_mul(ctx->cachedViewMatrix, obj->cachedModelMatrix, mv);
    glm_mat4_mul(ctx->cachedProjectionMatrix, mv, mvp);
    glUniformMatrix4fv(vector_get(obj->glyphProgram->uniformsLocation.data, 0, GLint), 1, GL_FALSE,(float*) mvp);
}

void _makeGlobalShaderReady() {
    if(globalShadersReady) return;

    char* glyphs_uniforms[] = {
        "MVP",
        "uTexture"
    };
    char* text_uniforms[] = {
        "MVP",
        "uTexture"
    };

    GlhInitProgram(&GlobalShaders.glyphs, "shaders/glyphs.frag", "shaders/glyphs.vert", glyphs_uniforms, 2, __GS_glyphs_uniform);
    GlhInitProgram(&GlobalShaders.text, "shaders/text.frag", "shaders/text.vert", text_uniforms, 2, __GS_text_uniform);
}

void GlhDeleteFBO(GlhFBOProvider *provider, GlhFBO fbo) {
    // getting a pointer because fbo could be a copy and outdated
    GlhFBO *pfbo = vector_get_pointer_to(provider->FBOs, fbo.id);

    if(pfbo->active) {
        printf("WARN: deleting active FBO\n");
    }

    switch (fbo.type) {
        case FBSizedTexture:
        {
            glDeleteTextures(1, &fbo.attachments[0]);
            glDeleteRenderbuffers(1, &fbo.attachments[1]);
            glDeleteFramebuffers(1, &fbo.FBO);
        }
        break;
    }

    vector_splice(&provider->FBOs, fbo.id, 1, NULL);
}

bool GlhVerrifieFBO(GlhFBOProvider *provider, GlhFBO fbo) {
    bool valid = true;

    switch (fbo.type) {
        case FBSizedTexture:
        {   
            int width, height;
            glfwGetFramebufferSize(provider->ctx->window, &width, &height);

            int w, h;
            int miplevel = 0;
            glBindTexture(GL_TEXTURE_2D, fbo.attachments[0]);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, miplevel, GL_TEXTURE_HEIGHT, &h);

            if(w != width || h != height){
                valid = false;
            }
        }
        break;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.FBO);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        valid = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(fbo.active && !valid) {
        printf("WARN: found invalid active FBO\n");
    }

    return valid;
}

GlhFBO GlhRequestFBO(GlhFBOProvider *provider, GlhFBOType type) {
    int found = -1;
    for(int i = 0; i < provider->FBOs.size; i++) {
        GlhFBO fbo = vector_get(provider->FBOs.data, i, GlhFBO);
        bool valid = GlhVerrifieFBO(provider, fbo);

        if(!valid) {
            GlhDeleteFBO(provider, fbo);
            // to stay at the same index next iteration
            i--;
            continue;
        }

        if(!fbo.active && fbo.type == type) {
            found = i;
            break;
        }
    }
    if(found != -1) {
        GlhFBO *fbo = vector_get_pointer_to(provider->FBOs, found);        

        fbo->active = true;

        switch (fbo->type) {
            case FBSizedTexture:
            {
                glBindFramebuffer(GL_FRAMEBUFFER, fbo->FBO);
                GLfloat oldClearColor[4];
                glGetFloatv(GL_COLOR_CLEAR_VALUE, oldClearColor);
                glClearColor(0, 0, 0, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glClearColor(oldClearColor[0], oldClearColor[1], oldClearColor[2], oldClearColor[3]);
            }
            break;
        }

        return *fbo;
    }
    GlhFBO fbo;
    fbo.active = true;
    fbo.type = type;
    fbo.id = provider->FBOs.size;

    switch (type) {
        case FBSizedTexture:
        {
            int width, height;
            glfwGetFramebufferSize(provider->ctx->window, &width, &height);
    
            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            set_opengl_label(GL_TEXTURE, texture, "TEXTURE_FBO_FBSIZED");
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            glGenerateMipmap(GL_TEXTURE_2D);

            unsigned int rbo;
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            set_opengl_label(GL_RENDERBUFFER, rbo, "RENDERBUFFER_FBO_FBSIZED");
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);  

            glGenFramebuffers(1, &fbo.FBO);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo.FBO);
            set_opengl_label(GL_FRAMEBUFFER, fbo.FBO, "FBO_FBSIZED");

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

            fbo.attachments[0] = texture;
            fbo.attachments[1] = rbo;

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        break;
    }
    vector_push(&provider->FBOs, &fbo);
    return fbo;
}

void GlhReleaseFBO(GlhFBOProvider provider, GlhFBO fbo) {
    GlhFBO *pfbo = vector_get_pointer_to(provider.FBOs, fbo.id);
    pfbo->active = false;
}

void GlhFreeProgram(GlhProgram *prg) {
    // free allocated vectors
    vector_free(prg->attributes);
    vector_free(prg->uniforms);
    vector_free(prg->uniformsLocation);
}

void GlhInitContext(GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle) {
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
    vector_init(&ctx->FBOProvider.FBOs, 2, sizeof(GlhFBO));
    ctx->FBOProvider.ctx = ctx;
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

    _makeGlobalShaderReady();
}

void GlhFreeContext(GlhContext *ctx) {
    vector_free(ctx->children);
    vector_free(ctx->FBOProvider.FBOs);
}

void GlhContextAppendChild(GlhContext *ctx, GlhElement *child) {
    vector_push(&ctx->children, &child);
}

void GlhComputeContextViewMatrix(GlhContext *ctx) {
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

void GlhComputeContextProjectionMatrix(GlhContext *ctx) { 
    // get window width and height to compute the aspect
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    float aspect = (float) width / (float) height;
    mat4 p;
    if(ctx->camera.perspective) {
        glm_perspective(ctx->camera.fov, aspect, ctx->camera.zNear, ctx->camera.zFar, p);
    } else {
        float f = tanf(M_PI_2 - 0.5 * ctx->camera.fov) * aspect;
        glm_ortho(-f, f, -1, 1, ctx->camera.zNear, ctx->camera.zFar, p);
    }

    // dirty way of setting cachedProjectionMatrix to p
    glm_mat4_identity(ctx->cachedProjectionMatrix);
    glm_mat4_mul(ctx->cachedProjectionMatrix, p, ctx->cachedProjectionMatrix);
}

void GlhRenderObject(GlhObject *obj, GlhContext *ctx) {
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

void GlhRenderTextObject(GlhTextObject *tob, GlhContext *ctx) {
    // get FBO to render the text to
    GlhFBO fbo = GlhRequestFBO(&ctx->FBOProvider, FBSizedTexture);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo.FBO);

    glUseProgram(tob->glyphProgram->shaderProgram);
    // set the uniforms related to the program
    (*tob->glyphProgram->setGlobalUniforms)(tob, ctx);
    // bind objects's texture
    glBindTexture(GL_TEXTURE_2D, tob->font->texture);
    // bind VAO
    glBindVertexArray(tob->bufferData.VAO);
    // draw object
    glDrawElements(GL_TRIANGLES, tob->bufferData.vertexCount, GL_UNSIGNED_INT, NULL);
    // unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(tob->textProgram->shaderProgram);
    // set the uniforms related to the program
    (*tob->textProgram->setGlobalUniforms)(tob, ctx);

    glBindTexture(GL_TEXTURE_2D, fbo.attachments[0]);
    // bind background VAO
    glBindVertexArray(tob->backgroundQuadBufferData.VAO);
    // draw background
    glDrawElements(GL_TRIANGLES, tob->backgroundQuadBufferData.vertexCount, GL_UNSIGNED_INT, NULL);

    GlhReleaseFBO(ctx->FBOProvider, fbo);
}

void GlhRenderElement(GlhElement *el, GlhContext *ctx) {
    GlhObjectTypes type = el->any.type;
    switch (type) {
        case regular: 
            GlhRenderObject(&el->regular, ctx);
            break;
        case text:
            GlhRenderTextObject(&el->text, ctx);
            break;
    }
}

void GlhRenderContext(GlhContext *ctx) {
    // clear screen and depth buffer (for depth testing)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for(int i = 0; i < ctx->children.size; i++) {
        GlhElement* el = vector_get(ctx->children.data, i, void*);
        
        GlhRenderElement(el, ctx);
    }
}

// internal, used to avoid repeats
void setAttribute(GLuint buffer, GLuint location, int comp) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(location, comp, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
}

void GlhInitMesh(GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount) {
    GlhMeshBufferData data = {};
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
    set_opengl_label(GL_VERTEX_ARRAY, mesh->bufferData.VAO, "VAO");
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

void GlhFreeMesh(GlhMesh *mesh) {
    vector_free(mesh->verticies);
    vector_free(mesh->normals);
    vector_free(mesh->indexes);
    vector_free(mesh->texCoords);
}

void GlhInitObject(GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, GlhMesh *mesh, GlhProgram *program) {
    // store transforms into GlhTransforms struct
    GlhTransforms tsfm = {};
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

void GlhUpdateObjectModelMatrix(GlhObject *obj) {
    GlhTransformsToMat4(&obj->transforms, &obj->cachedModelMatrix);
}
//internal, used to avoid repeating code
void initArrayBuffer(GLuint *buffer, int components, Vector *vec, char* label) {
    // create and bind buffer
    glGenBuffers(1, buffer);
    set_opengl_label(GL_BUFFER, *buffer, label);

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

void GlhGenerateMeshBuffers(GlhMesh *mesh) {
    // prepare data and create, bind and fills buffer with it
    initArrayBuffer(&mesh->bufferData.vertexBuffer, 3, &mesh->verticies, "BUFFER_VERTICIES");
    initArrayBuffer(&mesh->bufferData.normalBuffer, 3, &mesh->normals, "BUFFER_NORMALS");
    initArrayBuffer(&mesh->bufferData.tcoordBuffer, 2, &mesh->texCoords, "BUFFER_TEXCOORDS");
    // same thing for indices vectors (done like that because the type and buffer type are different (int and GL_ELEMENT_ARRAY here))
    glGenBuffers(1, &mesh->bufferData.indexsBuffer);
    set_opengl_label(GL_BUFFER, mesh->bufferData.indexsBuffer, "BUFFER_INDICIES");
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
void GlhFreeObject(GlhObject *obj) {}

void GlhInitFreeType() {
    if(FT_Init_FreeType(&ft)) {
        printf("ERROR, couldon't init freetype\n");
    }
}

void GlhFreeFreeType() {
    FT_Done_FreeType(ft);
}
 
void GlhFreeFont(GlhFont *font) {
    for(int i = 0; i < font->glyphsData.keyVector.size; i++) {
        free(vector_get(font->glyphsData.keyVector.data, i, char*));
    }
    map_free(&font->glyphsData);
    glDeleteTextures(1, &font->texture);
}

void GlhInitFont(GlhFont *font, char* ttfFileName, int size, int glyphCount, float packingPrecision) {
    // init the final map which will store all the existing glyph data, plus a single default char
    map_init(&font->glyphsData, sizeof(GlhFontGLyphData));
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
    } prePackingGlyphsData[_glyphCount + 2]; // glyphcount +2, to store every glyph, plus the "no glyph" glyph and a fill color glyph

    // char index in the font
    FT_UInt ci = 0;
    FT_ULong charcode;
    // of all chars
    int totalHeight = 1; // 1 to count the fill color glyph
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

    // add the fill color glyph
    prePackingGlyphsData[_glyphCount + 1].w = size;
    prePackingGlyphsData[_glyphCount + 1].h = size;
    prePackingGlyphsData[_glyphCount + 1].xo = 0;
    prePackingGlyphsData[_glyphCount + 1].yo = 0;
    prePackingGlyphsData[_glyphCount + 1].ad = size;
    prePackingGlyphsData[_glyphCount + 1].c = (unsigned long) -2;
    prePackingGlyphsData[_glyphCount + 1].px = calloc(size*size, 1);
    prePackingGlyphsData[_glyphCount + 1].ar = size*size;
    memset(prePackingGlyphsData[_glyphCount + 1].px, 0xff, size * size);

    // shitty easy to implement buble sort: sort prePackingGlyphsData by area (from largest to smallest)
    // to know if the array is sorted
    int swapCount = 1;
    while(swapCount != 0) {
        swapCount = 0;
        // the array has a length of glyphCount + 2, but because here we swap the current index and the next one
        // we need to not hit the end, or the next one won't exist
        for(int i = 0; i < _glyphCount + 1; i++) {
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
    int m = CharMarginSize * 2;
    int sideLength = totalHeight * (_glyphCount + 2) * m;
    bool packingSuccessfull = true;
    // will simply store the x and y offset of every glyph (with even indexes being the xs and odd indexes being the ys)
    int packs[(_glyphCount + 2) * 2];
    while(packingSuccessfull) {
        // same as packs
        int tmpPacks[(_glyphCount + 2) * 2];
        int packedGlyphsCount = 0;
        // TODO: better the packing algorithm
        // to know where to place the new glyph
        int lastY = 0;
        while(packedGlyphsCount < _glyphCount + 2) {
            // like wise
            int lastX = 0;
            while(packedGlyphsCount < _glyphCount + 2) {
                // if theres not enough horizontal or vertical room to put the glyph
                if(prePackingGlyphsData[packedGlyphsCount].w + m + lastX >= sideLength) break;
                if(prePackingGlyphsData[packedGlyphsCount].h + m + lastY >= sideLength) {
                    // to remember the reason we broke out of this while
                    packingSuccessfull = false;
                    break;
                }
                // store the offset
                tmpPacks[packedGlyphsCount*2+0] = lastX;
                tmpPacks[packedGlyphsCount*2+1] = lastY;
                // set the new offset
                lastX += prePackingGlyphsData[packedGlyphsCount].w + m;
                packedGlyphsCount ++;
            }
            // if the packing failed break now to avoid infinit loop
            if(!packingSuccessfull) break;
            // get max y value of all the packed glyph and set lastY to it
            for(int i = 0; i < packedGlyphsCount; i++) {
                int y = tmpPacks[i*2+1] + prePackingGlyphsData[i].h + m;
                lastY = lastY < y ? y : lastY;
            }
        }
        if(packingSuccessfull) {
            for(int i = 0; i < (_glyphCount + 2) * 2; i++) {
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
    for(int i = 0; i < _glyphCount + 2; i++) {
        // put the char info in the map
        GlhFontGLyphData info = {};
        float invSize = 1.0 / size;
        info.x0 = packs[i*2+0] + CharMarginSize;
        info.y0 = sideLength - 1 - (packs[i*2+1] + CharMarginSize) - prePackingGlyphsData[i].h; // all those sidelength - 1 - something is to convert
        info.y1 = sideLength - 1 - (packs[i*2+1] + CharMarginSize);            // the glyph's bitmap bottom left origin to the opengl top left origin
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
    set_opengl_label(GL_TEXTURE, font->texture, "TEXTURE_FONT_ATLAS");

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

void _characterToGlyphData(char c, GlhFont *font, GlhFontGLyphData *cdata) {
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

float GlhFontGetTextWidth(GlhFont *font, char* text) {
    int len = strlen(text);
    float width = 0;
    for(int i = 0; i < len; i++) {
        GlhFontGLyphData cdata;
        _characterToGlyphData(text[i], font, &cdata);
        width += cdata.advance;
    }
    return width;
}

void _characterToMesh(char c, GlhFont *font, float *xoff, float yoff, float *newVerticies, float *newTexCoords, int arrOffset) {
    GlhFontGLyphData cdata;
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

void GlhApplyTransformsToBoundingBox(GlhBoundingBox *box, GlhTransforms transforms) {
    mat4 mat;
    GlhTransformsToMat4(&transforms, &mat);
    glm_mat4_mulv3(mat, box->start, 1.0, box->start);
    glm_mat4_mulv3(mat, box->end, 1.0, box->end);
}

GlhBoundingBox GlhTextObjectGetBoundingBox(GlhTextObject *tob, float margin) {
    vec3 min = {(float)INT_MAX, (float)INT_MAX, (float)INT_MAX};
    vec3 max = {INT_MIN, INT_MIN, INT_MIN};
    for(int i = 0; i < tob->verticies.size; i+=3) {
        vec3 vert;
        vert[0] = vector_get(tob->verticies.data, i + 0, float);
        vert[1] = vector_get(tob->verticies.data, i + 1, float);
        vert[2] = vector_get(tob->verticies.data, i + 2, float);

        min[0] = vert[0] < min[0] ? vert[0] : min[0];
        min[1] = vert[1] < min[1] ? vert[1] : min[1];
        min[2] = vert[2] < min[2] ? vert[2] : min[2];

        max[0] = vert[0] > max[0] ? vert[0] : max[0];
        max[1] = vert[1] > max[1] ? vert[1] : max[1];
        max[2] = vert[2] > max[2] ? vert[2] : max[2];
    }

    min[0] -= margin; min[1] -= margin;
    max[0] += margin; max[1] += margin;

    GlhBoundingBox bndbx;
    
    glm_vec3_copy(min, bndbx.start);
    glm_vec3_copy(max, bndbx.end);

    return bndbx;
}

void GlhTextObjectUpdateMesh(GlhTextObject *tob, char* OldString) {
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
    vector_splice(&tob->verticies, charOff * 3 * 4, -1, NULL);
    vector_splice(&tob->texCoords, charOff * 2 * 4, -1, NULL);
    // recompute the new ones
    float newVerticies[changedLength * 3 * 4];
    float newTexCoords[changedLength * 2 * 4];
    // the offset where to put the quad
    float xoff = 0;
    // if we reuse data
    if (tob->verticies.size > 0 && charOff > 0) {
        // get the advance of the last reused glyph
        GlhFontGLyphData cd;
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
    float colorsArr[newLength * 4 * 4];
    memset_pattern(normalsArr, sizeof(normalsArr), normalsPattern, sizeof(normalsPattern));
    memset_pattern(colorsArr, sizeof(colorsArr), tob->color, sizeof(float) * 4);
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
    glBindBuffer(GL_ARRAY_BUFFER, tob->bufferData.colorsBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof( colorsArr), colorsArr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tob->bufferData.indexsBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indiciesArr), indiciesArr, GL_DYNAMIC_DRAW);
    // put data in buffers, using glBufferSubData if possible (= if no realloc needed)
    if(oldLength != newLength || OldString == NULL) {
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
    setAttribute(tob->bufferData.tcoordBuffer, 1, 2);
    setAttribute(tob->bufferData.normalBuffer, 2, 3);
    setAttribute(tob->bufferData.colorsBuffer, 3, 4);
    // "vertex" here as the number drawn, not the actual one (time 6 for two triangles per quad)
    tob->bufferData.vertexCount = newLength * 6;
    float max_y = vector_get(tob->verticies.data, tob->verticies.size -2, float);
    float max_x = vector_get(tob->verticies.data, tob->verticies.size -3, float);

    tob->transforms.transformsOrigin[0] = max_x * 0.5;
    tob->transforms.transformsOrigin[1] = max_y * 0.5;

    GlhBoundingBox boundingBox = GlhTextObjectGetBoundingBox(tob, 0.2);

    float backgroundVerts[] = {
        boundingBox.start[0], boundingBox.start[1], 0,
        boundingBox.end[0]  , boundingBox.start[1], 0,
        boundingBox.start[0], boundingBox.end[1]  , 0,
        boundingBox.end[0]  , boundingBox.end[1]  , 0
    };
    float backgroundColors[] = {
        tob->backgroundColor[0], tob->backgroundColor[1], tob->backgroundColor[2], tob->backgroundColor[3],
        tob->backgroundColor[0], tob->backgroundColor[1], tob->backgroundColor[2], tob->backgroundColor[3],
        tob->backgroundColor[0], tob->backgroundColor[1], tob->backgroundColor[2], tob->backgroundColor[3],
        tob->backgroundColor[0], tob->backgroundColor[1], tob->backgroundColor[2], tob->backgroundColor[3],
    };
    glBindVertexArray(tob->backgroundQuadBufferData.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, tob->backgroundQuadBufferData.vertexBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(backgroundVerts), backgroundVerts);
    glBindBuffer(GL_ARRAY_BUFFER, tob->backgroundQuadBufferData.colorsBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(backgroundColors), backgroundColors);
}

void GlhUpdateTextObjectModelMatrix(GlhTextObject *tob) {
    GlhTransformsToMat4(&tob->transforms, &tob->cachedModelMatrix);
}

void GlhTextObjectSetText(GlhTextObject *tob, char* string) {
    char oldState[strlen(tob->_text) + 1];
    strcpy(oldState, tob->_text);
    int strl = strlen(string) + 1;
    if(strl > tob->_textAllocated) {
        tob->_text = realloc(tob->_text, strl);
    }
    strcpy(tob->_text, string);
    GlhTextObjectUpdateMesh(tob, oldState);
}

char* GlhTextObjectGetText(GlhTextObject *tob) {
    return tob->_text;
}

// transforms can be NULL
void GlhInitTextObject(GlhTextObject *tob, char* string, GlhFont *font, vec4 color, vec4 backgroundColor, GlhTransforms *tsf) {
    tob->type = text;
    tob->font = font;
    tob->glyphProgram = &GlobalShaders.glyphs;
    tob->textProgram = &GlobalShaders.text;
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
    glm_vec4_copy(backgroundColor, tob->backgroundColor);
    tob->_text = malloc(tob->_textAllocated = strlen(string) + 1);
    tob->_text[0] = '\0';
    glGenVertexArrays(1, &tob->bufferData.VAO);
    glGenBuffers(1, &tob->bufferData.vertexBuffer);
    glGenBuffers(1, &tob->bufferData.tcoordBuffer);
    glGenBuffers(1, &tob->bufferData.normalBuffer);
    glGenBuffers(1, &tob->bufferData.indexsBuffer);
    glGenBuffers(1, &tob->bufferData.colorsBuffer);
    set_opengl_label(GL_VERTEX_ARRAY, tob->bufferData.VAO, "VAO_TEXT");
    set_opengl_label(GL_BUFFER, tob->bufferData.vertexBuffer, "BUFFER_TEXT_VERTEX");
    set_opengl_label(GL_BUFFER, tob->bufferData.tcoordBuffer, "BUFFER_TEXT_TEXCOORDS");
    set_opengl_label(GL_BUFFER, tob->bufferData.normalBuffer, "BUFFER_TEXT_NORMALS");
    set_opengl_label(GL_BUFFER, tob->bufferData.indexsBuffer, "BUFFER_TEXT_INDICES");
    set_opengl_label(GL_BUFFER, tob->bufferData.colorsBuffer, "BUFFER_TEXT_COLORS");

    glGenVertexArrays(1, &tob->backgroundQuadBufferData.VAO);
    glGenBuffers(1, &tob->backgroundQuadBufferData.vertexBuffer);
    glGenBuffers(1, &tob->backgroundQuadBufferData.normalBuffer);
    glGenBuffers(1, &tob->backgroundQuadBufferData.indexsBuffer);
    glGenBuffers(1, &tob->backgroundQuadBufferData.colorsBuffer);
    set_opengl_label(GL_VERTEX_ARRAY, tob->backgroundQuadBufferData.VAO, "VAO_TEXT_BACKGROUND");
    set_opengl_label(GL_BUFFER, tob->backgroundQuadBufferData.vertexBuffer, "BUFFER_TEXT_BACKGROUND_VERTEX");
    set_opengl_label(GL_BUFFER, tob->backgroundQuadBufferData.normalBuffer, "BUFFER_TEXT_BACKGROUND_NORMALS");
    set_opengl_label(GL_BUFFER, tob->backgroundQuadBufferData.indexsBuffer, "BUFFER_TEXT_BACKGROUND_INDICES");
    set_opengl_label(GL_BUFFER, tob->backgroundQuadBufferData.colorsBuffer, "BUFFER_TEXT_BACKGROUND_COLORS");

    // set background buffer
    float tmpVerts[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float tmpColors[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    float normals[12] = {0, 0, -1.0, 0, 0, -1.0, 0, 0, -1.0, 0, 0, -1.0};
    int indexes[6] = {0, 1, 2, 1, 3, 2};

    glBindVertexArray(tob->backgroundQuadBufferData.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, tob->backgroundQuadBufferData.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tmpVerts), tmpVerts, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, tob->backgroundQuadBufferData.normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(normals), normals, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, tob->backgroundQuadBufferData.colorsBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tmpColors), tmpColors, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tob->backgroundQuadBufferData.indexsBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexes), indexes, GL_STATIC_DRAW);

    // bind attributes to VAO
    setAttribute(tob->backgroundQuadBufferData.vertexBuffer, 0, 3);
    setAttribute(tob->backgroundQuadBufferData.normalBuffer, 1, 3);
    setAttribute(tob->backgroundQuadBufferData.colorsBuffer, 2, 4);

    tob->backgroundQuadBufferData.vertexCount = 6;

    GlhTextObjectSetText(tob, string);
}

void GlhFreeTextObject(GlhTextObject *tob) {
    vector_free(tob->verticies);
    vector_free(tob->texCoords);
    free(tob->_text);
}

void GlhInitComputeShader(GlhComputeShader *cs, char* filename) {
    GLuint shader;
    loadShader(filename, GL_COMPUTE_SHADER, &shader);
    cs->program = glCreateProgram();
    set_opengl_label(GL_PROGRAM, cs->program, "SHADER_PROGRAM_COMPUTE");
    glAttachShader(cs->program, shader);
    glLinkProgram(cs->program);
}

void GlhRunComputeShader(GlhComputeShader *cs, GLuint inputTexture, GLuint outputTexture, GLenum sizedInFormat, GLenum sizedOutFormat, int workGroupsWidth, int workGroupsHeight) {
    glUseProgram(cs->program);
    glBindImageTexture(1, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, sizedOutFormat);
    glBindImageTexture(0, inputTexture, 0, GL_FALSE, 0, GL_READ_ONLY, sizedInFormat);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glDispatchCompute(workGroupsWidth, workGroupsHeight, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

int loadTexture(GLuint *texture, char* filename, bool alpha, GLenum interpolation) {
    // create, bind texture and set parameters
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    set_opengl_label(GL_TEXTURE, *texture, "TEXTURE");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolation);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolation);
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
    set_opengl_label(GL_TEXTURE, *texture, "TEXTURE_SINGLE_COLOR");
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
    set_opengl_label(GL_TEXTURE, *texture, "TEXTURE_BLANK");
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, sizedFormat, width, height, 0, format, type, NULL);
    glGenerateMipmap(GL_TEXTURE_2D);
}