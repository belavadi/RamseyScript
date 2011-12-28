#ifndef RECURSE_H
#define RECURSE_H

#include "sequence.h"
#include "filters.h"

void recurse_sequence (Sequence *seed, int min, int max, filter_func filter);
void recurse_words (Sequence *seed, Sequence *alphabet, filter_func filter);
void recurse_colorings_breadth_first (Coloring *seed, int max_value, int min,
                                      int max, filter_func filter);
void recurse_colorings(Coloring *seed, int max_value, int min,
                       int max, filter_func filter);

#endif
