
/*
 * Licensed Materials - Property of IBM
 *
 * trousers - An open source TCG Software Stack
 *
 * (C) Copyright International Business Machines Corp. 2004, 2005
 *
 */


#ifndef _SPI_UTILS_H_
#define _SPI_UTILS_H_

#include "threads.h"
#include <netinet/in.h> // for endian routines

struct key_mem_cache
{
	TCS_KEY_HANDLE tcs_handle;
	TSS_HKEY tsp_handle;
	UINT16 flags;
	UINT32 time_stamp;
	TSS_UUID uuid;
	TSS_UUID p_uuid;
	TCPA_KEY *blob;
	struct key_mem_cache *parent;
	struct key_mem_cache *next;
};

extern struct key_mem_cache *key_mem_cache_head;
MUTEX_DECLARE_EXTERN(mem_cache_lock);

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define BOOL(x)		((x) == 0) ? FALSE : TRUE
#define INVBOOL(x)	((x) == 0) ? TRUE : FALSE

#define INCREMENT	1
#define DECREMENT	0

UINT32 UnicodeToArray(BYTE *, UNICODE *);
UINT32 ArrayToUnicode(BYTE *, UINT32, UNICODE *);
UINT32 StringToUnicodeArray(char *, BYTE *);

void *calloc_tspi(TSS_HCONTEXT, UINT32);
TSS_RESULT free_tspi(TSS_HCONTEXT, void *);
TSS_RESULT add_mem_entry(TSS_HCONTEXT, void *);

/* secrets.c */

TSS_RESULT policy_UsesAuth(TSS_HPOLICY, TSS_BOOL *);

TSS_RESULT secret_PerformAuth_OIAP(TSS_HOBJECT, UINT32, TSS_HPOLICY, TSS_BOOL, TCPA_DIGEST *,
				   TPM_AUTH *);
TSS_RESULT secret_PerformXOR_OSAP(TSS_HPOLICY, TSS_HPOLICY, TSS_HPOLICY, TSS_HOBJECT,
				  UINT16, UINT32, TCPA_ENCAUTH *, TCPA_ENCAUTH *,
				  BYTE *, TPM_AUTH *, TCPA_NONCE *);
TSS_RESULT secret_PerformAuth_OSAP(TSS_HOBJECT, UINT32, TSS_HPOLICY,
				   TSS_HPOLICY, TSS_HPOLICY, BYTE *,
				   TPM_AUTH *, BYTE *, TCPA_NONCE *);

TSS_RESULT secret_ValidateAuth_OSAP(TSS_HOBJECT, UINT32, TSS_HPOLICY,
				    TSS_HPOLICY, TSS_HPOLICY, BYTE *,
				    TPM_AUTH *, BYTE *, TCPA_NONCE *);

TSS_RESULT secret_TakeOwnership(TSS_HKEY, TSS_HTPM, TSS_HKEY, TPM_AUTH *,
				UINT32 *, BYTE *, UINT32 *, BYTE *);

#define next( x )	x = x->next

/* spi_utils.c */

UINT16 get_num_pcrs(TCS_CONTEXT_HANDLE);
void   free_key_refs(TCPA_KEY *);

#define UI_MAX_SECRET_STRING_LENGTH	256
#define UI_MAX_POPUP_STRING_LENGTH	256

#ifdef TSS_NO_GUI
#define DisplayPINWindow(a,b,c)			\
	do {					\
		*(b) = 0;			\
	} while (0)
#define DisplayNewPINWindow(a,b,c)			\
	do {					\
		*(b) = 0;			\
	} while (0)
#else
TSS_RESULT DisplayPINWindow(BYTE *, UINT32 *, BYTE *);
TSS_RESULT DisplayNewPINWindow(BYTE *, UINT32 *, BYTE *);
#endif

TSS_RESULT merge_key_hierarchies(TSS_HCONTEXT, UINT32, TSS_KM_KEYINFO *, UINT32, TSS_KM_KEYINFO *,
				 UINT32 *, TSS_KM_KEYINFO **);

