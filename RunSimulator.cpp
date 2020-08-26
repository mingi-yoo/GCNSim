/*
 * Simulator Main
*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include "IniParser.h"
#include "DataReader.h"
#include "DRAMInterface.h"
#include "DataController.h"
#include "GlobalBuffer.h"
#include "Distributer.h"
#include "Accelerator.h"
#include "VertexReader.h"
#include "XWReader.h"
#include "EdgeReader.h"
#include "BasisEdgeReader.h"
#include "Common.h"

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

extern int MAX_DRAM;
extern int MECHA_TYPE;

extern uint64_t w_h, w_w, x_h, x_w, a_w, a_h;

// defined in Distributer.cpp
extern uint64_t tot_axw_count;
extern int w_fold;

extern queue<uint64_t> zero_row;

extern uint64_t a_util;
extern uint64_t w_util;

extern char *optarg;

uint64_t cycle;

IniParser *ip;
DataReader *dr;
DRAMInterface *dram; 
Distributer *dt;
VertexReader **vr;
EdgeReader **er;
BasisEdgeReader **ber;
XWReader **xwr;
DataController **dc;
GlobalBuffer **gb;
Accelerator **acc;

void Deallocation();
void PrintState(); // for debugging

int main(int argc, char** argv) {
	// >>>>> Argument 받기
	int option = 0;
	string ini;
	string data1;
	string data2;
	uint64_t unit_axw_count, axw_count;
	int progress_idx = 1;

	clock_t start_t = clock();
	while ((option = getopt(argc, argv, "i:x:a:")) != EOF)
	{
		switch (option)
		{
			case 'i':
				ini = optarg;
				break;
			case 'x':
				data1 = optarg;
				break;
			case 'a':
				data2 = optarg;
				break;
			case '?':
				cout<<"You must follow this form: \'./sim -i inifile_path -x x_datafile_path -a a_datafile_path\'"<<endl;
				return 0;
		}
	}
	/* test code start */
	ip = new IniParser(ini);
	dr = new DataReader(data1, data2);
	dram = new DRAMInterface("DRAMsim3/configs/DDR4_8Gb_x16_2666.ini", "DRAMsim3");
	dt = new Distributer(ip, dr);

	axw_count = tot_axw_count;
	unit_axw_count = tot_axw_count / 10;

	//PrintState();
	//dt->Print(); // 현재 multi-unit 분배 까지 출력
	cout<<endl;

	uint64_t tot_w = w_h * w_w;

	cout<<"TOTAL SIZE OF WEIGHT: "<<tot_w<<endl<<endl;

	// 여기서 부터 본격적인 simulation 요소들 생성
	dc = new DataController* [ip->tot_acc];
	gb = new GlobalBuffer* [ip->tot_acc];
	acc = new Accelerator* [ip->tot_acc];
	// unit들의 요소 관리를 위한 요소 배열 생성 후
	// 각 unit 별로 요소 생성
	for (int i = 0; i < ip->tot_acc; i++) {
		// DataController에 given data (CSR)
		dc[i] = new DataController(i, dt->adjrowindex[i], dt->adjcolindex[i]);
		// 각 unit 별로 Global Buffer 생성(추후에 단일 GB로 수정 가능성 존재)
		gb[i] = new GlobalBuffer(i, ip->gbsize, tot_w, dram);
		// W에 대해서는 Global Buffer에 미리 pre-fetch 예정이므로 그냥 생성
		acc[i] = new Accelerator(i, dt->axw_offset[i], CACHE_LINE_COUNT, dt->a_row_info[i], dram);
	}

	// XW 처리 후의 A*XW를 위해서 VREADER, EDGEREADER, XWREADER
	vr = new VertexReader* [ip->tot_acc];
	if (MECHA_TYPE == 0)
		er = new EdgeReader* [ip->tot_acc];
	else if (MECHA_TYPE == 1)
		ber = new BasisEdgeReader* [ip->tot_acc];

	xwr = new XWReader* [ip->tot_acc];
	for (int i = 0; i < ip->tot_acc; i++) {
		vr[i] = new VertexReader(i, dt->a_row_offset[i], dt->adjrowindex[i].size(), dt->a_row_info[i], dram);
		if (MECHA_TYPE == 0)
			er[i] = new EdgeReader(i, dt->a_col_offset[i], dt->adjcolindex[i].size(), dt->a_row_info[i], dram);
		else if (MECHA_TYPE == 1)
			ber[i] = new BasisEdgeReader(i, dt->a_col_offset[i], dt->adjcolindex[i].size(), dt->a_row_info[i], dram);
		xwr[i] = new XWReader(i, dt->adjcolindex[i].size());
	}
	// 요소생성 (end)

	
	cout<<"TEST"<<endl;

	// xw op cycle 저장...
	uint64_t xw_cycle = cycle;

	// A * XW Simulation
	for (int i = 0; i < ip->tot_acc; i++)
		acc[i]->isA = true;

	progress_idx = 1;

	while (true) {
		//DRAM Turn Start
		bool a_read_ok[MAX_DRAM] = {0};
		bool w_read_ok[MAX_DRAM] = {0};
		uint64_t address[MAX_DRAM];
		uint64_t id[MAX_DRAM];
		for (int i = 0; i < MAX_DRAM; i++) {
			if (dram->FReadComplete()) {
				a_read_ok[i] = true;
				address[i] = dram->GetReadData(false);
				id[i] = dt->ReturnID(address[i]);
			}
			else if (dram->WReadComplete()) {
				w_read_ok[i] = true;
				address[i] = dram->GetReadData(true);
			}
		}
		//DRAM Turn End
		//DataController Turn Start
		queue<uint64_t> a_data[MAX_DRAM];
		Type a_type[MAX_DRAM];
		for (int i = 0; i < MAX_DRAM; i++) {
			if (a_read_ok[i]) {
				a_type[i] = dc[id[i]]->ReturnDataType(address[i]);
				if (a_type[i] == A_ROW)
					a_data[i] = dc[id[i]]->AdjRowDataReturn();
				else if (a_type[i] == A_COL) 
					a_data[i] = dc[id[i]]->AdjColDataReturn();
			}
			if (MECHA_TYPE == 0) {
				for (int i = 0; i < ip->tot_acc; i++) {
					if (dc[i]->NeedRefill(A_ROW))
						dc[i]->RefillQueue(A_ROW);
					if (dc[i]->NeedRefill(A_COL))
						dc[i]->RefillQueue(A_COL);
				}
			}
		}
		//DataController Turn End
		//VertexReader Turn Start
		for (int i = 0; i <MAX_DRAM; i++) {
			if (a_read_ok[i] && a_type[i] == A_ROW)
				vr[id[i]]->ReceiveData(a_data[i]);
		}
		for (int i = 0; i < ip->tot_acc; i++) {
			if (MECHA_TYPE == 0) {
				if (vr[i]->IsEndRequest()) {
					vr[i]->pre_w_fold++;
					vr[i]->ResetRequestStat();
				}
				else if (vr[i]->IsEndOperation())
					vr[i]->TurnOffFlag();
			}
			else if (MECHA_TYPE == 1) {
				if (vr[i]->BasisEndOperation()) {
					vr[i]->TurnOffFlag();
				}
			}
			if (vr[i]->flag.req_need)
				vr[i]->Request();
		}
		//VertexReader Turn End
		//EdgeReader & VertexReader Turn Start
		if (MECHA_TYPE == 0) {
			for (int i = 0; i < MAX_DRAM; i++) {
				if (a_read_ok[i] && a_type[i] == A_COL)
					er[id[i]]->ReceiveData(a_data[i]);
			}
			for (int i = 0; i < ip->tot_acc; i++) {
				if (er[i]->CanVertexReceive() && !(vr[i]->flag.q_empty)) 
					er[i]->ReceiveData(vr[i]->TransferData());
				if (er[i]->IsEndRequest()) {
					er[i]->pre_w_fold++;
					er[i]->ResetRequestStat();
				}
				else if (er[i]->IsEndOperation()) 
					er[i]->TurnOffFlag();
				if (er[i]->flag.req_need)
					er[i]->Request();
			}
		}
		else if (MECHA_TYPE == 1) {
			for (int i = 0; i < MAX_DRAM; i++) {
				if (a_read_ok[i] && a_type[i] == A_COL)
					ber[id[i]]->ReceiveData(a_data[i]);
			}
			for (int i = 0; i < ip->tot_acc; i++) {
				if (ber[i]->CanVertexReceive() && !(vr[i]->flag.q_empty))
					ber[i]->ReceiveData(vr[i]->TransferData());
				if (ber[i]->IsEndOperation())
					ber[i]->TurnOffFlag();
				if (ber[i]->flag.req_need)
					ber[i]->Request();
			}
		}
		//EdgeReader & VertexReader Turn Start
		//XWReader & EdgeReader Turn Start
		if (MECHA_TYPE == 0) {
			for (int i = 0; i < ip->tot_acc; i++) {
				if (xwr[i]->flag.can_receive && !(er[i]->flag.q_empty) && !(er[i]->CanVertexReceive()))
					xwr[i]->ReceiveData(er[i]->TransferData());
			}
		}
		else if (MECHA_TYPE == 1) {
			for (int i = 0; i < ip->tot_acc; i++) {
				if (xwr[i]->flag.can_receive && !(ber[i]->flag.q_empty) && !(ber[i]->CanVertexReceive()))
					xwr[i]->ReceiveData(ber[i]->TransferData());
			}
		} 
		//XWReader & EdgeReader Turn End
		for (int i = 0; i < MAX_DRAM; i++) {
			if (w_read_ok[i]) {
				int idx = dram->ReturnID(address[i]);
				gb[idx]->ReceiveData(address[i]);
			}
		}
		//GlobalBuffer & XWReader Turn Start
		if (MECHA_TYPE == 0) {
			for (int i = 0; i <ip->tot_acc; i++) {
				if (xwr[i]->IsEndRequest()) {
					xwr[i]->ResetRequestStat();
					xwr[i]->pre_w_fold++;
					gb[i]->pre_w_fold++;	
				}
				else if (xwr[i]->IsEndOperation()) {
					xwr[i]->TurnOffFlag();
				}
				if (!(xwr[i]->flag.q_empty) && gb[i]->axwflag.can_receive)
					gb[i]->ReceiveData(xwr[i]->Request());
				if (gb[i]->axwflag.can_transfer) {
					gb[i]->TransferAData();
					acc[i]->ReceiveData(xwr[i]->TransferData());
				}
				if (!(zero_row.empty()))
					acc[i]->WriteZeroRow();
			}
		}
		else if (MECHA_TYPE == 1) {
			for (int i = 0; i < ip->tot_acc; i++) {
				if (xwr[i]->count_up) {
					xwr[i]->count_up = false;
					xwr[i]->pre_w_fold++;
					gb[i]->pre_w_fold++;
				}
				else if (xwr[i]->count_reset) {
					xwr[i]->count_reset = false;
					xwr[i]->pre_w_fold = 0;
					gb[i]->pre_w_fold = 0;
				}
				if (xwr[i]->BasisEndOperation())
					xwr[i]->TurnOffFlag();
				if (!(xwr[i]->flag.q_empty) && gb[i]->axwflag.can_receive)
					gb[i]->ReceiveData(xwr[i]->Request());
				if (gb[i]->axwflag.can_transfer) {
					gb[i]->TransferAData();
					acc[i]->ReceiveData(xwr[i]->BasisTransferData());
				}
				while (!(zero_row.empty()))
					acc[i]->WriteZeroRow();
			}
		}
		
		//GlobalBuffer & XWReader Turn End
		//Accelerator Turn Start
		for (int i = 0; i < ip->tot_acc; i++)
			acc[i]->MAC();
		//Accelerator Turn End
		//Add Transaction to DRAM
		dram->AddTransaction();
		//Cylce Update
		dram->UpdateCycle();
		cycle++;

		if (axw_count - tot_axw_count > unit_axw_count * progress_idx || tot_axw_count == 0) {
			cout<<"Progress... "<<axw_count - tot_axw_count<<"/"<<axw_count<<endl;
			progress_idx++;
		}

		if (tot_axw_count == 0) {
			cout<<"AXW OPERATION OVER"<<endl;
			break;
		}
	}

	clock_t end_t = clock(); 
	double running_time = (double)(end_t - start_t) / CLOCKS_PER_SEC;

	uint64_t axw_cycle = cycle - xw_cycle;
	cout<<"TOTAL CYCLE: "<<cycle<<endl;
	cout <<"XW CYCLE: " << xw_cycle << ", AXW CYCLE: " << axw_cycle << endl;
	cout<<"DRAM UTIL(A): "<<a_util<<endl;
	cout<<"DRAM UTIL(W): "<<w_util<<endl;
	cout<<"TOTAL RUNNING TIME: "<<running_time<<" second"<<endl;
	cout<<"TEST END"<<endl;

	// make result file
	int idx = 0;
	struct stat st = {0};
	if (stat("results", &st) == -1) {
		mkdir("results", 0700);
	}
	string path = "results/output_0.txt";
	while (access(path.c_str(), F_OK) != -1) {
		idx++;
		path = "results/output_"+to_string(idx)+".txt";
	}
	ofstream output(path);

	output<<"YOUR CONFIGS: "<<ini<<endl;
	output<<"YOUR DATAFILE(XW): "<<data1<<endl;
	output<<"YOUR DATAFILE(A): "<<data2<<endl<<endl;
	output<<"TOTAL CYCLE: "<<cycle<<endl;
	output<<"XW CYCLE: "<<xw_cycle<<", AXW CYCLE: "<<axw_cycle<<endl;
	output<<"DRAM UTIL(A): "<<a_util<<endl;
	output<<"DRAM UTIL(W): "<<w_util<<endl;
	output<<"TOTAL RUNNING TIME: "<<running_time<<" second"<<endl;

	output.close();
	Deallocation();
	/* test code end */


	return 0;
}

