## Cherokee: cherokee/Makefile.am  -*- makefile -*-

# SUBDIRS = zlib

INCLUDES = \
$(TLS_CFLAGS) \
$(PTHREAD_CFLAGS) \
$(GNOMEVFS_CFLAGS) \
-I$(top_srcdir) \
-DCHEROKEE_COMPILATION \
-DCHEROKEE_PLUGINDIR="\"$(plugindir)\"" \
-DCHEROKEE_DEPSDIR=\""$(cherokeedepsdir)"\" \
-DCHEROKEE_DATADIR=\""$(cherokeedatadir)"\" \
-DCHEROKEE_CONFDIR=\""$(sysconfdir)/cherokee"\"

#
# Scripts
#
bin_SCRIPTS = cherokee-panic

#
# Data
#
cherokeedepsdir = "$(datadir)/cherokee/deps"
cherokeedeps_DATA = \
common.deps \
nn.deps \
combined.deps \
error_redir.deps \
phpcgi.deps

cherokeedatadir = "$(datadir)/cherokee"
cherokeedata_DATA = mime_types.txt

#
# Maybe add the getopt code from GNU C Library
#
if !HAVE_GETOPT_LONG
gnu_getopt =  \
    getopt/getopt.h  \
    getopt/getopt.c  \
    getopt/getopt1.c \
    getopt/gettext.h
endif

#
# Zlib files
#
zlib_src =     \
zlib/deflate.c \
zlib/deftree.c \
zlib/defutil.h \
zlib/zconf.h   \
zlib/zlib.h    \
zlib/zutil.h

dict_src =          \
dict/dict_generic.h \
dict/dict_generic.c \
dict/sp_tree.h      \
dict/sp_tree.c      \
dict/dict_private.h

avl_src = \
avl/avl.h \
avl/avl.c

#
# Handler server_info
#
handler_server_info = \
handler_server_info.c \
handler_server_info.h

libplugin_server_info_la_LDFLAGS = -module -avoid-version -no-undefined 
libplugin_server_info_la_SOURCES = $(handler_server_info)

if STATIC_HANDLER_SERVER_INFO
static_handler_server_info_src = $(handler_server_info)
else
dynamic_handler_server_info_lib = libplugin_server_info.la
endif


#
# Handler file
#
handler_file = \
handler_file.c \
handler_file.h

libplugin_file_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_file_la_SOURCES = $(handler_file)

if STATIC_HANDLER_FILE
static_handler_file_src = $(handler_file)
else
dynamic_handler_file_lib = libplugin_file.la
endif


#
# Handler dirlist
#
handler_dirlist = \
handler_dirlist.c \
handler_dirlist.h

libplugin_dirlist_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_dirlist_la_SOURCES = $(handler_dirlist)

if STATIC_HANDLER_DIRLIST
static_handler_dirlist_src = $(handler_dirlist)
else
dynamic_handler_dirlist_lib = libplugin_dirlist.la
endif


#
# Handler redir
#
if HAVE_PCRE
pcrelib_add=-lpcre
endif

handler_redir = \
handler_redir.c \
handler_redir.h

libplugin_redir_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_redir_la_SOURCES = $(handler_redir)
libplugin_redir_la_LIBADD  = $(pcrelib_add)

if STATIC_HANDLER_REDIR
static_handler_redir_src = $(handler_redir)
static_handler_redir_lib = $(pcrelib_add)
else
dynamic_handler_redir_lib = libplugin_redir.la
endif


#
# Handler error_redir
#
handler_error_redir = \
handler_error_redir.c \
handler_error_redir.h

libplugin_error_redir_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_error_redir_la_SOURCES = $(handler_error_redir)

if STATIC_HANDLER_ERROR_REDIR
static_handler_error_redir_src = $(handler_error_redir)
else
dynamic_handler_error_redir_lib = libplugin_error_redir.la
endif


#
# Handler cgi
#
handler_cgi = \
handler_cgi.c \
handler_cgi.h

libplugin_cgi_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_cgi_la_SOURCES = $(handler_cgi)

if STATIC_HANDLER_CGI
static_handler_cgi_src = $(handler_cgi)
else
dynamic_handler_cgi_lib = libplugin_cgi.la
endif


#
# Handler Mono
#
#handler_mono = \
#handler_mono.c \
#handler_mono.h
#
#libplugin_mono_la_LDFLAGS = -module -avoid-version -no-undefined  
#libplugin_mono_la_SOURCES = $(handler_mono)
#
#if STATIC_HANDLER_MONO
#static_handler_mono_src = $(handler_mono)
#else
#dynamic_handler_mono_lib = libplugin_mono.la
#endif


