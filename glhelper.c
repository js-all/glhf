#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "glhelper.h"

static const char* attributes[] = {
    "vPos",
    "vNormal",
    "vTexCoord"
};

int readFile(char* filename, int* size,char **content) {
	FILE* file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	long f_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	*content = malloc((f_size + 1) * sizeof(char));
	if(!*content) {
		printf("ERROR: readFile, unable to allocate memory");
		return -1;
	}
	fread(*content, f_size, 1, file);
	fclose(file);
    (*content)[f_size] = '\0';
	*size = f_size;
    printf("read file %s, content:\n\033[36m%s\033[0m\n", filename, *content);
	return 0;
}

int loadShader(char* filename, int type, GLuint *shader) {    
    char* source;
    int size;
    if(readFile(filename, &size, &source) != 0) {
        return -1;
    }; 
    const char* shader_source = source;
    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &shader_source, NULL);
    glCompileShader(*shader);
    GLint status;
    GLenum error;
    int logsLength;
    error = glGetError();
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logsLength);
    char log[logsLength+1];
    memset(log, '\0', logsLength+1);
    glGetShaderInfoLog(*shader, logsLength, NULL, log);
    printf("compiled shader %s, compile status: %i (GL_TRUE: %i), error: %i, logs: [%i]\n%s\n", filename, status, GL_TRUE, error, logsLength, log);
    free(source);
    return 0;
}

int initProgram(char* fragSourceFilename, char* vertSourceFilename, GLuint *program) {
    GLuint vert;
    GLuint frag;
    loadShader(vertSourceFilename, GL_VERTEX_SHADER, &vert);
    loadShader(fragSourceFilename, GL_FRAGMENT_SHADER, &frag);
    glAttachShader(*program, vert);
    glAttachShader(*program, frag);
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
    printf("linking status: %i, error: %i, logs: [%i] \n%s\n", status, error, logsLength, logs);
    return 0;
}

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)()) {
    int attributesCount = sizeof(attributes) / sizeof(attributes[0]);
    prg->shaderProgram = glCreateProgram();

    for(int i = 0; i < attributesCount; i++) {
        glBindAttribLocation(prg->shaderProgram, i, attributes[i]);
    }

    initProgram(fragSourceFilename, vertSourceFilename, &prg->shaderProgram);
    vector_init(&prg->uniforms, uniformsCount, sizeof(char*));
    vector_init(&prg->uniformsLocation, uniformsCount, sizeof(GLint));
    vector_init(&prg->attributes, attributesCount, sizeof(char*));

    vector_push_array(prg->uniforms, uniforms, uniformsCount);
    vector_push_array(prg->attributes, attributes, attributesCount);

    for(int i = 0; i < uniformsCount; i++) {
        GLint v = glGetUniformLocation(prg->shaderProgram, vector_get(prg->uniforms.data, i, char*));
        vector_push(&prg->uniformsLocation, &v);
    }

    printf("\n");
    prg->setGlobalUniforms = setUniforms;
}

void GlhFreeProgram(struct GlhProgram *prg) {
    vector_free(&prg->attributes);
    vector_free(&prg->uniforms);
    vector_free(&prg->uniformsLocation);
}

void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ctx->window = glfwCreateWindow(OPT(windowWidth, 640, 0), OPT(windowHeight, 480, 0), OPT(windowTitle, "window", NULL), NULL, NULL);
    glfwMakeContextCurrent(ctx->window);
    glewInit();
    glfwSwapInterval(1);
    ctx->camera.fov = glm_rad(90);
    glm_vec3_zero(ctx->camera.position);
    glm_vec3_zero(ctx->camera.rotation);
    ctx->camera.zNear = 0.1;
    ctx->camera.zFar = 100;
    vector_init(&ctx->children, 2, sizeof(void*));
    int width, height;
    glfwGetWindowSize(ctx->window, &width, &height);
    glViewport(0, 0, width, height);
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
    glm_vec3_negate_to(ctx->camera.position, translation);
    glm_vec3_negate_to(ctx->camera.rotation, rotation);
    glm_mat4_identity(ctx->cachedViewMatrix);
    glm_translate(ctx->cachedViewMatrix, translation);
    glm_rotate_x(ctx->cachedViewMatrix, rotation[0], ctx->cachedViewMatrix);
    glm_rotate_y(ctx->cachedViewMatrix, rotation[1], ctx->cachedViewMatrix);
    glm_rotate_z(ctx->cachedViewMatrix, rotation[2], ctx->cachedViewMatrix);
}

void GlhComputeContextProjectionMatrix(struct GlhContext *ctx) { 
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    float aspect = (float) width / (float) height;
    float f = tanf(M_PI * 0.5 - 0.5 * glm_rad(90));
    float fn = 1.0f / (ctx->camera.zNear - ctx->camera.zFar);
    mat4 p = {
        {f / aspect, 0, 0, 0},
        {0, f, 0, 0},
        {0, 0, (ctx->camera.zNear + ctx->camera.zFar) * fn, -1},
        {0, 0, ctx->camera.zNear * ctx->camera.zFar * fn * 2, 0}
    };
    glm_mat4_identity(ctx->cachedProjectionMatrix);
    glm_mat4_mul(ctx->cachedProjectionMatrix, p, ctx->cachedProjectionMatrix);
}

