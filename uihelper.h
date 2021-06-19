#ifndef _UIHELPER_H
#define _UIHELPER_H
#include "glhelper.h"
#include "vector.h"
#include "stack.h"

typedef struct {
    struct GlhContext glhctx;
    
} UIHContext;

typedef enum {
    object,
    layout
} UIHElementType;

typedef struct {
    UIHElementType type;
} UIHAbstractElement;

typedef struct {
    UIHElementType type;
    GlhElement glhel;
} UIHObject;

typedef struct {
    UIHElementType type;
    Vector childrens;
} UIHLayout;

typedef union {
    UIHAbstractElement any;
    UIHObject object;
    UIHLayout layout;
} UIHElement;

typedef struct {
    UIHContext *ctx;
    UIHElement content;
} UIHLayer;

#endif