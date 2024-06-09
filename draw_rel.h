#ifndef DRAW_REL_H
#define DRAW_REL_H

#include "tabs/graph.h"

#ifdef WITH_PLOT
#include "tabs/plot.h"
#define STYLE_SET style_set(opts.darktheme, opts.darkgraph, opts.darkplot)
#else
#define STYLE_SET style_set(opts.darktheme, opts.darkgraph, false)
#endif

#endif
