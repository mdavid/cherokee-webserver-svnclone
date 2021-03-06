ASCIIDOC=python $(top_srcdir)/doc/build/asciidoc.py --conf-file=$(top_srcdir)/doc/build/doc.conf

SUFFIXES = .txt .html

.txt.html:
	$(ASCIIDOC) -o $@ $(top_srcdir)/doc/$<

doc_DATA = \
index.html \
basics.html \
basics_why_cherokee.html \
basics_requirements.html \
basics_download.html \
basics_installation.html \
basics_installation_unix.html \
basics_installation_osx.html \
basics_installation_windows.html \
basics_installation_svn.html \
basics_upgrade.html \
basics_running_cherokee.html \
config.html \
config_quickstart.html \
config_index.html \
config_general.html \
config_virtual_servers.html \
config_virtual_servers_evhost.html \
config_virtual_servers_rule.html \
config_virtual_servers_rule_types.html \
config_advanced.html \
config_info_sources.html \
config_status.html \
config_wizards.html \
cookbook.html \
cookbook_managing_logs.html \
cookbook_optimizations.html \
cookbook_cross_compilation.html \
cookbook_django.html \
cookbook_php.html \
cookbook_ror.html \
cookbook_authentication.html \
cookbook_ssl.html \
cookbook_maintenance.html \
cookbook_redirs.html \
cookbook_embedding_cherokee.html \
cookbook_dbslayer.html \
cookbook_mono.html \
cookbook_mailman.html \
cookbook_joomla.html \
cookbook_glassfish.html \
cookbook_wordpress.html \
cookbook_drupal.html \
cookbook_zend.html \
cookbook_kumbia.html \
cookbook_nagios.html \
cookbook_symfony.html \
cookbook_moodle.html \
cookbook_alfresco.html \
cookbook_liferay.html \
cookbook_sugarcrm.html \
cookbook_trac.html \
cookbook_phpbb.html \
cookbook_phpmyadmin.html \
cookbook_managing_logs.html \
cookbook_streaming.html \
cookbook_uwsgi.html \
cookbook_coldfusion.html \
cookbook_concrete5.html \
cookbook_https_accelerator.html \
modules.html \
modules_balancers.html \
modules_balancers_ip_hash.html \
modules_balancers_round_robin.html \
modules_encoders.html \
modules_encoders_gzip.html \
modules_encoders_deflate.html \
modules_handlers.html \
modules_handlers_file.html \
modules_handlers_dbslayer.html \
modules_handlers_dirlist.html \
modules_handlers_custom_error.html \
modules_handlers_common.html \
modules_handlers_redir.html \
modules_handlers_cgi.html \
modules_handlers_fcgi.html \
modules_handlers_scgi.html \
modules_handlers_server_info.html \
modules_handlers_ssi.html \
modules_handlers_postreport.html \
modules_handlers_streaming.html \
modules_handlers_admin.html \
modules_handlers_proxy.html \
modules_handlers_secdownload.html \
modules_handlers_uwsgi.html \
modules_handlers_empty_gif.html \
modules_loggers.html \
modules_loggers_combined.html \
modules_loggers_ncsa.html \
modules_loggers_custom.html \
modules_validators.html \
modules_validators_htdigest.html \
modules_validators_htpasswd.html \
modules_validators_ldap.html \
modules_validators_mysql.html \
modules_validators_pam.html \
modules_validators_plain.html \
modules_validators_authlist.html \
other.html \
other_faq.html \
other_goodies.html \
other_signals.html \
other_community.html \
other_errors.html \
other_graphs.html \
other_bundle.html \
other_bundle_cherokee.html \
other_bundle_cherokee-admin.html \
other_bundle_cherokee-config.html \
other_bundle_cherokee-tweak.html \
other_bundle_cherokee-worker.html \
other_bundle_spawn-fcgi.html \
other_bundle_cget.html \
dev.html \
dev_debug.html \
dev_cherokee.conf.html \
dev_issues.html \
dev_qa.html

docmediacssdir = $(docdir)/media/css/
docmediacss_DATA = \
media/css/cherokee_doc.css

