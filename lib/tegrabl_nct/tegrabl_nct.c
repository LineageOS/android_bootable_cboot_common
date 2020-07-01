/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE	TEGRABL_ERR_NCT

#include <tegrabl_error.h>
#include <tegrabl_nct.h>
#include <tegrabl_utils.h>
#include <tegrabl_debug.h>
#include <string.h>
#include <tegrabl_partition_loader.h>

#define NCT_SPEC_ID_NAME ("\"id\":\"")
#define NCT_SPEC_CFG_NAME ("\"config\":\"")

/* TODO: Verify CRC32 checksum */
#define USE_CRC32_IN_NCT	0

static int tegra_nct_initialized;
static void *nct_ptr;

tegrabl_error_t tegrabl_nct_read_item(enum nct_id id, union nct_item *buf)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	struct nct_entry *entry;
	uint8_t *nct;
#if USE_CRC32_IN_NCT
	uint32_t crc = 0;
#endif

	if (!tegra_nct_initialized) {
		pr_error("%s: Error: NCT has not been initialized\n", __func__);
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_INITIALIZED, 0);
		goto fail;
	}

	if (id > NCT_ID_END) {
		pr_error("%s: Error: Invalid nct id: %u\n", __func__, id);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 1);
		goto fail;
	}

	if (!buf) {
		pr_error("%s: Error: Invalid buffer address: %p\n", __func__, buf);
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_ADDRESS, 2);
		goto fail;
	}

	nct = (uint8_t *)((uint8_t *)nct_ptr + NCT_ENTRY_OFFSET +
					  (id * sizeof(struct nct_entry)));
	entry = (struct nct_entry *)nct;

	/* check CRC integrity */
#if USE_CRC32_IN_NCT
	/* last checksum field of entry is not included in CRC calculation */
	crc = crc32_le(~0, (uint8_t *)entry, sizeof(struct nct_entry) -
			sizeof(entry->checksum)) ^ ~0;
	if (crc != entry->checksum) {
		pr_error("%s: checksum err(0x%x/0x%x)\n", __func__,
				crc, entry->checksum);
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, 3);
		goto fail;
	}
#endif
	/* check index integrity */
	if (id != entry->index) {
		pr_error("%s: id err(0x%x/0x%x)\n", __func__, id, entry->index);
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 4);
		goto fail;
	}

	memcpy(buf, &entry->data, sizeof(union nct_item));

fail:
	return err;
}

tegrabl_error_t tegrabl_nct_get_spec(char *id, char *config)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	union nct_item item;
	char *p;

	if ((id == NULL) || (config == NULL))
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);

	err = tegrabl_nct_read_item(NCT_ID_SPEC, &item);
	if (err != TEGRABL_NO_ERROR) {
		pr_error("Failed to get spec from NCT, err:%d\n", err);
		goto fail;
	}

	p = (char *)strstr((char *)&item.spec, NCT_SPEC_CFG_NAME);
	if (!p) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		goto fail;
	}
	p += strlen(NCT_SPEC_CFG_NAME);
	while (*p != '\"')
		*config++ = *p++;
	*config = '\0';

	p = (char *)strstr((char *)&item.spec, NCT_SPEC_ID_NAME);
	if (!p) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 1);
		goto fail;
	}
	p += strlen(NCT_SPEC_ID_NAME);
	while (*p != '\"')
		*id++ = *p++;
	*id = '\0';

fail:
	return err;
}

tegrabl_error_t tegrabl_nct_init(void)
{
	struct nct_part_head *nct_head;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	pr_debug("function %s\n", __func__);
	if (tegra_nct_initialized) {
		goto done;
	}

	err = tegrabl_load_binary(TEGRABL_BINARY_NCT, (void **)&nct_ptr, NULL);
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(err);
		goto done;
	}

	pr_debug("%s: nct_ptr = %p\n", __func__, nct_ptr);

	/* Sanity check the NCT header */
	nct_head = (struct nct_part_head *)nct_ptr;

	pr_debug("%s: magic(0x%x),vid(0x%x),pid(0x%x),ver(V%x.%x),rev(%d)\n",
			__func__, nct_head->magic_id, nct_head->vendor_id,
			nct_head->product_id, (nct_head->version >> 16) & 0xFFFF,
			(nct_head->version & 0xFFFF), nct_head->revision);

	pr_debug("%s: tns(0x%x),tns offset(0x%x),tns len(%d)\n", __func__,
			nct_head->tns_id, nct_head->tns_off, nct_head->tns_len);

	if (memcmp(&nct_head->magic_id, NCT_MAGIC_ID, NCT_MAGIC_ID_LEN)) {
		pr_error("NCT error: magic ID error (0x%x/0x%p:%s)\n",
				nct_head->magic_id, NCT_MAGIC_ID, NCT_MAGIC_ID);
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		goto done;
	}

	tegra_nct_initialized = 1;

done:
	return err;
}
