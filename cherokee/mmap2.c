/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001, 2002, 2003, 2004, 2005 Alvaro Lopez Ortega
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
#include "mmap2.h"
#include "table.h"
#include "list.h"
#include "buffer.h"
#include "list_merge_sort.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


struct cherokee_mmap2 {
	cherokee_table_t *table;
	unsigned int      table_items_num;
	unsigned int      table_max_items; 

	unsigned int      usage_times;

#ifdef HAVE_PTHREAD
	pthread_rwlock_t    table_rwlock;
#endif
};

typedef struct {
	cherokee_mmap2_entry_t   base;	
	cherokee_buffer_t       *filename;
	struct list_head         list_item;

	unsigned int             usage;
	unsigned int             ref;
#ifdef HAVE_PTHREAD
	pthread_rwlock_t         ref_rwlock;
#endif
} cherokee_mmap2_entry_priv_t;



#define PUBLIC(m) MMAP2_ENTRY(m)
#define PRIV(m)   ((cherokee_mmap2_entry_priv_t *)(m))

#ifdef MAP_FILE
# define MAP_OPTIONS MAP_SHARED | MAP_FILE
#else
# define MAP_OPTIONS MAP_SHARED
#endif


/* Entry operations
 */

static ret_t
entry_new (cherokee_mmap2_entry_t **entry, char *filename)
{
	CHEROKEE_NEW_STRUCT (n, mmap2_entry_priv);

	return_if_fail (filename != NULL, ret_error);
	
	PUBLIC(n)->mmaped     = NULL;
	PUBLIC(n)->mmaped_len = 0;

	PRIV(n)->ref          = 1;
	PRIV(n)->usage        = 1;

	cherokee_buffer_new (&n->filename);
	cherokee_buffer_add (n->filename, filename, strlen(filename));

	CHEROKEE_RWLOCK_INIT (&n->ref_rwlock, NULL);

	INIT_LIST_HEAD (&PRIV(n)->list_item);

	*entry = PUBLIC(n);
	return ret_ok;	
}

static ret_t
entry_set_mmap (cherokee_mmap2_entry_t *entry, const char *filename)
{
	int fd;
	int re;
	
	fd = open (filename, O_RDONLY);
	if (unlikely (fd == -1)) {
		return ret_deny;
	}

	re = fstat (fd, &entry->state);
	if (unlikely (re == -1)) {
		close (fd);
		return ret_deny;
	}

	if (unlikely (S_ISDIR(entry->state.st_mode))) {
		close (fd);
		return ret_deny;
	}

	entry->mmaped =
		mmap (NULL,                 /* void   *start  */
		      entry->state.st_size, /* size_t  length */
		      PROT_READ,            /* int     prot   */
		      MAP_OPTIONS,          /* int     flag   */
		      fd,                   /* int     fd     */
		      0);                   /* off_t   offset */

	if (entry->mmaped == MAP_FAILED) {
		close (fd);
		return ret_not_found;
	}

	entry->mmaped_len = entry->state.st_size;

	close (fd);
	return ret_ok;
}


/* Entry atomic reference operations
 */

static unsigned int
entry_ref_usage_inc (cherokee_mmap2_entry_t *entry)
{
	unsigned int ref;

	CHEROKEE_RWLOCK_WRITER (&PRIV(entry)->ref_rwlock);
	ref = ++PRIV(entry)->ref;
	PRIV(entry)->usage++;
	CHEROKEE_RWLOCK_UNLOCK (&PRIV(entry)->ref_rwlock);
	
	return ref;
}

static unsigned int
entry_ref_dec (cherokee_mmap2_entry_t *entry)
{
	unsigned int ref;

	CHEROKEE_RWLOCK_WRITER (&PRIV(entry)->ref_rwlock);
	ref = --PRIV(entry)->ref;
	CHEROKEE_RWLOCK_UNLOCK (&PRIV(entry)->ref_rwlock);

	return ref;
}

static unsigned int
entry_ref_usage_get_and_reset (cherokee_mmap2_entry_t *entry, unsigned int *usage)
{
	unsigned int ref;

	CHEROKEE_RWLOCK_READER (&PRIV(entry)->ref_rwlock);	
	*usage = PRIV(entry)->usage;
	PRIV(entry)->usage = 0;

	ref = PRIV(entry)->ref;
	CHEROKEE_RWLOCK_UNLOCK (&PRIV(entry)->ref_rwlock);

	return ref;
}


/* Table atomic operations
 */

