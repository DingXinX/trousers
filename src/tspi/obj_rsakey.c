
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2005
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include "trousers/tss.h"
#include "trousers/trousers.h"
#include "spi_internal_types.h"
#include "spi_utils.h"
#include "capabilities.h"
#include "tsplog.h"
#include "obj.h"

TSS_RESULT
obj_rsakey_add(TSS_HCONTEXT tspContext, TSS_FLAG initFlags, TSS_HOBJECT *phObject)
{
	UINT64 offset;
	TSS_RESULT result;
	TCPA_RSA_KEY_PARMS rsaKeyParms;
	TSS_FLAG flags = 0;
	struct tr_rsakey_obj *rsakey = calloc(1, sizeof(struct tr_rsakey_obj));
	TPM_VERSION ver = { 1, 1, 0, 0 };  // Must be 1.1.0.0 for 1.2 TPMs
	UINT32 ctx_ver;

	if (rsakey == NULL) {
		LogError("malloc of %zd bytes failed.", sizeof(struct tr_rsakey_obj));
		return TSPERR(TSS_E_OUTOFMEMORY);
	}

	if ((result = obj_context_get_policy(tspContext, &rsakey->usagePolicy))) {
		free(rsakey);
		return result;
	}

	if ((initFlags & TSS_KEY_STRUCT_BITMASK) == TSS_KEY_STRUCT_DEFAULT) {
		/* Its not set, go with the context's default */
		if ((result = obj_context_get_connection_version(tspContext, &ctx_ver))) {
			free(rsakey);
			return result;
		}

		switch (ctx_ver) {
			case TSS_TSPATTRIB_CONTEXT_VERSION_V1_2:
				initFlags |= TSS_KEY_STRUCT_KEY12;
				break;
			case TSS_TSPATTRIB_CONTEXT_VERSION_V1_1:
				/* fall through */
			default:
				initFlags |= TSS_KEY_STRUCT_KEY;
				break;
		}
	}

	switch (initFlags & TSS_KEY_STRUCT_BITMASK) {
		case TSS_KEY_STRUCT_KEY:
			memcpy(&rsakey->key.u.key11.ver, &ver, sizeof(TPM_VERSION));
			rsakey->type = TSS_KEY_STRUCT_KEY;
			break;
		case TSS_KEY_STRUCT_KEY12:
			Trspi_LoadBlob_UINT16(&offset, TPM_TAG_KEY12,
					      (BYTE *)&rsakey->key.u.key12.tag);
			rsakey->key.u.key12.fill = 0;
			rsakey->type = TSS_KEY_STRUCT_KEY12;
			break;
		default:
			free(rsakey);
			return TSPERR(TSS_E_INVALID_OBJECT_INITFLAG);
			break;
	}

	if (initFlags & TSS_KEY_EMPTY_KEY)
		goto add_key;

	memset(&rsaKeyParms, 0, sizeof(TCPA_RSA_KEY_PARMS));

	rsakey->key.algorithmParms.algorithmID = TCPA_ALG_RSA;
	rsakey->key.algorithmParms.parmSize = sizeof(TCPA_RSA_KEY_PARMS);

	rsakey->key.algorithmParms.parms = calloc(1, sizeof(TCPA_RSA_KEY_PARMS));
	if (rsakey->key.algorithmParms.parms == NULL) {
		LogError("calloc of %u bytes failed.", rsakey->key.algorithmParms.parmSize);
		free(rsakey);
		return TSPERR(TSS_E_OUTOFMEMORY);
	}
	rsaKeyParms.exponentSize = 0;
	rsaKeyParms.numPrimes = 2;
	memset(&rsakey->key.keyFlags, 0, sizeof(TCPA_KEY_FLAGS));

	rsakey->key.pubKey.keyLength = 0;
	rsakey->key.encSize = 0;
	rsakey->key.PCRInfoSize = 0;

	/* End of all the default stuff */

	if (initFlags & TSS_KEY_VOLATILE)
		rsakey->key.keyFlags |= volatileKey;
	if (initFlags & TSS_KEY_MIGRATABLE)
		rsakey->key.keyFlags |= migratable;
	if (initFlags & TSS_KEY_AUTHORIZATION) {
		rsakey->key.authDataUsage = TPM_AUTH_ALWAYS;
		flags |= TSS_OBJ_FLAG_USAGEAUTH;
	}

	/* set the key length */
	if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_512) {
		rsaKeyParms.keyLength = 512;
	} else if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_1024) {
		rsaKeyParms.keyLength = 1024;
	} else if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_2048) {
		rsaKeyParms.keyLength = 2048;
	} else if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_4096) {
		rsaKeyParms.keyLength = 4096;
	} else if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_8192) {
		rsaKeyParms.keyLength = 8192;
	} else if ((initFlags & TSS_KEY_SIZE_MASK) == TSS_KEY_SIZE_16384) {
		rsaKeyParms.keyLength = 16384;
	}

	/* assign encryption and signature schemes */
	if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_SIGNING) {
		rsakey->key.keyUsage = TPM_KEY_SIGNING;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_NONE;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_RSASSAPKCS1v15_SHA1;
	} else if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_BIND) {
		rsakey->key.keyUsage = TPM_KEY_BIND;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESOAEP_SHA1_MGF1;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_NONE;
	} else if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_LEGACY) {
		rsakey->key.keyUsage = TPM_KEY_LEGACY;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESOAEP_SHA1_MGF1;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_RSASSAPKCS1v15_SHA1;
	} else if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_STORAGE) {
		rsakey->key.keyUsage = TPM_KEY_STORAGE;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESOAEP_SHA1_MGF1;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_NONE;
	} else if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_IDENTITY) {
		rsakey->key.keyUsage = TPM_KEY_IDENTITY;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_NONE;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_RSASSAPKCS1v15_SHA1;
	} else if ((initFlags & TSS_KEY_TYPE_MASK) == TSS_KEY_TYPE_AUTHCHANGE) {
		rsakey->key.keyUsage = TPM_KEY_AUTHCHANGE;
		rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESOAEP_SHA1_MGF1;
		rsakey->key.algorithmParms.sigScheme = TCPA_SS_NONE;
	}

	/* Load the RSA key parms into the blob in the TCPA_KEY_PARMS pointer.
	 * If the exponent is left NULL, the parmSize variable will change
	 * here */
	offset = 0;
	Trspi_LoadBlob_RSA_KEY_PARMS(&offset, rsakey->key.algorithmParms.parms, &rsaKeyParms);
	rsakey->key.algorithmParms.parmSize = offset;

