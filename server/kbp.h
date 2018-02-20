#define KBP_MAGIC	0x4B454348

#define KBP_T_CLOSE	1
#define KBP_T_HEAD_OK	2
#define KBP_T_

struct kbp_request {
	/* 0x4B454348 - "KECH" */
	uint32_t	magic;
	/* Request type */
	int8_t		type;
};

#define KBP_LENGTH_MAX	1024

struct kbp_reply {
	/* 0x4B454348 - "KECH" */
	uint32_t	magic;
	/* Data length in bytes */
	int32_t		length;
};
