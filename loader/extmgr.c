/*
 * Copyright (C) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_EXTMGR)
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <zephyr/data/json.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/fs_loader.h>
#include <zephyr/logging/log.h>
#include "extmgr.h"

LOG_MODULE_REGISTER(extmgr, CONFIG_EXTMGR_LOG_LEVEL);

#define EXT_FLAG_BOOT (1 << 0) /* Load at boot */

#define MAX_EXTENSIONS    4
#define MANIFEST_PATH     "/storage/manifest.json"
#define MANIFEST_MAX_SIZE 2048

/* For symbol lookups */
struct find_sym_arg {
	const char *sym;
	const void *addr;
};

/* Extension entry data */
struct ext_data {
	struct llext *ext;
	llext_entry_fn_t entry;
	struct k_thread thread;
	uint32_t flags;
	char name[32];
	char path[64];
};

/* Manifest entry structure for JSON parsing */
struct manifest_entry {
	const char *name;
	const char *path;
	uint32_t flags;
};

static const struct json_obj_descr manifest_entry_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct manifest_entry, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct manifest_entry, path, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct manifest_entry, flags, JSON_TOK_NUMBER),
};

static size_t ext_count;
static struct ext_data ext_all[MAX_EXTENSIONS];
static K_THREAD_STACK_ARRAY_DEFINE(ext_stacks, MAX_EXTENSIONS, 4096);


static void ext_thread_entry(void *arg0, void *arg1, void *arg2) {
	struct ext_data *data = arg0;
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);

	llext_bootstrap(data->ext, data->entry, NULL);

	/* Extension exited, clean up */
	LOG_INF("%s exited", data->name);
	llext_unload(&data->ext);
	data->ext = NULL;
}

static int load_extension(struct ext_data *data) {
	struct llext_fs_loader fs_ldr = LLEXT_FS_LOADER(data->path);
	struct llext_load_param ldr_parm = LLEXT_LOAD_PARAM_DEFAULT;

	int res = llext_load(&fs_ldr.loader, data->name, &data->ext, &ldr_parm);
	if (res) {
		LOG_ERR("failed to load %s: %d", data->path, res);
		return res;
	}

	data->entry = (llext_entry_fn_t)llext_find_sym(&data->ext->exp_tab, "main");
	if (!data->entry) {
		LOG_INF("loaded library %s", data->name);
		return 0;
	}

	size_t idx = data - ext_all;

	k_thread_create(&data->thread, ext_stacks[idx],
		K_THREAD_STACK_SIZEOF(ext_stacks[idx]),
		ext_thread_entry, data, NULL, NULL,
		CONFIG_MAIN_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(&data->thread, data->name);

	return 0;
}

int extmgr_init(void) {
	int ret = 0;
	struct fs_file_t file;
	struct fs_dirent dirent;

	ret = fs_stat(MANIFEST_PATH, &dirent);
	if (ret) {
		LOG_ERR("manifest not found (%d)", ret);
		return ret;
	}

	if (dirent.size > MANIFEST_MAX_SIZE) {
		LOG_ERR("manifest too large (%zu)", dirent.size);
		return -ENOMEM;
	}

	char *buf = malloc(dirent.size + 1);
	if (!buf) {
		LOG_ERR("alloc failed");
		return -ENOMEM;
	}

	fs_file_t_init(&file);
	ret = fs_open(&file, MANIFEST_PATH, FS_O_READ);
	if (ret) {
		LOG_ERR("open failed (%d)", ret);
		goto exit_cleanup;
	}

	ssize_t bytes = fs_read(&file, buf, dirent.size);
	fs_close(&file);

	if (bytes < 0) {
		LOG_ERR("read failed (%zd)", bytes);
		goto exit_cleanup;
	}
	buf[bytes] = '\0';

	struct json_obj json;
	ret = json_arr_separate_object_parse_init(&json, buf, bytes);
	if (ret) {
		LOG_ERR("json parse failed (%d)", ret);
		goto exit_cleanup;
	}

	/* Parse manifest and populate ext_all[] */
	struct manifest_entry entry;
	while (ext_count < MAX_EXTENSIONS) {
		memset(&entry, 0, sizeof(entry));
		ret = json_arr_separate_parse_object(&json, manifest_entry_descr,
			ARRAY_SIZE(manifest_entry_descr), &entry);
		if (ret == 0) {
			break;
		}
		if (ret < 0) {
			LOG_ERR("entry parse failed (%d)", ret);
			break;
		}

		if (!entry.path || !entry.name) {
			LOG_WRN("entry missing path or name");
			continue;
		}

		struct ext_data *data = &ext_all[ext_count++];

		data->ext = NULL;
		data->flags = entry.flags;
		strncpy(data->name, entry.name, sizeof(data->name) - 1);
		strncpy(data->path, entry.path, sizeof(data->path) - 1);

		LOG_INF("manifest: %s [%s]", data->name,
			(data->flags & EXT_FLAG_BOOT) ? "BOOT" : "DEFERRED");

		if (data->flags & EXT_FLAG_BOOT) {
			load_extension(data);
		}
	}

exit_cleanup:
	free(buf);
	return ret;
}

void extmgr_deinit(void) {
	for (size_t i = 0; i < ext_count; i++) {
		extmgr_unload(&ext_all[i]);
	}
	ext_count = 0;

	extern struct k_heap llext_heap;
	k_heap_init(&llext_heap, llext_heap.heap.init_mem, llext_heap.heap.init_bytes);
}

void *extmgr_load(const char *name) {
    // Search for preloaded extension
	for (size_t i = 0; i < ext_count; i++) {
		struct ext_data *data = &ext_all[i];
		if (data->ext == NULL && strcmp(data->name, name) == 0) {
			if (load_extension(data) == 0) {
				return data;
			}
			return NULL;
		}
	}

    // If not, try loading from path
    if (ext_count < MAX_EXTENSIONS) {
        struct ext_data *data = &ext_all[ext_count];
        data->flags = 0;
        data->ext = NULL;
        strncpy(data->name, name, sizeof(data->name) - 1);
        strncpy(data->path, name, sizeof(data->path) - 1);

        LOG_INF("loading: %s ", data->name);
        if (load_extension(data) == 0) {
            ext_count++;
            return data;
        }
        return NULL;
    }

	LOG_WRN("extension %s not found", name);
	return NULL;
}

void extmgr_unload(void *handle) {
	struct ext_data *data = handle;
	if (!data || !data->ext) {
		return;
	}

	if (data->entry) {
		k_thread_abort(&data->thread);
	}

	llext_unload(&data->ext);
	data->ext = NULL;
}

static int find_sym_iterate(struct llext *ext, void *arg) {
	struct find_sym_arg *fsarg = arg;

	fsarg->addr = llext_find_sym(&ext->exp_tab, fsarg->sym);
	return fsarg->addr ? 1 : 0;
}

const void *extmgr_find_sym(void *handle, const char *sym) {
	if (handle) {
		struct ext_data *data = handle;
		if (!data->ext) {
			return NULL;
		}
		return llext_find_sym(&data->ext->exp_tab, sym);
	}

	struct find_sym_arg fsarg = { .sym = sym, .addr = NULL };

	llext_iterate(find_sym_iterate, &fsarg);
	return fsarg.addr;
}

#endif /* CONFIG_EXTMGR */
