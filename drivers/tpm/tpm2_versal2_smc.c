// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026, Advanced Micro Devices, Inc.
 *
 * Author:
 * Padmarao Begari <padmarao.begari@amd.com>
 *
 * Description:
 * AMD Versal Gen 2 and Versal Premium Gen 2 TPM2 driver via TF-A SMC.
 *
 * Versal Gen 2 is ARCH_VERSAL2; Versal Premium Gen 2 is a member of the
 * Versal Gen 1 family and so builds under ARCH_VERSAL.
 *
 * Exposes PLM XilOCP PCR services as a U-Boot DM TPM (UCLASS_TPM) device.
 * All calls go through zynqmp_pm_xilocp_extend_hwpcr() /
 * zynqmp_pm_xilocp_get_hwpcr() (firmware-zynqmp.c) which call
 * xilinx_pm_request() -> smc_call_enhanced() on Versal Gen 2 or
 * smc_call_legacy() on Versal Premium Gen 2 -> ARM SMC (PASS_THROUGH) ->
 * TF-A -> IPI -> PLM XilOCP (module ID 13).
 *
 * Communication path:
 *   U-Boot -> zynqmp_pm_xilocp_*() -> xilinx_pm_request()
 *          -> SMC (PASS_THROUGH) -> TF-A -> IPI -> PLM
 *
 * PCR mapping (U-Boot TPM PCR index -> XilOCP PLM PCR):
 *   0..7 -> HW PCR 0..7 (extend: XOCP_API_EXTEND_HWPCR,
 *                           read: XOCP_API_GET_HWPCR)
 *
 * Both SHA-256 and SHA-384 banks map to the same 8 HW PCRs.  SHA-256 digests
 * are zero-padded to 48 bytes before passing to PLM (SHA3-384 native width).
 *
 * Supported TPM2 commands:
 *   TPM2_CC_STARTUP         (0x0144) - no-op, always succeeds
 *   TPM2_CC_SELF_TEST       (0x0143) - no-op, always succeeds
 *   TPM2_CC_GET_CAPABILITY  (0x017a) - reports PCR banks and TPM properties
 *   TPM2_CC_PCR_EXTEND      (0x0182) - extend HW PCR via PLM
 *   TPM2_CC_PCR_READ        (0x017e) - read HW PCR via PLM
 *
 * No device tree node is required.  The driver is automatically bound as
 * a child of the zynqmp_firmware device by zynqmp_firmware_bind() after
 * confirming that PLM supports XOCP_API_EXTEND_HWPCR and XOCP_API_GET_HWPCR.
 */

#define LOG_CATEGORY UCLASS_TPM

#include <cpu_func.h>
#include <dm.h>
#include <log.h>
#include <tpm-common.h>
#include <tpm-v2.h>
#include <zynqmp_firmware.h>

#include <asm/cache.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/unaligned/be_byteshift.h>

/* PCR count and hash size (SHA3-384) */
#define HW_PCR_COUNT	8U
#define PCR_HASH_SIZE	TPM2_SHA384_DIGEST_SIZE

/* Common TPM2 response header: tag(2) + size(4) + rc(4) = 10 bytes */
struct tpm2_resp_hdr {
	__be16 tag;
	__be32 size;
	__be32 rc;
} __packed;

/* One PCR bank descriptor in a GetCapability(CAP_PCRS) response */
struct tpm2_pcr_bank_desc {
	__be16 hash_alg;
	u8 size_of_select;
	u8 pcr_select;
} __packed;

/* Full GetCapability(CAP_PCRS) response */
struct tpm2_resp_cap_pcrs {
	struct tpm2_resp_hdr hdr;
	u8 more_data;
	__be32 capability;
	__be32 count;
	struct tpm2_pcr_bank_desc banks[2];
} __packed;

/* PCR_Read response (digest payload follows the fixed header) */
struct tpm2_resp_pcr_read {
	struct tpm2_resp_hdr hdr;
	__be32 update_count;
	u8 digest[];
} __packed;

/* One property entry in a GetCapability(CAP_TPM_PROPERTIES) response */
struct tpm2_tagged_prop {
	__be32 property;
	__be32 value;
} __packed;

