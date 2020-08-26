#ifndef __XWREADER_H__
#define __XWREADER_H__

#include <iostream>
#include <queue>
#include "Accelerator.h"
#include "Common.h"

using namespace std;

struct XWRFlag {
	bool q_empty;
	bool can_receive;
};

class XWReader {
public:
	int pre_w_fold;
	bool count_up;
	bool count_reset;
	XWRFlag flag;
	XWReader(int id, uint64_t a_col_size);
	~XWReader();
	ERData TransferData();
	ERData BasisTransferData();
	ERData Request();
	bool IsEndRequest();
	bool IsEndOperation();
	bool BasisEndOperation();
	void ReceiveData(ERData data);
	void ResetRequestStat();
	void TurnOffFlag();
private:
	int id;
	uint64_t a_col_size;
	uint64_t tot_req;
	uint64_t req_cnt;
	uint64_t q_space;
	queue<ERData> xwq, xwq_archive;
};

#endif