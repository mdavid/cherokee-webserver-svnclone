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
#include "access.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <limits.h>

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>  
#endif

#ifndef AF_INET6
# define AF_INET6 10
#endif


typedef enum { 
	ipv4 = AF_INET,
	ipv6 = AF_INET6
} ip_type_t;


typedef union {
	struct in_addr  ip4;
#ifdef HAVE_IPV6
	struct in6_addr ip6;
#endif
} ip_t;

typedef struct {
	struct list_head node;
	
	ip_type_t type;
	ip_t      ip;
} ip_item_t;

typedef struct {
	ip_item_t base;
	ip_t      mask;
} subnet_item_t;

#define IP_NODE(x)     ((ip_item_t *)(x))
#define SUBNET_NODE(x) ((subnet_item_t *)(x))




static ip_item_t *
new_ip (void) 
{
	ip_item_t *n = malloc (sizeof(ip_item_t));
	if (n == NULL) return NULL;

	INIT_LIST_HEAD((list_t*)n);

	return n;
}

static ret_t
free_ip (ip_item_t *ip)
{
	free (ip);
	return ret_ok;
}

static subnet_item_t *
new_subnet (void) 
{
	subnet_item_t *n = malloc (sizeof(subnet_item_t));
	if (n == NULL) return NULL;

	INIT_LIST_HEAD((list_t*)n);
	return n;
}



ret_t 
cherokee_access_new (cherokee_access_t **entry)
{
	CHEROKEE_NEW_STRUCT (n, access);

	INIT_LIST_HEAD(&n->list_ips);
	INIT_LIST_HEAD(&n->list_subnets);
	
	*entry = n;
	return ret_ok;
}



static void
print_ip (ip_type_t type, ip_t *ip)
{
	CHEROKEE_TEMP(dir,255);

#ifdef HAVE_INET_PTON
# ifdef HAVE_IPV6
	if (type == ipv6) {
		printf ("%s", inet_ntop (AF_INET6, ip, dir, dir_size));
		return;
	}
# endif
	if (type == ipv4) {
		printf ("%s", inet_ntop (AF_INET, ip, dir, dir_size));
		return;
	}
#else
	printf ("%s", inet_ntoa (ip->ip4));
#endif
}


ret_t 
cherokee_access_free (cherokee_access_t *entry)
{
	list_t *i, *tmp;
	
	/* Free the IP list items
	 */
	list_for_each_safe (i, tmp, (list_t*)&entry->list_ips) {
		list_del (i);
		free (i);
	}

	/* Free the Subnet list items
	 */
	list_for_each_safe (i, tmp, (list_t*)&entry->list_subnets) {
		list_del (i);
		free (i);
	}

	free (entry);
	return ret_ok;
}


static ret_t
parse_ip (char *ip, ip_item_t *n)
{
	int ok;

	/* Test if it is a IPv4 or IPv6 connection
	 */
	n->type = ((strchr (ip, ':') != NULL) || 
		   (strlen(ip) > 15)) ? ipv6 : ipv4;
	
#ifndef HAVE_IPV6
	if (n->type == ipv6) {
		return ret_error;
	}
#endif

	/* Parse the IP string
	 */
#ifdef HAVE_INET_PTON
	ok = (inet_pton (n->type, ip, &n->ip) > 0);

# ifdef HAVE_IPV6
	if (n->type == ipv6) {
		if (IN6_IS_ADDR_V4MAPPED (&(n->ip).ip6)) {
			PRINT_ERROR ("This IP '%s' is IPv6-mapped IPv6 address.  "
				     "Please, specify IPv4 in a.b.c.d style instead "
				     "of ::ffff:a.b.c.d style\n", ip);
			return ret_error;
		}
	}
# endif /* HAVE_IPV6 */

#else
	ok = (inet_aton (ip, &n->ip) != 0);
#endif

	return (ok) ? ret_ok : ret_error;
}
 