void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx) {
    glUseProgram(obj->program->shaderProgram);
    (*obj->program->setGlobalUniforms)(obj, ctx);
    glBindTexture(GL_TEXTURE_2D, obj->texture);
    glBindVertexArray(obj->mesh->bufferData.VAO);
    glDrawElements(GL_TRIANGLES, obj->mesh->bufferData.vertexCount, GL_UNSIGNED_INT, NULL);
}

void GlhRenderContext(struct GlhContext *ctx) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for(int i = 0; i < ctx->children.size; i++) {
        struct GlhObject* obj = vector_get(ctx->children.data, i, struct GlhObject*);
        GlhRenderObject(obj, ctx);
    }
}

void setAttribute(GLuint buffer, GLuint location, int comp) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(location, comp, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
}

void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount) {
    struct GlhMeshBufferData data = {};
    mesh->bufferData = data;
    vector_init(&mesh->verticies, verticiesCount, sizeof(vec3));
    vector_init(&mesh->normals, verticiesCount, sizeof(vec3));
    vector_init(&mesh->indexes, indicesCount, sizeof(vec3));
    vector_init(&mesh->texCoords, texcoordsCount, sizeof(vec2));
    vector_push_array(mesh->verticies, verticies, verticiesCount);
    vector_push_array(mesh->normals, normals, verticiesCount);
    vector_push_array(mesh->indexes, indices, indicesCount);
    vector_push_array(mesh->texCoords, texcoords, texcoordsCount);
    glGenVertexArrays(1, &mesh->bufferData.VAO);
    glBindVertexArray(mesh->bufferData.VAO);
    GlhGenerateMeshBuffers(mesh);

    setAttribute(mesh->bufferData.vertexBuffer, 0, 3);
    setAttribute(mesh->bufferData.normalBuffer, 1, 3);
    setAttribute(mesh->bufferData.tcoordBuffer, 2, 2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->bufferData.indexsBuffer);
}

void GlhFreeMesh(struct GlhMesh *mesh) {
    vector_free(&mesh->verticies);
    vector_free(&mesh->normals);
    vector_free(&mesh->indexes);
    vector_free(&mesh->texCoords);
}

void GlhInitObject(struct GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program) {
    struct GlhTransforms tsfm = {};
    glm_vec3_dup(scale, tsfm.scale);
    glm_vec3_dup(rotation, tsfm.rotation);
    glm_vec3_dup(translation, tsfm.translation);
    obj->transforms = tsfm;
    obj->mesh = mesh;
    obj->program = program;
    obj->texture = texture;
}

void GlhUpdateObjectModelMatrix(struct GlhObject *obj) {
    glm_mat4_identity(obj->cachedModelMatrix);
    // don't ask me why, but that is the correct order
    glm_translate(obj->cachedModelMatrix, obj->transforms.translation);
    glm_rotate_x(obj->cachedModelMatrix, obj->transforms.rotation[0], obj->cachedModelMatrix);
    glm_rotate_y(obj->cachedModelMatrix, obj->transforms.rotation[1], obj->cachedModelMatrix);
    glm_rotate_z(obj->cachedModelMatrix, obj->transforms.rotation[2], obj->cachedModelMatrix);
    glm_scale(obj->cachedModelMatrix, obj->transforms.scale);
}

void initArrayBuffer(GLuint *buffer, int components, Vector *vec, char* name) {
    glGenBuffers(1, buffer);
    glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    float array[vec->size * components];
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
    glBufferData(GL_ARRAY_BUFFER, vec->size * components * sizeof(float), &array[0], GL_STATIC_DRAW);
}

void GlhGenerateMeshBuffers(struct GlhMesh *mesh) {

    initArrayBuffer(&mesh->bufferData.vertexBuffer, 3, &mesh->verticies, "verticies");
    initArrayBuffer(&mesh->bufferData.normalBuffer, 3, &mesh->normals, "normals");
    initArrayBuffer(&mesh->bufferData.tcoordBuffer, 2, &mesh->texCoords, "texture coordinates");

    glGenBuffers(1, &mesh->bufferData.indexsBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->bufferData.indexsBuffer);
    int indexes[mesh->indexes.size * 3];
    for (int i = 0; i < (mesh->indexes.size) * 3; i += 3) {
        indexes[i + 0] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[0]);
        indexes[i + 1] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[1]);
        indexes[i + 2] = (int)(vector_get(mesh->indexes.data, i/3, vec3)[2]);
    }
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexes.size * 3 * sizeof(unsigned int), &indexes[0], GL_STATIC_DRAW);
    printf("dfsgùmlikdfg %i\n", mesh->indexes.size);
    mesh->bufferData.vertexCount = mesh->indexes.size * 3;
}


void GlhFreeObject(struct GlhObject *obj) {
    
}

int loadTexture(GLuint *texture, char* filename) {
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if(data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        printf("unable to load image\n");
        return -1;
    }
    stbi_image_free(data);
    return 0;
}