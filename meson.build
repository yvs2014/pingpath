
project('pingpath', 'c',
  default_options: [ # 'c_std=gnu99,c99',
    'warning_level=2' ],
  meson_version: '>= 0.53',
  license: 'GPL-2.0-or-later',
  version: '.'.join(['0', '3', find_program('git', required: false).found() ?
    run_command('git', 'rev-list', '--count', 'd3da356..HEAD', check: false).stdout().strip() : ''
  ]).strip('.'),
)

add_project_arguments('-D_GNU_SOURCE', language: 'c')

name = meson.project_name()
dnd  = get_option('DND')
json = get_option('JSON')
plot = get_option('PLOT')
nls  = get_option('NLS')
docdir  = get_option('docdir')
pingdir = get_option('pingdir')
summary({'DND': dnd, 'JSON': json, 'PLOT': plot, 'NLS': nls}, bool_yn: true)
config = configuration_data()

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

#
cc = meson.get_compiler('c')

# aux
fnhdr = {
  'secure_getenv': 'stdlib.h',
  'localtime_r':   'time.h',
  'uselocale':     'locale.h',
}
foreach fn, hdr: fnhdr
  if cc.has_function(fn, prefix: '#define _GNU_SOURCE\n#include <' + hdr + '>\n')
    config.set('HAVE_' + fn.underscorify().to_upper(), 1)
  endif
endforeach

# dependencies
deps  = [cc.find_library('m', required: true)]
deps +=  dependency('gtk4',   required: true)

# optional: DND (default ON)
if dnd
  config.set('WITH_DND', 1)
endif

# optional: JSON (default ON)
if json
  config.set('WITH_JSON', 1)
  deps += dependency('json-glib-1.0', required: true)
endif

# optional: PLOT (default ON)
if plot
  config.set('WITH_PLOT', 1)
  srcs += tb/ 'plot.c'
  srcs += tb/ 'plot_aux.c'
  srcs += tb/ 'plot_pango.c'
  deps += dependency('gl',    required: true)
  deps += dependency('cglm',  required: true)
  deps += dependency('epoxy', required: true)
endif

# optional: NLS (default ON)
if nls
  config.set('WITH_NLS', 1)
  subdir('po')
endif

# optional: WITH_SNAPHINT (default OFF)
if get_option('WITH_SNAPHINT')
  config.set('WITH_SNAPHINT', 1)
endif

if pingdir != ''
  config.set_quoted('PINGDIR', pingdir)
endif

prefix    = get_option('prefix')
datadir   = get_option('datadir')
localedir = get_option('localedir')
config.set_quoted('DATADIR',   prefix / datadir)
config.set_quoted('LOCALEDIR', prefix / localedir)

config_h = 'config.h'
configure_file(output: config_h, configuration: config)
add_project_arguments('-include', config_h, language: 'c')

pp = executable(name, srcs, dependencies: deps, install: true)

asst = 'assets'

desk = name + '.desktop'
install_data(asst / desk, rename: 'net.tools.' + desk, install_dir: datadir / 'applications')

install_man(name + '.1')
install_data(asst / 'icons' / name + '.svg', install_dir: datadir / 'icons/hicolor/scalable/apps')
conf = name + '.conf'
datadoc = docdir != '' ? docdir : datadir / 'doc'
install_data(conf + '.sample', rename: conf, install_dir: datadoc / name / 'examples')

text_mode  =  ['--cycles', '1', '--recap', 't', 'localhost']
test_args  = [['--version'], text_mode]
if json
test_args += [['--cycles', '1', '--recap', 'j', 'localhost']]
test_args += [['--cycles', '1', '--recap', 'J', 'localhost']]
endif
test_args += [['--cycles', '1', '--recap', 'c', 'localhost']]
test_args += [['--numeric']          + text_mode]
test_args += [['--interval', '2']    + text_mode]
test_args += [['--payload', 'ff']    + text_mode]
test_args += [['--ttl', '2']         + text_mode]
test_args += [['--info', 'hacdr']    + text_mode]
test_args += [['--stat', 'lsrmbwaj'] + text_mode]
test_args += [['-4']                 + text_mode]
test_args += [['-6'] + ['--cycles', '1', '--recap', 't', '::1']]
test_args += [['--file', meson.current_source_dir() / name + '.conf.sample'] + text_mode]
foreach args: test_args
  cmd = ' '.join([name] + args)
  if meson.version().version_compare('>=0.58')
    cmd = cmd.replace(':', '_')
  endif
  test(cmd, pp, args: args)
endforeach

