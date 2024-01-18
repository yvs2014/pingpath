#ifndef UI_OPTION_H
#define UI_OPTION_H

#include "common.h"

#define ENT_BUFF_SIZE 64
#define SUBLIST_MAX   16

enum { SPN_AUX_MIN, SPN_AUX_LIM, SPN_AUX_MAX };

enum { ENT_SPN_NONE, ENT_SPN_TTL, ENT_SPN_MAX };
enum { ENT_RAD_NONE, ENT_RAD_IPV, ENT_RAD_GRAPH, ENT_RAD_MAX };

typedef struct ent_ndx {
  int typ;
  const gchar *name;
} t_ent_ndx;

typedef struct ent_str {
  t_ent_ndx en;
  int  *pint, idef; // int fields
  char *pstr; int slen; const char *sdef; // str fields
  GtkWidget *box, *input;
  const int len, width;
  const t_minmax range;
  gchar hint[ENT_BUFF_SIZE];
  gchar buff[ENT_BUFF_SIZE];
} t_ent_str;

typedef struct ent_bool {
  t_ent_ndx en;
  gboolean *pval;
  const gchar *prefix;
  GtkCheckButton *check;
} t_ent_bool;

typedef struct ent_exp_common {
  t_ent_ndx en;
  GtkWidget *arrow, *box, *sub; // aux widgets
  gboolean atrun;               // active at run
} t_ent_exp_common;

typedef struct ent_exp {
  t_ent_exp_common c;
  int ndxs[SUBLIST_MAX];  // 0 terminated indexes, otherwise max
} t_ent_exp;

typedef struct ent_rad_map {
  int ndx, val;
  gchar *str;
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
void option_toggled(int ndx);

extern t_ent_bool ent_bool[];
extern t_ent_str  ent_str[];
extern t_ent_spn  ent_spn[];

#endif
