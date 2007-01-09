
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2004
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "trousers/tss.h"
#include "spi_internal_types.h"
#include "tcs_tsp.h"
#include "tcsps.h"
#include "tcs_utils.h"
#include "tcs_int_literals.h"
#include "capabilities.h"
#include "tcslog.h"
#include "req_mgr.h"
#include "tcsd_wrap.h"
#include "tcsd.h"


TSS_RESULT
TCSP_CertifyKey_Internal(TCS_CONTEXT_HANDLE hContext,	/* in */
			 TCS_KEY_HANDLE certHandle,	/* in */
			 TCS_KEY_HANDLE keyHandle,	/* in */
			 TCPA_NONCE antiReplay,	/* in */
			 TPM_AUTH * certAuth,	/* in, out */
			 TPM_AUTH * keyAuth,	/* in, out */
			 UINT32 * CertifyInfoSize,	/* out */
			 BYTE ** CertifyInfo,	/* out */
			 UINT32 * outDataSize,	/* out */
			 BYTE ** outData)	/* out */
{
	UINT64 offset;
	UINT32 paramSize;
	TSS_RESULT result;
	TCPA_KEY_HANDLE certKeySlot, keySlot;
	TCPA_CERTIFY_INFO certifyContainer;
	UINT16 tag;
	BYTE txBlob[TSS_TPM_TXBLOB_SIZE];

	LogDebug("Entering Certify Key");
	offset = 10;
	if ((result = ctx_verify_context(hContext)))
		goto done;

	if (certAuth != NULL) {
		LogDebug("Auth Used for Cert signing key");
		if ((result = auth_mgr_check(hContext, &certAuth->AuthHandle)))
			goto done;
	} else {
		LogDebug("No Auth used for Cert signing key");
	}

	if (keyAuth != NULL) {
		LogDebug("Auth Used for Key being signed");
		if ((result = auth_mgr_check(hContext, &keyAuth->AuthHandle)))
			goto done;
	} else {
		LogDebug("No Auth used for Key being signed");
	}

	if ((result = ensureKeyIsLoaded(hContext, certHandle, &certKeySlot)))
		goto done;

	if ((result = ensureKeyIsLoaded(hContext, keyHandle, &keySlot)))
		goto done;

	LoadBlob_UINT32(&offset, certKeySlot, txBlob);
	LoadBlob_UINT32(&offset, keySlot, txBlob);
	LoadBlob(&offset, TCPA_NONCE_SIZE, txBlob, antiReplay.nonce);

	tag = TPM_TAG_RQU_COMMAND;
	if (certAuth != NULL) {
		tag++;
		LoadBlob_Auth(&offset, txBlob, certAuth);
	}
	if (keyAuth != NULL) {
		tag++;
		LoadBlob_Auth(&offset, txBlob, keyAuth);
	}
	LoadBlob_Header(tag, offset, TPM_ORD_CertifyKey, txBlob);

	if ((result = req_mgr_submit_req(txBlob)))
		goto done;

	offset = 10;

	result = UnloadBlob_Header(txBlob, &paramSize);

	if (!result) {
		if ((result = UnloadBlob_CERTIFY_INFO(&offset, txBlob, &certifyContainer)))
			goto done;
		free(certifyContainer.algorithmParms.parms);
		free(certifyContainer.PCRInfo);

		*CertifyInfoSize = offset - 10;
		*CertifyInfo = calloc(1, *CertifyInfoSize);
		if (*CertifyInfo == NULL) {
			LogError("malloc of %u bytes failed.", *CertifyInfoSize);
			result = TCSERR(TSS_E_OUTOFMEMORY);
			goto done;
		} else {
			memcpy(*CertifyInfo, &txBlob[10], *CertifyInfoSize);
		}

		UnloadBlob_UINT32(&offset, outDataSize, txBlob);
		*outData = calloc(1, *outDataSize);
		if (*outData == NULL) {
			free(*CertifyInfo);
			LogError("malloc of %u bytes failed.", *outDataSize);
			result = TCSERR(TSS_E_OUTOFMEMORY);
			goto done;
		} else {
			UnloadBlob(&offset, *outDataSize, txBlob, *outData);
		}

		if (certAuth != NULL) {
			UnloadBlob_Auth(&offset, txBlob, certAuth);
		}
		if (keyAuth != NULL) {
			UnloadBlob_Auth(&offset, txBlob, keyAuth);
		}
	}
	LogResult("Certify Key", result);
done:
	auth_mgr_release_auth(certAuth, keyAuth, hContext);
	return result;
}
