
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2004-2006
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "trousers/tss.h"
#include "trousers/trousers.h"
#include "spi_internal_types.h"
#include "spi_utils.h"
#include "capabilities.h"
#include "tsplog.h"
#include "hosttable.h"
#include "tcsd_wrap.h"
#include "obj.h"
#include "rpc_tcstp_tsp.h"


TSS_RESULT
TCSP_CreateMaintenanceArchive_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
					     TSS_BOOL generateRandom,	/* in */
					     TPM_AUTH * ownerAuth,	/* in, out */
					     UINT32 * randomSize,	/* out */
					     BYTE ** random,	/* out */
					     UINT32 * archiveSize,	/* out */
					     BYTE ** archive	/* out */
    ) {
	TSS_RESULT result;
	struct tsp_packet data;
	struct tcsd_packet_hdr *hdr;
	TSS_HCONTEXT tspContext;

	if ((tspContext = obj_lookupTspContext(hContext)) == NULL_HCONTEXT)
		return TSPERR(TSS_E_INTERNAL_ERROR);

	memset(&data, 0, sizeof(struct tsp_packet));

	data.ordinal = TCSD_ORD_CREATEMAINTENANCEARCHIVE;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_BOOL, 1, &generateRandom, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_AUTH, 2, ownerAuth, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte, 0, &data, &hdr);

	if (result == TSS_SUCCESS)
		result = hdr->result;

	if (result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_AUTH, 0, ownerAuth, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);
		if (getData(TCSD_PACKET_TYPE_UINT32, 1, randomSize, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);

		if (*randomSize > 0) {
			*random = calloc_tspi(tspContext, *randomSize);
			if (*random == NULL) {
				LogError("malloc of %u bytes failed.", *randomSize);
				result = TSPERR(TSS_E_OUTOFMEMORY);
				goto done;
			}

			if (getData(TCSD_PACKET_TYPE_PBYTE, 2, *random, *randomSize, hdr)) {
				free_tspi(tspContext, *random);
				result = TSPERR(TSS_E_INTERNAL_ERROR);
				goto done;
			}
		} else {
			*random = NULL;
		}

		/* Assume all elements are in the list, even when *randomSize == 0. */
		if (getData(TCSD_PACKET_TYPE_UINT32, 3, archiveSize, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);

		if (*archiveSize > 0) {
			*archive = calloc_tspi(tspContext, *archiveSize);
			if (*archive == NULL) {
				LogError("malloc of %u bytes failed.", *archiveSize);
				result = TSPERR(TSS_E_OUTOFMEMORY);
				goto done;
			}

			if (getData(TCSD_PACKET_TYPE_PBYTE, 4, *archive, *archiveSize, hdr)) {
				free_tspi(tspContext, *random);
				free_tspi(tspContext, *archive);
				result = TSPERR(TSS_E_INTERNAL_ERROR);
				goto done;
			}
		} else {
			*archive = NULL;
		}
	}
done:
	free(hdr);
	return result;
}

TSS_RESULT
TCSP_LoadMaintenanceArchive_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
					   UINT32 dataInSize,	/* in */
					   BYTE * dataIn,	/* in */
					   TPM_AUTH * ownerAuth,	/* in, out */
					   UINT32 * dataOutSize,	/* out */
					   BYTE ** dataOut	/* out */
    ) {
	TSS_RESULT result;
	struct tsp_packet data;
	struct tcsd_packet_hdr *hdr;

	memset(&data, 0, sizeof(struct tsp_packet));

	data.ordinal = TCSD_ORD_LOADMAINTENANCEARCHIVE;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, 1, &dataInSize, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_PBYTE, 2, &dataIn, dataInSize, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_AUTH, 3, ownerAuth, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte, 0, &data, &hdr);

	if (result == TSS_SUCCESS)
		result = hdr->result;

	if (result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_AUTH, 0, ownerAuth, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);
		if (getData(TCSD_PACKET_TYPE_UINT32, 1, dataOutSize, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);

		if (*dataOutSize > 0) {
			*dataOut = malloc(*dataOutSize);
			if (*dataOut == NULL) {
				LogError("malloc of %u bytes failed.", *dataOutSize);
				result = TSPERR(TSS_E_OUTOFMEMORY);
				goto done;
			}

			if (getData(TCSD_PACKET_TYPE_PBYTE, 2, *dataOut, *dataOutSize, hdr)) {
				free(*dataOut);
				result = TSPERR(TSS_E_INTERNAL_ERROR);
				goto done;
			}
		} else {
			*dataOut = NULL;
		}
	}
done:
	free(hdr);
	return result;
}

TSS_RESULT
TCSP_KillMaintenanceFeature_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
					   TPM_AUTH * ownerAuth	/* in , out */
    ) {
	TSS_RESULT result;
	struct tsp_packet data;
	struct tcsd_packet_hdr *hdr;

	memset(&data, 0, sizeof(struct tsp_packet));

	data.ordinal = TCSD_ORD_KILLMAINTENANCEFEATURE;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_AUTH, 1, ownerAuth, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte, 0, &data, &hdr);

	if (result == TSS_SUCCESS)
		result = hdr->result;

	if (result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_AUTH, 0, ownerAuth, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);
	}

	free(hdr);
	return result;
}

TSS_RESULT
TCSP_LoadManuMaintPub_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
				     TCPA_NONCE antiReplay,	/* in */
				     UINT32 PubKeySize,	/* in */
				     BYTE * PubKey,	/* in */
				     TCPA_DIGEST * checksum	/* out */
    ) {
	TSS_RESULT result;
	struct tsp_packet data;
	struct tcsd_packet_hdr *hdr;

	memset(&data, 0, sizeof(struct tsp_packet));

	data.ordinal = TCSD_ORD_LOADMANUFACTURERMAINTENANCEPUB;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_NONCE, 1, &antiReplay, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_UINT32, 2, &PubKeySize, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_PBYTE, 3, PubKey, PubKeySize, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte, 0, &data, &hdr);

	if (result == TSS_SUCCESS)
		result = hdr->result;

	if (result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_DIGEST, 0, checksum, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);
	}

	free(hdr);
	return result;
}

TSS_RESULT
TCSP_ReadManuMaintPub_TP(struct host_table_entry *hte, TCS_CONTEXT_HANDLE hContext,	/* in */
				     TCPA_NONCE antiReplay,	/* in */
				     TCPA_DIGEST * checksum	/* out */
    ) {
	TSS_RESULT result;
	struct tsp_packet data;
	struct tcsd_packet_hdr *hdr;

	memset(&data, 0, sizeof(struct tsp_packet));

	data.ordinal = TCSD_ORD_READMANUFACTURERMAINTENANCEPUB;
	LogDebugFn("TCS Context: 0x%x", hContext);

	if (setData(TCSD_PACKET_TYPE_UINT32, 0, &hContext, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);
	if (setData(TCSD_PACKET_TYPE_NONCE, 1, &antiReplay, 0, &data))
		return TSPERR(TSS_E_INTERNAL_ERROR);

	result = sendTCSDPacket(hte, 0, &data, &hdr);

	if (result == TSS_SUCCESS)
		result = hdr->result;

	if (result == TSS_SUCCESS) {
		if (getData(TCSD_PACKET_TYPE_DIGEST, 0, checksum, 0, hdr))
			result = TSPERR(TSS_E_INTERNAL_ERROR);
	}

	free(hdr);
	return result;
}
