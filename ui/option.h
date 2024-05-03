#ifndef UI_OPTION_H
#define UI_OPTION_H

#include "common.h"

#define ENT_BUFF_SIZE 64
#define SUBLIST_MAX   16

enum { SPN_AUX_MIN, SPN_AUX_LIM, SPN_AUX_MAX };

enum { ENT_SPN_NONE, ENT_SPN_TTL, ENT_SPN_MAX };
enum { ENT_RAD_NONE, ENT_RAD_IPV, ENT_RAD_GRAPH, ENT_RAD_MAX };

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

typedef struct ent_rad_map {
  int ndx, val;
  char *str;
} t_ent_rad_map;

typedef struct ent_rad {
  t_ent_exp_common c;
  int *pval;
  GCallback cb;
  t_ent_rad_map map[SUBLIST_MAX]; // 0 str terminated map, otherwise max
} t_ent_rad;

typedef struct ent_spn_aux {
  int *pval; // indexed: 0 .. lim-1
  int def;   // not-indexed default: 1 .. lim
  GtkWidget *spn;
  GCallback cb;
} t_ent_spn_aux;

typedef struct ent_spn {
  t_ent_exp_common c;
  t_ent_spn_aux aux[SPN_AUX_MAX];
} t_ent_spn;

gboolean option_init(GtkWidget* bar);
void option_update(void);
void option_legend(void);
gboolean option_update_pingmenu(void);

extern t_ent_bool ent_bool[];
extern t_ent_str  ent_str[];
extern t_ent_spn  ent_spn[];

#endif
