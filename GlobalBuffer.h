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

using namespace std;

struct XWFlag {
	bool w_fill_complete;
	bool can_transfer;
};

struct AXWFlag {
	bool can_transfer;
	bool can_receive;
	bool cache_full;
	bool *requested; // it maybe changed
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
    void access(int setIdx, int data);
    int replace(int setIdx);
	void clear();
};

// BF replacement 보조 클래스
class BloomFilter {
    private:
    int wayN;
    int setN;
    public:
    BloomFilter(int wayN, int setN);
    int replace(int setIdx, int blkIdx, vector<int>* setVector);
};

class GlobalBuffer {
public:
	int pre_w_fold;
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
	/* temporary (may be changed)*/
	map<uint64_t, uint64_t> addr_col_table;
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
};
#endif