/*** temporary file ***
	Must Be Revised 
*/
#include "XWReader.h"

using namespace std;

extern uint64_t CACHE_LINE_BYTE; 
extern uint64_t CACHE_LINE_COUNT;
extern uint64_t UNIT_BYTE;

extern uint64_t A_COL_START;  
extern uint64_t A_ROW_START;  
extern uint64_t X_START;  
extern uint64_t WEIGHT_START; 
extern uint64_t XW_START;
extern uint64_t AXW_START;

extern uint64_t w_h, w_w, x_h, x_w, a_w, a_h;

extern uint64_t w_fold;

extern int MECHA_TYPE;
extern int UNIT_W_READ;

XWReader::XWReader(int id, uint64_t a_col_size) : q_space(0), pre_w_fold(0), req_cnt(0) {
	this->id = id;
	this->a_col_size = a_col_size;
	if (MECHA_TYPE == 1)
		tot_req = a_col_size * w_fold;
	else if (MECHA_TYPE == 0)
		tot_req = a_col_size * UNIT_W_READ;
	flag.q_empty = true;
	flag.can_receive = true;
	count_up = false;
	count_reset = false;
	tot_repeat = ceil((float)w_fold/UNIT_W_READ);
	pre_repeat = 0;
}

XWReader::~XWReader() {}

ERData XWReader::TransferData() {
	ERData ret = xwq_archive.front();
	xwq_archive.pop();
	q_space -= 2;
	if (!IsEndOperation())
		flag.can_receive = true;

	return ret;
}

ERData XWReader::BasisTransferData() {
	ERData ret = xwq_archive.front();
	xwq_archive.pop();
	q_space -= 2;
	if (!IsEndOperation())
		flag.can_receive = true;

	return ret;
}

ERData XWReader::Request() {
	ERData ret = xwq.front();
	xwq.pop();
	req_cnt++;
	if (xwq.empty() && !IsEndOperation())
		flag.q_empty = true;

	if (MECHA_TYPE == 1)
	{
		pre_w_fold++;

		if (pre_w_fold == 1)
			count_reset = true;
		else {
			if (pre_w_fold == w_fold)
				pre_w_fold = 0;
			count_up = true;
		}
	}
	else if (MECHA_TYPE == 0)
	{
		pre_w_fold++;
		int limit_w_fold = pre_repeat * UNIT_W_READ + UNIT_W_READ;
		int start_w_fold = pre_repeat * UNIT_W_READ;

		if(limit_w_fold > w_fold)
			limit_w_fold = w_fold;

		if (pre_w_fold % UNIT_W_READ == 1) {
			count_reset = true;
		}
		else {
			if (pre_w_fold == limit_w_fold)
				pre_w_fold = start_w_fold;
			count_up = true;
		}
	}

	return ret;
}

bool XWReader::IsEndRequest() {
	return (req_cnt == tot_req) && (w_fold > 1) && (pre_repeat < tot_repeat - 1);
}

bool XWReader::IsEndOperation() {
	return (req_cnt == tot_req) && (pre_repeat == tot_repeat - 1);
}

bool XWReader::BasisEndOperation() {
	return (req_cnt == tot_req);
}

void XWReader::ReceiveData(ERData data) {
	//cout<<"XWR) RECEIVE DATA: "<<dec<<data.colindex<<endl;
	xwq.push(data);
	xwq_archive.push(data);
	q_space += 2;
	flag.q_empty = false;
	if (MAX_QUEUE_SIZE - q_space < 2)
		flag.can_receive = false;
}

void XWReader::ResetRequestStat() {
	req_cnt = 0;
	int remain_req = w_fold - (pre_repeat * UNIT_W_READ);
	if (remain_req < UNIT_W_READ)
		tot_req = remain_req;
	pre_repeat++;
	pre_w_fold = pre_repeat * UNIT_W_READ;
}

void XWReader::TurnOffFlag() {
	flag.can_receive = false;
}