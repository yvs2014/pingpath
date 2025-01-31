#ifndef TEXT_H
#define TEXT_H

#ifdef WITH_NLS
#include <glib/gi18n.h>
#else
#define _(text) (text)
#endif

// common
#define BUTTON_OKAY      _("Okay")
#define UNIT_SEC         _("sec")
#define UNIT_HEX         _("0x")

// tab names-n-tips
#define PING_TAB_TAG  _("Trace")
#define PING_TAB_TIP  _("Ping path with stats")
#define LOG_TAB_TAG   _("Log")
#define LOG_TAB_TIP   _("Auxiliary logs")
#define GRAPH_TAB_TAG _("Graphs")
#define GRAPH_TAB_TIP _("2D graphs")
#define PLOT_TAB_TAG  _("3D")
#define PLOT_TAB_TIP  _("3D graphics")

// help window
#define HELP_ACT_TITLE   _("Actions")
#define HELP_CLI_TITLE   _("CLI")
#define HELP_THEME_MAIN  _("Main theme")
#define HELP_THEME_GRAPH _("Graph theme")
#define HELP_THEME_3D    _("3D-plot theme")

// menu tooltips
#define OPT_ACTIONS_TIP  _("Command Menu")
#define OPT_MAINMENU_TIP _("Main Options")
#define OPT_AUXMENU_TIP  _("Auxiliary")

// actions
#define ACT_START_HDR   _("Start")
#define ACT_STOP_HDR    _("Stop")
#define ACT_PAUSE_HDR   _("Pause")
#define ACT_RESUME_HDR  _("Resume")
#define ACT_RESET_HDR   _("Reset")
#define ACT_HELP_HDR    _("Help")
#define ACT_QUIT_HDR    _("Exit")
#define ACT_LGND_HDR    "Legend"
#define ACT_COPY_HDR    "Copy"
#define ACT_SALL_HDR    "Select all"
#define ACT_UNSALL_HDR  "Unselect all"
#define ACT_LEFT_K_HDR   "Rotate Left"
#define ACT_RIGHT_K_HDR  "Rotate Right"
#define ACT_UP_K_HDR     "Rotate Up"
#define ACT_DOWN_K_HDR   "Rotate Down"
#define ACT_PGUP_K_HDR   "Rotate Clockwise"
#define ACT_PGDN_K_HDR   "Rotate AntiClockwise"
#define ACT_IN_K_HDR     "Scale Up"
#define ACT_OUT_K_HDR    "Scale Down"

// general headers
#define TARGET_HDR       _("Target")
#define TARGET_HINT      _("Enter hostname or IP address ...")
#define DATE_HDR         _("Date")

// options
#define OPT_CYCLES_HDR   _("Cycles")
#define OPT_IVAL_HDR     _("Interval")
#define OPT_DNS_HDR      _("DNS")
#define OPT_INFO_HDR     _("Hop Info")
#define OPT_STAT_HDR     _("Statistics")
#define OPT_TTL_HDR      _("TTL")
#define OPT_QOS_HDR      _("QoS")
#define OPT_PLOAD_HDR    _("Payload")
#define OPT_PSIZE_HDR    _("Size")
#define OPT_IPV_HDR      _("IP Version")
#define OPT_IPVA_HDR     _("Auto")
#define OPT_IPV4_HDR     _("IPv4")
#define OPT_IPV6_HDR     _("IPv6")

#define OPT_MN_DARK_HDR  _("Main dark theme")
#define OPT_GR_DARK_HDR  _("Graph dark theme")
#define OPT_PL_DARK_HDR  _("3D-plot dark theme")
#define OPT_GRAPH_HDR    _("Graph type")
#define OPT_GR_DOT_HDR   _("Dots")
#define OPT_GR_LINE_HDR  _("Lines")
#define OPT_GR_CURVE_HDR _("Splines")
#define OPT_LGND_HDR     _("Graph legend")
#define OPT_GRLG_HDR     _("Legend fields")
#define OPT_GREX_HDR     _("Extra graph elements")
#define OPT_PLOT_HDR     _("Plot elements")
#define OPT_PLEX_HDR     _("Plot parameters")
#define OPT_GRAD_HDR     _("Plot gradient")
#define OPT_GLOB_HDR     _("Global space")
#define OPT_LOCAL_HDR    _("Local space")
#define OPT_ROTOR_HDR    _("Plot rotation")
#define OPT_SCALE_HDR    _("Plot scale")
#define OPT_FOV_HDR      _("Field of view")
#define OPT_LOGMAX_HDR   _("LogTab rows")

