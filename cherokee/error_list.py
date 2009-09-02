# Macros
#
INTERNAL_ISSUE = """The server found an internal problem.
Bla, bla, bla..
"""

CODING_BUG = """Looks like a bug in the plug-in that you're trying to
use. Please report it to its author or maintainer so he can fix it
up."""

SYSTEM_ISSUE = """The issue seems to be related to your system."""


# cherokee/ncpus.c
#
e('NCPUS_PSTAT',
  title = "pstat_getdynamic() failed: '${errno}'",
  desc  = SYSTEM_ISSUE)

e('NCPUS_HW_NCPU',
  title = "sysctl(CTL_HW:HW_NCPU) failed: '${errno}'",
  desc  = SYSTEM_ISSUE)

e('NCPUS_SYSCONF',
  title = "sysconf(_SC_NPROCESSORS_ONLN) failed: '${errno}'",
  desc  = SYSTEM_ISSUE)


# cherokee/init.c
#
e('INIT_CPU_NUMBER',
  title = "Couldn't figure the CPU/core number of your server. Read %d, set to 1")

e('INIT_GET_FD_LIMIT',
  title = "Couldn't get the file descriptor limit of your system",
  desc  = SYSTEM_ISSUE)
  

# cherokee/utils.c
#
e('UTIL_F_GETFL',
  title = "fcntl (F_GETFL, fd=%d, 0): ${errno}",
  desc  = CODING_BUG)

e('UTIL_F_SETFL',
  title = "fcntl (F_GETFL, fd=%d, flags=%d (+%s)): ${errno}",
  desc  = CODING_BUG)

e('UTIL_F_GETFD',
  title = "fcntl (F_GETFD, fd=%d, 0): ${errno}",
  desc  = CODING_BUG)

e('UTIL_F_SETFD',
  title = "fcntl (F_GETFD, fd=%d, flags=%d (+%s)): ${errno}",
  desc  = CODING_BUG)

e('UTIL_MKDIR',
  title = "Could not mkdir '%s': ${errno}",
  desc  = "Most probably there you have to adjust some permissions.")


# cherokee/avl.c
#
e('AVL_PREVIOUS',
  title = "AVL Tree inconsistency: Right child",
  desc  = CODING_BUG)

e('AVL_NEXT',
  title = "AVL Tree inconsistency: Left child",
  desc  = CODING_BUG)

e('AVL_BALANCE',
  title = "AVL Tree inconsistency: Balance",
  desc  = CODING_BUG)


# cherokee/buffer.c
#
e('BUFFER_NEG_ESTIMATION',
  title = "Buffer: Bad memory estimation. The format '%s' estimated a negative length: %d.",
  desc  = CODING_BUG)

e('BUFFER_NO_SPACE',
  title = "Buffer: No target memory. The format '%s' got a free size of %d (estimated %d).",
  desc  = CODING_BUG)

e('BUFFER_BAD_ESTIMATION',
  title = "Buffer: Bad estimation. Too few memory: '%s' -> '%s', esti=%d real=%d size=%d.",
  desc  = CODING_BUG)

e('BUFFER_AVAIL_SIZE',
  title = "Buffer: Bad estimation: Estimation=%d, needed=%d available size=%d: %s.",
  desc  = CODING_BUG)

e('BUFFER_OPEN_FILE',
  title = "Could open the file: %s, ${errno}",
  desc  = "Please check that the file exists and the server has read access.")

e('BUFFER_READ_FILE',
  title = "Could not read from fd: read(%d, %d, ..) = ${errno}",
  desc  = "Please check that the file exists and the server has read access.")


# cherokee/plugin_loader.c
#
UNAVAILABLE_PLUGIN = """Either you are trying to use an unavailable
(uninstalled?) plugin, or there is a installation issue."""

e('PLUGIN_LOAD_NO_SYM',
  title = "Couldn't get simbol '%s': %s",
  desc  = INTERNAL_ISSUE)

e('PLUGIN_DLOPEN',
  title = "Something just happened while opening a plug-in file",
  desc  = "The operating system reported '%s' while trying to load '%s'.")

e('PLUGIN_NO_INIT',
  title = "The plug-in initialization function (%s) could not be found",
  desc  = CODING_BUG)

e('PLUGIN_NO_OPEN',
  title = "Could not open the '%s' module",
  desc  = UNAVAILABLE_PLUGIN)

e('PLUGIN_NO_INFO',
  title = "Could not access the 'info' entry of the %s plug-in",
  desc  = UNAVAILABLE_PLUGIN)


# cherokee/virtual_server.c
#
e('VSERVER_BAD_METHOD',
  title = "Unsupported method '%s'",
  admin = "/vserver/%d/rule/%d",
  desc  = "")

e('VSERVER_LOAD_MODULE',
  title = "Could not load rule module '%s'",
  admin = "/vserver/%d",
  desc  = "The server could not load a plug-in file. This might be due to some problem in the installation.")


# cherokee/regex.c
#
e('REGEX_COMPILATION',
  title = "Could not compile <<%s>>: %s (offset=%d)",
  desc  = "For some reason, PCRE could not compile the regular expression. Please modify the regular expression in order to solve this problem.")


# cherokee/access.c
#
e('ACCESS_IPV4_MAPPED',
  title = "This IP '%s' is IPv6-mapped IPv6 address",
  desc  = "It can be solved by specifying the IP in IPv4 style: a.b.c.d, instead of IPv6 style: ::ffff:a.b.c.d style")

e('ACCESS_INVALID_IP',
  title = "The IP address '%s' seems to be invalid",
  desc  = "You must have made a mistake. Please, try to fix the IP and try again.")

e('ACCESS_INVALID_MASK',
  title = "The network mask '%s' seems to be invalid",
  desc  = "You must have made a mistake. Please, try to fix the IP and try again.")


# cherokee/bind.c
#
e('BIND_PORT_NEEDED',
  title = "A port entry is need",
  desc  = "It seems that the configuration file includes a port listening entry with the wrong format. It should contain one port specification, but it does not in this case.")

e('BIND_COULDNT_BIND_PORT',
  title = "Could not bind() port=%d (UID=%d, GID=%d)",
  desc  = "Most probably there is another web server listening to the same port. You'll have to shut it down before launching Cherokee. It could also be a permissions issue. Remember that non-root user cannot listen to ports > 1024.")