static cherokee_boolean_t
table_is_full (cherokee_mmap2_t *mmap2)
{
	size_t             len;
	cherokee_boolean_t full;

	CHEROKEE_RWLOCK_READER (&mmap2->table_rwlock);
	cherokee_table_len (mmap2->table, &len);
	full = (len >= mmap2->table_max_items);
	CHEROKEE_RWLOCK_UNLOCK (&mmap2->table_rwlock);

	return full;
}

static ret_t
table_add_entry (cherokee_mmap2_t *mmap2, cherokee_mmap2_entry_t *entry)
{
	ret_t ret;

	return_if_fail (PRIV(entry)->filename->buf != NULL, ret_error);

	CHEROKEE_RWLOCK_WRITER (&mmap2->table_rwlock);
	ret = cherokee_table_add (mmap2->table, PRIV(entry)->filename->buf, entry);
	mmap2->table_items_num++;
	CHEROKEE_RWLOCK_UNLOCK (&mmap2->table_rwlock);

	return ret;
}

static ret_t
table_del_entry (cherokee_mmap2_t *mmap2, cherokee_mmap2_entry_t *entry)
{
	ret_t ret;

	/* Sanity check
	 */
	if (cherokee_buffer_is_empty (PRIV(entry)->filename)) {
		SHOULDNT_HAPPEN;
		return ret_error;
	}

	/* It doesn't need mutex, see: cherokee_mmap2_clean_up()
	 */
	ret = cherokee_table_del (mmap2->table, PRIV(entry)->filename->buf, NULL);
	mmap2->table_items_num--;

	return ret;
}


/* Entry cleaning operations
 */

static ret_t
entry_clean (cherokee_mmap2_entry_t *entry)
{
	int re;

	if (entry->mmaped != NULL) { 
		re = munmap (entry->mmaped, entry->mmaped_len);
	}

	entry->mmaped     = NULL;
	entry->mmaped_len = 0;
	
	cherokee_buffer_make_empty (PRIV(entry)->filename);

	return (re == 0) ? ret_ok : ret_error;

}

/* Entry freeing operations
 */

static void
entry_free (cherokee_mmap2_entry_t *entry)
{	
	entry_clean (entry);
	cherokee_buffer_free (PRIV(entry)->filename);

	free (entry);
}


/* Mmap2 freeing up operations
 */

ret_t 
cherokee_mmap2_free (cherokee_mmap2_t *mmap2)
{
	cherokee_table_free2 (mmap2->table, (cherokee_table_free_item_t)entry_free);
	CHEROKEE_RWLOCK_DESTROY (&mmap2->table_rwlock);

	free (mmap2);
	return ret_ok;
}

ret_t 
cherokee_mmap2_clean (cherokee_mmap2_t *mmap2)
{
	CHEROKEE_RWLOCK_WRITER (&mmap2->table_rwlock);
	cherokee_table_clean2 (mmap2->table, (cherokee_table_free_item_t)entry_free);
	CHEROKEE_RWLOCK_UNLOCK (&mmap2->table_rwlock);

	return ret_ok;
}


ret_t 
cherokee_mmap2_unref_entry (cherokee_mmap2_t *mmap2, cherokee_mmap2_entry_t *entry)
{
	unsigned ref;

	/* Decrease the reference counter
	 */
	ref = entry_ref_dec (entry);
	if (ref < 0) return ret_error;

	return ret_ok;
}


/* Common methods
 */

ret_t 
cherokee_mmap2_new (cherokee_mmap2_t **mmap2)
{
	CHEROKEE_NEW_STRUCT(n,mmap2);

	cherokee_table_new (&n->table);
	CHEROKEE_RWLOCK_INIT (&n->table_rwlock, NULL);

	n->table_max_items = 20;  /* TODO! */
	n->table_items_num = 0;
	n->usage_times     = 0;

	*mmap2 = n;
	return ret_ok;
}


#ifdef HAVE_PTHREAD
static pthread_mutex_t cherokee_mmap2_get_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