// statistic elements and their tooltips
#define ELEM_HOST_HDR    _("Host")
#define ELEM_HOST_HEADER _("_Host")
#define ELEM_HOST_TIP    _("Hostname or IP-address")
#define ELEM_AS_HDR      _("AS")
#define ELEM_AS_HEADER   _("AS Number")
#define ELEM_AS_TIP      _("Autonomous System")
#define ELEM_CC_HDR      _("CC")
#define ELEM_CC_HEADER   _("Country Code")
#define ELEM_CC_TIP      _("Registrant Country Code")
#define ELEM_DESC_HDR    _("Desc")
#define ELEM_DESC_HEADER _("Description")
#define ELEM_DESC_TIP    _("Short description in free form")
#define ELEM_RT_HDR      _("Route")
#define ELEM_RT_HEADER   _("Network Route")
#define ELEM_RT_TIP      _("Network prefix")
#define ELEM_LOSS_HDR    _("Loss")
#define ELEM_LOSS_HEADER _("Lost")
#define ELEM_LOSS_TIP    _("Loss in percentage")
#define ELEM_SENT_HDR    _("Sent")
#define ELEM_SENT_HEADER _("_Sent")
#define ELEM_SENT_TIP    _("Number of pings sent")
#define ELEM_RECV_HDR    _("Recv")
#define ELEM_RECV_HEADER _("Received")
#define ELEM_RECV_TIP    _("Number of pongs received")
#define ELEM_LAST_HDR    _("Last")
#define ELEM_LAST_HEADER _("_Last")
#define ELEM_LAST_TIP    _("Last delay in milliseconds")
#define ELEM_BEST_HDR    _("Best")
#define ELEM_BEST_HEADER _("_Best")
#define ELEM_BEST_TIP    _("Best delay in milliseconds")
#define ELEM_WRST_HDR    _("Wrst")
#define ELEM_WRST_HEADER _("Worst")
#define ELEM_WRST_TIP    _("Worst delay in milliseconds")
#define ELEM_AVRG_HDR    _("Avrg")
#define ELEM_AVRG_HEADER _("Average")
#define ELEM_AVRG_TIP    _("Average delay in milliseconds")
#define ELEM_JTTR_HDR    _("Jttr")
#define ELEM_JTTR_HEADER _("Jitter")
#define ELEM_JTTR_TIP    _("Jitter (variation in delay)")

// 2D-graphs elements
#define GRLG_DASH_HDR    _("Color")
#define GRLG_DASH_HEADER _("Color dash")
#define GRLG_AVJT_HDR    _("RTT±Jitter")
#define GRLG_AVJT_HEADER _("Average RTT ± Jitter")
#define GRLG_CCAS_HDR    _("CC:ASN")
#define GRLG_CCAS_HEADER _("Country Code : AS Number")
#define GRLG_LGHN_HDR    _("Hopname")
#define GRLG_LGHN_HEADER _("_Hopname")
//
#define GREX_MEAN_HDR    _("Midline")
#define GREX_MEAN_HEADER _("Average line")
#define GREX_JRNG_HDR    _("Scopes")
#define GREX_JRNG_HEADER _("Jitter range")
#define GREX_AREA_HDR    _("JArea")
#define GREX_AREA_HEADER _("Jitter area")

// 3D-graphics elements
#define PLEL_BACK_HDR    _("Backside")
#define PLEL_AXIS_HDR    _("Axis")
#define PLEL_GRID_HDR    _("Grid")
#define PLEL_ROTR_HDR    _("Rotator")

// toggle tips
#define TOGGLE_ON_HDR  _("on")
#define TOGGLE_OFF_HDR _("off")