/* Fixed header of a GetCapability(CAP_TPM_PROPERTIES) response */
struct tpm2_resp_cap_props {
	struct tpm2_resp_hdr hdr;
	u8 more_data;
	__be32 capability;
	__be32 count;
	struct tpm2_tagged_prop props[];
} __packed;

/* Common TPM2 command header: tag(2) + size(4) + cc(4) = 10 bytes */
struct tpm2_req_hdr {
	__be16 tag;
	__be32 size;
	__be32 cc;
} __packed;

/* GET_CAPABILITY command */
struct tpm2_cmd_get_capability {
	struct tpm2_req_hdr hdr;
	__be32 capability;
	__be32 property;
	__be32 property_count;
} __packed;

/*
 * PCR_EXTEND command fixed prefix.
 * A variable-length auth area of auth_size bytes follows at offset
 * sizeof(*c), then __be32 digest_count, __be16 hash_alg, u8 digest[].
 */
struct tpm2_cmd_pcr_extend {
	struct tpm2_req_hdr hdr;
	__be32 pcr_handle;
	__be32 auth_size;
} __packed;

/* PCR_READ command */
struct tpm2_cmd_pcr_read {
	struct tpm2_req_hdr hdr;
	__be32 sel_count;
	__be16 hash_alg;
	u8 size_of_select;
	u8 pcr_select[];
} __packed;

struct tpm2_versal2_priv {
	u8 *pcr_buf;
};

/**
 * hw_pcr_extend() - Extend a Hardware PCR with a 48-byte hash.
 * @pcr_buf: DMA buffer holding the digest (cache-flushed before PLM DMA read)
 * @pcr_num: HW PCR index (0..HW_PCR_COUNT-1)
 *
 * Return: 0 on success, PLM error code on failure.
 */
static int hw_pcr_extend(u8 *pcr_buf, u32 pcr_num)
{
	ulong hash_addr = (ulong)pcr_buf;

	flush_dcache_range(hash_addr, hash_addr + PCR_HASH_SIZE);

	return zynqmp_pm_xilocp_extend_hwpcr(pcr_num, hash_addr, PCR_HASH_SIZE);
}

/**
 * hw_pcr_get() - Read a Hardware PCR value into pcr_buf.
 * @pcr_buf:  DMA buffer for PLM to write the result into
 * @pcr_mask: Bitmask of PCRs to read; bit N selects HW PCR N
 *
 * Cache handling (DMA receive pattern):
 *   1. Invalidate BEFORE: discard dirty CPU cache lines so they cannot be
 *      written back after PLM's DMA write, corrupting PLM's data.
 *   2. PLM DMA writes PCR value(s) to physical memory.
 *   3. Invalidate AFTER: discard stale CPU cache so the CPU reads PLM's
 *      freshly written data from physical memory.
 *
 * Return: 0 on success, PLM error code on failure.
 */
static int hw_pcr_get(u8 *pcr_buf, u32 pcr_mask)
{
	ulong buf_addr = (ulong)pcr_buf;
	int ret;

	invalidate_dcache_range(buf_addr, buf_addr + PCR_HASH_SIZE);

	ret = zynqmp_pm_xilocp_get_hwpcr(pcr_mask, buf_addr, PCR_HASH_SIZE);
	if (!ret)
		invalidate_dcache_range(buf_addr, buf_addr + PCR_HASH_SIZE);

	return ret;
}

/**
 * build_simple_response() - Build a minimal 10-byte TPM2 response.
 * @resp: Output buffer (must be >= TPM_HEADER_SIZE bytes)
 * @rc:   TPM2 response code (e.g. TPM2_RC_SUCCESS)
 *
 * Return: the response size (TPM_HEADER_SIZE).
 */
static size_t build_simple_response(u8 *resp, u32 rc)
{
	struct tpm2_resp_hdr *r = (struct tpm2_resp_hdr *)resp;

	r->tag = cpu_to_be16(TPM2_ST_NO_SESSIONS);
	r->size = cpu_to_be32(sizeof(*r));
	r->rc = cpu_to_be32(rc);

	return sizeof(*r);
}

