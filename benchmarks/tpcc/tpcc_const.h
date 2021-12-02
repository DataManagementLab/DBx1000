#if TPCC_SMALL 
enum {
	W_ID,
	W_NAME,
	W_STREET_1,
	W_STREET_2,
	W_CITY,
	W_STATE,
	W_ZIP,
	W_TAX,
	W_YTD
};
enum {
	D_ID,
	D_W_ID,
	D_NAME,
	D_STREET_1,
	D_STREET_2,
	D_CITY,
	D_STATE,
	D_ZIP,
	D_TAX,
	D_YTD,
	D_NEXT_O_ID
};
enum {
	C_ID,
	C_D_ID,
	C_W_ID,
	C_MIDDLE,
	C_LAST,
	C_STATE,
	C_CREDIT,
	C_DISCOUNT,
	C_BALANCE,
	C_YTD_PAYMENT,
	C_PAYMENT_CNT
};
enum {
	H_C_ID,
	H_C_D_ID,
	H_C_W_ID,
	H_D_ID,
	H_W_ID,
	H_DATE,
	H_AMOUNT
};
enum {
	NO_O_ID,
	NO_D_ID,
	NO_W_ID
};
enum {
	O_ID,
	O_C_ID,
	O_D_ID,
	O_W_ID,
	O_ENTRY_D,
	O_CARRIER_ID,
	O_OL_CNT,
	O_ALL_LOCAL
};
enum {
	OL_O_ID,
	OL_D_ID,
	OL_W_ID,
	OL_NUMBER,
	OL_I_ID
};
enum {
	I_ID,
	I_IM_ID,
	I_NAME,
	I_PRICE,
	I_DATA
};
enum {
	S_I_ID,
	S_W_ID,
	S_QUANTITY,
	S_REMOTE_CNT
};

struct __attribute__((packed)) w_record {
	int64_t W_ID;
	char W_NAME[10];
	char W_STREET_1[20];
	char W_STREET_2[20];
	char W_CITY[20];
	char W_STATE[2];
	char W_ZIP[9];
	double W_TAX;
	double W_YTD;
};

struct __attribute__((packed)) d_record {
	int64_t D_ID;
	int64_t D_W_ID;
	char D_NAME[10];
	char D_STREET_1[20];
	char D_STREET_2[20];
	char D_CITY[20];
	char D_STATE[2];
	char D_ZIP[9];
	double D_TAX;
	double D_YTD;
	int64_t D_NEXT_O_ID;
};

struct __attribute__((packed)) c_record {
	int64_t C_ID;
	int64_t C_D_ID;
	int64_t C_W_ID;
	char C_MIDDLE[2];
	char C_LAST[16];
	char C_STATE[2];
	char C_CREDIT[2];
	int64_t  C_DISCOUNT;
	double  C_BALANCE;
	double  C_YTD_PAYMENT;
	uint64_t  C_PAYMENT_CNT;
};

struct __attribute__((packed)) h_record {
	int64_t H_C_ID;
	int64_t H_C_D_ID;
	int64_t H_C_W_ID;
	int64_t H_D_ID;
	int64_t H_W_ID;
	int64_t H_DATE;
	double H_AMOUNT;
};

struct __attribute__((packed)) o_record {
	int64_t O_ID;
	int64_t O_C_ID;
	int64_t O_D_ID;
	int64_t O_W_ID;
	int64_t O_ENTRY_D;
	int64_t O_CARRIER_ID;
	int64_t O_OL_CNT;
	int64_t O_ALL_LOCAL;
};

struct __attribute__((packed)) ol_record {
	int64_t OL_O_ID;
	int64_t OL_D_ID;
	int64_t OL_W_ID;
	int64_t OL_NUMBER;
	int64_t OL_I_ID;
};

struct __attribute__((packed)) no_record {
	int64_t NO_O_ID;
	int64_t NO_D_ID;
	int64_t NO_W_ID;
};

struct __attribute__((packed)) i_record {
	int64_t I_ID;
	int64_t I_IM_ID;
	char I_NAME[24];
	int64_t I_PRICE;
	char I_DATA[50];
};

struct __attribute__((packed)) s_record {
	int64_t S_I_ID;
	int64_t S_W_ID;
	int64_t S_QUANTITY;
	int64_t S_REMOTE_CNT;
};

