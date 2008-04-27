/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2008 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "common-internal.h"
#include "virtual_server.h"
#include "config_entry.h"
#include "socket.h"
#include "server-protected.h"
#include "util.h"
#include "access.h"
#include "handler_error.h"
#include "rule_default.h"

#include <errno.h>

#define ENTRIES "vserver"


ret_t 
cherokee_virtual_server_new (cherokee_virtual_server_t **vserver, void *server)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, virtual_server);

	INIT_LIST_HEAD (&n->list_node);
	INIT_LIST_HEAD (&n->index_list);

	n->server_ref      = server;
	n->default_handler = NULL;
	n->error_handler   = NULL;
	n->logger          = NULL;
	n->logger_props    = NULL;

	/* Virtual entries
	 */
	ret = cherokee_rule_list_init (&n->rules);
	if (ret != ret_ok)
		return ret;

	ret = cherokee_rule_list_init (&n->userdir_rules);
	if (ret != ret_ok)
		return ret;

	/* Data transference 
	 */
	n->data.rx         = 0;
	n->data.tx         = 0;
	CHEROKEE_MUTEX_INIT(&n->data.rx_mutex, NULL);
	CHEROKEE_MUTEX_INIT(&n->data.tx_mutex, NULL);

	/* TLS related files
	 */
	cherokee_buffer_init (&n->server_cert);
	cherokee_buffer_init (&n->server_key);
	cherokee_buffer_init (&n->ca_cert);

#ifdef HAVE_TLS
	ret = cherokee_avl_r_init (&n->session_cache);
	if (unlikely(ret < ret_ok))
		return ret;

# ifdef HAVE_GNUTLS
	n->credentials     = NULL;
# endif
# ifdef HAVE_OPENSSL
	n->context         = NULL;
# endif 

#endif /* HAVE_TLS */

	ret = cherokee_buffer_init (&n->root);
	if (unlikely(ret < ret_ok))
		return ret;	
	
	ret = cherokee_buffer_init (&n->name);
	if (unlikely(ret < ret_ok))
		return ret;

	INIT_LIST_HEAD (&n->domains);

	ret = cherokee_buffer_init (&n->userdir);
	if (unlikely(ret < ret_ok))
		return ret;

	/* Return the object
	 */
	*vserver = n;
	return ret_ok;
}


ret_t 
cherokee_virtual_server_free (cherokee_virtual_server_t *vserver)
{
	cherokee_buffer_mrproper (&vserver->server_cert);
	cherokee_buffer_mrproper (&vserver->server_key);
	cherokee_buffer_mrproper (&vserver->ca_cert);

	if (vserver->error_handler != NULL) {
		cherokee_config_entry_free (vserver->error_handler);
		vserver->error_handler = NULL;
	}

	if (vserver->default_handler != NULL) {
		cherokee_config_entry_free (vserver->default_handler);
		vserver->default_handler = NULL;
	}

#ifdef HAVE_TLS
	cherokee_avl_r_mrproper (&vserver->session_cache, NULL); //FIXIT

# ifdef HAVE_GNUTLS
	if (vserver->credentials != NULL) {
		gnutls_certificate_free_credentials (vserver->credentials);
		vserver->credentials = NULL;
	}
# endif
# ifdef HAVE_OPENSSL
	if (vserver->context != NULL) {
		SSL_CTX_free (vserver->context);
		vserver->context = NULL;
	}
# endif
#endif

	cherokee_buffer_mrproper (&vserver->name);
	cherokee_vserver_names_mrproper (&vserver->domains);

	cherokee_buffer_mrproper (&vserver->root);

	if (vserver->logger != NULL) {
		cherokee_logger_free (vserver->logger);
		vserver->logger = NULL;
	}
	if (vserver->logger_props != NULL) {
		cherokee_avl_free (vserver->logger_props, NULL); // FIXIT
		vserver->logger_props = NULL;
	}

	cherokee_buffer_mrproper (&vserver->userdir);

	/* Destroy the virtual_entries
	 */
	cherokee_rule_list_mrproper (&vserver->rules);
	cherokee_rule_list_mrproper (&vserver->userdir_rules);

	/* Index list
	 */
	cherokee_list_content_free (&vserver->index_list, free);

	free (vserver);	
	return ret_ok;
}


