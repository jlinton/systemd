/*
 * namedev.c
 *
 * Userspace devfs
 *
 * Copyright (C) 2003 Greg Kroah-Hartman <greg@kroah.com>
 *
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation version 2 of the License.
 * 
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	General Public License for more details.
 * 
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include "list.h"
#include "udev.h"
#include "udev_version.h"
#include "namedev.h"

#define TYPE_LABEL	"LABEL"
#define TYPE_NUMBER	"NUMBER"
#define TYPE_TOPOLOGY	"TOPOLOGY"
#define TYPE_REPLACE	"REPLACE"

enum config_type {
	KERNEL_NAME	= 0,	/* must be 0 to let memset() default to this value */
	LABEL		= 1,
	NUMBER		= 2,
	TOPOLOGY	= 3,
	REPLACE		= 4,
};

#define BUS_SIZE	30
#define FILE_SIZE	50
#define VALUE_SIZE	100
#define ID_SIZE		50
#define PLACE_SIZE	50
#define NAME_SIZE	100
#define OWNER_SIZE	30
#define GROUP_SIZE	30


struct config_device {
	struct list_head node;

	enum config_type type;

	char bus[BUS_SIZE];
	char sysfs_file[FILE_SIZE];
	char sysfs_value[VALUE_SIZE];
	char id[ID_SIZE];
	char place[PLACE_SIZE];
	char kernel_name[NAME_SIZE];
	
	/* what to set the device to */
	int mode;
	char name[NAME_SIZE];
	char owner[OWNER_SIZE];
	char group[GROUP_SIZE];
};


static LIST_HEAD(config_device_list);

#define copy_var(a, b, var)		\
	if (b->var)			\
		b->var = a->var;

#define copy_string(a, b, var)		\
	if (strlen(b->var))		\
		strcpy(b->var, a->var);

static int add_dev(struct config_device *new_dev)
{
	struct list_head *tmp;
	struct config_device *tmp_dev;

	/* loop through the whole list of devices to see if we already have
	 * this one... */
	list_for_each(tmp, &config_device_list) {
		struct config_device *dev = list_entry(tmp, struct config_device, node);
		if (strcmp(dev->name, new_dev->name) == 0) {
			/* the same, copy the new info into this structure */
			copy_var(new_dev, dev, type);
			copy_var(new_dev, dev, mode);
			copy_string(new_dev, dev, bus);
			copy_string(new_dev, dev, sysfs_file);
			copy_string(new_dev, dev, sysfs_value);
			copy_string(new_dev, dev, id);
			copy_string(new_dev, dev, place);
			copy_string(new_dev, dev, kernel_name);
			copy_string(new_dev, dev, owner);
			copy_string(new_dev, dev, group);
			return 0;
		}
	}

	/* not found, lets create a new structure, and add it to the list */
	tmp_dev = malloc(sizeof(*tmp_dev));
	if (!tmp_dev)
		return -ENOMEM;
	memcpy(tmp_dev, new_dev, sizeof(*tmp_dev));
	list_add(&tmp_dev->node, &config_device_list);
	return 0;
}

static int get_value(const char *left, char **orig_string, char **ret_string)
{
	char *temp;
	char *string = *orig_string;

	/* eat any whitespace */
	while (isspace(*string))
		++string;

	/* split based on '=' */
	temp = strsep(&string, "=");
	if (strcasecmp(temp, left) == 0) {
		/* got it, now strip off the '"' */
		while (isspace(*string))
			++string;
		if (*string == '"')
			++string;
		temp = strsep(&string, "\"");
		*ret_string = temp;
		*orig_string = string;
		return 0;
	}
	return -ENODEV;
}
	