int pin_mem(void *, size_t);
int unpin_mem(void *, size_t);


#define TSS_MAX_SYM_BLOCK_SIZE	16

TSS_RESULT internal_GetMachineName(UNICODE *, int);
TSS_RESULT internal_GetCap(TSS_HCONTEXT, TSS_FLAG, UINT32, UINT32 *, BYTE **);

/* For an unconnected context that wants to do PCR operations, assume that
 * the TPM has TSS_DEFAULT_NUM_PCRS pcrs */
#define TSS_DEFAULT_NUM_PCRS		16
#define TSS_LOCAL_RANDOM_DEVICE		"/dev/urandom"
#define TSS_LOCALHOST_STRING		"localhost"
TSS_RESULT get_local_random(TSS_HCONTEXT, TSS_BOOL, UINT32, BYTE **);

#define AUTH_RETRY_NANOSECS	500000000
#define AUTH_RETRY_COUNT	5
#define TPM_AUTH_RQU_SIZE	(sizeof(TPM_AUTHHANDLE) + sizeof(TPM_NONCE) \
				 + sizeof(TPM_BOOL) + sizeof(TPM_AUTHDATA))
#define TPM_AUTH_RSP_SIZE	(sizeof(TPM_NONCE) + sizeof(TPM_BOOL) + sizeof(TPM_AUTHDATA))

#define endian32(x)	htonl(x)
#define endian16(x)	htons(x)

extern TSS_VERSION VERSION_1_1;

TSS_RESULT rsa_encrypt(TSS_HKEY, UINT32, BYTE*, UINT32*, BYTE*);
TSS_RESULT rsa_verify(TSS_HKEY, UINT32, UINT32, BYTE*, UINT32, BYTE*);

TSS_RESULT Init_AuthNonce(TCS_CONTEXT_HANDLE, TSS_BOOL, TPM_AUTH *);
TSS_BOOL validateReturnAuth(BYTE *, BYTE *, TPM_AUTH *);
void HMAC_Auth(BYTE *, BYTE *, TPM_AUTH *);
TSS_RESULT OSAP_Calc(TCS_CONTEXT_HANDLE, UINT16, UINT32, BYTE *, BYTE *, BYTE *,
			TCPA_ENCAUTH *, TCPA_ENCAUTH *, BYTE *, TPM_AUTH *);

void UINT64ToArray(UINT64, BYTE *);
void UINT32ToArray(UINT32, BYTE *);
void UINT16ToArray(UINT16, BYTE *);
UINT16 Decode_UINT16(BYTE *);
UINT32 Decode_UINT32(BYTE *);
UINT64 Decode_UINT64(BYTE *);

TSS_RESULT popup_GetSecret(UINT32, UINT32, BYTE *, void *);

TSS_BOOL check_flagset_collision(TSS_FLAG, UINT32);
TSS_RESULT get_tpm_flags(TCS_CONTEXT_HANDLE, TSS_HTPM, UINT32 *, UINT32 *);
TSS_RESULT pcrs_calc_composite(TCPA_PCR_SELECTION *, TCPA_PCRVALUE *, TCPA_DIGEST *);
struct tr_pcrs_obj;
TSS_RESULT pcrs_sanity_check_selection(TCS_CONTEXT_HANDLE, struct tr_pcrs_obj *, TPM_PCR_SELECTION *);

void LoadBlob_AUTH(UINT64 *, BYTE *, TPM_AUTH *);
void UnloadBlob_AUTH(UINT64 *, BYTE *, TPM_AUTH *);
void LoadBlob_LOADKEY_INFO(UINT64 *, BYTE *, TCS_LOADKEY_INFO *);
void UnloadBlob_LOADKEY_INFO(UINT64 *, BYTE *, TCS_LOADKEY_INFO *);


