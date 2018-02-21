#define KBP_MAGIC	0x4B454348

/* Limits */
#define KBP_ERROR_MAX	10
#define KBP_LENGTH_MAX	1024
#define KBP_BALANCE_MAX	34

/* Request types */
#define KBP_T_CLOSE	1
#define KBP_T_HEAD_OK	2
/* #define KBP_T_CHECKPIN	3 */
/* #define KBP_T_UPDATEPIN	4 */
#define KBP_T_TRANSFER	5
#define KBP_T_BALANCE	6
/* #define KBP_T_CNTTRANS	7
#define KBP_T_GETTRANS	8 */

/* Reply statuses */
#define KBP_S_TIMEOUT	-2
#define KBP_S_INVALID	-1
#define KBP_S_FAILED	0
#define KBP_S_OK	1

struct kbp_request {
	/* 0x4B454348 - "KECH" */
	uint32_t	magic;
	/* Request type */
	uint8_t		type;
	/* Data length in bytes */
	int32_t		length;
};

struct kbp_reply {
	/* 0x4B454348 - "KECH" */
	uint32_t	magic;
	/* Data length in bytes */
	int32_t		length;
	/* Status */
	int8_t		status;
};

/* KBP_T_TRANSFER - Tranfer money to other account */
struct kbp_transfer {
	char		iban_in[KBP_IBAN_MAX + 1];
	char		iban_out[KBP_IBAN_MAX + 1];
	/* Amount is stored * 100 (2 decimals */
	int64_t		amount;
};

/* KBP_T_BALANCE - Retrieve balance */
struct kbp_balance {
	char balance[KBP_BALANCE_MAX + 1];
};