#
# Handler phpcgi
#
handler_phpcgi = \
handler_phpcgi.c \
handler_phpcgi.h

libplugin_phpcgi_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_phpcgi_la_SOURCES = $(handler_phpcgi)

if STATIC_HANDLER_PHPCGI
static_handler_phpcgi_src = $(handler_phpcgi)
else
dynamic_handler_phpcgi_lib = libplugin_phpcgi.la
endif


#
# Handler remote_control
#
handler_remote_control = \
handler_remote_control.c \
handler_remote_control.h

libplugin_remote_control_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_remote_control_la_SOURCES = $(handler_remote_control)

if STATIC_HANDLER_REMOTE_CONTROL
static_handler_remote_control_src = $(handler_remote_control)
else
dynamic_handler_remote_control_lib = libplugin_remote_control.la
endif


#
# Handler common
#
handler_common = \
handler_common.c \
handler_common.h

libplugin_common_la_LDFLAGS = -module -avoid-version -no-undefined 
libplugin_common_la_SOURCES = $(handler_common)

if STATIC_HANDLER_COMMON
static_handler_common_src = $(handler_common)
else
dynamic_handler_common_lib = libplugin_common.la
endif





#
# Handler nn
#
handler_nn = \
levenshtein_distance.c \
levenshtein_distance.h \
handler_nn.c \
handler_nn.h

libplugin_nn_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_nn_la_SOURCES = $(handler_nn)

if STATIC_HANDLER_NN
static_handler_nn_src = $(handler_nn)
else
dynamic_handler_nn_lib = libplugin_nn.la
endif


#
# Handler GNOME VFS
#
libplugin_gnomevfs_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_gnomevfs_la_LIBADD  = $(GNOMEVFS_LIBS) 
libplugin_gnomevfs_la_SOURCES = \
handler_gnomevfs.c \
handler_gnomevfs.h


#
# Encoder GZip
#
encoder_gzip = \
encoder_gzip.h \
encoder_gzip.c

libplugin_gzip_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_gzip_la_SOURCES = $(encoder_gzip)

if STATIC_ENCODER_GZIP
static_encoder_gzip_src = $(encoder_gzip)
else
dynamic_encoder_gzip_lib = libplugin_gzip.la
endif


#
# Logger NCSA
#
logger_ncsa = \
logger_ncsa.h \
logger_ncsa.c

libplugin_ncsa_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_ncsa_la_SOURCES = $(logger_ncsa)

if STATIC_LOGGER_NCSA
static_logger_ncsa_src = $(logger_ncsa)
else
dynamic_logger_ncsa_lib = libplugin_ncsa.la
endif


#
# Logger Combined
#
logger_combined = \
logger_combined.h \
logger_combined.c

libplugin_combined_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_combined_la_SOURCES = $(logger_combined)

if STATIC_LOGGER_COMBINED
static_logger_combined_src = $(logger_combined)
else
dynamic_logger_combined_lib = libplugin_combined.la
endif


#
# Logger W3C
#
logger_w3c = \
logger_w3c.h \
logger_w3c.c

libplugin_w3c_la_LDFLAGS = -module -avoid-version -no-undefined  
libplugin_w3c_la_SOURCES = $(logger_w3c)

if STATIC_LOGGER_W3C
static_logger_w3c_src = $(logger_w3c)
else
dynamic_logger_w3c_lib = libplugin_w3c.la
endif


#
# Validator PAM
#
if HAVE_PAM
validator_pam = \
validator_pam.c \
validator_pam.h

libplugin_pam_la_LDFLAGS = -module -avoid-version -no-undefined 
libplugin_pam_la_SOURCES = $(validator_pam)
libplugin_pam_la_LIBADD  = -lpam

if STATIC_VALIDATOR_PAM
static_validator_pam_src = $(validator_pam)
static_validator_pam_lib = -lpam
else
dynamic_validator_pam_lib = libplugin_pam.la
endif
endif

#
# Validator htpasswd
#
if HAVE_CRYPT
validator_htpasswd = \
validator_htpasswd.c \
validator_htpasswd.h

libplugin_htpasswd_la_LDFLAGS = -module -avoid-version -no-undefined 
libplugin_htpasswd_la_SOURCES = $(validator_htpasswd)
libplugin_htpasswd_la_LIBADD  = $(CRYPT_LIBS)
libplugin_htpasswd_la_CFLAGS  = $(CRYPT_CFLAGS)

if STATIC_VALIDATOR_HTPASSWD
static_validator_htpasswd_src = $(validator_htpasswd)
static_validator_htpasswd_lib = $(CRYPT_LIBS)
else
dynamic_validator_htpasswd_lib = libplugin_htpasswd.la
endif
endif

#
# Validator plain text
#
validator_plain = \
validator_plain.c \
validator_plain.h

