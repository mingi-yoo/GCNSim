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

extern int UNIT_W_READ;

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
	this->a_col_size = a_col_size;
	req_address = A_COL_START + offset;
	flag.req_need = true;
	flag.q_empty = true;
	req_stat.tot_read_cnt = ceil((float)a_col_size/CACHE_LINE_COUNT);
	req_stat.pre_read_cnt = 0;
	req_stat.read_cnt_acm = 0;
	pre_row = row_info.row_start - 1;
	col_num_archive = 0;
	can_receive = true;
	transfer_cnt = 0;
	tot_repeat = ceil((float)w_fold/UNIT_W_READ);
	tot_req_repeat = tot_repeat;
	pre_repeat = 0;
	pre_req_repeat = 0;
	pre_w_fold_start = 0;
}

EdgeReader::~EdgeReader() {}

// function activate order
// TransferData -> ReceiveData(queue) -> ReceiveData(vertex)

ERData EdgeReader::TransferData() {
	ERData ret;
	int limit_w_fold = pre_repeat * UNIT_W_READ + UNIT_W_READ;
	int start_w_fold = pre_repeat * UNIT_W_READ;

	transfer_cnt++;

	if (limit_w_fold > w_fold)
		limit_w_fold = w_fold;

	ret.rowindex = pre_row;
	if (pre_w_fold % UNIT_W_READ == 0) {
		ev.push_back(eq.front());
		eq.pop();
	}
	ret.colindex = ev[col_num_archive - remain_col_num];
	ret.pre_w_fold = pre_w_fold;
	ret.pre_w_fold_start = pre_w_fold_start;
	ret.pre_repeat = pre_repeat;
	ret.address = XW_START + (x_h*pre_repeat*UNIT_W_READ + ret.colindex*UNIT_W_READ + (pre_w_fold - pre_w_fold_start))*CACHE_LINE_BYTE;
	
	if ((req_stat.pre_read_cnt < req_stat.tot_read_cnt) 
		&& (MAX_QUEUE_SIZE - q_space > CACHE_LINE_COUNT))
		flag.req_need = true;

	pre_w_fold++;

	if (remain_col_num == 1) {
		ret.is_end = true;
		if (pre_w_fold == limit_w_fold) {
			q_space -= ev.size();
			ev.clear();
			pre_w_fold = start_w_fold;
			remain_col_num--;
			if (!(pre_row == row_info.row_end))
				can_receive = true;
			else
				flag.q_empty = true;
		}
	}
	else {
		if (pre_w_fold == limit_w_fold) {
			pre_w_fold = start_w_fold;
			remain_col_num--;
		}	
		ret.is_end = false;
	}

	if (pre_w_fold == start_w_fold && eq.empty())
		flag.q_empty = true;

	if (transfer_cnt == a_col_size * UNIT_W_READ) {
		pre_repeat++;
		pre_w_fold = pre_repeat * UNIT_W_READ;
		pre_w_fold_start = pre_w_fold;
		transfer_cnt = 0;
	}

	return ret;
}

bool EdgeReader::IsEndRequest() {
	return (req_stat.pre_read_cnt == req_stat.tot_read_cnt) && (pre_req_repeat < tot_req_repeat - 1);
}

bool EdgeReader::IsEndOperation() {
	return (req_stat.pre_read_cnt == req_stat.tot_read_cnt) && (pre_req_repeat == tot_req_repeat - 1);
}

bool EdgeReader::CanVertexReceive() {
	return can_receive;
}

void EdgeReader::ReceiveData(queue<uint64_t> data) {
	//cout<<"ER) DATA RECEIVE"<<endl;
	req_stat.read_cnt_acm++;
	int bound = data.size();
	for (int i = 0; i < bound; i++) {
		eq.push(data.front());
		data.pop();
	}
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
	col_num_archive = remain_col_num;
	pre_row++;
	can_receive = false;
	if (pre_row == row_info.row_end)
		pre_row = row_info.row_start - 1;
	if (remain_col_num == 0) {
		for (int i = 0; i < UNIT_W_READ; i++)
			zero_row.push(pre_row);
		can_receive = true;
	}
}

void EdgeReader::Request() {
	q_space += CACHE_LINE_COUNT;
	req_stat.pre_read_cnt++;
	if ((MAX_QUEUE_SIZE - q_space < CACHE_LINE_COUNT) ||
		(req_stat.pre_read_cnt == req_stat.tot_read_cnt))
		flag.req_need = false;
	dram->DRAMRequest(req_address, READ);
	//cout<<"ER) REQUEST. ADDRESS: "<<hex<<req_address<<endl;
	req_address += CACHE_LINE_BYTE;
}

void EdgeReader::ResetRequestStat() {
	req_stat.pre_read_cnt = 0;
	req_address = A_COL_START + offset;
	pre_req_repeat++;
}

void EdgeReader::TurnOffFlag() {
	flag.req_need = false;
}