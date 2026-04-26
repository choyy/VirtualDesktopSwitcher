#ifndef GRADIENT_H
#define GRADIENT_H

#include <windows.h>

typedef enum {
    GRADIENT_NONE = 0,
} GradientType;

static inline GradientType GetGradientTypeByName(const wchar_t* name) {
    (void)name;
    return GRADIENT_NONE;
}

#endif