libplugin_plain_la_LDFLAGS = -module -avoid-version -no-undefined 
libplugin_plain_la_SOURCES = $(validator_plain)

if STATIC_VALIDATOR_PLAIN
static_validator_plain_src = $(validator_plain)
else
dynamic_validator_plain_lib = libplugin_plain.la
endif



#
# Yacc and Lex paramters
# -d Generate .h file
# -l Don't use #line
# -t Debug
AM_YFLAGS=-d -l -t
AM_LFLAGS=-i


lib_LTLIBRARIES = \
libcherokee-base.la \
libcherokee-client.la \
libcherokee-server.la 

# Common LDFLAGS for libraries
common_ldflags = \
-no-undefined \
-export-symbols-regex "^[^_].*"

# the macro puts -pthread into cflags, but we want to link
# with this flag too, so stick it into the ldflags if it's there...
# extra cflags at link time can't hurt
libcherokee_base_la_LDFLAGS = $(common_ldflags)
libcherokee_base_la_LIBADD = \
$(TLS_LIBS) \
$(PTHREAD_LIBS) \
$(PTHREAD_CFLAGS)

libcherokee_client_la_LDFLAGS = $(common_ldflags)
libcherokee_client_la_LIBADD = \
$(TLS_LIBS) \
$(PTHREAD_LIBS) \
$(PTHREAD_CFLAGS) \
libcherokee-base.la

libcherokee_server_la_LDFLAGS = $(common_ldflags)
libcherokee_server_la_LIBADD = \
$(TLS_LIBS) \
$(PTHREAD_LIBS) \
$(PTHREAD_CFLAGS) \
$(static_validator_pam_lib) \
$(static_handler_redir_lib) \
$(static_validator_htpasswd_lib) \
libcherokee-base.la


if COMPILE_EPOLL
poll_epoll_src = fdpoll-epoll.c
endif

if COMPILE_KQUEUE
poll_kqueue_src = fdpoll-kqueue.c
endif

if COMPILE_PORT
poll_port_src = fdpoll-port.c
endif

if COMPILE_POLL
poll_poll_src = fdpoll-poll.c
endif

if COMPILE_SELECT
poll_select_src = fdpoll-select.c
endif


libcherokee_base_la_SOURCES = \
$(avl_src) \
$(zlib_src) \
$(dict_src) \
$(gnu_getopt) \
$(poll_poll_src) \
$(poll_epoll_src) \
$(poll_kqueue_src) \
$(poll_port_src) \
$(poll_select_src) \
\
cherokee.h \
http.h \
http.c \
list.h \
list_merge_sort.h \
list_merge_sort.c \
list_ext.h \
list_ext.c \
ncpus.h \
ncpus.c \
crc32.h \
crc32.c \
macros.h \
common.h \
common-internal.h \
util.h \
util.c \
buffer.h \
buffer.c \
table.h \
table-protected.h \
table.c \
dict.h \
dict.c \
buffer_escape.h \
buffer_escape.c \
socket.h \
socket.c \
fdpoll.h \
fdpoll-protected.h \
fdpoll.c \
mime.h \
mime-protected.h \
mime.c \
matching_list.h \
matching_list.c \
mmap2.h \
mmap2.c \
md5.h \
md5.c \
sha1.h \
sha1.c \
match.h \
match.c \
tdate_parse.h \
tdate_parse.c \
header.h \
header.c \
session_cache.h \
session_cache.c \
access.h \
access.c \
\
mime_grammar.y \
mime_scanner.l


libcherokee_client_la_SOURCES = \
url.h \
url.c \
request.h \
request.c \
downloader.h \
downloader-protected.h \
downloader.c 

libcherokee_server_la_SOURCES = \
read_config_grammar.y \
read_config_scanner.l \
\
icons_grammar.y \
icons_scanner.l \
\
$(static_handler_file_src) \
$(static_handler_dirlist_src) \
$(static_handler_redir_src) \
$(static_handler_error_redir_src) \
$(static_handler_cgi_src) \
$(static_handler_phpcgi_src) \
$(static_handler_common_src) \
$(static_handler_nn_src) \
$(static_handler_server_info_src) \
$(static_handler_remote_control_src) \
\
$(static_logger_w3c_src) \
$(static_logger_ncsa_src) \
$(static_logger_combined_src) \
\
$(static_encoder_gzip_src) \
\
$(static_validator_pam_src) \
$(static_validator_plain_src) \
$(static_validator_htpasswd_src) \
\
connection.h \
connection.c \
handler.h \
handler.c \
handler_table.h \
handler_table.c \
handler_table_entry.h \
handler_table_entry.c \
server-protected.h \
server.h \
server.c \
virtual_server.h \
virtual_server.c \
encoder.h \
encoder.c \
encoder_table.h \
encoder_table.c \
logger.h \
logger.c \
logger_table.h \
logger_table.c \
thread.h \
thread.c \
module.h \
module.c \
validator.h \
validator.c \
module_loader.h \
module_loader-protected.h \
module_loader.c \
icons.h \
icons.c \
handler_error.c \
handler_error.h

