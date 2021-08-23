/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __FILE_LIST
#define __FILE_LIST

#include <linux/list.h>

#define FILE_LIST_FLAG_SAFE	(1 << 0)
#define FILE_LIST_FLAG_READBACK	(1 << 1)
#define FILE_LIST_FLAG_CREATE	(1 << 2)
#define FILE_LIST_FLAG_UBI	(1 << 3)

struct file_list_entry {
	char *name;
	char *filename;
	unsigned long flags;
	struct list_head list;
};

struct file_list {
	struct list_head list;
	int num_entries;
};

struct file_list *file_list_parse(const char *str);
struct file_list *file_list_parse_null(const char *str);
char *file_list_to_str(const struct file_list *files);
void file_list_free(struct file_list *);

int file_list_add_entry(struct file_list *files, const char *name, const char *filename,
			unsigned long flags);

struct file_list *file_list_dup(struct file_list *old);

int file_list_detect_all(const struct file_list *files);

struct file_list_entry *file_list_entry_by_name(struct file_list *files, const char *name);

#define file_list_for_each_entry(files, entry) \
	list_for_each_entry(entry, &files->list, list)

static inline bool file_list_empty(struct file_list *files)
{
	return !files || !files->num_entries;
}

#endif /* __FILE_LIST */