static ret_t
parse_netmask (char *netmask, subnet_item_t *n)
{
	int num;

	/* IPv6 or IPv4 Mask
	 * Eg: 255.255.0.0
	 */
	if ((strchr (netmask, ':') != NULL) ||
	    (strchr (netmask, '.') != NULL))
	{
		int ok;

#ifdef HAVE_INET_PTON
		ok = (inet_pton (IP_NODE(n)->type, netmask, &n->mask) > 0);
#else
		ok = (inet_aton (netmask, &n->mask) != 0);
#endif		
		return (ok) ? ret_ok : ret_error;
	}


	/* Lenght mask
	 * Eg: 16
	 */
	if (strlen(netmask) > 3) {
		return ret_error;
	}

	num = strtol(netmask, NULL, 10);
	if (num <= LONG_MIN) 
		return ret_error;

	/* Sanity checks
	 */
	if ((IP_NODE(n)->type == ipv4) && (num >  32)) 
		return ret_error;

	if ((IP_NODE(n)->type == ipv6) && (num > 128)) 
		return ret_error;

#ifdef HAVE_IPV6
	if (num > 128) {
		return ret_error;
	}

	/* Special case
	 */
	if (num == 128) {
		int i;

		for (i=0; i<16; i++) {
			n->mask.ip6.s6_addr[i] = 0;
		}

		return ret_ok;
	}

	if (IP_NODE(n)->type == ipv6) {
		int i, j, jj;
		unsigned char mask    = 0;
		unsigned char maskbit = 0x80L;

		j  = (int) num / 8;
		jj = num % 8;

		for (i=0; i<j; i++) {
			n->mask.ip6.s6_addr[i] = 0xFF;
		}

		while (jj--) {
			mask |= maskbit;
			maskbit >>= 1;
		}
		n->mask.ip6.s6_addr[j] = mask;

		return ret_ok;
	} else
#endif
		n->mask.ip4.s_addr = (in_addr_t) htonl(~0L << (32 - num));

	return ret_ok;
}


static ret_t 
cherokee_access_add_ip (cherokee_access_t *entry, char *ip)
{
	ret_t ret;
	ip_item_t *n;

	n = new_ip();
	if (n == NULL) return ret_error;

	ret = parse_ip (ip, n);
	if (ret < ret_ok) {
		PRINT_ERROR ("IP address '%s' seems to be invalid\n", ip);

		free_ip(n);
		return ret;
	}

	list_add ((list_t *)n, &entry->list_ips);

	return ret;
}


static ret_t 
cherokee_access_add_subnet (cherokee_access_t *entry, char *subnet)
{
	char *slash;
	char *mask;
	ret_t ret;
	subnet_item_t *n;
	CHEROKEE_NEW (ip,buffer);

	/* Split the string
	 */
	slash = strpbrk (subnet, "/\\");
	if (slash == NULL) return ret_error;

	mask = slash +1;
	cherokee_buffer_add (ip, subnet, mask-subnet-1);
	
	/* Create the new list object
	 */
	n = new_subnet();
	if (n == NULL) return ret_error;

	list_add ((list_t *)n, &entry->list_subnets);

	/* Parse the IP
	 */
	ret = parse_ip (ip->buf, IP_NODE(n));
	if (ret < ret_ok) {
		PRINT_ERROR ("IP address '%s' seems to be invalid\n", ip->buf);
		goto error;
	}

	/* Parse the Netmask
	 */
	ret = parse_netmask (mask, n);
	if (ret < ret_ok) {
		PRINT_ERROR ("Netmask '%s' seems to be invalid\n", mask);
		goto error;	
	}

	cherokee_buffer_free (ip);
	return ret_ok;

error:
	cherokee_buffer_free (ip);
	return ret_error;
}


ret_t 
cherokee_access_add (cherokee_access_t *entry, char *ip_or_subnet)
{
	char *slash;
	int   mask;

	slash = strpbrk(ip_or_subnet, "/\\");

	/* Add a single IP address
	 */
	if (slash == NULL) {
		return cherokee_access_add_ip (entry, ip_or_subnet);
	}
	
	/* Special cases of subnets
	 */
	mask = atoi(slash+1);

	if ((strchr(ip_or_subnet, ':') != NULL) && (mask == 128)) {
		return cherokee_access_add_ip (entry, ip_or_subnet);		
	}

	if ((strchr(ip_or_subnet, '.') != NULL) && (mask == 32)) {
		return cherokee_access_add_ip (entry, ip_or_subnet);		
	}

	/* Add a subnet
	 */
	return cherokee_access_add_subnet (entry, ip_or_subnet);		
}