add_key:
	if ((result = obj_list_add(&rsakey_list, tspContext, flags, rsakey, phObject))) {
		free(rsakey->key.algorithmParms.parms);
		free(rsakey);
		return result;
	}

	return TSS_SUCCESS;
}

/* Add a new rsakey to the list when its pulled from user PS */
TSS_RESULT
obj_rsakey_add_by_key(TSS_HCONTEXT tspContext, TSS_UUID *uuid, BYTE *key, TSS_FLAG flags,
		      TSS_HKEY *phKey)
{
	TSS_RESULT result;
	UINT64 offset;
	struct tr_rsakey_obj *rsakey = calloc(1, sizeof(struct tr_rsakey_obj));

	if (rsakey == NULL) {
		LogError("malloc of %zd bytes failed.", sizeof(struct tr_rsakey_obj));
		return TSPERR(TSS_E_OUTOFMEMORY);
	}

	memcpy(&rsakey->uuid, uuid, sizeof(TSS_UUID));

	offset = 0;
	if ((result = Trspi_UnloadBlob_KEY(&offset, key, (TCPA_KEY *)&rsakey->key))) {
		free(rsakey);
		return result;
	}

	flags |= TSS_OBJ_FLAG_KEY_SET;
	if (rsakey->key.authDataUsage)
		flags |= TSS_OBJ_FLAG_USAGEAUTH;

	if ((result = obj_context_get_policy(tspContext, &rsakey->usagePolicy))) {
		free(rsakey);
		return result;
	}

	if ((result = obj_list_add(&rsakey_list, tspContext, flags, rsakey, phKey))) {
		free_key_refs((TCPA_KEY *)&rsakey->key);
		free(rsakey);
		return result;
	}

	return TSS_SUCCESS;
}

TSS_BOOL
obj_is_rsakey(TSS_HOBJECT hObject)
{
	TSS_BOOL answer = FALSE;

	if ((obj_list_get_obj(&rsakey_list, hObject))) {
		answer = TRUE;
		obj_list_put(&rsakey_list);
	}

	return answer;
}