static int namedev_init_config(void)
{
	char filename[255];
	char line[255];
	char *temp;
	char *temp2;
	char *temp3;
	FILE *fd;
	int retval = 0;
	struct config_device dev;

	strcpy(filename, NAMEDEV_CONFIG_ROOT NAMEDEV_CONFIG_FILE);
	dbg("opening %s to read as permissions config", filename);
	fd = fopen(filename, "r");
	if (fd == NULL) {
		dbg("Can't open %s", filename);
		return -ENODEV;
	}

	/* loop through the whole file */
	while (1) {
		/* get a line */
		temp = fgets(line, sizeof(line), fd);
		if (temp == NULL)
			break;

		dbg("read %s", temp);

		/* eat the whitespace at the beginning of the line */
		while (isspace(*temp))
			++temp;

		/* no more line? */
		if (*temp == 0x00)
			continue;

		/* see if this is a comment */
		if (*temp == COMMENT_CHARACTER)
			continue;

		memset(&dev, 0x00, sizeof(dev));

		/* parse the line */
		temp2 = strsep(&temp, ",");
		if (strcasecmp(temp2, TYPE_LABEL) == 0) {
			/* label type */
			dev.type = LABEL;

			/* BUS="bus" */
			retval = get_value("BUS", &temp, &temp3);
			if (retval)
				continue;
			strcpy(dev.bus, temp3);
			dbg("LABEL name = %s, bus = %s", dev.name, dev.bus);
		}

		if (strcasecmp(temp2, TYPE_NUMBER) == 0) {
			/* number type */
			dev.type = NUMBER;

			/* BUS="bus" */
			retval = get_value("BUS", &temp, &temp3);
			if (retval)
				continue;
			strcpy(dev.bus, temp3);
			dbg("NUMBER name = %s, bus = %s", dev.name, dev.bus);
		}

		if (strcasecmp(temp2, TYPE_TOPOLOGY) == 0) {
			/* number type */
			dev.type = TOPOLOGY;

			/* BUS="bus" */
			retval = get_value("BUS", &temp, &temp3);
			if (retval)
				continue;
			strcpy(dev.bus, temp3);
			dbg("TOPOLOGY name = %s, bus = %s", dev.name, dev.bus);
		}

		if (strcasecmp(temp2, TYPE_REPLACE) == 0) {
			/* number type */
			dev.type = REPLACE;

			/* KERNEL="kernel_name" */
			retval = get_value("KERNEL", &temp, &temp3);
			if (retval)
				continue;
			strcpy(dev.kernel_name, temp3);

			/* NAME="new_name" */
			temp2 = strsep(&temp, ",");
			retval = get_value("NAME", &temp, &temp3);
			if (retval)
				continue;
			strcpy(dev.name, temp3);
			dbg("REPLACE name = %s, kernel_name = %s", dev.name, dev.kernel_name);
		}

		retval = add_dev(&dev);
		if (retval) {
			dbg("add_dev returned with error %d", retval);
			goto exit;
		}
	}

exit:
	fclose(fd);
	return retval;
}	


static int namedev_init_permissions(void)
{
	char filename[255];
	char line[255];
	char *temp;
	char *temp2;
	FILE *fd;
	int retval = 0;
	struct config_device dev;

	strcpy(filename, NAMEDEV_CONFIG_ROOT NAMEDEV_CONFIG_PERMISSION_FILE);
	dbg("opening %s to read as permissions config", filename);
	fd = fopen(filename, "r");
	if (fd == NULL) {
		dbg("Can't open %s", filename);
		return -ENODEV;
	}

	/* loop through the whole file */
	while (1) {
		/* get a line */
		temp = fgets(line, sizeof(line), fd);
		if (temp == NULL)
			break;

		dbg("read %s", temp);

		/* eat the whitespace at the beginning of the line */
		while (isspace(*temp))
			++temp;

		/* no more line? */
		if (*temp == 0x00)
			continue;

		/* see if this is a comment */
		if (*temp == COMMENT_CHARACTER)
			continue;

		memset(&dev, 0x00, sizeof(dev));

		/* parse the line */
		temp2 = strsep(&temp, ":");
		strncpy(dev.name, temp2, sizeof(dev.name));

		temp2 = strsep(&temp, ":");
		strncpy(dev.owner, temp2, sizeof(dev.owner));

		temp2 = strsep(&temp, ":");
		strncpy(dev.group, temp2, sizeof(dev.owner));

		dev.mode = strtol(temp, NULL, 8);

		dbg("name = %s, owner = %s, group = %s, mode = %x", dev.name, dev.owner, dev.group, dev.mode);
		retval = add_dev(&dev);
		if (retval) {
			dbg("add_dev returned with error %d", retval);
			goto exit;
		}
	}

exit:
	fclose(fd);
	return retval;
}	



int namedev_init(void)
{
	int retval;

	retval = namedev_init_config();
	if (retval)
		return retval;

	retval = namedev_init_permissions();
	if (retval)
		return retval;

	return retval;
}

