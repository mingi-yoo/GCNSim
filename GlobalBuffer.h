/*** temporary file ***
	Must Be Revised 
*/
#ifndef __GLOBALBUFFER_H__
#define __GLOBALBUFFER_H__

#include <iostream>
#include <queue>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include "DRAMInterface.h"
#include "Common.h"

// Replacement 알고리즘
#define FIFO_ALGO 0
#define LRU_ALGO 1
#define OPTIMAL_ALGO 2
#define BF_ALGO 3

#define LIMIT_WQ_SIZE 32

using namespace std;

struct XWFlag {
	bool w_fill_complete;
	bool can_transfer;
};

struct AXWFlag {
	bool can_transfer;
	bool can_receive;
	bool cache_full;
	bool q_empty;
	// bool *requested; // it maybe changed
};

class FIFO {
    private:
    int wayN;
    int setN;

    public:
    FIFO(int wayN, int setN);
    void access(int setIdx, int data);
    int replace(int setIdx);
	void clear();
};

// LRU replacemnt 보조 클래스
class LRU {
    private:
    int wayN;
    int setN;
    public:
    LRU(int wayN, int setN);
    void access(int setIdx, int data, int weightBlkIdx);
    pair<int, int> replace(int setIdx);
	void clear();
};

// BF replacement 보조 클래스
class BloomFilter {
    private:
    int wayN;
    int setN;
	// 각 블록 별로 몇 인덱스씩 가져야하는지
	vector<int> blkIdxVector;
	// Overlap 블록에 대해서도 계산
	vector<int> ovlBlkIdxVector;
	// Overlap Block StartIdx
	int ovlStartIdx;
	int curRowIdx;
    public:
    BloomFilter(int wayN, int setN);
	int firstCycleAccess(uint64_t from, uint64_t to);
    pair<int, int> replace(int setIdx, int blkIdx, bool useOvl, vector<int>* setVector, vector<int>* weightVector);
};

class GlobalBuffer {
public:
	int w_fold_save;
	int pre_repeat;
	XWFlag xwflag;
	AXWFlag axwflag;
	GlobalBuffer(int id,
				 uint64_t gbsize, 
				 uint64_t tot_w,
				 DRAMInterface *dram);
	~GlobalBuffer();
	void FillWeight();
	void ReceiveData(XRData data);
	void ReceiveData(ERData data);
	void ReceiveData(uint64_t address); // you can change
	void CacheReplacement(); // you can change
	void CanTransfer();
	XRData TransferXData();
	ERData TransferAData();
private:
	int id;
	int c_space;
	uint64_t gbsize;
	uint64_t tot_w; //total size of weight (only XW allowed)
	bool isA;
	XRData x_data;
	ERData a_data;
	DRAMInterface *dram;
	vector<uint64_t> w_data;
	int blockN;
	int setN;
	int wayN;
	vector<vector<int>> cache;
	vector<vector<int>> weight_blk_idx;
	// replacement algo classes
    FIFO* fifo;
    LRU* lru;
    BloomFilter *bf;
	int before_pre_w_fold;
	void Request(ERData data);
	/*****************************/
	// for mshr
	queue<ERData> wq;
	map<uint64_t, ERData> w_map;
	map<uint64_t, bool> w_req_table;
};
#endif