ret_t 
cherokee_access_print_debug (cherokee_access_t *entry)
{
	list_t *i;

	printf ("IPs: ");
	list_for_each (i, (list_t*)&entry->list_ips) {
		print_ip (IP_NODE(i)->type, &IP_NODE(i)->ip);
		printf(" ");
	}
	printf("\n");

	printf ("Subnets: ");
	list_for_each (i, (list_t*)&entry->list_subnets) {
		print_ip (IP_NODE(i)->type, &IP_NODE(i)->ip);
		printf("/");
		print_ip (IP_NODE(i)->type, &SUBNET_NODE(i)->mask);
		printf(" ");
	}
	printf("\n");

	return ret_ok;
}


ret_t 
cherokee_access_ip_match (cherokee_access_t *entry, cherokee_socket_t *sock)
{
	int     re;
	list_t *i;

	/* Check in the IP list
	 */
	list_for_each (i, (list_t*)&entry->list_ips) {
		if (SOCKET_AF(sock) == IP_NODE(i)->type) {
			switch (IP_NODE(i)->type) {
			case ipv4:
				re = memcmp (&SOCKET_ADDR_IPv4(sock)->sin_addr, &IP_NODE(i)->ip, 4);
				break;
#ifdef HAVE_IPV6
			case ipv6:
				re = memcmp (&SOCKET_ADDR_IPv6(sock)->sin6_addr, &IP_NODE(i)->ip, 16);
				/*
				printf ("family=%d, ipv6=%d\n", SOCKET_ADDR_IPv6(sock)->sin6_family, ipv6);
				printf ("port=%d\n",            SOCKET_ADDR_IPv6(sock)->sin6_port);
				printf ("remote "); print_ip(ipv6, &SOCKET_ADDRESS_IPv6(sock)); printf ("\n");
				printf ("lista  "); print_ip(ipv6, &IP_NODE(i)->ip); printf ("\n");
				printf ("re = %d\n", re);
				*/
				break;
#endif
			default:
				SHOULDNT_HAPPEN;
				return ret_error;
			}

			if (re == 0) {
				return ret_ok;
			}
		}
	}


	/* Check in the Subnets list
	 */
	list_for_each (i, (list_t*)&entry->list_subnets) {
		int j;
		ip_t masqued_remote, masqued_list;
		
		if (SOCKET_AF(sock) == IP_NODE(i)->type) {
			switch (IP_NODE(i)->type) {
			case ipv4:
				masqued_list.ip4.s_addr   = (IP_NODE(i)->ip.ip4.s_addr &
							     SUBNET_NODE(i)->mask.ip4.s_addr);
				masqued_remote.ip4.s_addr = (SOCKET_ADDR_IPv4(sock)->sin_addr.s_addr & 
							     SUBNET_NODE(i)->mask.ip4.s_addr);

				if (masqued_remote.ip4.s_addr == masqued_list.ip4.s_addr) {
					return ret_ok;
				}

				break;
#ifdef HAVE_IPV6
			case ipv6:
			{
				cherokee_boolean_t equal = true;

//				printf ("remote IPv6: ");
//				print_ip (ipv6, &SOCKET_ADDR_IPv6(sock)->sin6_addr);
//				printf ("\n");

				for (j=0; j<16; j++) {
					masqued_list.ip6.s6_addr[j] = (
						IP_NODE(i)->ip.ip6.s6_addr[j] &
						SUBNET_NODE(i)->mask.ip6.s6_addr[j]);
					masqued_remote.ip6.s6_addr[j] = (
						SOCKET_ADDR_IPv6(sock)->sin6_addr.s6_addr[j] &
						SUBNET_NODE(i)->mask.ip6.s6_addr[j]);

					if (masqued_list.ip6.s6_addr[j] !=
					    masqued_remote.ip6.s6_addr[j])
					{
						equal = false;
						break;
					}
				}

				if (equal == true) {
					return ret_ok;
				}
				break;
			}
#endif
			default:
				SHOULDNT_HAPPEN;
				return ret_error;
			}
		}
	}

	return ret_not_found;
}