#ifdef HAVE_GNUTLS
static ret_t
generate_dh_params (gnutls_dh_params *dh_params)
{
	gnutls_dh_params_init (dh_params);
	gnutls_dh_params_generate2 (*dh_params, 1024);

	return ret_ok;
}

static ret_t
generate_rsa_params (gnutls_rsa_params *rsa_params)
{
	/* Generate RSA parameters - for use with RSA-export cipher
	 * suites. These should be discarded and regenerated once a
	 * day, once every 500 transactions etc. Depends on the
	 * security requirements.
	 */

	gnutls_rsa_params_init (rsa_params);
	gnutls_rsa_params_generate2 (*rsa_params, 512);

	return ret_ok;
}
#endif /* HAVE_GNUTLS */


ret_t 
cherokee_virtual_server_has_tls (cherokee_virtual_server_t *vserver)
{
#ifdef HAVE_TLS
	if (! cherokee_buffer_is_empty (&vserver->server_cert))
		return ret_ok;
	if (! cherokee_buffer_is_empty (&vserver->server_key))
		return ret_ok;
	if (! cherokee_buffer_is_empty (&vserver->ca_cert))
		return ret_ok;
#endif
	return ret_not_found;
}


ret_t 
cherokee_virtual_server_init_tls (cherokee_virtual_server_t *vsrv)
{
#ifdef HAVE_TLS
	int rc;

	/* Check if all of them are empty
	 */
	if (cherokee_buffer_is_empty (&vsrv->ca_cert)    &&
	    cherokee_buffer_is_empty (&vsrv->server_key) &&
	    cherokee_buffer_is_empty (&vsrv->server_cert))
		return ret_not_found;

	/* Check one or more are empty
	 */
	if (cherokee_buffer_is_empty (&vsrv->ca_cert)    ||
	    cherokee_buffer_is_empty (&vsrv->server_key) ||
	    cherokee_buffer_is_empty (&vsrv->server_cert))
		return ret_error;

# ifdef HAVE_GNUTLS
	rc = gnutls_certificate_allocate_credentials (&vsrv->credentials);
	if (rc < 0) {
		PRINT_ERROR_S ("ERROR: Couldn't allocate credentials.\n");
		return ret_error;
	}

	/* CA file
	 */
	rc = gnutls_certificate_set_x509_trust_file (vsrv->credentials,
						     vsrv->ca_cert.buf,
						     GNUTLS_X509_FMT_PEM);
	if (rc < 0) {
		PRINT_ERROR ("ERROR: reading X.509 CA Certificate: '%s'\n", vsrv->ca_cert.buf);
		return ret_error;
	}

	/* Key file
	 */
	rc = gnutls_certificate_set_x509_key_file (vsrv->credentials,
						   vsrv->server_cert.buf,
						   vsrv->server_key.buf,
						   GNUTLS_X509_FMT_PEM);	
	if (rc < 0) {
		PRINT_ERROR ("ERROR: reading X.509 key '%s' or certificate '%s' file\n", 
			     vsrv->server_key.buf, vsrv->server_cert.buf);
		return ret_error;
	}

	generate_dh_params (&vsrv->dh_params);
	generate_rsa_params (&vsrv->rsa_params);

	gnutls_certificate_set_dh_params (vsrv->credentials, vsrv->dh_params);
	gnutls_anon_set_server_dh_params (vsrv->credentials, vsrv->dh_params);
	gnutls_certificate_set_rsa_export_params (vsrv->credentials, vsrv->rsa_params);
# endif

# ifdef HAVE_OPENSSL
	/* Init the OpenSSL context
	 */
	vsrv->context = SSL_CTX_new (SSLv23_server_method());
	if (vsrv->context == NULL) {
		PRINT_ERROR_S("ERROR: OpenSSL: Couldn't allocate OpenSSL context\n");
		return ret_error;
	}

	/* Certificate
	 */
	rc = SSL_CTX_use_certificate_file (vsrv->context, vsrv->server_cert.buf, SSL_FILETYPE_PEM);
	if (rc < 0) {
		char *error;

		OPENSSL_LAST_ERROR(error);
		PRINT_ERROR("ERROR: OpenSSL: Can not use certificate file '%s':  %s\n", 
			    vsrv->server_cert.buf, error);
		return ret_error;
	}

	/* Private key
	 */
	rc = SSL_CTX_use_PrivateKey_file (vsrv->context, vsrv->server_key.buf, SSL_FILETYPE_PEM);
	if (rc < 0) {
		char *error;

		OPENSSL_LAST_ERROR(error);
		PRINT_ERROR("ERROR: OpenSSL: Can not use private key file '%s': %s\n", 
			    vsrv->server_key.buf, error);
		return ret_error;
	}

	/* Check private key
	 */
	rc = SSL_CTX_check_private_key (vsrv->context);
	if (rc != 1) {
		PRINT_ERROR_S("ERROR: OpenSSL: Private key does not match the certificate public key\n");
		return ret_error;
	}
# endif

#endif /* HAVE_TLS */

	return ret_ok;
}