#else 
enum {
	W_ID,
	W_NAME,
	W_STREET_1,
	W_STREET_2,
	W_CITY,
	W_STATE,
	W_ZIP,
	W_TAX,
	W_YTD
};
enum {
	D_ID,
	D_W_ID,
	D_NAME,
	D_STREET_1,
	D_STREET_2,
	D_CITY,
	D_STATE,
	D_ZIP,
	D_TAX,
	D_YTD,
	D_NEXT_O_ID
};
enum {
	C_ID,
	C_D_ID,
	C_W_ID,
	C_FIRST,
	C_MIDDLE,
	C_LAST,
	C_STREET_1,
	C_STREET_2,
	C_CITY,
	C_STATE,
	C_ZIP,
	C_PHONE,
	C_SINCE,
	C_CREDIT,
	C_CREDIT_LIM,
	C_DISCOUNT,
	C_BALANCE,
	C_YTD_PAYMENT,
	C_PAYMENT_CNT,
	C_DELIVERY_CNT,
	C_DATA
};
enum {
	H_C_ID,
	H_C_D_ID,
	H_C_W_ID,
	H_D_ID,
	H_W_ID,
	H_DATE,
	H_AMOUNT,
	H_DATA
};
enum {
	NO_O_ID,
	NO_D_ID,
	NO_W_ID
};
enum {
	O_ID,
	O_C_ID,
	O_D_ID,
	O_W_ID,
	O_ENTRY_D,
	O_CARRIER_ID,
	O_OL_CNT,
	O_ALL_LOCAL
};
enum {
	OL_O_ID,
	OL_D_ID,
	OL_W_ID,
	OL_NUMBER,
	OL_I_ID,
	OL_SUPPLY_W_ID,
	OL_DELIVERY_D,
	OL_QUANTITY,
	OL_AMOUNT,
	OL_DIST_INFO
};
enum {
	I_ID,
	I_IM_ID,
	I_NAME,
	I_PRICE,
	I_DATA
};
enum {
	S_I_ID,
	S_W_ID,
	S_QUANTITY,
	S_DIST_01,
	S_DIST_02,
	S_DIST_03,
	S_DIST_04,
	S_DIST_05,
	S_DIST_06,
	S_DIST_07,
	S_DIST_08,
	S_DIST_09,
	S_DIST_10,
	S_YTD,
	S_ORDER_CNT,
	S_REMOTE_CNT,
	S_DATA
};

struct __attribute__((packed)) w_record {
	int64_t W_ID;
	char W_NAME[10];
	char W_STREET_1[20];
	char W_STREET_2[20];
	char W_CITY[20];
	char W_STATE[2];
	char W_ZIP[9];
	double W_TAX;
	double W_YTD;
};

struct __attribute__((packed)) d_record {
	int64_t D_ID;
	int64_t D_W_ID;
	char D_NAME[10];
	char D_STREET_1[20];
	char D_STREET_2[20];
	char D_CITY[20];
	char D_STATE[2];
	char D_ZIP[9];
	double D_TAX;
	double D_YTD;
	int64_t D_NEXT_O_ID;
};

struct __attribute__((packed)) c_record {
	int64_t C_ID;
	int64_t C_D_ID;
	int64_t C_W_ID;
	char C_FIRST[16];
	char C_MIDDLE[2];
	char C_LAST[16];
	char C_STREET_1[20];
	char C_STREET_2[20];
	char C_CITY[20];
	char C_STATE[2];
	char C_ZIP[9];
	char C_PHONE[16];
	int64_t C_SINCE;
	char C_CREDIT[2];
	int64_t C_CREDIT_LIM;
	int64_t C_DISCOUNT;
	double C_BALANCE;
	double C_YTD_PAYMENT;
	uint64_t C_PAYMENT_CNT;
	uint64_t C_DELIVERY_CNT;
	char C_DATA[500];
};

struct __attribute__((packed)) h_record {
	int64_t H_C_ID;
	int64_t H_C_D_ID;
	int64_t H_C_W_ID;
	int64_t H_D_ID;
	int64_t H_W_ID;
	int64_t H_DATE;
	double H_AMOUNT;
	char H_DATA[24];
};

struct __attribute__((packed)) no_record {
	int64_t NO_O_ID;
	int64_t NO_D_ID;
	int64_t NO_W_ID;
};

struct __attribute__((packed)) o_record {
	int64_t O_ID;
	int64_t O_C_ID;
	int64_t O_D_ID;
	int64_t O_W_ID;
	int64_t O_ENTRY_D;
	int64_t O_CARRIER_ID;
	int64_t O_OL_CNT;
	int64_t O_ALL_LOCAL;
};

struct __attribute__((packed)) ol_record {
	int64_t OL_O_ID;
	int64_t OL_D_ID;
	int64_t OL_W_ID;
	int64_t OL_NUMBER;
	int64_t OL_I_ID;
	int64_t OL_SUPPLY_W_ID;
	int64_t OL_DELIVERY_D;
	int64_t OL_QUANTITY;
	double OL_AMOUNT;
	int64_t OL_DIST_INFO;
};

struct __attribute__((packed)) i_record {
	int64_t I_ID;
	int64_t I_IM_ID;
	char I_NAME[24];
	int64_t I_PRICE;
	char I_DATA[50];
};

struct __attribute__((packed)) s_record {
	int64_t S_I_ID;
	int64_t S_W_ID;
	int64_t S_QUANTITY;
	char S_DIST_01[24];
	char S_DIST_02[24];
	char S_DIST_03[24];
	char S_DIST_04[24];
	char S_DIST_05[24];
	char S_DIST_06[24];
	char S_DIST_07[24];
	char S_DIST_08[24];
	char S_DIST_09[24];
	char S_DIST_10[24];
	int64_t S_YTD;
	int64_t S_ORDER_CNT;
	int64_t S_REMOTE_CNT;
	char S_DATA[50];
};

#endif