/**
 * build_pcr_read_response() - Build a TPM2 PCR_Read response.
 * @resp:       Output buffer (must be >= 14 + digest_len bytes)
 * @pcr_data:   48-byte PCR value from PLM
 * @digest_len: Number of bytes to include from pcr_data (32 for SHA256,
 *              48 for SHA384)
 *
 * PLM stores SHA3-384 (48 bytes) internally.  We return the first
 * digest_len bytes of that value to match the algorithm the caller
 * requested:
 *   SHA256 -> digest_len = 32 -> response is 46 bytes
 *   SHA384 -> digest_len = 48 -> response is 62 bytes
 *
 * The caller (tpm2_pcr_read) extracts the digest as:
 *   digest = response + (response_len - digest_len) = response + 14
 *
 * Return: the response size (14 + digest_len).
 */
static size_t build_pcr_read_response(u8 *resp, const u8 *pcr_data,
				      u32 digest_len)
{
	struct tpm2_resp_pcr_read *r = (struct tpm2_resp_pcr_read *)resp;
	size_t total = sizeof(*r) + digest_len;

	r->hdr.tag = cpu_to_be16(TPM2_ST_NO_SESSIONS);
	r->hdr.size = cpu_to_be32((u32)total);
	r->hdr.rc = cpu_to_be32(TPM2_RC_SUCCESS);
	r->update_count = cpu_to_be32(0);
	memcpy(r->digest, pcr_data, digest_len);

	return total;
}

/**
 * build_cap_pcrs_response() - Build a TPM2 GetCapability(CAP_PCRS) response.
 * @resp: Output buffer (must be >= 27 bytes)
 *
 * Reports two PCR banks - SHA256 and SHA384 - each with 8 PCRs
 * (indices 0-7, 1-byte pcrSelect bitmap, 0xff).
 *
 * Both banks are backed by PLM HW PCR 0-7.  They share the same
 * underlying 48-byte PCR value; the algorithm only controls how many
 * bytes are returned on a read (32 for SHA256, 48 for SHA384) and how
 * the incoming digest is formatted on extend (zero-padded to 48 for
 * SHA256, passed directly for SHA384).
 *
 * CONFIG_SHA384=y is required so that tpm2_algorithm_supported(SHA384)
 * returns true and tpm2_check_active_banks() passes for pcr_extend.
 *
 * Response layout (27 bytes):
 *   [0..1]   tag          = TPM2_ST_NO_SESSIONS
 *   [2..5]   size         = 27
 *   [6..9]   rc           = TPM2_RC_SUCCESS
 *   [10]     moreData     = 0
 *   [11..14] capability   = TPM2_CAP_PCRS
 *   [15..18] count        = 2
 *   [19..20] hash[0]      = TPM2_ALG_SHA256 (0x000b)
 *   [21]     sizeofSelect = 1  (1 byte covers PCRs 0-7)
 *   [22]     pcrSelect[0] = 0xff
 *   [23..24] hash[1]      = TPM2_ALG_SHA384 (0x000c)
 *   [25]     sizeofSelect = 1
 *   [26]     pcrSelect[1] = 0xff
 *
 * Return: the response size (27).
 */
static size_t build_cap_pcrs_response(u8 *resp)
{
	struct tpm2_resp_cap_pcrs *r = (struct tpm2_resp_cap_pcrs *)resp;

	r->hdr.tag = cpu_to_be16(TPM2_ST_NO_SESSIONS);
	r->hdr.size = cpu_to_be32(sizeof(*r));
	r->hdr.rc = cpu_to_be32(TPM2_RC_SUCCESS);
	r->more_data = 0;
	r->capability = cpu_to_be32(TPM2_CAP_PCRS);
	r->count = cpu_to_be32(ARRAY_SIZE(r->banks));
	/* bank 0: SHA256 */
	r->banks[0].hash_alg = cpu_to_be16(TPM2_ALG_SHA256);
	r->banks[0].size_of_select = 1; /* 1 byte covers PCRs 0-7 */
	r->banks[0].pcr_select = 0xff; /* PCRs 0-7 active */
	/* bank 1: SHA384 */
	r->banks[1].hash_alg = cpu_to_be16(TPM2_ALG_SHA384);
	r->banks[1].size_of_select = 1;
	r->banks[1].pcr_select = 0xff;

	return sizeof(*r);
}

