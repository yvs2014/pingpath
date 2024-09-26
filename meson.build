
project('pingpath', 'c',
  default_options: [
    'warning_level=1', # -Wall
#    'c_std=c23',      # compat test
  ],
  meson_version: '>= 0.53',
  license: 'GPL-2.0-or-later',
  version: '.'.join(['0', '3', find_program('git', required: false).found() ?
    run_command('git', 'rev-list', '--count', 'd3da356..HEAD', check: false).stdout().strip() : ''
  ]).strip('.'),
)

name = meson.project_name()
dnd  = get_option('DND')
json = get_option('JSON')
plot = get_option('PLOT')
summary({'DND': dnd, 'JSON': json, 'PLOT': plot}, bool_yn: true)

# sources
srcs  = [name + '.c']
srcs += 'common.c'
srcs += 'pinger.c'
srcs += 'parser.c'
srcs += 'stat.c'
srcs += 'series.c'
srcs += 'dns.c'
srcs += 'whois.c'
srcs += 'cli.c'
ui = 'ui'
srcs += ui/ 'style.c'
srcs += ui/ 'appbar.c'
srcs += ui/ 'action.c'
srcs += ui/ 'option.c'
srcs += ui/ 'clipboard.c'
srcs += ui/ 'notifier.c'
tb = 'tabs'
srcs += tb/ 'aux.c'
srcs += tb/ 'ping.c'
srcs += tb/ 'graph.c'
srcs += tb/ 'log.c'

# dependencies
cc    = meson.get_compiler('c')
deps  = [cc.find_library('m', required: true)]
deps +=  dependency('gtk4',   required: true)
cpps  = []

# optional: DND (default ON)
if dnd
  cpps += '-DWITH_DND=1'
endif

# optional: JSON (default ON)
if json
  cpps += '-DWITH_JSON=1'
  deps += dependency('json-glib-1.0', required: true)
endif

datadir = get_option('datadir')
deskdir = datadir / 'applications'
rdns = 'net.tools.'
asst = 'assets'
dotdsk = '.desktop'
name2 = name + '2'
desk2 = name2 + dotdsk
executable(name2, srcs, dependencies: deps, c_args: cpps, install: true)
install_data(asst / desk2, rename: rdns + desk2, install_dir: deskdir)

# optional: PLOT (default ON)
if plot
  srcs += tb/ 'plot.c'
  srcs += tb/ 'plot_aux.c'
  srcs += tb/ 'plot_pango.c'
  cpps += '-DWITH_PLOT=1'
  deps += dependency('gl',    required: true)
  deps += dependency('cglm',  required: true)
  deps += dependency('epoxy', required: true)
  executable(name, srcs, dependencies: deps, c_args: cpps, install: true)
  desk = name + dotdsk
  install_data(asst / desk, rename: rdns + desk, install_dir: deskdir)
endif

install_man(name + '.1')
install_data(asst / 'icons' / name + '.svg', install_dir: datadir / 'icons/hicolor/scalable/apps')
conf = name + '.conf'
docdir = get_option('docdir') != '' ? get_option('docdir') : datadir / 'doc'
install_data(conf + '.sample', rename: conf, install_dir: docdir / name / 'examples')