ret_t 
cherokee_mmap2_get (cherokee_mmap2_t *mmap2, char *filename, cherokee_mmap2_entry_t **mmap_entry)
{
	ret_t                   ret;
	cherokee_mmap2_entry_t *new;

	return_if_fail (filename != NULL, ret_error);

	CHEROKEE_MUTEX_LOCK (&cherokee_mmap2_get_mutex);                         /* 1.- Adquire */

	/* Try to get filename from the cache
	 */
	CHEROKEE_RWLOCK_READER (&mmap2->table_rwlock);                           
	ret = cherokee_table_get (mmap2->table, filename, (void **)&new);
	mmap2->usage_times++;
	CHEROKEE_RWLOCK_UNLOCK (&mmap2->table_rwlock);                           
	
	/* Found!
	 */
	if (ret == ret_ok) {
		entry_ref_usage_inc (new);
		*mmap_entry = new;

		CHEROKEE_MUTEX_UNLOCK (&cherokee_mmap2_get_mutex);               /* 2.- Release */
		return ret_ok;
	}

	/* Is it full?
	 */
	if (table_is_full(mmap2)) {
		CHEROKEE_MUTEX_UNLOCK (&cherokee_mmap2_get_mutex);               /* 2.- Release */
 		return ret_deny;
	}

	/* Create a new entry object
	 */
	ret = entry_new (&new, filename);
	if (unlikely (ret != ret_ok)) goto error;

	/* Set mmap information
	 */
	ret = entry_set_mmap (new, filename);
	if (unlikely (ret != ret_ok)) goto error;

	/* There is a new entry that should be inserted in the table
	 */
	ret = table_add_entry (mmap2, new);
	if (unlikely (ret != ret_ok)) goto error;

	*mmap_entry = new;

	CHEROKEE_MUTEX_UNLOCK (&cherokee_mmap2_get_mutex);                       /* 2.- Release */
	return ret_ok;

error:
	if (new != NULL) {
		entry_free (new);
	}

	CHEROKEE_MUTEX_UNLOCK (&cherokee_mmap2_get_mutex);                       /* 2.- Release */
	return ret_error;
}


/* Clean up
 */

struct clean_up_params {
	cherokee_mmap2_t *mmap2;
	unsigned int      average;
	struct list_head  to_delete;
};

static int
mmap2_print_item (const char *key, void *value, void *param)
{
	struct clean_up_params *params = param;
	cherokee_mmap2_entry_t *entry  = MMAP2_ENTRY(value);

	printf ("entry: %s ref=%d used=%d\n", 
		PRIV(entry)->filename->buf, PRIV(entry)->ref, PRIV(entry)->usage);

	return true;
}

static int 
mmap2_clean_up_item (const char *key, void *value, void *param)
{
	struct clean_up_params *params = param;
	cherokee_mmap2_entry_t *entry  = MMAP2_ENTRY(value);
	unsigned int            ref;
	unsigned int            usage_times;

	/* Read the reference and usage counter and then 
	 * reset the usage one.
	 */
	ref = entry_ref_usage_get_and_reset (entry, &usage_times);

	/* If it is not being used at the moment and the usage rate 
	 * is under the average, remove it from the cache and the
	 * this mmap2 entry.
	 */
	if ((ref == 0) && (usage_times < params->average)) {

		/* Add this entry to the "to delete" list
		 */
		list_add (&PRIV(entry)->list_item, &params->to_delete);

		return true;
	}
	
	return true;
}


ret_t 
cherokee_mmap2_clean_up (cherokee_mmap2_t *mmap2)
{
	struct clean_up_params params;
	list_t *i, *tmp;


	/* Calculate the average entry usage and set the params
	 */
	if (mmap2->usage_times == 0) {
		return ret_ok;
	}

	/* Initialize the parameter list
	 */
	params.average = mmap2->table_items_num / mmap2->usage_times;
	params.mmap2   = mmap2;
	INIT_LIST_HEAD(&params.to_delete);

	/* Clean the less used entries
	 */
	CHEROKEE_RWLOCK_WRITER (&mmap2->table_rwlock);                    /* 1.- lock */

#if 0
	/* Print entries for debugging
	 */
	cherokee_table_while (mmap2->table,        /* table obj */
			      mmap2_print_item,    /* func */
			      &params,             /* param */
			      NULL,                /* key */
			      NULL);               /* value */
	printf ("\n");
#endif

	cherokee_table_while (mmap2->table,        /* table obj */
			      mmap2_clean_up_item, /* func */
			      &params,             /* param */
			      NULL,                /* key */
			      NULL);               /* value */

	/* Free the entries in the to_delete list
	 */
	list_for_each_safe (i, tmp, &params.to_delete) {
		cherokee_mmap2_entry_priv_t *entry;

		entry = list_entry (i, cherokee_mmap2_entry_priv_t, list_item);

		table_del_entry (mmap2, MMAP2_ENTRY(entry));
		entry_free (MMAP2_ENTRY(entry));		
	}

	/* Reset the mmap2 table usage counter
	 */
	mmap2->usage_times = 0;

	CHEROKEE_RWLOCK_UNLOCK (&mmap2->table_rwlock);                    /* 2.- release */


	return ret_ok;
}


