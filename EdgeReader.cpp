#include "EdgeReader.h"

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

queue<uint64_t> zero_row;

EdgeReader::EdgeReader(int id, 
					   uint64_t offset,
					   uint64_t a_col_size,
					   RowInfo row_info, 
					   DRAMInterface *dram) : q_space(0), prev_v(0), cur_v(0), pre_w_fold(0), remain_col_num(0) {
	this->id = id;
	this->offset = offset;
	this->row_info = row_info;
	this->dram = dram;
	req_address = A_COL_START + offset;
	flag.req_need = true;
	flag.q_empty = true;
	req_stat.tot_read_cnt = ceil((float)a_col_size/CACHE_LINE_COUNT);
	req_stat.pre_read_cnt = 0;
	req_stat.read_cnt_acm = 0;
	pre_row = row_info.row_start - 1;
}

EdgeReader::~EdgeReader() {}

// function activate order
// TransferData -> ReceiveData(queue) -> ReceiveData(vertex)

ERData EdgeReader::TransferData() {
	ERData ret;

	remain_col_num--;
	ret.rowindex = pre_row;
	ret.colindex = eq.front();
	eq.pop();
	q_space--;

	if (eq.empty())
		flag.q_empty = true;
	if ((req_stat.pre_read_cnt < req_stat.tot_read_cnt) 
		&& (MAX_QUEUE_SIZE - q_space > CACHE_LINE_COUNT))
		flag.req_need = true;

	if (remain_col_num == 0)
		ret.is_end = true;
	else
		ret.is_end = false;

	return ret;
}

bool EdgeReader::IsEndRequest() {
	return (req_stat.pre_read_cnt == req_stat.tot_read_cnt) && (pre_w_fold < w_fold - 1) && (w_fold > 1);
}

bool EdgeReader::IsEndOperation() {
	return (req_stat.pre_read_cnt == req_stat.tot_read_cnt) && (pre_w_fold == w_fold - 1);
}

bool EdgeReader::IsEndColomn() {
	return (req_stat.read_cnt_acm == req_stat.tot_read_cnt) && flag.q_empty;
}

bool EdgeReader::CanVertexReceive() {
	return (remain_col_num == 0);
}

void EdgeReader::ReceiveData(queue<uint64_t> data) {
	//cout<<"ER) DATA RECEIVE"<<endl;
	req_stat.read_cnt_acm++;
	int bound = data.size();
	for (int i = 0; i < bound; i++) {
		eq.push(data.front());
		data.pop();
	}
	q_space += bound;
	flag.q_empty = false;
} 

void EdgeReader::ReceiveData(uint64_t vertex) {
	//cout<<"ER) VERTEX RECEIVE"<<endl;
	if (cur_v > vertex)
		prev_v = 0;
	else
		prev_v = cur_v;
	cur_v = vertex;
	remain_col_num = cur_v - prev_v;
	pre_row++;
	if (pre_row == row_info.row_end)
		pre_row = row_info.row_start - 1;
	if (remain_col_num == 0)
		zero_row.push(pre_row);
}

void EdgeReader::Request() {
	req_stat.pre_read_cnt++;
	if (MAX_QUEUE_SIZE - q_space < CACHE_LINE_COUNT)
		flag.req_need = false;
	dram->DRAMRequest(req_address, READ);
	//cout<<"ER) REQUEST. ADDRESS: "<<hex<<req_address<<endl;
	req_address += CACHE_LINE_BYTE;
}

void EdgeReader::ResetRequestStat() {
	req_stat.pre_read_cnt = 0;
	req_address = A_COL_START + offset;
}

void EdgeReader::TurnOffFlag() {
	flag.req_need = false;
}