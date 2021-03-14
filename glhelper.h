#define DEBUG
#include <cglm/cglm.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "vector.h"

#define OPT(a, b, c) (a == c ? b : a)

struct GlhCamera {
    vec3 position;
    vec3 rotation;
    float fov;
    float zNear;
    float zFar;
};

struct GlhTransforms {
    vec3 scale;
    vec3 translation;
    vec3 rotation;
};

struct GlhMeshBufferData {
    GLuint vertexBuffer;
    GLuint normalBuffer;
    GLuint indexsBuffer;
    GLuint tcoordBuffer;
    GLuint VAO;
    int vertexCount;
};

struct GlhMesh {
    struct GlhMeshBufferData bufferData;
    Vector verticies;
    Vector normals;
    Vector indexes;
    Vector texCoords;
};

struct GlhObject {
    struct GlhTransforms transforms;
    struct GlhMesh *mesh;
    struct GlhProgram *program;
    mat4 cachedModelMatrix;
    GLuint texture;
};

struct GlhContext {
    GLFWwindow *window;
    struct GlhCamera camera;
    mat4 cachedViewMatrix;
    mat4 cachedProjectionMatrix;
    Vector children;
};

struct GlhProgram {
    Vector uniforms;
    Vector uniformsLocation;
    Vector attributes;
    GLuint shaderProgram;
    void (*setGlobalUniforms)(struct GlhObject*, struct GlhContext*);
};

void GlhInitProgram(struct GlhProgram *prg, char* fragSourceFilename, char* vertSourceFilename, char* uniforms[], int uniformsCount, void (*setUniforms)());
void GlhFreeProgram(struct GlhProgram *prg);
void GlhInitContext(struct GlhContext *ctx, int windowWidth, int windowHeight, char* windowTitle);
void GlhFreeContext(struct GlhContext *ctx);
void GlhContextAppendChild(struct GlhContext *ctx, struct GlhObject *child);
void GlhComputeContextViewMatrix(struct GlhContext *ctx);
void GlhComputeContextProjectionMatrix(struct GlhContext *ctx);
void GlhRenderContext(struct GlhContext *ctx);
void GlhInitMesh(struct GlhMesh *mesh, vec3 verticies[], int verticiesCount, vec3 normals[], vec3 indices[], int indicesCount, vec2 texcoords[], int texcoordsCount);
void GlhGenerateMeshBuffers(struct GlhMesh *mesh);
void GlhFreeMesh(struct GlhMesh *mesh);
void GlhRenderObject(struct GlhObject *obj, struct GlhContext *ctx);
void GlhInitObject(struct GlhObject *obj, GLuint texture, vec3 scale, vec3 rotation, vec3 translation, struct GlhMesh *mesh, struct GlhProgram *program);
void GlhUpdateObjectModelMatrix(struct GlhObject *obj);
void GlhFreeObject(struct GlhObject *obj);
int loadTexture(GLuint *texture, char* filename);