# Macros
#
INTERNAL_ISSUE = """The server found an internal problem.
Bla, bla, bla..
"""

CODING_BUG = """Looks like a bug in the plug-in that you're trying to
use. Please report it to its author or maintainer so he can fix it
up."""

SYSTEM_ISSUE = """The issue seems to be related to your system."""


# cherokee/socket.c
#
e('SOCKET_SET_KEEPALIVE',
  title = "Could not set SO_KEEPALIVE on fd=%d: ${errno}",
  desc  = CODING_BUG)

e('SOCKET_SET_LINGER',
  title = "Could not set SO_LINGER on fd=%d: ${errno}",
  desc  = CODING_BUG)

e('SOCKET_RM_NAGLES',
  title = "Could not disable Nagle's algorithm",
  desc  = SYSTEM_ISSUE)

e('SOCKET_NON_BLOCKING',
  title = "Could not set non-blocking, fd %d",
  desc  = CODING_BUG)

e('SOCKET_NO_SOCKET',
  title = "%s isn't a socket",
  desc  = "The file is supposed to be a Unix socket, although it does not look like one.")

e('SOCKET_REMOVE',
  title = "Could not remove %s",
  desc  = "Could not remove the Unix socket because: ${errno}")

e('SOCKET_WRITE',
  title = "Could not write to socket: write(%d, ..): '${errno}'",
  desc  = CODING_BUG)

e('SOCKET_READ',
  title = "Could not read from socket: read(%d, ..): '${errno}'",
  desc  = CODING_BUG)

e('SOCKET_WRITEV',
  title = "Could not write a vector to socket: writev(%d, ..): '${errno}'",
  desc  = CODING_BUG)

e('SOCKET_CONNECT',
  title = "Could not connect: ${errno}",
  desc  = SYSTEM_ISSUE)

e('SOCKET_BAD_FAMILY',
  title = "Unknown socket family: %d",
  desc  = CODING_BUG)

e('SOCKET_SET_NODELAY',
  title = "Could not set TCP_NODELAY to fd %d: ${errno}",
  desc  = CODING_BUG)

e('SOCKET_RM_NODELAY',
  title = "Could not remove TCP_NODELAY from fd %d: ${errno}",
  desc  = CODING_BUG)

e('SOCKET_SET_CORK',
  title = "Could not set TCP_CORK to fd %d: ${errno}",
  desc  = CODING_BUG)

e('SOCKET_RM_CORK',
  title = "Could not set TCP_CORK from fd %d: ${errno}",
  desc  = CODING_BUG)


# cherokee/thread.c
#
e('THREAD_RM_FD_POLL',
  title = "Couldn't remove fd(%d) from fdpoll",
  desc  = CODING_BUG)

e('THREAD_HANDLER_RET',
  title = "Unknown ret %d from handler %s",
  desc  = CODING_BUG)

e('THREAD_OUT_OF_FDS',
  title = "Run out of file descriptors",
  desc  = "The server is under heavy load and it has run out of file descriptors. It can be fixed by raising the file descriptor limit and restarting the server.",
  admin = "/advanced")

e('THREAD_GET_CONN_OBJ',
  title = "Trying to get a new connection object",
  desc  = "Either the system run out of memory, or you've hit a bug in the code.")

e('THREAD_SET_SOCKADDR',
  title = "Could not set sockaddr",
  desc  = CODING_BUG);


# cherokee/connection.c
#
e('CONNECTION_AUTH',
  title = "Unknown authentication method",
  desc  = "To-do")

e('CONNECTION_LOCAL_DIR',
  title = "Could not build the local directory string",
  desc  = "To-do")

e('CONNECTION_GET_VSERVER',
  title = "Couldn't get virtual server: '%s'",
  desc  = "To-do")


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
  desc  = "(TO DO)")

e('VSERVER_TIME_MISSING',
  title = "Expiration time without a 'time' property",
  admin = "/vserver/%d/rule/%d",
  desc  = "(TO DO)")

e('VSERVER_RULE_UNKNOWN_KEY',
  title = "Virtual Server Rule, Unknown key '%s'",
  admin = "/vserver/%d/rule/%d",
  desc  = "(TO DO)")

e('VSERVER_TYPE_MISSING',
  title = "Rule matches must specify a 'type' property",
  admin = "/vserver/%d/rule/%d",
  desc  = "(TO DO)")

e('VSERVER_LOAD_MODULE',
  title = "Could not load rule module '%s'",
  admin = "/vserver/%d",
  desc  = "The server could not load a plug-in file. This might be due to some problem in the installation.")

e('VSERVER_BAD_PRIORITY',
  title = "Invalid priority '%s'",
  admin = "/vserver/%d",
  desc  = "(TO DO)")

e('VSERVER_RULE_MATCH_MISSING',
  title = "Rules must specify a 'match' property",
  admin = "/vserver/%d/rule/%d",
  desc  = "(TO DO)")
  
e('VSERVER_MATCH_MISSING',
  title = "Virtual Server must specify a 'match' property",
  admin = "/vserver/%d",
  desc  = "(TO DO)")

e('VSERVER_UNKNOWN_KEY',
  title = "Virtual Server, Unknown key '%s'",
  admin = "/vserver/%d",
  desc  = "(TO DO)")

e('VSERVER_NICK_MISSING',
  title = "Virtual Server  without a 'nick' property",
  admin = "/vserver/%d",
  desc  = "(TO DO)")

e('VSERVER_DROOT_MISSING',
  title = "Virtual Server  without a 'document_root' property",
  admin = "/vserver/%d",
  desc  = "(TO DO)")


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
