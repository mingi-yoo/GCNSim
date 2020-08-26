#include "BasisEdgeReader.h"

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

extern queue<uint64_t> zero_row;

BasisEdgeReader::BasisEdgeReader(int id, 
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
	col_num_archive = 0;
}

BasisEdgeReader::~BasisEdgeReader() {}

ERData BasisEdgeReader::TransferData() {
	ERData ret;

	ret.rowindex = pre_row;
	if (pre_w_fold == 0) {
		ev.push_back(eq.front());
		eq.pop();
	}
	ret.colindex = ev[col_num_archive - remain_col_num];
	remain_col_num--;

	if ((pre_w_fold == w_fold - 1) && eq.empty() && remain_col_num == 0)
		flag.q_empty = true;
	if (pre_w_fold == 0 && eq.empty() && remain_col_num != 0)
		flag.q_empty = true;
	if ((req_stat.pre_read_cnt < req_stat.tot_read_cnt) 
		&& (MAX_QUEUE_SIZE - q_space > CACHE_LINE_COUNT))
		flag.req_need = true;

	if (remain_col_num == 0) {
		ret.is_end = true;
		if (pre_w_fold == w_fold - 1 && !IsEndOperation()) {
			q_space -= ev.size();
			ev.clear();
			pre_w_fold = 0;
		}
		else {
			remain_col_num = col_num_archive;
			pre_w_fold++;
		}
	}
	else
		ret.is_end = false;

	return ret;
}

bool BasisEdgeReader::IsEndOperation() {
	return (req_stat.pre_read_cnt == req_stat.tot_read_cnt) && (pre_w_fold == w_fold - 1);
}

bool BasisEdgeReader::IsEndColomn() {
	return (req_stat.read_cnt_acm == req_stat.tot_read_cnt) && flag.q_empty;
}

bool BasisEdgeReader::CanVertexReceive() {
	return (remain_col_num == 0);
}

void BasisEdgeReader::ReceiveData(queue<uint64_t> data) {
	req_stat.read_cnt_acm++;
	int bound = data.size();
	for (int i = 0; i < bound; i++) {
		eq.push(data.front());
		data.pop();
	}
	q_space += bound;
	flag.q_empty = false;
}

void BasisEdgeReader::ReceiveData(uint64_t vertex) {
	prev_v = cur_v;
	cur_v = vertex;
	remain_col_num = cur_v - prev_v;
	col_num_archive = remain_col_num;
	pre_row++;
	if (remain_col_num == 0) {
		for (int i = 0; i < w_fold; i++)
			zero_row.push(pre_row);
	}
}

void BasisEdgeReader::Request() {
	req_stat.pre_read_cnt++;
	if (MAX_QUEUE_SIZE - q_space < CACHE_LINE_COUNT)
		flag.req_need = false;
	dram->DRAMRequest(req_address, READ);
	req_address += CACHE_LINE_BYTE;
}

void BasisEdgeReader::TurnOffFlag() {
	flag.req_need = false;
}