//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

#include <stdbool.h>
#include <string.h>

static inline bool streq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}
