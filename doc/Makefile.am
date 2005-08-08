SUBDIRS = develop

update:
	wget -O - http://0x50.org/doc/cherokee_doc.tar.gz | tar xfvz -

docdir = $(datadir)/doc/cherokee/
doc_DATA = \
index.html \
Behavior_configuration.html \
CGI_executing.html \
Cget.html \
Combined_logger.html \
Common_behavior.html \
Compiling_and_Installing.html \
Configuration.html \
Current_events.html \
Directory_configuration.html \
Directory_listing.html \
Extension_configuration.html \
FastCGI.html \
File_sending.html \
How_to_generate_SSL_keys.html \
Icons_configuration.html \
Logging_system.html \
Main_Page.html \
Mime_types_configuration.html \
NCSA_logger.html \
PHP_execution.html \
Redirections.html \
Server_configuration.html \
U-Cherokee.html \
Validators.html \
W3C_logger.html \
WebApplications.html \
cherokee.png \
images/Server-conf-small.png 

EXTRA_DIST=\
$(doc_DATA)