TSS_RESULT TSP_SetCapability(TSS_HCONTEXT, TSS_HTPM, TSS_HPOLICY, TPM_CAPABILITY_AREA,
			     UINT32, TSS_BOOL);

TSS_RESULT TCS_CloseContext(TSS_HCONTEXT);
TSS_RESULT TCS_OpenContext_RPC(TSS_HCONTEXT, BYTE *, int);
TSS_RESULT TCS_GetCapability(TSS_HCONTEXT, TCPA_CAPABILITY_AREA, UINT32, BYTE *,
                              UINT32 *, BYTE **);
TSS_RESULT TCSP_GetCapability(TSS_HCONTEXT, TCPA_CAPABILITY_AREA,	UINT32, BYTE *, UINT32 *,
				BYTE **);
TSS_RESULT TCSP_SetCapability(TSS_HCONTEXT, TCPA_CAPABILITY_AREA,	UINT32, BYTE *, UINT32,
				BYTE *, TPM_AUTH *);
TSS_RESULT Transport_LoadKeyByBlob(TSS_HCONTEXT, TPM_COMMAND_CODE, TSS_HKEY, UINT32, BYTE *,
				   TPM_AUTH *, TCS_KEY_HANDLE *, TPM_KEY_HANDLE *);
TSS_RESULT TCSP_LoadKeyByBlob(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, TPM_AUTH *,
                               TCS_KEY_HANDLE *, TCS_KEY_HANDLE *);
TSS_RESULT TCSP_LoadKeyByUUID(TSS_HCONTEXT, TSS_UUID, TCS_LOADKEY_INFO *, TCS_KEY_HANDLE *);
TSS_RESULT TCS_GetRegisteredKeyBlob(TSS_HCONTEXT, TSS_UUID, UINT32 *, BYTE **);
TSS_RESULT TCS_RegisterKey(TSS_HCONTEXT, TSS_UUID, TSS_UUID, UINT32, BYTE *, UINT32, BYTE *);
TSS_RESULT TCSP_UnregisterKey(TSS_HCONTEXT, TSS_UUID);
TSS_RESULT TCS_EnumRegisteredKeys(TSS_HCONTEXT, TSS_UUID *, UINT32 *, TSS_KM_KEYINFO **);
TSS_RESULT TCSP_GetRegisteredKeyByPublicInfo(TSS_HCONTEXT, TCPA_ALGORITHM_ID, UINT32,
                                              BYTE *, UINT32 *, BYTE **);
TSS_RESULT TCSP_ChangeAuth(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_PROTOCOL_ID, TCPA_ENCAUTH,
				TCPA_ENTITY_TYPE, UINT32, BYTE *, TPM_AUTH *, TPM_AUTH *,
	                        UINT32 *, BYTE **);
TSS_RESULT TCSP_ChangeAuthOwner(TSS_HCONTEXT, TCPA_PROTOCOL_ID, TCPA_ENCAUTH, TCPA_ENTITY_TYPE,
                                 TPM_AUTH *);
TSS_RESULT TCSP_TerminateHandle(TSS_HCONTEXT, TCS_AUTHHANDLE);
TSS_RESULT TCSP_GetRandom(TSS_HCONTEXT, UINT32, BYTE **);
TSS_RESULT TCSP_ChangeAuthAsymStart(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_NONCE, UINT32, BYTE *,
                                     TPM_AUTH *, UINT32 *, BYTE **, UINT32 *, BYTE **, UINT32 *,
                                     BYTE **, TCS_KEY_HANDLE *);
TSS_RESULT TCSP_ChangeAuthAsymFinish(TSS_HCONTEXT, TCS_KEY_HANDLE, TCS_KEY_HANDLE,
					TCPA_ENTITY_TYPE, TCPA_HMAC, UINT32, BYTE *, UINT32,
					BYTE *, TPM_AUTH *, UINT32 *, BYTE **, TCPA_SALT_NONCE *,
					TCPA_DIGEST *);