docmediaimagesdir = $(docdir)/media/images/
docmediaimages_DATA = \
media/images/admin_advanced0.png \
media/images/admin_advanced1.png \
media/images/admin_advanced2.png \
media/images/admin_advanced3.png \
media/images/admin_advanced4.png \
media/images/admin_advanced5.png \
media/images/admin_behavior.png \
media/images/admin_general.png \
media/images/admin_general_networking.png \
media/images/admin_general_permissions.png \
media/images/admin_general_ports.png \
media/images/admin_general_icons.png \
media/images/admin_general_mime.png \
media/images/admin_info_sources.png \
media/images/admin_handler_admin.png \
media/images/admin_handler_cgi.png \
media/images/admin_handler_custom_error.png \
media/images/admin_handler_dirlist_ex.png \
media/images/admin_handler_dirlist.png \
media/images/admin_handler_fastcgi1.png \
media/images/admin_handler_fastcgi2.png \
media/images/admin_handler_file.png \
media/images/admin_handler_onlylisting_ex.png \
media/images/admin_handler_onlylisting.png \
media/images/admin_handler_dbslayer.png \
media/images/admin_handler_secdownload.png \
media/images/admin_index.png \
media/images/admin_launch.png \
media/images/admin_noconfig.png \
media/images/admin_notrunning.png \
media/images/admin_rule_regex.png \
media/images/admin_validators_pam.png \
media/images/admin_validators_plain.png \
media/images/admin_validators_authlist.png \
media/images/admin_validators_ldap.png \
media/images/admin_vservers_wizard.png \
media/images/admin_vserver_wizard.png \
media/images/admin_vserver_errors.png \
media/images/admin_vserver_loggers.png \
media/images/admin_vserver.png \
media/images/admin_vserver_security.png \
media/images/admin_vserver_evhost.png \
media/images/admin_handler_proxy.png \
media/images/cookbook_django_common.png \
media/images/cookbook_django_infosources.png \
media/images/cookbook_ror_common.png \
media/images/cookbook_ror_fcgi.png \
media/images/cookbook_mysql_rule.png \
media/images/cookbook_mysql_validator.png \
media/images/cookbook_maintenance_copy.png \
media/images/cookbook_maintenance_domain.png \
media/images/cookbook_maintenance_error.png \
media/images/cookbook_maintenance_result.png \
media/images/cookbook_maintenance_rule.png \
media/images/cookbook_maintenance_advanced_rules.png \
media/images/cookbook_maintenance_advanced_redir.png \
media/images/cookbook_maintenance_advanced_domains.png \
media/images/cookbook_redir_behavior.png \
media/images/cookbook_redir_domains.png \
media/images/cookbook_redir_handler.png \
media/images/cookbook_dbslayer1.png \
media/images/cookbook_dbslayer2.png \
media/images/cookbook_dbslayer3.png \
media/images/cookbook_dbslayer4.png \
media/images/cookbook_mono_demo.png \
media/images/cookbook_mono_handler.png \
media/images/cookbook_django.png \
media/images/cookbook_ror.png \
media/images/cookbook_ror_wizard.png \
media/images/cookbook_mailman.png \
media/images/cookbook_glassfish.png \
media/images/cookbook_drupal.png \
media/images/cookbook_wp1.png \
media/images/cookbook_wp2.png \
media/images/cookbook_wp3.png \
media/images/cookbook_wp4.png \
media/images/cookbook_wp5.png \
media/images/cookbook_nagios1.png \
media/images/cookbook_nagios2.png \
media/images/cookbook_nagios3.png \
media/images/cookbook_nagios4.png \
media/images/cookbook_symfony.png \
media/images/cookbook_moodle_db.png \
media/images/cookbook_moodle_document_root.png \
media/images/cookbook_moodle_modules.png \
media/images/cookbook_moodle_paths.png \
media/images/cookbook_moodle.png \
media/images/cookbook_alfresco_alfresco.png \
media/images/cookbook_alfresco_assistant.png \
media/images/cookbook_alfresco_install.png \
media/images/cookbook_alfresco_path.png \
media/images/cookbook_alfresco_share.png \
media/images/cookbook_alfresco_soffice.png \
media/images/cookbook_liferay.png \
media/images/cookbook_sugarcrm_install0.png \
media/images/cookbook_sugarcrm_install1.png \
media/images/cookbook_sugarcrm_running0.png \
media/images/cookbook_sugarcrm_running1.png \
media/images/cookbook_trac.png \
media/images/cookbook_trac_info.png \
media/images/cookbook_trac_rules.png \
media/images/cookbook_trac_scgi.png \
media/images/cookbook_trac_static.png \
media/images/cookbook_phpbb_1_intro.png \
media/images/cookbook_phpbb_2_req.png \
media/images/cookbook_phpbb_3_db.png \
media/images/cookbook_phpbb_4_admin.png \
media/images/cookbook_phpbb_5_config.png \
media/images/cookbook_phpbb_6_advanced.png \
media/images/cookbook_phpbb_7_create.png \
media/images/cookbook_phpbb_8_final.png \
media/images/cookbook_phpbb_adm.png \
media/images/cookbook_phpbb_adm_warning.png \
media/images/cookbook_phpbb_rules.png \
media/images/cookbook_phpbb_running.png \
media/images/cookbook_phpmyadmin.png \
media/images/cookbook_phpmyadmin_denied.png \
media/images/cookbook_managing_logs_security.png \
media/images/cookbook_managing_logs_rules.png \
media/images/cookbook_managing_logs_handler.png \
media/images/cookbook_zend1.png \
media/images/cookbook_joomla.png \
media/images/cookbook_https_accelerator_handler.png \
media/images/cookbook_https_accelerator_sources.png \
media/images/proxy_phase1.png \
media/images/proxy_phase2.png \
media/images/screencast.png \
media/images/tracelor.png \
media/images/other_graphs_timeouts.png \
media/images/other_graphs_conns.png \
media/images/other_graphs_logging.png \
media/images/other_graphs_traffic.png


EXTRA_DIST = \
$(doc_DATA) \
$(docmediacss_DATA) \
$(docmediaimages_DATA) \
test_images.py

serve:
	../cherokee/cherokee -r .

test:
	./test_images.py
