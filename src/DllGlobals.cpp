#include "DllGlobals.h"
HMODULE g_module = nullptr;
volatile long g_objectCount = 0;
volatile long g_lockCount = 0;
