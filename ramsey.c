
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "global.h"
#include "ramsey.h"
#include "sequence.h"
#include "coloring.h"
#include "recurse.h"
#include "filters.h"
#include "check.h"

#define strmatch(s, r) (!strcmp ((s), (r)))
#define MATCH_THEN_SET(tok, text)		\
  ((tok) != NULL && strmatch ((tok), #text))	\
    {						\
      char *new_tok = strtok (NULL, " \t\n");	\
      state->text = strtoul (new_tok, NULL, 0);	\
    }

struct _global_data *set_defaults ()
{
  struct _global_data *rv = malloc (sizeof *rv);
  if (rv)
    {
      rv->iterations = 0;
      rv->min_gap = 1;
      rv->max_gap = 0;
      rv->n_colors = 3;
      rv->ap_length = 3;
      rv->alphabet = sequence_parse ("[1 2 3 4]");
      rv->gap_set = NULL;
      rv->filter = cheap_check_sequence3;

      rv->dump_stream = file_stream_new ("w");
      rv->dump_stream->_data = stdout;
      rv->dump_iters = 0;
      rv->dump_depth = 400;
      rv->iters_data = NULL;

      rv->kill_now = 0;
    }
  return rv;
}

void process (struct _global_data *state)
{
  char *buf;
  int i;

  /* Parse */
  while ((buf = state->in_stream->read_line (state->in_stream)))
    {
      char *tok;
      /* Convert all - signs to _ so lispers can feel at home */
      for (i = 0; buf[i]; ++i)
        if (buf[i] == '-')
          buf[i] = '_';
        else if (isalpha (buf[i]))
          buf[i] = tolower (buf[i]);

      tok = strtok (buf, " \t\n");

      /* skip comments and blank lines */
      if (tok == NULL || *tok == '#')
        {
          free (buf);
          continue;
        }

      /* set <min_gap|max_gap|n_colors|ap_length|alphabet|gap_set> <N> */
      if (strmatch (tok, "set"))
        {
          tok = strtok (NULL, " \t\n");
          if MATCH_THEN_SET (tok, min_gap)
          else if MATCH_THEN_SET (tok, max_gap)
          else if MATCH_THEN_SET (tok, n_colors)
          else if MATCH_THEN_SET (tok, ap_length)
          else if MATCH_THEN_SET (tok, iterations)
          else if MATCH_THEN_SET (tok, dump_depth)
          else if (strmatch (tok, "alphabet"))
            {
              tok = strtok (NULL, "\n");
              sequence_delete (state->alphabet);
              state->alphabet = sequence_parse (tok);
            }
          else if (strmatch (tok, "gap_set"))
            {
              tok = strtok (NULL, "\n");
              state->gap_set = sequence_parse (tok);
            }
          else if (strmatch (tok, "dump_file"))
            {
              tok = strtok (NULL, "\n");
              if (state->dump_stream->_data && state->dump_stream->_data != stdout)
                fclose (state->dump_stream->_data);
              if (strmatch (tok, "_"))
                state->dump_stream->_data = stdout;
              else
                {
                  state->dump_stream->_data = fopen (tok, "a");
                  if (state->dump_stream->_data == NULL)
                    {
                      fprintf (stderr, "Failed to open ``%s'' for writing. Using stdout instead.\n", tok);
                      state->dump_stream->_data = stdout;
                    }
                }
            }
        }
      /* filter <no-double-3-aps|no-additive-squares> */
      else if (strmatch (tok, "filter"))
        {
          tok = strtok (NULL, " \t\n");
          if (tok && strmatch (tok, "no_double_3_aps"))
            state->filter = cheap_check_sequence3;
          else if (tok && strmatch (tok, "no_additive_squares"))
            state->filter = cheap_check_additive_square;
          else
            fprintf (stderr, "Unknown filter ``%s''\n", tok);
	}
      /* dump <iterations-per-length> */
      else if (strmatch (tok, "dump"))
        {
          tok = strtok (NULL, " \t\n");
          if (strmatch (tok, "iterations_per_length"))
            state->dump_iters = 1;
          else
            fprintf (stderr, "Unknown dump format ``%s''\n", tok);
        }
      /* search <seqences|colorings|words> [seed] */
      else if (strmatch (tok, "search"))
        {
          time_t start = time (NULL);
          reset_max ();

          if (state->dump_iters)
            {
              free (state->iters_data);
              state->iters_data = sequence_new_zeros (state->dump_depth);
            }

          tok = strtok (NULL, " \t\n");
          if (tok && strmatch (tok, "sequences"))
            {
              Sequence *seek;

              tok = strtok (NULL, " \t\n");
              if (tok && *tok == '[')
                seek = sequence_parse (tok);
              else
                {
                  seek = sequence_new ();
                  sequence_append (seek, 1);
                }

              state->out_stream->write_line (state->out_stream, "#### Starting sequence search ####\n");
              if (state->iterations > 0)
                stream_printf (state->out_stream, "  Stop after: \t%ld iterations\n", state->iterations);
              stream_printf (state->out_stream,
                             "  Minimum gap:\t%d\n"
                             "  Maximum gap:\t%d\n"
                             "  AP length:\t%d\n"
                             "  Seed Seq.:\t",
                             state->min_gap, state->max_gap, state->ap_length);
              sequence_print (seek, state->out_stream);
              state->out_stream->write_line (state->out_stream, "\n");

              if (seek == NULL)
                {
                  fprintf (stderr, "Failed to allocate sequence.");
                  exit (EXIT_FAILURE);
                }

              if (state->gap_set == NULL)
                {
                  state->gap_set = sequence_new ();
                  for (i = state->min_gap; i <= state->max_gap; ++i)
                    sequence_append (state->gap_set, i);
                }
              recurse_sequence (seek, state);
              sequence_delete (seek);
            }
          else if (strmatch (tok, "colorings") ||
                   strmatch (tok, "partitions"))
            {
              Coloring *seek;

/*
              tok = strtok (NULL, " \t\n");
              if (tok && *tok == '[')
                seek = sequence_parse (tok);
              else
*/
                {
                  seek = coloring_new (state->n_colors);
                  coloring_append (seek, 1, 0);
                }

              stream_printf (state->out_stream, "#### Starting coloring search ####\n");
              if (state->iterations > 0)
                stream_printf (state->out_stream, "  Stop after: \t%ld iterations\n", state->iterations);
              stream_printf (state->out_stream,
                             "  Minimum gap:\t%d\n"
                             "  Maximum gap:\t%d\n"
                             "  AP length:\t%d\n"
                             "  Seed Col.:\t",
                             state->min_gap, state->max_gap, state->ap_length);
              state->out_stream->write_line (state->out_stream, "\n");

              recurse_colorings (seek, 1, state);

              coloring_delete (seek);
            }
          else if (tok && strmatch (tok, "words"))
            {
              Sequence *seek = sequence_new ();

              stream_printf (state->out_stream, "#### Starting word search ####\n");
              if (state->iterations > 0)
                stream_printf (state->out_stream, "  Stop after: \t%ld iterations\n", state->iterations);
              stream_printf (state->out_stream, "  Alphabet:\t"); sequence_print (state->alphabet, state->out_stream);
              stream_printf (state->out_stream, "\n  Seed Seq.:\t"); sequence_print (seek, state->out_stream);
              stream_printf (state->out_stream, "\n");

              recurse_words (seek, state);

              sequence_delete (state->alphabet);
              sequence_delete (seek);
            }
          else
            fprintf (stderr, "Unrecognized search space ``%s''\n", tok);
          stream_printf (state->out_stream, "Done. Time taken: %ds. Iterations: %ld\n",
                         (int) (time (NULL) - start), get_iterations());
          if (state->dump_iters)
            {
              sequence_print_real (state->iters_data, 1, state->dump_stream);
              fputs ("\n", state->dump_stream->_data);
            }
          state->out_stream->write_line (state->out_stream, "\n");
        }
      free (buf);
    }

  /* Cleanup */
  state->dump_stream->destroy (state->dump_stream);
}

