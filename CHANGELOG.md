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
