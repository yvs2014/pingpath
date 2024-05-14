## 0.3.2
- add GLES support for wider compat (default for GTK>=4.13.4)
- pair GL bind/unbind functions, etc.

## 0.3.1
- bump version to 0.3

## 0.3.0
- support OpenGL 3D plotting: version .0

## 0.2.9
- [almost finished] add 3D-plots

## 0.2.8
- [draft] plot 3D data

## 0.2.7
- [in progress] base of 3D-plot with OpenGL support

## 0.2.6
- DnD reorder: suppress not needed notifications
- Clipboard: precalculate field lengths due to SizeGroups

## 0.2.5
- DnD reordering for Info and Stat fields
- SizeGroups instead of setting width in chars

## 0.2.4
- CLI: make -S and -I reorderable
- CLI: changed -G and -L values to reordable chars
- CLI: -T move legend on/off here
- manual corresponded to CLI changes

## 0.2.3
- retoss some view elements on a way to make them reorderable for DnD

## 0.2.2
- make graph legend draggable
- aurbuild minor correction

## 0.2.1
- bump version to 0.2

## 0.1.86
- add short annotation
- small correction in pkg build

## 0.1.85
- add .rpm packaging
- install conf sample too

## 0.1.84
- add -f option (set options from .ini config file)

## 0.1.83
- bring back '-g 0' CLI call
- aur PKGBUILD versioning

## 0.1.82
- test .aur packaging

## 0.1.81
- set LANG=C to predictably parse ping's output (spotted i18d ping on ArchLinux)

## 0.1.80
- optimize for speed a bit
- fix dup memleaks of dns references

## 0.1.79
- add CLI dark/light themes option
- update screenshots

## 0.1.78
- add dark/light theme switch (main and graph)

## 0.1.77
- fix some mem leaks from analyzers reports
- unify cleanup at exit
- add '*' mark to display whois data presented in multiple sources

## 0.1.76
- greedy split of whois description element
- enclose csv elements with delim inside

## 0.1.75
- put recap in common g_app_run() context
- rm unused i18n preinit
- update screenshot with opened graph menu

## 0.1.74
- print runtime versions
- untie recap mode from gtk display

## 0.1.73
- add summary/recap option (text, csv, and json)
- optimize a bit fn calls

## 0.1.72
- add GLR options
- -G Extra graph elements
- -L Legend and its fields
- -R run (autostart)

## 0.1.71
- show/hide certain graphs by clicking

## 0.1.70
- a bit of polish up: github action up-to-date, modifier mask for keyevents, a couple more screenshots

## 0.1.69
- make helpdialog scrollable
- tune up a bit init() of pingtab's lists

## 0.1.68
- fix some sanitizer warnings

## 0.1.67
- graph: jitter area background

## 0.1.66
- graph: add jitter range marks

## 0.1.65
- graph: switch to SEQuential data requests to (un)display drops
- graph: add separate cache to legend elems with NL chars

## 0.1.64
- switch timers to more efficient g_timeout_add_seconds()
- set expiration pings' runtime to deal with https://github.com/iputils/iputils/issues/480 issue

## 0.1.63
- keep graph data in pause
- graph time marks depending on interval
- small other fixes

## 0.1.62
- sysicons as a set with backup names

## 0.1.61
- fix (more accurate) calculation of averages
- add average lines to graphs

## 0.1.60
- revert set sensitive(false) for legend label row

## 0.1.59
- add selector of legend elements

## 0.1.58
- update screenshots to display graphs

## 0.1.57
- add more internal checks and warns

## 0.1.56
- add graph legend (main graph functionality is done)

## 0.1.55
- [in progress] add graph option menu

## 0.1.54
- [in progress] add splines

## 0.1.53
- [in progress] add line graphs
- update manual

## 0.1.52
- [in progress] draw graph dots and add graph states

## 0.1.51
- [in progress] add graph template and base layers

## 0.1.50
- tune up snap building

## 0.1.49
- testing snap and flatpak packaging

## 0.1.48
- add short manual page

## 0.1.47
- cli handlers: step2
- use only 'gboolean' as boolean to not mess up with sizing

## 0.1.46
- cli handlers: step1

## 0.1.45
- add a couple check-points at external exit

## 0.1.44
- readme and screenshots

## 0.1.43
- add .deb builder

## 0.1.42
- tune up clipboard popovers

## 0.1.41
- compat with earlier gtk4 versions
- fix refs following gcc analyzer warnings

## 0.1.40
- add tooltips on hover

## 0.1.39
- add popup notifier (actions, changes, etc.)

## 0.1.38
- add clipboard menu to ping tab

## 0.1.37
- add clipboard menu to log tab

## 0.1.36
- close pipe fds after ping termination and suppress clang-tidy complains on g_clear_object()

## 0.1.35
- dup log to log-tab too

## 0.1.34
- add notebook and log-tab template

## 0.1.33
- add help dialog
- add 'exit' keyboard shortcut

## 0.1.32
- rewrite whois resolver using g_lists in place of static arrays
- tune up a bit dns lookups
- tune up UI dns/whois enablers

## 0.1.31
- [intermediate] add dns hostname lookup

## 0.1.30
- replace some temporary static data

## 0.1.29
- add whois resolver

## 0.1.28
- cache hop info
- add whois elem template
- misc correction of hostname widths

## 0.1.27
- add payload pad setter/validator
- fix typos in argv list

## 0.1.26
- add int option setters/validators

## 0.1.25
- ping with ttl settings
- a bit clear unreach messaging

## 0.1.24
- add TTL and IPvN handlers

## 0.1.23
- add selected stat element handlers

## 0.1.22
- add expanded sublists to popover menu

## 0.1.21
- fix stat clearing at reset action
- split menu on action,option

## 0.1.20
- add menu shortcuts

## 0.1.19
- move options to popover listbox

## 0.1.18
- add menu states
- add g_if_fail asserts at init

## 0.1.17
- various nodata/noresponse cases

## 0.1.16
- add all stat fields in view
- revamp stats a bit

## 0.1.15
- check out settings on dark theme preference

## 0.1.14
- add table template to viewarea

## 0.1.13
- add reset handler

## 0.1.12
- add target name validation

## 0.1.11
- set menu button icons
- add todo-template for option menu

## 0.1.10
- move appbar into window titlebar

## 0.1.9
- add target input field

## 0.1.8
- test basic styling

## 0.1.7
- add datetime displaying

## 0.1.6
- unbind statview updates and parsing (update on timer into separate thread)
- stat-addrnames: simple array instead of slists

## 0.1.5
- stat collecting
- parsing mem manage
- full range of pings

## 0.1.4
- add dataline parser

## 0.1.3
- display stderr messages

## 0.1.2
- basic ping call

## 0.1.1
- Initial commit
