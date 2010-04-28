# Ports
PORT              = 1978
PORT_TLS          = 1979
HOST              = "localhost"

# Basic paths
CHEROKEE_PATH     = "../cherokee/cherokee-worker"
CHEROKEE_MODS     = "../cherokee/.libs/"
CHEROKEE_DEPS     = "../cherokee/"
CHEROKEE_PANIC    = "../cherokee/cherokee-panic"

# Logging
LOGGER_TYPE       = "combined"
LOGGER_ACCESS     = "access.log"
LOGGER_ERROR      = "error.log"

# TLS/SSL
SSL_CERT_FILE     = "/etc/cherokee/ssl/cherokee.pem"
SSL_CERT_KEY_FILE = "/etc/cherokee/ssl/cherokee.pem"
SSL_CA_FILE       = "/etc/cherokee/ssl/cherokee.pem"

# Misc options
SERVER_DELAY      = 10

# Third party utilities
STRACE_PATH       = "/usr/bin/strace"
DTRUSS_PATH       = "/usr/bin/dtruss"
VALGRIND_PATH     = "/usr/bin/valgrind"
PYTHON_PATH       = "auto"
PHPCGI_PATH       = "auto"
PHP_FCGI_PORT     = 1980

PHP_DIRS          = ["/usr/lib/cgi-bin/",
                     "/usr/bin/",
                     "/usr/local/bin/",
                     "/opt/csw/php5/bin/",
                     "/opt/local/bin"]

PYTHON_DIRS       = ["/usr/bin",
                     "/usr/local/bin",
                     "/opt/csw/bin",
                     "/opt/local/bin"]

PHP_NAMES         = ["php5-cgi",
                     "php-cgi",
                     "php4-cgi"
                     "php5",
                     "php",
                     "php4"]

PYTHON_NAMES      = ["python",
                     "python2.7",
                     "python2.6",
                     "python2.5",
                     "python2.4",
                     "python2.3"]