# Temporaly removed
#$(static_handler_mono_src) 


#
# Header files
#
libcherokeeincludedir = $(includedir)/cherokee
libcherokeeinclude_HEADERS = \
cherokee.h \
macros.h \
common.h \
util.h \
buffer.h\
fdpoll.h \
table.h \
http.h \
mime.h \
list.h \
list_merge_sort.h \
url.h \
\
server.h \
module.h \
logger.h \
handler.h \
encoder.h \
module_loader.h \
\
downloader.h



# automake isn't too clever about "non-standard" use of lex and yacc
# Icons:
icons_scanner.c : icons_scanner.l
	$(LEX) -Pyy_icons_ -i -o$@ $<

icons_grammar.h : icons_grammar.c
icons_grammar.c : icons_grammar.y
	$(YACC) -p yy_icons_ -v -d $< -o $@

# Mime:
mime_scanner.c : mime_scanner.l
	$(LEX) -Pyy_mime_ -i -o$@ $<

mime_grammar.h : mime_grammar.c
mime_grammar.c : mime_grammar.y
	$(YACC) -p yy_mime_ -v -d $< -o $@


#
# Cherokee plugins
#
if HAVE_GNOMEVFS
gnomevfs_lib = libplugin_gnomevfs.la
endif


plugindir = "$(libdir)/cherokee"
plugin_LTLIBRARIES = \
$(gnomevfs_lib) \
$(dynamic_handler_cgi_lib) \
$(dynamic_handler_phpcgi_lib) \
$(dynamic_handler_server_info_lib) \
$(dynamic_handler_file_lib) \
$(dynamic_handler_dirlist_lib) \
$(dynamic_handler_redir_lib) \
$(dynamic_handler_error_redir_lib) \
$(dynamic_handler_remote_control_lib) \
$(dynamic_handler_common_lib) \
$(dynamic_handler_nn_lib) \
$(dynamic_encoder_gzip_lib) \
$(dynamic_logger_ncsa_lib) \
$(dynamic_logger_combined_lib) \
$(dynamic_logger_w3c_lib) \
$(dynamic_validator_pam_lib) \
$(dynamic_validator_plain_lib) \
$(dynamic_validator_htpasswd_lib)

# Temporaly removed
#$(dynamic_handler_mono_lib) 

#
# Main binary: Cherokee server
#
cherokee_SOURCES = main.c
cherokee_LDADD   = \
$(PTHREAD_LIBS) \
$(PTHREAD_CFLAGS) \
libcherokee-base.la \
libcherokee-server.la

sbin_PROGRAMS = cherokee



#
# Test programs
#
#noinst_PROGRAMS = cherokee_ip

##cherokee_table_test_SOURCES = cherokee_table_test.c
##cherokee_table_test_LDADD = libcherokee.la

#noinst_PROGRAMS = cherokee_modules cherokee_base64 cherokee_headers cherokee_fastcgi

#cherokee_modules_SOURCES = cherokee_modules.c
#cherokee_modules_LDADD = libcherokee.la

#cherokee_base64_SOURCES = cherokee_base64.c
#cherokee_base64_LDADD = libcherokee.la

#cherokee_headers_SOURCES = cherokee_headers.c
#cherokee_headers_LDADD = libcherokee.la

#cherokee_fastcgi_SOURCES = cherokee_fastcgi.c
#cherokee_fastcgi_LDADD = libcherokee.la 
#cherokee_fastcgi_LDFLAGS = -lfcgi

#cherokee_ip_SOURCES = cherokee_ip.c
#cherokee_ip_LDADD = libcherokee.la


CLEANFILES = \
read_config_grammar.h \
read_config_grammar.c \
read_config_scanner.c \
icons_grammar.h \
icons_grammar.c \
icons_scanner.c \
mime_grammar.h \
mime_grammar.c \
mime_scanner.c

WINDOWS_PORT_FILES = \
Makefile.mingw \
unix4win32_errno.h \
unix4win32.h \
unix4win32.c

EXTRA_DIST = \
$(bin_SCRIPTS) \
$(cherokeedeps_DATA) \
$(cherokeedata_DATA) \
$(WINDOWS_PORT_FILES) \
read_config_grammar.h \
loader.autoconf.h \
loader.autoconf.inc \
logo.inc 