// spinner fields
#define SPN_TTL_DELIM  _("range")
#define SPN_RCOL_DELIM _("red")
#define SPN_GCOL_DELIM _("green")
#define SPN_BCOL_DELIM _("blue")
//
#define PLOT_GRAD_COLR _("Plot red gradient")
#define PLOT_GRAD_COLG _("Plot green gradient")
#define PLOT_GRAD_COLB _("Plot blue gradient")

// 3D-rotation parameters
#define ORIENT_HDR    _("Orientation")
#define ROT_ATTITUDE  _("Attitude")
#define ROT_ROLL      _("Roll")
#define ROT_PITCH     _("Pitch")
#define ROT_YAW       _("Yaw")
#define ROT_AXES      _("Rotation")
#define ROT_ANGLE_X   _("Axis X")
#define ROT_ANGLE_Y   _("Axis Y")
#define ROT_ANGLE_Z   _("Axis Z")
#define ROT_STEP      _("Angular step")

// CLI option help
#define CLI_FOPT_DESC _("Get options from file")
#define CLI_NOPT_DESC _("IP addresses in numeric form")
#define CLI_IOPT_DESC _("Interval between pings")
#define CLI_TTL_DESC  _("TTL range")
#define CLI_QOS_DESC  _("QoS byte")
#define CLI_SIZE_DESC _("Payload size")
#define CLI_POPT_DESC _("Payload in hex notation")
#define CLI_STAT_DESC _("Statistics fields")
#define CLI_TOPT_DESC _("Toggle dark/light themes")
#define CLI_SUMM_DESC _("Summary at exit")
#define CLI_SUMM_TCJ  _("Text, CSV, or JSON")
#define CLI_SUMM_TC   _("Text, CSV")
#define CLI_ROPT_DESC _("Autostart")
#define CLI_AOPT_DESC _("Initial active tab")
#define CLI_VERB_DESC _("More detailed messaging")
#define CLI_VERS_DESC _("Build features and runtime lib versions")
#define CLI_1D_DESC   _("Without graphics")
#define CLI_2D_DESC   _("Without 3D graphics")

// CLI option hints
#define CLI_FOPT_HINT _("<filepath>")
#define CLI_COPT_HINT _("<number>")
#define CLI_IOPT_HINT _("<seconds>")
#define CLI_TTL_HINT  _("[min][,max]")
#define CLI_BITS_HINT _("<bits>")
#define CLI_SIZE_HINT _("<in-bytes>")
#define CLI_POPT_HINT _("<upto-16-bytes>")
#define CLI_XOPT_HINT _("<tag:values>")
#define CLI_VOPT_HINT _("<6bit-level>")

// CLI misc messages
#define CLI_OPTS_HDR    _("Options")
#define CLI_INVAL_HDR   _("Invalid value")
#define CLI_NOVAL_HDR   _("No value")
#define CLI_CONF_HDR    _("CONFIG")
#define CLI_NOMATCH_HDR _("No match")
#define CLI_NOPARSE_HDR _("Cannot parse")
#define CLI_BADTAG_HDR  _("Wrong tag")
#define CLI_DUPTAG_HDR  _("Tag duplicate")
#define CLI_NOPAIR_HDR  _("Wrong pair")
#define CLI_BADTYPE_HDR _("Unknown type")
#define CLI_NDXDIFF_HDR _("Number of indexes are different")
#define CLI_ROPT_T_HDR  _("Text")
#define CLI_ROPT_C_HDR  _("CSV")
#define CLI_ROPT_J_HDR  _("JSON")
#define CLI_TOPT_HDR    _("Theme bits")
#define CLI_TOVAL_HDR   _("validate")
#define CLI_MUT_EXC_HDR _("Mutually exclusive options")
#define CLI_APPFEAT_HDR _("Build features")
#define CLI_LIBVER_HDR  _("Runtime lib versions")
#define CLI_SKIPARG_HDR _("Skipping argument")
#define CLI_BADOPT_HDR  _("Unknown option")
#define CLI_NOGOAL_HDR  _("No goal")
#define CLI_RECAP_HDR   _("Non-interactive mode with summary at exit")

#endif