TSS_RESULT
obj_rsakey_set_flags(TSS_HKEY hKey, UINT32 flags)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	rsakey->key.keyFlags = flags;
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_size(TSS_HKEY hKey, UINT32 len)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	rsakey->key.pubKey.keyLength = len/8;
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_key_parms(TSS_HKEY hKey, TCPA_KEY_PARMS *parms)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	free(rsakey->key.algorithmParms.parms);

	memcpy(&rsakey->key.algorithmParms, parms, sizeof(TCPA_KEY_PARMS));

	if (parms->parmSize > 0) {
		if ((rsakey->key.algorithmParms.parms =
					malloc(parms->parmSize)) == NULL) {
			LogError("calloc of %d bytes failed.", parms->parmSize);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}

		memcpy(rsakey->key.algorithmParms.parms, parms->parms,
		       parms->parmSize);
	} else {
		rsakey->key.algorithmParms.parms = NULL;
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_policy(TSS_HKEY hKey, UINT32 type, TSS_HPOLICY hPolicy)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if (type == TSS_POLICY_USAGE)
		rsakey->usagePolicy = hPolicy;
	else
		rsakey->migPolicy = hPolicy;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_pstype(TSS_HKEY hKey, UINT32 type)
{
	struct tsp_object *obj;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	switch (type) {
		case TSS_PS_TYPE_USER:
			obj->flags |= TSS_OBJ_FLAG_USER_PS;
			obj->flags &= ~TSS_OBJ_FLAG_SYSTEM_PS;
			break;
		case TSS_PS_TYPE_SYSTEM:
			obj->flags |= TSS_OBJ_FLAG_SYSTEM_PS;
			obj->flags &= ~TSS_OBJ_FLAG_USER_PS;
			break;
		case TSS_PS_TYPE_NO:
		default:
			obj->flags &= ~TSS_OBJ_FLAG_USER_PS;
			obj->flags &= ~TSS_OBJ_FLAG_SYSTEM_PS;
			break;
	}

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

/* WARN: Nobody should call this function directly except for the
 * Get/Set Attrib functions. The TCPA_KEY structure wants values
 * for keyUsage to be TPM_KEY_* values, and this function translates
 * to TSS_KEYUSAGE_* values for passing to an app. */
TSS_RESULT
obj_rsakey_get_usage(TSS_HKEY hKey, UINT32 *usage)
{
	TSS_RESULT result = TSS_SUCCESS;
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	switch (rsakey->key.keyUsage) {
		case TPM_KEY_SIGNING:
			*usage = TSS_KEYUSAGE_SIGN;
			break;
		case TPM_KEY_BIND:
			*usage = TSS_KEYUSAGE_BIND;
			break;
		case TPM_KEY_LEGACY:
			*usage = TSS_KEYUSAGE_LEGACY;
			break;
		case TPM_KEY_AUTHCHANGE:
			*usage = TSS_KEYUSAGE_AUTHCHANGE;
			break;
		case TPM_KEY_IDENTITY:
			*usage = TSS_KEYUSAGE_IDENTITY;
			break;
		case TPM_KEY_STORAGE:
			*usage = TSS_KEYUSAGE_STORAGE;
			break;
		default:
			result = TSPERR(TSS_E_INVALID_ATTRIB_DATA);
			break;
	}

	obj_list_put(&rsakey_list);

	return result;
}

/* WARN: Nobody should call this function directly except for the
 * Get/Set Attrib functions. The TCPA_KEY structure wants values
 * for keyUsage to be TPM_KEY_* values, and this function translates
 * to TSS_KEYUSAGE_* values for passing to an app. */
TSS_RESULT
obj_rsakey_set_usage(TSS_HKEY hKey, UINT32 usage)
{
	TSS_RESULT result = TSS_SUCCESS;
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	switch (usage) {
		case TSS_KEYUSAGE_SIGN:
			rsakey->key.keyUsage = TPM_KEY_SIGNING;
			break;
		case TSS_KEYUSAGE_BIND:
			rsakey->key.keyUsage = TPM_KEY_BIND;
			break;
		case TSS_KEYUSAGE_LEGACY:
			rsakey->key.keyUsage = TPM_KEY_LEGACY;
			break;
		case TSS_KEYUSAGE_AUTHCHANGE:
			rsakey->key.keyUsage = TPM_KEY_AUTHCHANGE;
			break;
		case TSS_KEYUSAGE_IDENTITY:
			rsakey->key.keyUsage = TPM_KEY_IDENTITY;
			break;
		case TSS_KEYUSAGE_STORAGE:
			rsakey->key.keyUsage = TPM_KEY_STORAGE;
			break;
		default:
			result = TSPERR(TSS_E_INVALID_ATTRIB_DATA);
			break;
	}
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_migratable(TSS_HKEY hKey, UINT32 mig)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (mig)
		rsakey->key.keyFlags |= migratable;
	else
		rsakey->key.keyFlags &= (~migratable);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_redirected(TSS_HKEY hKey, UINT32 redir)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (redir)
		rsakey->key.keyFlags |= redirection;
	else
		rsakey->key.keyFlags &= (~redirection);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_volatile(TSS_HKEY hKey, UINT32 vol)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (vol)
		rsakey->key.keyFlags |= volatileKey;
	else
		rsakey->key.keyFlags &= (~volatileKey);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_authdata_usage(TSS_HKEY hKey, UINT32 *usage)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	*usage = (UINT32)rsakey->key.authDataUsage ? TRUE : FALSE;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_authdata_usage(TSS_HKEY hKey, UINT32 usage)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	rsakey->key.authDataUsage = (BYTE)usage;
	if (usage)
		obj->flags |= TSS_OBJ_FLAG_USAGEAUTH;
	else
		obj->flags &= ~TSS_OBJ_FLAG_USAGEAUTH;
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_alg(TSS_HKEY hKey, UINT32 *alg)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	switch (rsakey->key.algorithmParms.algorithmID) {
		case TCPA_ALG_RSA:
			*alg = TSS_ALG_RSA;
			break;
		default:
			*alg = rsakey->key.algorithmParms.algorithmID;
			break;
	}

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_alg(TSS_HKEY hKey, UINT32 alg)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	switch (alg) {
		case TSS_ALG_RSA:
			rsakey->key.algorithmParms.algorithmID = TCPA_ALG_RSA;
			break;
		default:
			rsakey->key.algorithmParms.algorithmID = alg;
			break;
	}
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_es(TSS_HKEY hKey, UINT32 *es)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	/* translate TPM numbers to TSS numbers */
	switch (rsakey->key.algorithmParms.encScheme) {
		case TCPA_ES_NONE:
			*es = TSS_ES_NONE;
			break;
		case TCPA_ES_RSAESPKCSv15:
			*es = TSS_ES_RSAESPKCSV15;
			break;
		case TCPA_ES_RSAESOAEP_SHA1_MGF1:
			*es = TSS_ES_RSAESOAEP_SHA1_MGF1;
			break;
		default:
			*es = rsakey->key.algorithmParms.encScheme;
			break;
	}

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_es(TSS_HKEY hKey, UINT32 es)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	/* translate TSS numbers to TPM numbers */
	switch (es) {
		case TSS_ES_NONE:
			rsakey->key.algorithmParms.encScheme = TCPA_ES_NONE;
			break;
		case TSS_ES_RSAESPKCSV15:
			rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESPKCSv15;
			break;
		case TSS_ES_RSAESOAEP_SHA1_MGF1:
			rsakey->key.algorithmParms.encScheme = TCPA_ES_RSAESOAEP_SHA1_MGF1;
			break;
		default:
			rsakey->key.algorithmParms.encScheme = es;
			break;
	}
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_ss(TSS_HKEY hKey, UINT32 *ss)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	/* translate TPM numbers to TSS numbers */
	switch (rsakey->key.algorithmParms.sigScheme) {
		case TCPA_SS_NONE:
			*ss = TSS_SS_NONE;
			break;
		case TCPA_SS_RSASSAPKCS1v15_SHA1:
			*ss = TSS_SS_RSASSAPKCS1V15_SHA1;
			break;
		case TCPA_SS_RSASSAPKCS1v15_DER:
			*ss = TSS_SS_RSASSAPKCS1V15_DER;
			break;
		default:
			*ss = rsakey->key.algorithmParms.sigScheme;
			break;
	}


	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_ss(TSS_HKEY hKey, UINT32 ss)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	/* translate TSS numbers to TPM numbers */
	switch (ss) {
		case TSS_SS_NONE:
			rsakey->key.algorithmParms.sigScheme = TCPA_SS_NONE;
			break;
		case TSS_SS_RSASSAPKCS1V15_SHA1:
			rsakey->key.algorithmParms.sigScheme = TCPA_SS_RSASSAPKCS1v15_SHA1;
			break;
		case TSS_SS_RSASSAPKCS1V15_DER:
			rsakey->key.algorithmParms.sigScheme = TCPA_SS_RSASSAPKCS1v15_DER;
			break;
		default:
			rsakey->key.algorithmParms.sigScheme = ss;
			break;
	}
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_num_primes(TSS_HKEY hKey, UINT32 num)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	UINT32ToArray(num, &rsakey->key.algorithmParms.parms[4]);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_num_primes(TSS_HKEY hKey, UINT32 *num)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TCPA_RSA_KEY_PARMS *parms;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	parms = (TCPA_RSA_KEY_PARMS *)rsakey->key.algorithmParms.parms;
	*num = endian32(parms->numPrimes);

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_get_flags(TSS_HKEY hKey, UINT32 *flags)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	*flags = rsakey->key.keyFlags;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_get_size(TSS_HKEY hKey, UINT32 *len)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	switch (rsakey->key.pubKey.keyLength) {
		case 512/8:
			*len = TSS_KEY_SIZEVAL_512BIT;
			break;
		case 1024/8:
			*len = TSS_KEY_SIZEVAL_1024BIT;
			break;
		case 2048/8:
			*len = TSS_KEY_SIZEVAL_2048BIT;
			break;
		default:
			*len = rsakey->key.pubKey.keyLength * 8;
			break;
	}

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_get_pstype(TSS_HKEY hKey, UINT32 *type)
{
	struct tsp_object *obj;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_SYSTEM_PS)
		*type = TSS_PS_TYPE_SYSTEM;
	else if (obj->flags & TSS_OBJ_FLAG_USER_PS)
		*type = TSS_PS_TYPE_USER;
	else
		*type = TSS_PS_TYPE_NO;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_BOOL
obj_rsakey_is_migratable(TSS_HKEY hKey)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_BOOL answer = FALSE;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return answer;

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (rsakey->key.keyFlags & migratable)
		answer = TRUE;

	obj_list_put(&rsakey_list);

	return answer;
}

TSS_BOOL
obj_rsakey_is_redirected(TSS_HKEY hKey)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_BOOL answer = FALSE;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return answer;

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (rsakey->key.keyFlags & redirection)
		answer = TRUE;

	obj_list_put(&rsakey_list);

	return answer;
}

TSS_BOOL
obj_rsakey_is_volatile(TSS_HKEY hKey)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_BOOL answer = FALSE;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return answer;

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (rsakey->key.keyFlags & volatileKey)
		answer = TRUE;

	obj_list_put(&rsakey_list);

	return answer;
}