TSS_RESULT TCSP_GetPubKey(TSS_HCONTEXT, TCS_KEY_HANDLE, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_CreateWrapKey(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_ENCAUTH, TCPA_ENCAUTH,
				UINT32, BYTE *, UINT32 *, BYTE **, TPM_AUTH *);
TSS_RESULT TCSP_CertifyKey(TSS_HCONTEXT, TCS_KEY_HANDLE, TCS_KEY_HANDLE, TCPA_NONCE, TPM_AUTH *,
				TPM_AUTH *, UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_CreateMigrationBlob(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_MIGRATE_SCHEME, UINT32,
					BYTE *, UINT32, BYTE *, TPM_AUTH *, TPM_AUTH *, UINT32 *,
					BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_ConvertMigrationBlob(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, UINT32,
				     BYTE *, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_PcrRead(TSS_HCONTEXT, TCPA_PCRINDEX, TCPA_PCRVALUE *);
TSS_RESULT TCSP_PcrReset(TSS_HCONTEXT, UINT32, BYTE *);
TSS_RESULT TCSP_OSAP(TSS_HCONTEXT, TCPA_ENTITY_TYPE, UINT32, TCPA_NONCE, TCS_AUTHHANDLE *,
			TCPA_NONCE *, TCPA_NONCE *);
TSS_RESULT TCSP_GetCapabilityOwner(TSS_HCONTEXT, TPM_AUTH *, TCPA_VERSION *, UINT32 *, UINT32 *);
TSS_RESULT TCSP_OIAP(TSS_HCONTEXT, TCS_AUTHHANDLE *, TCPA_NONCE *);
TSS_RESULT TCSP_Seal(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_ENCAUTH, UINT32, BYTE *, UINT32, BYTE *,
                                       TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_Sealx(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_ENCAUTH, UINT32, BYTE *, UINT32, BYTE *,
                                       TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_Unseal(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, TPM_AUTH *, TPM_AUTH *,
                                         UINT32 *, BYTE **);
TSS_RESULT TCSP_UnBind(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, TPM_AUTH *, UINT32 *,
                                         BYTE **);
TSS_RESULT TCSP_Sign(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_CreateEndorsementKeyPair(TSS_HCONTEXT, TCPA_NONCE, UINT32, BYTE *, UINT32 *,
						BYTE **, TCPA_DIGEST *);
TSS_RESULT TCSP_ReadPubek(TSS_HCONTEXT, TCPA_NONCE, UINT32 *, BYTE **, TCPA_DIGEST *);
TSS_RESULT TCSP_OwnerReadPubek(TSS_HCONTEXT, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_TakeOwnership(TSS_HCONTEXT, UINT16, UINT32, BYTE *, UINT32, BYTE *, UINT32,
				BYTE *, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_MakeIdentity(TSS_HCONTEXT, TCPA_ENCAUTH, TCPA_CHOSENID_HASH, UINT32, BYTE *,
				TPM_AUTH *, TPM_AUTH *, UINT32 *, BYTE **, UINT32 *, BYTE **, UINT32 *,
				BYTE **, UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_ActivateTPMIdentity(TSS_HCONTEXT, TCS_KEY_HANDLE, UINT32, BYTE *, TPM_AUTH *,
					TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_OwnerClear(TSS_HCONTEXT, TPM_AUTH *);
TSS_RESULT TCSP_DisableOwnerClear(TSS_HCONTEXT, TPM_AUTH *);
TSS_RESULT TCSP_ForceClear(TSS_HCONTEXT);
TSS_RESULT TCSP_DisableForceClear(TSS_HCONTEXT);
TSS_RESULT TCSP_PhysicalDisable(TSS_HCONTEXT);
TSS_RESULT TCSP_PhysicalEnable(TSS_HCONTEXT);
TSS_RESULT TCSP_PhysicalSetDeactivated(TSS_HCONTEXT, TSS_BOOL);
TSS_RESULT TCSP_PhysicalPresence(TSS_HCONTEXT, TCPA_PHYSICAL_PRESENCE);
TSS_RESULT TCSP_SetTempDeactivated(TSS_HCONTEXT);
TSS_RESULT TCSP_OwnerSetDisable(TSS_HCONTEXT, TSS_BOOL, TPM_AUTH *);
TSS_RESULT TCSP_ResetLockValue(TSS_HCONTEXT, TPM_AUTH *);
TSS_RESULT TCSP_SetOwnerInstall(TSS_HCONTEXT, TSS_BOOL);
TSS_RESULT TCSP_DisablePubekRead(TSS_HCONTEXT, TPM_AUTH *);
TSS_RESULT TCSP_SelfTestFull(TSS_HCONTEXT);
TSS_RESULT TCSP_CertifySelfTest(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_NONCE, TPM_AUTH *, UINT32 *,
				BYTE **);
TSS_RESULT TCSP_GetTestResult(TSS_HCONTEXT, UINT32 *, BYTE **);
TSS_RESULT TCSP_StirRandom(TSS_HCONTEXT, UINT32, BYTE *);
TSS_RESULT TCSP_AuthorizeMigrationKey(TSS_HCONTEXT, TCPA_MIGRATE_SCHEME, UINT32, BYTE *,
					TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCS_GetPcrEvent(TSS_HCONTEXT, UINT32, UINT32 *, TSS_PCR_EVENT **);
TSS_RESULT TCS_GetPcrEventsByPcr(TSS_HCONTEXT, UINT32, UINT32, UINT32 *, TSS_PCR_EVENT **);
TSS_RESULT TCS_GetPcrEventLog(TSS_HCONTEXT, UINT32 *, TSS_PCR_EVENT **);
TSS_RESULT TCSP_Quote(TSS_HCONTEXT, TCS_KEY_HANDLE, TCPA_NONCE, UINT32, BYTE *, TPM_AUTH *,
			UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_Extend(TSS_HCONTEXT, TCPA_PCRINDEX, TCPA_DIGEST, TCPA_PCRVALUE *);
TSS_RESULT TCSP_DirWriteAuth(TSS_HCONTEXT, TCPA_DIRINDEX, TCPA_DIRVALUE, TPM_AUTH *);
TSS_RESULT TCSP_DirRead(TSS_HCONTEXT, TCPA_DIRINDEX, TCPA_DIRVALUE *);
TSS_RESULT TCS_LogPcrEvent(TSS_HCONTEXT, TSS_PCR_EVENT, UINT32 *);
TSS_RESULT TCSP_EvictKey(TSS_HCONTEXT, TCS_KEY_HANDLE);
TSS_RESULT TCSP_CreateMaintenanceArchive(TSS_HCONTEXT, TSS_BOOL, TPM_AUTH *, UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_KillMaintenanceFeature(TSS_HCONTEXT, TPM_AUTH *);
TSS_RESULT TCSP_LoadMaintenanceArchive(TSS_HCONTEXT, UINT32, BYTE *, TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_LoadManuMaintPub(TSS_HCONTEXT, TCPA_NONCE, UINT32, BYTE *, TCPA_DIGEST *);
TSS_RESULT TCSP_ReadManuMaintPub(TSS_HCONTEXT, TCPA_NONCE, TCPA_DIGEST *);
TSS_RESULT TCSP_DaaJoin(TSS_HCONTEXT,  TPM_HANDLE, BYTE, UINT32, BYTE *, UINT32, BYTE *,
			TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_DaaSign(TSS_HCONTEXT,  TPM_HANDLE, BYTE, UINT32, BYTE *, UINT32, BYTE *,
			TPM_AUTH *, UINT32 *, BYTE **);
TSS_RESULT TCSP_ReadCounter(TSS_HCONTEXT, TSS_COUNTER_ID, TPM_COUNTER_VALUE *);
TSS_RESULT TCSP_CreateCounter(TSS_HCONTEXT, UINT32, BYTE *, TPM_ENCAUTH, TPM_AUTH *,
			      TSS_COUNTER_ID *, TPM_COUNTER_VALUE *);
TSS_RESULT TCSP_IncrementCounter(TSS_HCONTEXT, TSS_COUNTER_ID, TPM_AUTH *, TPM_COUNTER_VALUE *);
TSS_RESULT TCSP_ReleaseCounter(TSS_HCONTEXT, TSS_COUNTER_ID, TPM_AUTH *);
TSS_RESULT TCSP_ReleaseCounterOwner(TSS_HCONTEXT, TSS_COUNTER_ID, TPM_AUTH *);
TSS_RESULT TCSP_ReadCurrentTicks(TSS_HCONTEXT, UINT32 *, BYTE **);
TSS_RESULT TCSP_TickStampBlob(TSS_HCONTEXT, TCS_KEY_HANDLE, TPM_NONCE *, TPM_DIGEST *, TPM_AUTH *, UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_EstablishTransport(TSS_HCONTEXT, UINT32, TCS_KEY_HANDLE, UINT32, BYTE *, UINT32, BYTE *, TPM_AUTH *, TPM_MODIFIER_INDICATOR *, TCS_HANDLE *, UINT32 *, BYTE **, TPM_NONCE *);
TSS_RESULT TCSP_ExecuteTransport(TSS_HCONTEXT, TPM_COMMAND_CODE, UINT32, BYTE *, UINT32 *, TCS_HANDLE **, TPM_AUTH *, TPM_AUTH *, TPM_AUTH *, UINT64 *, TPM_MODIFIER_INDICATOR *, TPM_RESULT *, UINT32 *, BYTE **);
TSS_RESULT TCSP_ReleaseTransportSigned(TSS_HCONTEXT, TCS_KEY_HANDLE, TPM_NONCE *, TPM_AUTH *, TPM_AUTH *, TPM_MODIFIER_INDICATOR *, UINT32 *, BYTE **, UINT32 *, BYTE **);
TSS_RESULT TCSP_NV_DefineOrReleaseSpace(TSS_HCONTEXT, UINT32, BYTE *, TCPA_ENCAUTH, TPM_AUTH *);
TSS_RESULT TCSP_NV_WriteValue(TSS_HCONTEXT, TSS_NV_INDEX, UINT32, UINT32, BYTE *, TPM_AUTH *);
TSS_RESULT TCSP_NV_WriteValueAuth(TSS_HCONTEXT, TSS_NV_INDEX, UINT32, UINT32, BYTE *, TPM_AUTH *);
TSS_RESULT TCSP_NV_ReadValue(TSS_HCONTEXT, TSS_NV_INDEX, UINT32, UINT32 *, TPM_AUTH *, BYTE **);
TSS_RESULT TCSP_NV_ReadValueAuth(TSS_HCONTEXT, TSS_NV_INDEX, UINT32, UINT32 *, TPM_AUTH *, BYTE **);
TSS_RESULT TCSP_SetOrdinalAuditStatus(TCS_CONTEXT_HANDLE, TPM_AUTH *, UINT32, TSS_BOOL);
TSS_RESULT TCSP_GetAuditDigest(TCS_CONTEXT_HANDLE, UINT32, TPM_DIGEST *, UINT32 *, BYTE **, TSS_BOOL *, UINT32 *, UINT32 **);
TSS_RESULT TCSP_GetAuditDigestSigned(TCS_CONTEXT_HANDLE, TCS_KEY_HANDLE, TSS_BOOL, TPM_NONCE, TPM_AUTH *, UINT32 *,
				     BYTE **, TPM_DIGEST *, TPM_DIGEST *, UINT32 *, BYTE **);
TSS_RESULT TCSP_SetOperatorAuth(TCS_CONTEXT_HANDLE, TCPA_SECRET *);
#endif
