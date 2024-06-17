#ifndef UI_OPTION_H
#define UI_OPTION_H

#include "common.h"

#define ENT_BUFF_SIZE 64
#define SUBLIST_MAX   8

#ifdef WITH_PLOT
#define MIN_VIEW_ANGLE -180
#define MAX_VIEW_ANGLE  180
#endif

enum { MINIMAX_SPIN, ROTOR_COLUMN };

enum { ENT_SPN_TTL,
#ifdef WITH_PLOT
  ENT_SPN_COLOR, ENT_SPN_GLOBAL, ENT_SPN_LOCAL,
#endif
};

enum { ENT_RAD_IPV, ENT_RAD_GRAPH };

typedef struct ent_ndx {
  int type;
  const char *name;
} t_ent_ndx;

typedef struct ent_str {
  t_ent_ndx en;
  int  *pint, idef; // int fields
  char *pstr; int slen; const char *sdef; // str fields
  GtkWidget *box, *input;
  const int len, width;
  const t_minmax range;
  char hint[ENT_BUFF_SIZE];
  char buff[ENT_BUFF_SIZE];
} t_ent_str;

typedef gboolean* (*bool_fn)(int);

typedef struct ent_bool {
  t_ent_ndx en;
  gboolean *pval; // at compile-time
  bool_fn valfn; int valtype; // at run-time
  const char *prefix;
  GtkCheckButton *check;
} t_ent_bool;

typedef struct ent_exp_common {
  t_ent_ndx en;
  GtkWidget *arrow, *box, *sub; // aux widgets
  gboolean atrun;               // active at run
} t_ent_exp_common;

typedef struct ent_exp {
  t_ent_exp_common c;
  t_elem_desc *desc; // reference to element group descripotion
} t_ent_exp;

typedef struct ent_spn_aux {
  GCallback cb;
  int *pval;   // indexed: 0 .. lim-1
  int pn, wrap; gboolean rev; // pval ndx, wrap, reverse axis
  int def;     // not-indexed default: 1 .. lim
  int *step;
  int sn;      // ref to parent's ndx
  t_minmax *mm;
  const char *prfx, *sfx;
  GtkSpinButton *spin;
} t_ent_spn_aux;

typedef struct ent_spn_elem {
  const char *title, *delim;
  gboolean bond;
  int kind;
  t_ent_spn_aux aux[4];
} t_ent_spn_elem;

typedef struct ent_spn {
  t_ent_exp_common c;
  t_ent_spn_elem list[SUBLIST_MAX];
} t_ent_spn;

gboolean option_init(GtkWidget* bar);
void option_update(void);
void option_legend(void);
void option_up_menu_main(void);
void option_up_menu_ext(void);

extern t_ent_bool ent_bool[];
extern t_ent_str  ent_str[];
extern t_ent_spn  ent_spn[];

extern t_minmax opt_mm_ttl;
#ifdef WITH_PLOT
extern t_minmax opt_mm_col;
extern t_minmax opt_mm_rot;
extern t_minmax opt_mm_ang;
void set_rotor_n_redraw(int step, gboolean rev, int n);
#endif

#endif