TSS_RESULT
obj_rsakey_get_tsp_context(TSS_HKEY hKey, TSS_HCONTEXT *tspContext)
{
	struct tsp_object *obj;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	*tspContext = obj->tspContext;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_get_policy(TSS_HKEY hKey, TSS_FLAG policyType,
		      TSS_HPOLICY *phPolicy, TSS_BOOL *auth)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if (policyType == TSS_POLICY_USAGE) {
		*phPolicy = rsakey->usagePolicy;
		if (auth != NULL) {
			if (obj->flags & TSS_OBJ_FLAG_USAGEAUTH)
				*auth = TRUE;
			else
				*auth = FALSE;
		}
	} else {
		if (!rsakey->migPolicy) {
			result = TSPERR(TSS_E_KEY_NO_MIGRATION_POLICY);
			goto done;
		}

		*phPolicy = rsakey->migPolicy;
		if (auth != NULL) {
			if (obj->flags & TSS_OBJ_FLAG_MIGAUTH)
				*auth = TRUE;
			else
				*auth = FALSE;
		}
	}
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_blob(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;
	BYTE temp[2048];

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = 0;
	Trspi_LoadBlob_KEY(&offset, temp, (TCPA_KEY *)&rsakey->key);

	if (offset > 2048) {
		LogError("memory corruption");
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		goto done;
	} else {
		*data = calloc_tspi(obj->tspContext, offset);
		if (*data == NULL) {
			LogError("malloc of %" PRIu64 " bytes failed.", offset);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = offset;
		memcpy(*data, temp, offset);
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_priv_blob(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = rsakey->key.encSize;

	*data = calloc_tspi(obj->tspContext, offset);
	if (*data == NULL) {
		LogError("malloc of %" PRIu64 " bytes failed.", offset);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	*size = offset;
	memcpy(*data, rsakey->key.encData, offset);

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_modulus(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = rsakey->key.pubKey.keyLength;

	/* if this key object represents the SRK and the public key
	 * data here is all 0's, then we shouldn't return it, we
	 * should return TSS_E_BAD_PARAMETER. This is part of protecting
	 * the SRK public key. */
	if (rsakey->tcsHandle == TPM_KEYHND_SRK) {
		BYTE zeroBlob[2048] = { 0, };

		if (!memcmp(rsakey->key.pubKey.key, zeroBlob, offset)) {
			result = TSPERR(TSS_E_BAD_PARAMETER);
			goto done;
		}
	}

	*data = calloc_tspi(obj->tspContext, offset);
	if (*data == NULL) {
		LogError("malloc of %" PRIu64 " bytes failed.", offset);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	*size = offset;
	memcpy(*data, rsakey->key.pubKey.key, offset);

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_modulus(TSS_HKEY hKey, UINT32 size, BYTE *data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	BYTE *free_ptr;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	free_ptr = rsakey->key.pubKey.key;

	rsakey->key.pubKey.key = malloc(size);
	if (rsakey->key.pubKey.key == NULL) {
		rsakey->key.pubKey.key = free_ptr; // restore
		LogError("malloc of %u bytes failed.", size);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	rsakey->key.pubKey.keyLength = size;
	memcpy(rsakey->key.pubKey.key, data, size);

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_pub_blob(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	BYTE blob[1024];
	UINT64 offset;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = rsakey->key.pubKey.keyLength;

	/* if this key object represents the SRK and the public key
	 * data here is all 0's, then we shouldn't return it, we
	 * should return TSS_E_BAD_PARAMETER. This is part of protecting
	 * the SRK public key. */
	if (rsakey->tcsHandle == TPM_KEYHND_SRK) {
		BYTE zeroBlob[2048] = { 0, };

		if (!memcmp(rsakey->key.pubKey.key, zeroBlob, offset)) {
			result = TSPERR(TSS_E_BAD_PARAMETER);
			goto done;
		}
	}

	offset = 0;
	Trspi_LoadBlob_KEY_PARMS(&offset, blob, &rsakey->key.algorithmParms);
	Trspi_LoadBlob_STORE_PUBKEY(&offset, blob, &rsakey->key.pubKey);

	*data = calloc_tspi(obj->tspContext, offset);
	if (*data == NULL) {
		LogError("malloc of %" PRIu64 " bytes failed.", offset);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	*size = offset;
	memcpy(*data, blob, offset);

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_version(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;
	BYTE temp[128];

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = 0;
	Trspi_LoadBlob_TCPA_VERSION(&offset, temp, *(TCPA_VERSION *)&rsakey->key.u.key11.ver);

	if (offset > 128) {
		LogError("memory corruption");
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		goto done;
	} else {
		*data = calloc_tspi(obj->tspContext, offset);
		if (*data == NULL) {
			LogError("malloc of %" PRIu64 " bytes failed.", offset);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = offset;
		memcpy(*data, temp, offset);
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_exponent(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	TCPA_RSA_KEY_PARMS *parms;
	BYTE default_exp[3] = { 0x1, 0x0, 0x1 };
	UINT32 offset;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	parms = (TCPA_RSA_KEY_PARMS *)rsakey->key.algorithmParms.parms;
	offset = parms->exponentSize;

	/* see TPM 1.1b spec pg. 51. If exponentSize is 0, we're using the
	 * default exponent of 2^16 + 1. */
	if (offset == 0) {
		offset = 3;
		*data = calloc_tspi(obj->tspContext, offset);
		if (*data == NULL) {
			LogError("malloc of %u bytes failed.", offset);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = offset;
		memcpy(*data, default_exp, offset);
	} else {
		*data = calloc_tspi(obj->tspContext, offset);
		if (*data == NULL) {
			LogError("malloc of %u bytes failed.", offset);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = offset;
		memcpy(*data, parms->exponent, offset);
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_exponent(TSS_HKEY hKey, UINT32 size, BYTE *data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	TCPA_RSA_KEY_PARMS *parms;
	BYTE *free_ptr;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;
	parms = (TCPA_RSA_KEY_PARMS *)rsakey->key.algorithmParms.parms;

	free_ptr = parms->exponent;

	parms->exponent = malloc(size);
	if (parms->exponent == NULL) {
		parms->exponent = free_ptr; // restore
		LogError("malloc of %u bytes failed.", size);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	parms->exponentSize = size;
	memcpy(parms->exponent, data, size);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_uuid(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	BYTE temp[128];
	UINT64 offset;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = 0;
	Trspi_LoadBlob_UUID(&offset, temp, rsakey->uuid);

	if (offset > 128) {
		LogError("memory corruption");
		result = TSPERR(TSS_E_INTERNAL_ERROR);
		goto done;
	}

	*data = calloc_tspi(obj->tspContext, offset);
	if (*data == NULL) {
		LogError("malloc of %" PRIu64 " bytes failed.", offset);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}
	*size = offset;
	memcpy(*data, temp, offset);

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_uuid(TSS_HKEY hKey, TSS_FLAG ps_type, TSS_UUID *uuid)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	memcpy(&rsakey->uuid, uuid, sizeof(TSS_UUID));

	switch (ps_type) {
		case TSS_PS_TYPE_SYSTEM:
			obj->flags |= TSS_OBJ_FLAG_SYSTEM_PS;
			obj->flags &= ~TSS_OBJ_FLAG_USER_PS;
			break;
		case TSS_PS_TYPE_USER:
			obj->flags |= TSS_OBJ_FLAG_USER_PS;
			obj->flags &= ~TSS_OBJ_FLAG_SYSTEM_PS;
			break;
		case TSS_PS_TYPE_NO:
		default:
			obj->flags &= ~TSS_OBJ_FLAG_USER_PS;
			obj->flags &= ~TSS_OBJ_FLAG_SYSTEM_PS;
			break;
	}

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_tcs_handle(TSS_HKEY hKey, TCS_KEY_HANDLE tcsHandle)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	rsakey->tcsHandle = tcsHandle;

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_get_tcs_handle(TSS_HKEY hKey, TCS_KEY_HANDLE *tcsHandle)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	if (rsakey->tcsHandle)
		*tcsHandle = rsakey->tcsHandle;
	else
		result = TSPERR(TSS_E_KEY_NOT_LOADED);

	obj_list_put(&rsakey_list);

	return TSS_SUCCESS;
}

TSS_RESULT
obj_rsakey_set_tcpakey(TSS_HKEY hKey, UINT32 size, BYTE *data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	UINT64 offset;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	offset = 0;
	if ((result = Trspi_UnloadBlob_KEY(&offset, data, (TCPA_KEY *)&rsakey->key)))
		goto done;

	if (rsakey->key.authDataUsage)
		obj->flags |= TSS_OBJ_FLAG_USAGEAUTH;
	else
		obj->flags &= ~TSS_OBJ_FLAG_USAGEAUTH;

	obj->flags |= TSS_OBJ_FLAG_KEY_SET;

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_pcr_atcreation(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;
	TPM_PCR_INFO *info;
	TPM_PCR_INFO_LONG *info_long;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if (rsakey->key.PCRInfo == NULL) {
		*data = NULL;
		*size = 0;
	} else {
		*data = calloc_tspi(obj->tspContext, sizeof(TCPA_DIGEST));
		if (*data == NULL) {
			LogError("malloc of %zd bytes failed.", sizeof(TCPA_DIGEST));
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = sizeof(TCPA_DIGEST);

		if (rsakey->type == TSS_KEY_STRUCT_KEY) { /* 1.1 */
			info = (TPM_PCR_INFO *)rsakey->key.PCRInfo;
			offset = 0;
			Trspi_LoadBlob(&offset, sizeof(TCPA_DIGEST), *data,
				       (BYTE *)&info->digestAtCreation);
		} else {
			info_long = (TPM_PCR_INFO_LONG *)rsakey->key.PCRInfo;
			offset = 0;
			Trspi_LoadBlob(&offset, sizeof(TCPA_DIGEST), *data,
				       (BYTE *)&info_long->digestAtCreation);
		}
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_pcr_atrelease(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT64 offset;
	TCPA_PCR_INFO *info;
	TPM_PCR_INFO_LONG *info_long;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if (rsakey->key.PCRInfo == NULL) {
		*data = NULL;
		*size = 0;
	} else {
		*data = calloc_tspi(obj->tspContext, sizeof(TCPA_DIGEST));
		if (*data == NULL) {
			LogError("malloc of %zd bytes failed.", sizeof(TCPA_DIGEST));
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = sizeof(TCPA_DIGEST);

		if (rsakey->type == TSS_KEY_STRUCT_KEY) { /* 1.1 */
			info = (TCPA_PCR_INFO *)rsakey->key.PCRInfo;
			offset = 0;
			Trspi_LoadBlob(&offset, sizeof(TCPA_DIGEST), *data,
					(BYTE *)&info->digestAtRelease);
		} else {
			info_long = (TPM_PCR_INFO_LONG *)rsakey->key.PCRInfo;
			offset = 0;
			Trspi_LoadBlob(&offset, sizeof(TCPA_DIGEST), *data,
					(BYTE *)&info_long->digestAtRelease);
		}
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_get_pcr_selection(TSS_HKEY hKey, UINT32 *size, BYTE **data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	UINT16 offset;
	TCPA_PCR_INFO *info;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if (rsakey->type != TSS_KEY_STRUCT_KEY) {
		result = TSPERR(TSS_E_BAD_PARAMETER);
		goto done;
	}

	if (rsakey->key.PCRInfo == NULL) {
		*data = NULL;
		*size = 0;
	} else {
		info = (TCPA_PCR_INFO *)rsakey->key.PCRInfo;
		offset = info->pcrSelection.sizeOfSelect;
		*data = calloc_tspi(obj->tspContext, offset);
		if (*data == NULL) {
			LogError("malloc of %hu bytes failed.", offset);
			result = TSPERR(TSS_E_OUTOFMEMORY);
			goto done;
		}
		*size = offset;
		memcpy(*data, &info->pcrSelection.pcrSelect, *size);
	}

done:
	obj_list_put(&rsakey_list);

	return result;
}


/* Expect a TPM_PUBKEY as is explained in the portable data section of the spec */
TSS_RESULT
obj_rsakey_set_pubkey(TSS_HKEY hKey, UINT32 force, BYTE *data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result;
	UINT64 offset = 0;
	TCPA_PUBKEY pub;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;

	/* Another SRK workaround. For all keys that aren't the SRK, disallow the setting of their
	 * pub key after creation. Since the SRK's pub isn't stored anywhere, allow its value to be
	 * set */
	if (!force && (obj->flags & TSS_OBJ_FLAG_KEY_SET)) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	if ((result = Trspi_UnloadBlob_PUBKEY(&offset, data, &pub)))
		return result;

	free(rsakey->key.pubKey.key);
	free(rsakey->key.algorithmParms.parms);

	memcpy(&rsakey->key.pubKey, &pub.pubKey, sizeof(TCPA_STORE_PUBKEY));
	memcpy(&rsakey->key.algorithmParms, &pub.algorithmParms, sizeof(TCPA_KEY_PARMS));
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_privkey(TSS_HKEY hKey, UINT32 force, UINT32 size, BYTE *data)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;
	void *to_free;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (!force && (obj->flags & TSS_OBJ_FLAG_KEY_SET)) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	to_free = rsakey->key.encData;

	rsakey->key.encData = calloc(1, size);
	if (rsakey->key.encData == NULL) {
		rsakey->key.encData = to_free; // restore
		LogError("malloc of %u bytes failed.", size);
		result = TSPERR(TSS_E_OUTOFMEMORY);
		goto done;
	}

	free(to_free);
	rsakey->key.encSize = size;
	memcpy(rsakey->key.encData, data, size);
done:
	obj_list_put(&rsakey_list);

	return result;
}

TSS_RESULT
obj_rsakey_set_pcr_data(TSS_HKEY hKey, TSS_HPCRS hPcrComposite)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	if (obj->flags & TSS_OBJ_FLAG_KEY_SET) {
		result = TSPERR(TSS_E_INVALID_OBJ_ACCESS);
		goto done;
	}

	rsakey = (struct tr_rsakey_obj *)obj->data;

	if ((result = obj_pcrs_create_info(hPcrComposite, &rsakey->key.PCRInfoSize,
					   &rsakey->key.PCRInfo)))
		goto done;

done:
	obj_list_put(&rsakey_list);

	return result;
}

void
rsakey_free(struct tr_rsakey_obj *rsakey)
{
	free(rsakey->key.algorithmParms.parms);
	free(rsakey->key.encData);
	free(rsakey->key.PCRInfo);
	free(rsakey->key.pubKey.key);
	free(rsakey);
}

/* Remove an individual rsakey object from the rsakey list with handle
 * equal to hObject. Clean up the TSP's key handle table. */
TSS_RESULT
obj_rsakey_remove(TSS_HOBJECT hObject, TSS_HCONTEXT tspContext)
{
	struct tsp_object *obj, *prev = NULL;
	struct obj_list *list = &rsakey_list;
	TSS_RESULT result = TSPERR(TSS_E_INVALID_HANDLE);

	MUTEX_LOCK(list->lock);

	for (obj = list->head; obj; prev = obj, obj = obj->next) {
		if (obj->handle == hObject) {
			/* validate tspContext */
			if (obj->tspContext != tspContext)
				break;

			rsakey_free(obj->data);
			if (prev)
				prev->next = obj->next;
			else
				list->head = obj->next;
			free(obj);
			result = TSS_SUCCESS;
			break;
		}
	}

	MUTEX_UNLOCK(list->lock);

	return result;
}

void
obj_list_rsakey_close(struct obj_list *list, TSS_HCONTEXT tspContext)
{
	struct tsp_object *index;
	struct tsp_object *next = NULL;
	struct tsp_object *toKill;
	struct tsp_object *prev = NULL;

	MUTEX_LOCK(list->lock);

	for (index = list->head; index; ) {
		next = index->next;
		if (index->tspContext == tspContext) {
			toKill = index;
			if (prev == NULL) {
				list->head = toKill->next;
			} else {
				prev->next = toKill->next;
			}

			rsakey_free(toKill->data);
			free(toKill);

			index = next;
		} else {
			prev = index;
			index = next;
		}
	}

	MUTEX_UNLOCK(list->lock);
}

TSS_RESULT
obj_rsakey_get_by_pub(UINT32 pub_size, BYTE *pub, TSS_HKEY *hKey)
{
	struct obj_list *list = &rsakey_list;
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	MUTEX_LOCK(list->lock);

	for (obj = list->head; obj; obj = obj->next) {
		rsakey = (struct tr_rsakey_obj *)obj->data;

		if (rsakey->key.pubKey.keyLength == pub_size &&
		    !memcmp(&rsakey->key.pubKey.key, pub, pub_size)) {
			*hKey = obj->handle;
			goto done;
		}
	}

	*hKey = 0;
done:
	MUTEX_UNLOCK(list->lock);

	return result;
}

TSS_RESULT
obj_rsakey_get_by_uuid(TSS_UUID *uuid, TSS_HKEY *hKey)
{
	struct obj_list *list = &rsakey_list;
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result = TSS_SUCCESS;

	MUTEX_LOCK(list->lock);

	for (obj = list->head; obj; obj = obj->next) {
		rsakey = (struct tr_rsakey_obj *)obj->data;

		if (!memcmp(&rsakey->uuid, uuid, sizeof(TSS_UUID))) {
			*hKey = obj->handle;
			goto done;
		}
	}

	result = TSPERR(TSS_E_PS_KEY_NOTFOUND);
done:
	MUTEX_UNLOCK(list->lock);

	return result;
}

void
obj_rsakey_remove_policy_refs(TSS_HPOLICY hPolicy, TSS_HCONTEXT tspContext)
{
	struct tsp_object *obj, *prev = NULL;
	struct obj_list *list = &rsakey_list;
	struct tr_rsakey_obj *rsakey;

	pthread_mutex_lock(&list->lock);

	for (obj = list->head; obj; prev = obj, obj = obj->next) {
		if (obj->tspContext != tspContext)
			continue;

		rsakey = (struct tr_rsakey_obj *)obj->data;
		if (rsakey->usagePolicy == hPolicy)
			rsakey->usagePolicy = NULL_HPOLICY;

		if (rsakey->migPolicy == hPolicy)
			rsakey->migPolicy = NULL_HPOLICY;
	}

	pthread_mutex_unlock(&list->lock);
}

TSS_RESULT
obj_rsakey_get_transport_attribs(TSS_HKEY hKey, TCS_KEY_HANDLE *hTCSKey, TPM_DIGEST *pubDigest)
{
	struct tsp_object *obj;
	struct tr_rsakey_obj *rsakey;
	TSS_RESULT result;
	Trspi_HashCtx hashCtx;

	if ((obj = obj_list_get_obj(&rsakey_list, hKey)) == NULL)
		return TSPERR(TSS_E_INVALID_HANDLE);

	rsakey = (struct tr_rsakey_obj *)obj->data;
	*hTCSKey = rsakey->tcsHandle;

	result = Trspi_HashInit(&hashCtx, TSS_HASH_SHA1);
	result |= Trspi_Hash_STORE_PUBKEY(&hashCtx, &rsakey->key.pubKey);
	result |= Trspi_HashFinal(&hashCtx, pubDigest->digest);

	obj_list_put(&rsakey_list);

	return result;
}