void  
cherokee_virtual_server_add_rx (cherokee_virtual_server_t *vserver, size_t rx)
{
	CHEROKEE_MUTEX_LOCK(&vserver->data.rx_mutex);
	vserver->data.rx += rx;
	CHEROKEE_MUTEX_UNLOCK(&vserver->data.rx_mutex);
}


void
cherokee_virtual_server_add_tx (cherokee_virtual_server_t *vserver, size_t tx)
{
	CHEROKEE_MUTEX_LOCK(&vserver->data.tx_mutex);
	vserver->data.tx += tx;
	CHEROKEE_MUTEX_UNLOCK(&vserver->data.tx_mutex);
}


ret_t 
cherokee_virtual_server_set_documentroot (cherokee_virtual_server_t *vserver, const char *documentroot)
{
	cherokee_buffer_clean (&vserver->root);
	cherokee_buffer_add (&vserver->root, documentroot, strlen(documentroot));

	return ret_ok;
}


static ret_t 
add_directory_index (char *index, void *data)
{
	cherokee_virtual_server_t *vserver = VSERVER(data);

	TRACE(ENTRIES, "Adding directory index '%s'\n", index);

	cherokee_list_add_tail_content (&vserver->index_list, strdup(index));
	return ret_ok;
}

static ret_t
add_access (char *address, void *data)
{
	ret_t                    ret;
	cherokee_config_entry_t *entry = CONF_ENTRY(data);

	if (entry->access == NULL) {
		ret = cherokee_access_new ((cherokee_access_t **) &entry->access);
		if (ret != ret_ok) return ret;
	}

	ret = cherokee_access_add (entry->access, address);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}


static ret_t 
init_entry_property (cherokee_config_node_t *conf, void *data)
{
	ret_t                      ret;
	cherokee_buffer_t         *tmp;
	cherokee_plugin_info_t    *info    = NULL;
	cherokee_virtual_server_t *vserver = ((void **)data)[0];
	cherokee_config_entry_t   *entry   = ((void **)data)[1];

	if (equal_buf_str (&conf->key, "allow_from")) {
		ret = cherokee_config_node_read_list (conf, NULL, add_access, entry);
		if (ret != ret_ok)
			return ret;

	} else if (equal_buf_str (&conf->key, "document_root")) {
		cherokee_config_node_read_path (conf, NULL, &tmp);

		if (entry->document_root == NULL) 
			cherokee_buffer_new (&entry->document_root);
		else
			cherokee_buffer_clean (entry->document_root);

		TRACE(ENTRIES, "DocumentRoot: %s\n", tmp->buf);
		cherokee_buffer_add_buffer (entry->document_root, tmp);

	} else if (equal_buf_str (&conf->key, "handler")) {
		tmp = &conf->val;

		ret = cherokee_plugin_loader_get (&SRV(vserver->server_ref)->loader, tmp->buf, &info);
		if (ret != ret_ok)
			return ret;

		if (info->configure) {
			handler_func_configure_t configure = info->configure;

			ret = configure (conf, vserver->server_ref, &entry->handler_properties);
			if (ret != ret_ok)
				return ret;
		}

		TRACE(ENTRIES, "Handler: %s\n", tmp->buf);
		cherokee_config_entry_set_handler (entry, PLUGIN_INFO_HANDLER(info));

	} else if (equal_buf_str (&conf->key, "auth")) {
		cherokee_plugin_info_validator_t *vinfo;

		/* Load module
		 */
		tmp = &conf->val;

		ret = cherokee_plugin_loader_get (&SRV(vserver->server_ref)->loader, tmp->buf, &info);
		if (ret != ret_ok)
			return ret;

		entry->validator_new_func = info->instance;

		if (info->configure) {
			validator_func_configure_t configure = info->configure;

			ret = configure (conf, vserver->server_ref, &entry->validator_properties);
			if (ret != ret_ok) return ret;
		}

		/* Configure the entry
		 */
		vinfo = (cherokee_plugin_info_validator_t *)info;

		ret = cherokee_validator_configure (conf, entry);
		if (ret != ret_ok)
			return ret;

		if ((entry->authentication & vinfo->valid_methods) != entry->authentication) {
			PRINT_MSG ("ERROR: '%s' unsupported methods\n", tmp->buf);
			return ret_error;
		}		

		TRACE(ENTRIES, "Validator: %s\n", tmp->buf);

	} else if (equal_buf_str (&conf->key, "only_secure")) {
		entry->only_secure = !! atoi(conf->val.buf);

	} else if (equal_buf_str (&conf->key, "only_secure")) {
		entry->only_secure = !! atoi(conf->val.buf);

	} else if (equal_buf_str (&conf->key, "match")) {
		/* Ignore: Previously handled 
		 */
	} else {
		PRINT_MSG ("ERROR: Virtual Server parser: Unknown key \"%s\"\n", conf->key.buf);
		return ret_error;
	}

	return ret_ok;
}


