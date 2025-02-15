#ifndef CONFIG_H
#define CONFIG_H

/* featutes */
#cmakedefine WITH_DND
#cmakedefine WITH_JSON
#cmakedefine WITH_PLOT
#cmakedefine WITH_NLS

/* aux */
#cmakedefine PINGDIR "@PINGDIR@"
#cmakedefine DATADIR "@DATADIR@"
#cmakedefine LOCALEDIR "@LOCALEDIR@"
#cmakedefine HAVE_SECURE_GETENV
#cmakedefine HAVE_LOCALTIME_R
#cmakedefine HAVE_USELOCALE
#cmakedefine WITH_SNAPHINT

#endif