void Deallocation() {
	for (int i = 0; i < ip->tot_acc; i++) {
		delete dc[i];
		delete gb[i];
	}
	delete dc;
	delete gb;
	//delete dt;
	delete dram;
	delete dr;
	delete ip;
}

void PrintState() {
	cout<<"-----IniParser Result-----"<<endl;
	cout<<"GlobalBufferSize: "<<dec<<ip->gbsize<<endl;
	cout<<"XBufferSize: "<<dec<<ip->xbsize<<endl<<endl;;

	cout<<"TotalNumOfAcc: "<<dec<<ip->tot_acc<<endl<<endl;

	cout<<"CACHE_LINE_BYTE: "<<hex<<CACHE_LINE_BYTE<<endl;
	cout<<"CACHE_LINE_COUNT: "<<hex<<CACHE_LINE_COUNT<<endl;
	cout<<"X_START: "<<hex<<X_START<<endl;
	cout<<"A_ROW_START: "<<hex<<A_ROW_START<<endl;
	cout<<"A_COL_START: "<<hex<<A_COL_START<<endl;
	cout<<"WEIGHT_START: "<<hex<<WEIGHT_START<<endl;
	cout<<"XW_START: "<<hex<<XW_START<<endl;
	cout<<"AXW_START: "<<hex<<AXW_START<<endl;
	cout<<"--------------------------"<<endl;
	cout<<endl;

	cout<<"-----DataReader Result-----"<<endl;
	cout<<"adjrowindex"<<endl;
	queue<uint64_t> adjrowindex = dr->adjrowindex;
	for (int i = 0; i < dr->adjrowindex.size(); i++) {
		cout<<dec<<adjrowindex.front()<<" ";
		adjrowindex.pop();
	}
	cout<<endl<<endl;
	cout<<"adjcolindex"<<endl;
	queue<uint64_t> adjcolindex = dr->adjcolindex;
	for (int i = 0; i < dr->adjcolindex.size(); i++) {
		cout<<dec<<adjcolindex.front()<<" ";
		adjcolindex.pop();
	}
	cout<<endl;
	cout<<"-----PrintState END-----"<<endl;
}