/**
 * tpm_props - Minimal TPM2 fixed-property table (TPM2_CAP_TPM_PROPERTIES).
 *
 * Covers the properties most commonly queried by U-Boot and host software.
 * The table is searched linearly; entries need not be sorted.
 */
static const struct {
	u32 prop;
	u32 val;
} tpm_props[] = {
	{ TPM2_PT_MANUFACTURER, ('A' << 24) | ('M' << 16) | ('D' << 8) | ' ' },
	{ TPM2_PT_PCR_COUNT,         HW_PCR_COUNT },
	{ TPM2_PT_MAX_COMMAND_SIZE,  TPM_MAX_BUF_SIZE },
	{ TPM2_PT_MAX_RESPONSE_SIZE, TPM_MAX_BUF_SIZE },
};

/**
 * build_cap_tpm_props_response() - Build a GetCapability(CAP_TPM_PROPERTIES)
 * response for any property range.
 * @resp:        Output buffer of @buf_size bytes
 * @buf_size:    Size of @resp in bytes; prop_count is clamped to fit
 * @prop_start:  First property ID requested (inclusive)
 * @prop_count:  Number of consecutive property IDs requested
 *
 * Always returns exactly prop_count entries (after clamping to buf_size) so
 * the U-Boot tpm2_get_capability() caller reads the correct number of
 * properties from the buffer.  Properties not found in tpm_props[] are
 * returned with value 0.
 *
 * Return: the response size.
 */
static size_t build_cap_tpm_props_response(u8 *resp, size_t buf_size,
					   u32 prop_start, u32 prop_count)
{
	struct tpm2_resp_cap_props *r = (struct tpm2_resp_cap_props *)resp;
	u32 max_count;
	size_t total;
	size_t j;
	u32 n;

	if (buf_size < sizeof(*r))
		return build_simple_response(resp, TPM2_RC_SIZE);

	max_count = (u32)((buf_size - sizeof(*r)) / sizeof(r->props[0]));
	if (prop_count > max_count)
		prop_count = max_count;

	total = sizeof(*r) + prop_count * sizeof(r->props[0]);

	r->hdr.tag = cpu_to_be16(TPM2_ST_NO_SESSIONS);
	r->hdr.size = cpu_to_be32((u32)total);
	r->hdr.rc = cpu_to_be32(TPM2_RC_SUCCESS);
	r->more_data = 0;
	r->capability = cpu_to_be32(TPM2_CAP_TPM_PROPERTIES);
	r->count = cpu_to_be32(prop_count);

	/*
	 * Always return exactly prop_count entries so the U-Boot
	 * tpm2_get_capability() caller reads the correct number of entries
	 * from the buffer. Properties not in our table are returned as 0.
	 */
	for (n = 0; n < prop_count; n++) {
		u32 pid = prop_start + n;
		u32 val = 0;

		for (j = 0; j < ARRAY_SIZE(tpm_props); j++) {
			if (tpm_props[j].prop == pid) {
				val = tpm_props[j].val;
				break;
			}
		}
		r->props[n].property = cpu_to_be32(pid);
		r->props[n].value = cpu_to_be32(val);
	}

	return total;
}

