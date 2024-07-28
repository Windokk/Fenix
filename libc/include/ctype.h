#pragma once

#ifndef __CTYPE_H
#define __CTYPE_H

#include "stdbool.h"

bool isalpha(int);
bool isctrl(int);
bool isdigit(int);
bool isspace(int);
bool isprint(int);
int tolower(int);
int toupper(int);

#endif