// Test substitute for src/phd.h.
//
// The real phd.h pulls in ~30 wxWidgets headers and the entire project's
// header surface (Mount, MyFrame, GuideAlgorithm, ...). Test binaries don't
// need any of that — they instantiate small isolated pieces (JsonParser,
// AlpacaDiscovery's parser, individual guide algorithms via stub Mount).
//
// This file gets onto the include path BEFORE src/ for test targets, so a
// production .cpp doing `#include "phd.h"` lands here instead. The
// substitute provides:
//   - the C++/std headers that production .cpp files implicitly rely on
//     phd.h to bring in (<string>, <algorithm>, <math.h>, etc.)
//   - the small set of macros used outside of GUI code (POSSIBLY_UNUSED,
//     ROUND, ERROR_INFO).
//
// Wider tests (guide algorithms, star detection) include test_phd_full.h
// AFTER this header to layer on stub Mount/MyFrame/Debug etc.

#ifndef PHD_H_INCLUDED
#define PHD_H_INCLUDED

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <map>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <string>
#include <vector>

#define POSSIBLY_UNUSED(x) (void) (x)

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// In production these add a line to the Debug log. In tests they do
// nothing — the math methods we exercise call them via Debug.Write(...)
// from outside this macro, so we don't need to capture them here.
#define THROW_INFO_BASE(intro, file, line) intro " " file ":" TOSTRING(line)
#define LOG_INFO(s) ((void) 0)
#define THROW_INFO(s) (s)
#define ERROR_INFO(s) (s)

#define ROUND(x) (int) floor((x) + 0.5)
#define ROUNDF(x) (int) floorf((x) + 0.5)
#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))

#endif // PHD_H_INCLUDED