/**
 * tpm2_versal2_smc_xfer() - Translate TPM2 command bytes to PLM SMC calls.
 * @dev:       TPM device
 * @sendbuf:   Raw TPM2 command buffer
 * @send_size: Length of @sendbuf in bytes
 * @recvbuf:   Output buffer for the TPM2 response
 * @recv_len:  On return, the number of bytes written to @recvbuf
 *
 * Implements struct tpm_ops.xfer.  Receives a raw TPM2 command buffer,
 * dispatches to the appropriate PLM function, and writes a TPM2-formatted
 * response.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int tpm2_versal2_smc_xfer(struct udevice *dev, const u8 *sendbuf,
				 size_t send_size, u8 *recvbuf,
				 size_t *recv_len)
{
	const struct tpm2_req_hdr *hdr = (const struct tpm2_req_hdr *)sendbuf;
	size_t resp_len = build_simple_response(recvbuf, TPM2_RC_FAILURE);
	struct tpm2_versal2_priv *priv = dev_get_priv(dev);
	u8 *pcr_buf = priv->pcr_buf;
	u32 cc;

	if (send_size < sizeof(*hdr)) {
		log_debug("command too short (%zu bytes)\n", send_size);
		*recv_len = build_simple_response(recvbuf, TPM2_RC_SIZE);
		return -EINVAL;
	}

	cc = be32_to_cpu(hdr->cc);

	switch (cc) {
	case TPM2_CC_STARTUP:
	case TPM2_CC_SELF_TEST:
		/* PLM PCR module is always ready; no initialization needed. */
		resp_len = build_simple_response(recvbuf, TPM2_RC_SUCCESS);
		break;

	case TPM2_CC_GET_CAPABILITY: {
		const struct tpm2_cmd_get_capability *c =
			(const struct tpm2_cmd_get_capability *)sendbuf;
		u32 cap, prop, prop_count;

		if (send_size < sizeof(*c)) {
			resp_len = build_simple_response(recvbuf, TPM2_RC_SIZE);
			break;
		}

		cap = be32_to_cpu(c->capability);
		prop = be32_to_cpu(c->property);
		prop_count = be32_to_cpu(c->property_count);

		if (cap == TPM2_CAP_PCRS) {
			resp_len = build_cap_pcrs_response(recvbuf);
		} else if (cap == TPM2_CAP_TPM_PROPERTIES) {
			resp_len = build_cap_tpm_props_response(recvbuf,
								TPM_MAX_BUF_SIZE,
								prop,
								prop_count);
		} else {
			log_debug("unsupported cap=0x%08x\n", cap);
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
		}
		break;
	}

	case TPM2_CC_PCR_EXTEND: {
		const struct tpm2_cmd_pcr_extend *c =
			(const struct tpm2_cmd_pcr_extend *)sendbuf;
		u32 pcr_handle, auth_size, digest_len = 0;
		const u8 *digest;
		u16 hash_alg;

		if (send_size < sizeof(*c)) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_SIZE);
			break;
		}

		pcr_handle = be32_to_cpu(c->pcr_handle);
		auth_size = be32_to_cpu(c->auth_size);

		/* Need auth area + digest_count(4) + hash_alg(2) */
		if (send_size < sizeof(*c) + auth_size +
				sizeof(__be32) + sizeof(__be16)) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_SIZE);
			break;
		}

		hash_alg = get_unaligned_be16(sendbuf + sizeof(*c) +
					      auth_size + sizeof(__be32));

		switch (hash_alg) {
		case TPM2_ALG_SHA384:
			digest_len = PCR_HASH_SIZE;
			break;
		case TPM2_ALG_SHA256:
			digest_len = TPM2_SHA256_DIGEST_SIZE;
			break;
		default:
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		if (!digest_len)
			break;

		if (send_size < sizeof(*c) + auth_size +
				sizeof(__be32) + sizeof(__be16) + digest_len) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_SIZE);
			break;
		}

		if (pcr_handle >= HW_PCR_COUNT) {
			log_debug("PCR index %u out of range\n", pcr_handle);
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		digest = sendbuf + sizeof(*c) + auth_size +
			 sizeof(__be32) + sizeof(__be16);

		/*
		 * Copy digest into priv buffer, zero-padding to 48 bytes.
		 * Both SHA256 and SHA384 extend the same HW PCR; SHA256
		 * digests are zero-padded to 48 bytes, SHA384 used as-is.
		 */
		memset(pcr_buf, 0, PCR_HASH_SIZE);
		memcpy(pcr_buf, digest, digest_len);

		resp_len = build_simple_response(recvbuf,
						 hw_pcr_extend(pcr_buf, pcr_handle)
						 ? TPM2_RC_FAILURE
						 : TPM2_RC_SUCCESS);
		break;
	}

	case TPM2_CC_PCR_READ: {
		const struct tpm2_cmd_pcr_read *c =
			(const struct tpm2_cmd_pcr_read *)sendbuf;
		u32 pcr_handle, pcr_mask, digest_len = 0;
		bool found = false;
		const u8 *sel;
		u8 sel_sz;
		int i;

		if (send_size < sizeof(*c)) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_SIZE);
			break;
		}

		/* Only one PCR selection block is supported */
		if (be32_to_cpu(c->sel_count) != 1) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		sel_sz = c->size_of_select;

		if (send_size < sizeof(*c) + sel_sz) {
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_SIZE);
			break;
		}

		sel = c->pcr_select;

		switch (be16_to_cpu(c->hash_alg)) {
		case TPM2_ALG_SHA384:
			digest_len = PCR_HASH_SIZE;
			break;
		case TPM2_ALG_SHA256:
			digest_len = TPM2_SHA256_DIGEST_SIZE;
			break;
		default:
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		if (!digest_len)
			break;

		/* Find the first set PCR bit in the selection bitmap */
		for (i = 0; i < (int)sel_sz * 8; i++) {
			if (sel[i / 8] & BIT(i % 8)) {
				pcr_handle = (u32)i;
				found = true;
				break;
			}
		}

		if (!found) {
			log_debug("PCR_READ: no PCR selected\n");
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		if (pcr_handle >= HW_PCR_COUNT) {
			log_debug("PCR index %u out of range\n", pcr_handle);
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_VALUE);
			break;
		}

		pcr_mask = BIT(pcr_handle);

		/*
		 * Both SHA256 and SHA384 read the same HW PCR.
		 * digest_len controls how many bytes of the 48-byte PLM
		 * value are returned (32 for SHA256, 48 for SHA384).
		 */
		if (hw_pcr_get(pcr_buf, pcr_mask))
			resp_len = build_simple_response(recvbuf,
							 TPM2_RC_FAILURE);
		else
			resp_len = build_pcr_read_response(recvbuf, pcr_buf,
							   digest_len);
		break;
	}

	default:
		log_debug("unsupported command code 0x%08x\n", cc);
		resp_len = build_simple_response(recvbuf,
						 TPM2_RC_COMMAND_CODE);
		break;
	}

	*recv_len = resp_len;

	return 0;
}

