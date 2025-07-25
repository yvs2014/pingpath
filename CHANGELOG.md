## 0.3.71
- bump version to 0.3.71
- small corrections in APKBUILD and version_changelog

## 0.3.70
- obs16.0 typo in workaround

## 0.3.69
- obs: opensuse16.0 'if' workaround
- take into account shell-lint hints

## 0.3.68
- justfile: add packaging recipes

## 0.3.66
- add simple 'justfile'

## 0.3.65
- fix asan(tabs/plot_aux.c): out of 'series' scope
- clang-tidy: unused includes
- cmake: simpler LOCALEDIR set

## 0.3.64
- cmake-install: workaround for lack of textdomain
- misc: pkg optional deps

## 0.3.63
- optional snap connection hint (enabled in snap build only)

## 0.3.62
- NLS: Portuguese .po

## 0.3.61
- NLS: Italian .po
- NLS: update a bit prev .po

## 0.3.60
- fix plot NULL-ended hash title
- 2d-graph: better time-title positioning

## 0.3.59
- NLS: Spanish .po

## 0.3.58
- NLS: set subprocesses' LC_ correctly

## 0.3.57
- reverse g_timeout_add_seconds*() to compatibility with older glib versions
- fix some nls packaging issues

## 0.3.56
- NLS wip: gl-area: nls axis titles
- fix typos in log_add_line()
- less debug text in warn lines

## 0.3.55
- NLS wip: level2 .c files

## 0.3.54
- NLS wip: start adding NLS to level2 .c files

## 0.3.53
- NLS wip: debug messages in level1 files

## 0.3.52
- notifier: take into account overlapping timeouts
- logging: multiline messages
- verbose bits: wrap in structure for readability

## 0.3.51
- a bit more readable logging
- NLS wip: level1 .c files

## 0.3.50
- NLS wip: almost all of pinger.c messages
- a bit clearer logic of extra info
- json-print: rm redundant "timing" prop

## 0.3.49
- NLS wip: all cli.c messages
- common TTL range in structure

## 0.3.48
- NLS wip: add remained menu text
- NLS wip: tab names-n-tips

## 0.3.46
- work-in-progress: prepare NLS infra
- wip: NLS menu

## 0.3.45
- define _GNU_SOURCE via project arguments

## 0.3.44
- add config.h via cc option

## 0.3.43
- increase warning-level to 2 (+Wextra) and fix warnings

## 0.3.42
- notifier position: reassure again gtk_widget_measure() call

## 0.3.41
- notifier on the right side: a bit more universal

## 0.3.40
- tuneup prev workaround

## 0.3.39
- workaround for overlay positioning on xfce4 session

## 0.3.38
- add build option to set ping dir (Guix, GnomeOS, etc)

## 0.3.37
- add workaround for GSK4.16
- add Alpine apk packaging template

## 0.3.36
- simplify by building full 3d version only
- add basic config.h
- uselocale() if available

## 0.3.35
- use secure_getenv() and localtime_r() if available

## 0.3.34
- reassure opt-ent-kb init order

## 0.3.33
- tidy up and add a bit of readability

## 0.3.32
- add simple test unit

## 0.3.31
- logtab: do not dispose lines manually
- retoss headers in clang-tidy friendlier way

## 0.3.30
- friendlier to clang SA
- tidy up a bit

## 0.3.29
- follow most static code analysis hints

## 0.3.28
- gtk4.16: follow default gsk_renderer
- combine graceful exit workarounds

## 0.3.27
- tuning up meson builds

## 0.3.26
- migrate from cmake to meson

## 0.3.25
- plot: release gl_area before exit (4.16 workaround)

## 0.3.24
- add cmake PLOT option
- simplify CMakeLists.txt a bit

## 0.3.23
- tuning up cmake builds

## 0.3.22
- OBS: docdir fix

## 0.3.21
- migrate from make to cmake

## 0.3.20
- ipv6: add ':' to allowed characters

## 0.3.19
- snap related: add git versioning and reduce dist size

## 0.3.18
- free selection at exit

## 0.3.17
- packaging stuff

## 0.3.16
- packaging: snap-core24, obs-rpm

## 0.3.15
- plot: additional keyboard shortcuts
- graph: force legend redraw with theme changing

## 0.3.14
- a bit better option warnings
- git sparse img/ dir

## 0.3.13
- add plot scale with field-of-view
- add 1x2 1x3 collages from screenshots

## 0.3.12
- a bit of enum renaming and little optimization

## 0.3.11
- switch from gimbal rotation to quaternion
- add space rotation switch (set default to global)

## 0.3.10
- switch the default build to 3D version

## 0.3.9
- options for 1D/2D/3D prog versions

## 0.3.8
- safer css customization
- try to use 'gl' renderer with gtk-4.14

## 0.3.7
- add rotator
- update manual and .conf example

## 0.3.6
- add rotation
- misc optimization

## 0.3.5
- plot: add customization options to config and CLI too
- update manual and help

## 0.3.4
- plot: add customization options

## 0.3.3
- plot: split grid/axis and optimize its drawing

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
