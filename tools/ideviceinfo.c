/*
 * ideviceinfo.c
 * Simple utility to show information about an attached device
 *
 * Copyright (c) 2009 Martin Szulecki All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA 
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#define FORMAT_KEY_VALUE 1
#define FORMAT_XML 2

static const char *domains[] = {
	"com.apple.disk_usage",
	"com.apple.mobile.battery",
/* FIXME: For some reason lockdownd segfaults on this, works sometimes though 
	"com.apple.mobile.debug",. */
	"com.apple.xcode.developerdomain",
	"com.apple.international",
	"com.apple.mobile.data_sync",
	"com.apple.mobile.tethered_sync",
	"com.apple.mobile.mobile_application_usage",
	"com.apple.mobile.backup",
	"com.apple.mobile.nikita",
	"com.apple.mobile.restriction",
	"com.apple.mobile.user_preferences",
	"com.apple.mobile.sync_data_class",
	"com.apple.mobile.software_behavior",
	"com.apple.mobile.iTunes.SQLMusicLibraryPostProcessCommands",
	"com.apple.mobile.iTunes.accessories",
	"com.apple.fairplay",
	"com.apple.iTunes",
	"com.apple.mobile.iTunes.store",
	"com.apple.mobile.iTunes",
	NULL
};

static int indent_level = 0;

static int is_domain_known(char *domain)
{
	int i = 0;
	while (domains[i] != NULL) {
		if (strstr(domain, domains[i++])) {
			return 1;
		}
	}
	return 0;
}

static void plist_node_to_string(plist_t node);

static void plist_array_to_string(plist_t node)
{
	/* iterate over items */
	int i, count;
	plist_t subnode = NULL;

	count = plist_array_get_size(node);

	for (i = 0; i < count; i++) {
		subnode = plist_array_get_item(node, i);
		printf("%*s", indent_level, "");
		printf("%d: ", i);
		plist_node_to_string(subnode);
	}
}

static void plist_dict_to_string(plist_t node)
{
	/* iterate over key/value pairs */
	plist_dict_iter it = NULL;

	char* key = NULL;
	plist_t subnode = NULL;
	plist_dict_new_iter(node, &it);
	plist_dict_next_item(node, it, &key, &subnode);
	while (subnode)
	{
		printf("%*s", indent_level, "");
		printf("%s", key);
		if (plist_get_node_type(subnode) == PLIST_ARRAY)
			printf("[%d]: ", plist_array_get_size(subnode));
		else
			printf(": ");
		free(key);
		key = NULL;
		plist_node_to_string(subnode);
		plist_dict_next_item(node, it, &key, &subnode);
	}
	free(it);
}

static void plist_node_to_string(plist_t node)
{
	char *s = NULL;
	char *data = NULL;
	double d;
	uint8_t b;
	uint64_t u = 0;
	GTimeVal tv = { 0, 0 };

	plist_type t;

	if (!node)
		return;

	t = plist_get_node_type(node);

	switch (t) {
	case PLIST_BOOLEAN:
		plist_get_bool_val(node, &b);
		printf("%s\n", (b ? "true" : "false"));
		break;

	case PLIST_UINT:
		plist_get_uint_val(node, &u);
		printf("%llu\n", (long long)u);
		break;

	case PLIST_REAL:
		plist_get_real_val(node, &d);
		printf("%f\n", d);
		break;

	case PLIST_STRING:
		plist_get_string_val(node, &s);
		printf("%s\n", s);
		free(s);
		break;

	case PLIST_KEY:
		plist_get_key_val(node, &s);
		printf("%s: ", s);
		free(s);
		break;

	case PLIST_DATA:
		plist_get_data_val(node, &data, &u);
		s = g_base64_encode((guchar *)data, u);
		free(data);
		printf("%s\n", s);
		g_free(s);
		break;

	case PLIST_DATE:
		plist_get_date_val(node, (int32_t*)&tv.tv_sec, (int32_t*)&tv.tv_usec);
		s = g_time_val_to_iso8601(&tv);
		printf("%s\n", s);
		free(s);
		break;

	case PLIST_ARRAY:
		printf("\n");
		indent_level++;
		plist_array_to_string(node);
		indent_level--;
		break;

	case PLIST_DICT:
		printf("\n");
		indent_level++;
		plist_dict_to_string(node);
		indent_level--;
		break;

	default:
		break;
	}
}