static ret_t 
init_entry (cherokee_virtual_server_t *vserver, 
	    cherokee_config_node_t    *config, 
	    cherokee_config_entry_t   *entry)
{
	ret_t  ret;
	void  *params[2] = { vserver, entry };

	ret = cherokee_config_node_while (config, init_entry_property, (void *)params);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}


static ret_t 
add_error_handler (cherokee_config_node_t *config, cherokee_virtual_server_t *vserver)
{
	ret_t                    ret;
	cherokee_config_entry_t *entry;
	cherokee_plugin_info_t  *info   = NULL;
	cherokee_buffer_t       *name   = &config->val;

	ret = cherokee_config_entry_new (&entry);
	if (unlikely (ret != ret_ok))
		return ret;

	ret = cherokee_plugin_loader_get (&SRV(vserver->server_ref)->loader, name->buf, &info);
	if (ret != ret_ok)
		return ret;

	if (info->configure) {
		handler_func_configure_t configure = info->configure;

		ret = configure (config, vserver->server_ref, &entry->handler_properties);
		if (ret != ret_ok)
			return ret;
	}

	TRACE(ENTRIES, "Error handler: %s\n", name->buf);

	cherokee_config_entry_set_handler (entry, PLUGIN_INFO_HANDLER(info));		
	vserver->error_handler = entry;

	return ret_ok;
}


static ret_t
configure_rule_match (cherokee_config_node_t     *config, 
		      cherokee_virtual_server_t  *vserver, 
		      cherokee_rule_list_t       *rule_list,
		      cuint_t                     priority,
		      cherokee_rule_t           **rule)
{
	ret_t                   ret;
	rule_func_new_t         func_new;
	cherokee_plugin_info_t *info      = NULL;
	cherokee_buffer_t      *type      = &config->val;
	cherokee_server_t      *srv       = VSERVER_SRV(vserver);

	/* Sanity check
	 */
	if (type == NULL) {
		PRINT_ERROR ("Rule match prio=%d must include a type property\n", priority);
		return ret;
	}

	TRACE (ENTRIES, "Adding type=%s\n", type->buf);

	/* Default is compled in, the rest are loded as plug-ins
	 */
	if (equal_buf_str (type, "default")) {
		func_new = (rule_func_new_t) cherokee_rule_default_new;

	} else {
		ret = cherokee_plugin_loader_get (&srv->loader, type->buf, &info);
		if (ret < ret_ok) {
			PRINT_MSG ("ERROR: Couldn't load rule module '%s'\n", type->buf);
			return ret_error;
		}

		func_new = (rule_func_new_t) info->instance;
	}

	/* Instance the rule object
	 */
	if (func_new == NULL)
		return ret_error;

	ret = func_new ((void **) rule);
	if ((ret != ret_ok) || (*rule == NULL))
		return ret_error;

	/* Configure it
	 */
	(*rule)->priority = priority;

	ret = cherokee_rule_configure (*rule, config, vserver);
	if (ret != ret_ok)
		return ret_error;

	return ret_ok;
}


