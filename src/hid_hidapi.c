/*
 * Copyright (c) 2019 Google LLC. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <hidapi/hidapi.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "fido.h"

struct hid_hidapi {
	void *handle;
	size_t report_in_len;
	size_t report_out_len;
};

static size_t
fido_wcslen(const wchar_t *wcs)
{
	size_t l = 0;
	while (*wcs++ != L'\0')
		l++;
	return l;
}

static char *
wcs_to_cs(const wchar_t *wcs)
{
	char *cs;
	size_t i;

	if (wcs == NULL || (cs = calloc(fido_wcslen(wcs) + 1, 1)) == NULL) 
		return NULL;

	for (i = 0; i < fido_wcslen(wcs); i++) {
		if (wcs[i] >= 128) {
			/* give up on parsing non-ASCII text */
			free(cs);
			return strdup("hidapi device");
		}
		cs[i] = (char)wcs[i];
	}

	return cs;
}

static int
copy_info(fido_dev_info_t *di, const struct hid_device_info *d)
{
	memset(di, 0, sizeof(*di));

	if (d->path != NULL)
		di->path = strdup(d->path);
	else
		di->path = strdup("");

	if (d->manufacturer_string != NULL)
		di->manufacturer = wcs_to_cs(d->manufacturer_string);
	else
		di->manufacturer = strdup("");

	if (d->product_string != NULL)
		di->product = wcs_to_cs(d->product_string);
	else
		di->product = strdup("");

	if (di->path == NULL ||
	    di->manufacturer == NULL ||
	    di->product == NULL) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		return -1;
	}

	di->product_id = d->product_id;
	di->vendor_id = d->vendor_id;
	di->io = (fido_dev_io_t) {
		&fido_hid_open,
		&fido_hid_close,
		&fido_hid_read,
		&fido_hid_write,
	};

	return 0;
}

void *
fido_hid_open(const char *path)
{
	struct hid_hidapi *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		return (NULL);
	}

	if ((ctx->handle = hid_open_path(path)) == NULL) {
		free(ctx);
		return (NULL);
	}

	ctx->report_in_len = ctx->report_out_len = CTAP_MAX_REPORT_LEN;

	return ctx;
}

void
fido_hid_close(void *handle)
{
	struct hid_hidapi *ctx = handle;

	hid_close(ctx->handle);
	free(ctx);
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_hidapi *ctx = handle;

	return hid_read_timeout(ctx->handle, buf, len, ms);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_hidapi *ctx = handle;

	return hid_write(ctx->handle, buf, len);
}

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	struct hid_device_info *hdi;

	*olen = 0;

	if (ilen == 0)
		return FIDO_OK; /* nothing to do */
	if (devlist == NULL)
		return FIDO_ERR_INVALID_ARGUMENT;
	if ((hdi = hid_enumerate(0, 0)) == NULL)
		return FIDO_OK; /* nothing to do */

	for (struct hid_device_info *d = hdi; d != NULL; d = d->next) {
#if defined(_WIN32) || defined(__APPLE__)
		if (d->usage_page != 0xf1d0)
			continue;
#endif
		if (copy_info(&devlist[*olen], d) == 0) {
			if (++(*olen) == ilen)
				break;
		}
	}

	hid_free_enumeration(hdi);

	return FIDO_OK;
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_hidapi *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_hidapi *ctx = handle;

	return (ctx->report_out_len);
}