static void print_usage(int argc, char **argv)
{
	int i = 0;
	char *name = NULL;
	
	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
	printf("Show information about a connected iPhone/iPod Touch.\n\n");
	printf("  -d, --debug\t\tenable communication debugging\n");
	printf("  -s, --simple\t\tuse a simple connection to avoid auto-pairing with the device\n");
	printf("  -u, --uuid UUID\ttarget specific device by its 40-digit device UUID\n");
	printf("  -q, --domain NAME\tset domain of query to NAME. Default: None\n");
	printf("  -k, --key NAME\tonly query key specified by NAME. Default: All keys.\n");
	printf("  -x, --xml\t\toutput information as xml plist instead of key/value pairs\n");
	printf("  -h, --help\t\tprints usage information\n");
	printf("\n");
	printf("  Known domains are:\n\n");
	while (domains[i] != NULL) {
		printf("  %s\n", domains[i++]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	lockdownd_client_t client = NULL;
	idevice_t phone = NULL;
	idevice_error_t ret = IDEVICE_E_UNKNOWN_ERROR;
	int i;
	int simple = 0;
	int format = FORMAT_KEY_VALUE;
	char uuid[41];
	char *domain = NULL;
	char *key = NULL;
	char *xml_doc = NULL;
	uint32_t xml_length;
	plist_t node = NULL;
	plist_type node_type;
	uuid[0] = 0;

	/* parse cmdline args */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
			idevice_set_debug_level(1);
			continue;
		}
		else if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--uuid")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) != 40)) {
				print_usage(argc, argv);
				return 0;
			}
			strcpy(uuid, argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--domain")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) < 4)) {
				print_usage(argc, argv);
				return 0;
			}
			if (!is_domain_known(argv[i])) {
				fprintf(stderr, "WARNING: Sending query with unknown domain \"%s\".\n", argv[i]);
			}
			domain = strdup(argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-k") || !strcmp(argv[i], "--key")) {
			i++;
			if (!argv[i] || (strlen(argv[i]) <= 1)) {
				print_usage(argc, argv);
				return 0;
			}
			key = strdup(argv[i]);
			continue;
		}
		else if (!strcmp(argv[i], "-x") || !strcmp(argv[i], "--xml")) {
			format = FORMAT_XML;
			continue;
		}
		else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--simple")) {
			simple = 1;
			continue;
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_usage(argc, argv);
			return 0;
		}
		else {
			print_usage(argc, argv);
			return 0;
		}
	}

	if (uuid[0] != 0) {
		ret = idevice_new(&phone, uuid);
		if (ret != IDEVICE_E_SUCCESS) {
			printf("No device found with uuid %s, is it plugged in?\n", uuid);
			return -1;
		}
	}
	else
	{
		ret = idevice_new(&phone, NULL);
		if (ret != IDEVICE_E_SUCCESS) {
			printf("No device found, is it plugged in?\n");
			return -1;
		}
	}

	if (LOCKDOWN_E_SUCCESS != (simple ?
			lockdownd_client_new(phone, &client, "ideviceinfo"):
			lockdownd_client_new_with_handshake(phone, &client, "ideviceinfo"))) {
		idevice_free(phone);
		return -1;
	}

	/* run query and output information */
	if(lockdownd_get_value(client, domain, key, &node) == LOCKDOWN_E_SUCCESS) {
		if (node) {
			switch (format) {
			case FORMAT_XML:
				plist_to_xml(node, &xml_doc, &xml_length);
				printf("%s", xml_doc);
				free(xml_doc);
				break;
			case FORMAT_KEY_VALUE:
				node_type = plist_get_node_type(node);
				if (node_type == PLIST_DICT) {
					plist_dict_to_string(node);
				} else if (node_type == PLIST_ARRAY) {
					plist_array_to_string(node);
					break;
				}
			default:
				if (key != NULL)
					plist_node_to_string(node);
			break;
			}
			plist_free(node);
			node = NULL;
		}
	}

	if (domain != NULL)
		free(domain);
	lockdownd_client_free(client);
	idevice_free(phone);

	return 0;
}