static ret_t 
add_rule (cherokee_config_node_t    *config, 
	  cherokee_virtual_server_t *vserver, 
	  cherokee_rule_list_t      *rule_list)
{
	ret_t                   ret;
	cuint_t                 prio;
	cherokee_rule_t        *rule    = NULL;
	cherokee_config_node_t *subconf = NULL; 

	/* Validate priority
	 */
	prio = atoi(config->key.buf);
	if (prio <= CHEROKEE_RULE_PRIO_NONE) {
		PRINT_ERROR ("Invalid priority: '%s'\n", config->key.buf);
		return ret_error;
	}

	/* Configure the rule match section
	 */
	ret = cherokee_config_node_get (config, "match", &subconf);
	if (ret != ret_ok) {
		PRINT_ERROR ("Rule prio=%d needs a 'match' section\n", prio);
		return ret_error;
	}

	ret = configure_rule_match (subconf, vserver, rule_list, prio, &rule);
	if (ret != ret_ok) 
		goto failed;

	/* config_node -> config_entry
	 */
	ret = init_entry (vserver, config, &rule->config);
	if (ret != ret_ok) 
		goto failed;

	/* Add the rule to the vserver's list
	 */
	ret = cherokee_rule_list_add (rule_list, rule);
	if (ret != ret_ok) 
		goto failed;

	return ret_ok;

failed:
	if (rule != NULL)
		cherokee_rule_free (rule);

	return ret_error;
}


static ret_t 
add_domain (cherokee_config_node_t *config, cherokee_virtual_server_t *vserver)
{
	ret_t ret;

	TRACE (ENTRIES, "Adding vserver '%s' domain name '%s'\n", vserver->name.buf, config->val.buf);

	ret = cherokee_vserver_names_add_name (&vserver->domains, &config->val);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}


static ret_t 
add_logger (cherokee_config_node_t *config, cherokee_virtual_server_t *vserver)
{
	ret_t                   ret;
	logger_func_new_t       func_new;
	cherokee_plugin_info_t *info      = NULL;
	cherokee_server_t      *srv       = SRV(vserver->server_ref);

	if (cherokee_buffer_is_empty (&config->val)) {
		PRINT_ERROR_S ("ERROR: A logger must be specified\n");
		return ret_error;
	}

	ret = cherokee_plugin_loader_get (&srv->loader, config->val.buf, &info);
	if (ret < ret_ok) {
		PRINT_MSG ("ERROR: Couldn't load logger module '%s'\n", config->val.buf);
		return ret_error;
	}

	/* Instance a new logger
	 */
	func_new = (logger_func_new_t) info->instance;
	if (func_new == NULL)
		return ret_error;

	ret = func_new ((void **) &vserver->logger, config);
	if (ret != ret_ok)
		return ret;

	/* Logger initialization
	 */
	ret = cherokee_logger_init (vserver->logger);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}


static ret_t
configure_rules (cherokee_config_node_t    *config, 
		 cherokee_virtual_server_t *vserver,
		 cherokee_rule_list_t      *rule_list)
{
	ret_t                   ret;
	cherokee_list_t        *i; //, *j;
	cherokee_config_node_t *subconf;
//	cherokee_boolean_t      did_default = false;

	cherokee_config_node_foreach (i, config) {
		subconf = CONFIG_NODE(i);
		
		ret = add_rule (subconf, vserver, rule_list);
		if (ret != ret_ok) return ret;
	}

	/* Sort rules by its priority
	 */
	cherokee_rule_list_sort (rule_list);

// TODO:
/* 	if (! did_default) { */
/* 		PRINT_ERROR ("ERROR: vserver '%s': A default rule is needed\n", vserver->name.buf); */
/* 		return ret_error; */
/* 	} */

	return ret_ok;
}