static int tpm2_versal2_smc_open(struct udevice *dev)
{
	return 0;
}

static int tpm2_versal2_smc_close(struct udevice *dev)
{
	return 0;
}

static int tpm2_versal2_smc_get_desc(struct udevice *dev, char *buf, int size)
{
	return snprintf(buf, size, "AMD Versal TPM2 (via TF-A SMC)");
}

static int tpm2_versal2_smc_probe(struct udevice *dev)
{
	struct tpm_chip_priv *tpm_priv = dev_get_uclass_priv(dev);
	struct tpm2_versal2_priv *priv = dev_get_priv(dev);

	priv->pcr_buf = memalign(ARCH_DMA_MINALIGN, PCR_HASH_SIZE);
	if (!priv->pcr_buf)
		return -ENOMEM;

	tpm_priv->version = TPM_V2;
	tpm_priv->pcr_count = HW_PCR_COUNT;
	tpm_priv->pcr_select_min = 1;

	return 0;
}

static int tpm2_versal2_smc_remove(struct udevice *dev)
{
	struct tpm2_versal2_priv *priv = dev_get_priv(dev);

	free(priv->pcr_buf);

	return 0;
}

static const struct tpm_ops tpm2_versal2_smc_ops = {
	.open = tpm2_versal2_smc_open,
	.close = tpm2_versal2_smc_close,
	.get_desc = tpm2_versal2_smc_get_desc,
	.xfer = tpm2_versal2_smc_xfer,
};

U_BOOT_DRIVER(tpm2_versal2_smc) = {
	.name = "tpm2_versal2_smc",
	.id = UCLASS_TPM,
	.ops = &tpm2_versal2_smc_ops,
	.probe = tpm2_versal2_smc_probe,
	.remove = tpm2_versal2_smc_remove,
	.priv_auto = sizeof(struct tpm2_versal2_priv),
};