static ret_t 
configure_user_dir (cherokee_config_node_t *config, cherokee_virtual_server_t *vserver)
{
	ret_t                   ret;
	cherokee_config_node_t *subconf;

	/* Set the user_dir directory. It must end by slash.
	 */
	cherokee_buffer_add_buffer (&vserver->userdir, &config->val);

	if (cherokee_buffer_end_char (&vserver->userdir) != '/')
		cherokee_buffer_add_str (&vserver->userdir, "/");

	/* Configure the rest of the entries
	 */
 	ret = cherokee_config_node_get (config, "rule", &subconf); 
 	if (ret == ret_ok) { 
		ret = configure_rules (subconf, vserver, &vserver->userdir_rules);
		if (ret != ret_ok) return ret;
	}

	return ret_ok;
}


static ret_t 
configure_virtual_server_property (cherokee_config_node_t *conf, void *data)
{
	ret_t                      ret;
	cherokee_buffer_t         *tmp;
	cherokee_list_t           *i;
	cherokee_virtual_server_t *vserver = VSERVER(data);

	if (equal_buf_str (&conf->key, "document_root")) {
		ret = cherokee_config_node_read_path (conf, NULL, &tmp);
		if (ret != ret_ok)
			return ret;
		
		ret = cherokee_virtual_server_set_documentroot (vserver, (const char *)tmp->buf);
		if (ret != ret_ok) {
			PRINT_MSG ("ERROR: Virtual server: Error setting DocumentRoot: '%s'\n", tmp->buf);
			return ret_error;
		}

	} else if (equal_buf_str (&conf->key, "user_dir")) {
		ret = configure_user_dir (conf, vserver);
		if (ret != ret_ok)
			return ret;

	} else if (equal_buf_str (&conf->key, "rule")) {
		ret = configure_rules (conf, vserver, &vserver->rules);
		if (ret != ret_ok) return ret;

	} else if (equal_buf_str (&conf->key, "domain")) {
		cherokee_config_node_foreach (i, conf) {
			ret = add_domain (CONFIG_NODE(i), vserver);
			if (ret != ret_ok)
				return ret;		
		}
	} else if (equal_buf_str (&conf->key, "error_handler")) {
		ret = add_error_handler (conf, vserver);
		if (ret != ret_ok)
			return ret;

	} else if (equal_buf_str (&conf->key, "logger")) {
		ret = add_logger (conf, vserver);
		if (ret != ret_ok)
			return ret;

	} else if (equal_buf_str (&conf->key, "directory_index")) {
		cherokee_config_node_read_list (conf, NULL, add_directory_index, vserver);

	} else if (equal_buf_str (&conf->key, "ssl_certificate_file")) {
		cherokee_buffer_init (&vserver->server_cert);
		cherokee_buffer_add_buffer (&vserver->server_cert, &conf->val);

	} else if (equal_buf_str (&conf->key, "ssl_certificate_key_file")) {
		cherokee_buffer_init (&vserver->server_key);
		cherokee_buffer_add_buffer (&vserver->server_key, &conf->val);

	} else if (equal_buf_str (&conf->key, "ssl_ca_list_file")) {
		cherokee_buffer_init (&vserver->ca_cert);
		cherokee_buffer_add_buffer (&vserver->ca_cert, &conf->val);

	} else {
		PRINT_MSG ("ERROR: Virtual Server: Unknown key '%s'\n", conf->key.buf);
		return ret_error;
	}

	return ret_ok;
}


ret_t 
cherokee_virtual_server_configure (cherokee_virtual_server_t *vserver, cherokee_buffer_t *name, cherokee_config_node_t *config)
{
	ret_t ret;

	/* Set vserver name
	 */
	if (cherokee_buffer_is_empty (&vserver->name)) {
		cherokee_buffer_add_buffer (&vserver->name, name);
	}

	/* Parse properties
	 */
	ret = cherokee_config_node_while (config, configure_virtual_server_property, vserver);
	if (ret != ret_ok)
		return ret;

	/* Perform some sanity checks
	 */
	if (cherokee_buffer_is_empty (&vserver->root)) {
		PRINT_MSG ("ERROR: Virtual host '%s' needs a document_root\n", name->buf);
		return ret_error;
	}

	return ret_ok;
}
