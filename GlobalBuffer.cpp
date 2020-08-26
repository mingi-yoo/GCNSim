/*** temporary file ***
	Must Be Revised 
*/
#include "GlobalBuffer.h"

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

extern uint64_t w_h, w_w, x_h, x_w, a_w, a_h;

extern uint64_t cycle;
extern uint64_t w_fold;

int tot_req;

// 캐시 교체 알고리즘
extern uint64_t CACHE_REPL_ALGO;

// Bloom Filter Num
extern uint64_t BLOOM_FILTER_NUM;
extern uint64_t BF_LEN; // 나중에 a_w를 가지고 알아서 계산하도록 할 필요도!

// todo !!!!! multi unit 관련해서 상의해볼 필요 있음
// todo !!!!!!!!!! adjList 채워넣는 방식에 대해서 논의 필요!!
vector<vector<int>> adjList;
vector<int> edgeTo; // n번째 edge가 어디로 향하는지!
// todo!!!! BF가 simulation 할 때 필요하므로 논읩해볼것!!!
vector<vector<int>> fifoList; // fifo를 위한 tmp
vector<vector<int>> lruList;  // lru를 위한 tmp
vector<vector<int>> bfList; // BF를 위한 tmp


FIFO::FIFO(int wayN, int setN) {
	this->wayN = wayN;
	this->setN = setN;
	for(int i = 0; i < setN; i++) {
		fifoList.push_back(vector<int> ());
	}
}

// 이미 cache 내의 꽉찬지 여부는 cache에서 관리중이므로
// 여기서는 fifo만 단순하게 관리해주면 됨...

// hit이 아닌 miss의 경우에만 들어오게 됨
// 따라서 그냥 넣어주기만 하면 됨
// 넘치는 경우에 대해서는 replace에서 제거함
void FIFO::access(int setIdx, int data) {
	fifoList[setIdx].push_back(data);
}

// 해당 set에서 없애야할(교체해야할) edgeTo를 알려줌
int FIFO::replace(int setIdx) {
	vector<int>::iterator iter;
	iter = fifoList[setIdx].begin();
	// 첫번째 (먼저 들어온) 요소 취하고 삭제
	int firstIn = fifoList[setIdx][0];
	fifoList[setIdx].erase(iter);
	return firstIn;
}

// fold 변경 시 clear
void FIFO::clear() {
	fifoList.clear();
	for(int i = 0; i < setN; i++) {
		fifoList.push_back(vector<int> ());
	}
}

LRU::LRU(int wayN, int setN) {
	this->wayN = wayN;
	this->setN = setN;
	for(int i = 0; i < setN; i++) {
		lruList.push_back(vector<int> ());
	}
}

void LRU::access(int setIdx, int data) {
	bool isExist = false;
	for(int i = 0; i < lruList[setIdx].size(); i++) {
		// 이미 존재하는 경우
		// 우선 삭제!
		if(lruList[setIdx][i] == data) {
			vector<int>::iterator iter = lruList[setIdx].begin();
			iter += i;
			lruList[setIdx].erase(iter);
			break;
		}
	}
	// 사용한 값은 무조건 앞으로 추가
	lruList[setIdx].insert(lruList[setIdx].begin(), data);
}

int LRU::replace(int setIdx) {
	// 무조건 miss 인 경우에만 호출되기 때문에
	// 루틴에만 집중하면 됨

	// 가장 덜 사용한 요소를 반환함...
	// lruList[setIdx]의 마지막 요소
	int target = lruList[setIdx][lruList[setIdx].size()-1];
	lruList[setIdx].pop_back();
	return target;
}

void LRU::clear() {
	lruList.clear();
	for(int i = 0; i < setN; i++) {
		lruList.push_back(vector<int> ());
	}
}

BloomFilter::BloomFilter(int wayN, int setN) {
	this->wayN = wayN;
	this->setN = setN;

	// BF 초기화
	for(int i = 0; i < BLOOM_FILTER_NUM; i++) {
		bfList.push_back(vector<int> (BF_LEN, 0));
	}

	// 여기서 미리 bloomFilter 만들자!
	// Adj Matrix Block으로 나누어 진행

	// 한 Block당 일반적인 vertex 갯수
	int blockVertexN = a_w / BLOOM_FILTER_NUM;
	int vertexN = 0;
	int edgeN = 0;
	// 마지막 블럭 이전까지 진행
	for(int blkIdx = 0; blkIdx < (BLOOM_FILTER_NUM-1); blkIdx++) {
		// 블럭에 해당되는 범위의 vertex들에 대해서
		// 캐시에 올리는 작업을 진행
		// 이미 있다면 cache 내에서 hit이고
		// 없다면 cache 내에서 miss로 기록됨...
		for(int i = 0; i < blockVertexN; i++) {
			// vertex에 연결됨 edge들 대상으로 cache simulation
			for(int edgIdx = 0; edgIdx < adjList[vertexN + i].size(); edgIdx++) {
				int target = edgeTo[edgeN];
				bfList[blkIdx][target%BF_LEN] += 1;
				edgeN += 1;
			}
		}
		vertexN += blockVertexN;
	}
	// 마지막 블럭 나머지 vertex 진행
	while(vertexN < a_w) {
		// 나머지 범위의 vertex들에 대해서
		// 캐시에 올리는 작업을 진행
		// hit와 miss가 cache 내에서 계산

		// vertex에 연결됨 edge들 대상으로 cache simulation
		for(int edgIdx = 0; edgIdx < adjList[vertexN].size(); edgIdx++) {
			int target = edgeTo[edgeN];
			bfList[BLOOM_FILTER_NUM-1][target%BF_LEN] += 1;
			edgeN += 1;
		}
		vertexN += 1;
	}
}

// 교체할 대상에 대해서 알려줌
// 이미 set이 찬 상태에서 호출되므로
// 기본 루틴에만 집중할 것
int BloomFilter::replace(int setIdx, int blkIdx, vector<int>* setVector) {
	// 해당 set에서 교체 대상을 선택해야함
	// 이 때 blkIdx(BF idx)를 이용해서 해당 bloomFilter에서
	// set 안의 각 요소(n-way이면 n개) 중 cnt가 가장 낮은 것을
	// 교체 대상으로 선정한다...
	vector<int> cntVector;
	for(int i = 0; i < setVector->size(); i++) {
		cntVector.push_back(0);
	}

	for(int i = 0; i < setVector->size(); i++) {
		int bfIdx = setVector->at(i) % BF_LEN;
		// bf에서 cnt 가져와서 저장
		cntVector[i] = bfList[blkIdx][bfIdx];
	}
	// cnt가 가장 적은 교체 대상을 반환함...
	auto minIdx = min_element(cntVector.begin(), cntVector.end());
	return setVector->at(minIdx - cntVector.begin());
}

GlobalBuffer::GlobalBuffer(int id,
						   uint64_t gbsize, 
						   uint64_t tot_w,
						   DRAMInterface *dram) : c_space(0), pre_w_fold(0) {
	this->id = id;
	this->gbsize = gbsize;
	this->tot_w = tot_w;
	this->dram = dram;
	isA = false;
	xwflag.w_fill_complete = false;
	xwflag.can_transfer = false;
	axwflag.can_transfer = false;
	axwflag.can_receive = true;
	axwflag.cache_full = false;
	
	/* 캐시 교체 알고리즘 */
	// 단위 계산
	this-> blockN = gbsize / CACHE_LINE_BYTE;
	this-> setN = blockN / CACHE_LINE_COUNT;
	this-> wayN = CACHE_LINE_COUNT;
	// n-way set associative cache
	for(int i = 0; i < setN; i++) {
		this->cache.push_back(vector<int> (CACHE_LINE_COUNT, -1));
		// 각 집합마다 way 수만큼 라인을 가지고 사용중이지 않기 때문에
		// -1로 채움
		this->weight_blk_idx.push_back(vector<int> (CACHE_LINE_COUNT, -1));
	}
	if(CACHE_REPL_ALGO == FIFO_ALGO) {
		FIFO newFifo = FIFO(wayN, setN);
		this->fifo =  &newFifo;
	} else if(CACHE_REPL_ALGO == LRU_ALGO) {
		LRU newLRU = LRU(wayN, setN);
		this->lru = &newLRU;
	} else if(CACHE_REPL_ALGO == BF_ALGO) {
		BloomFilter newBF = BloomFilter(wayN, setN);
		this->bf = &newBF;
	} else {
		cout << "[Warning] unknown algorithm selected!" << endl;
	}
	this->before_pre_w_fold = pre_w_fold;
	axwflag.requested = new bool[a_w];
	for (int i = 0; i < a_w; i++)
		axwflag.requested[i] = false;
	/**************************/
}

GlobalBuffer::~GlobalBuffer() {
	delete axwflag.requested;
}

void GlobalBuffer::FillWeight() {
	tot_req = ceil((float)tot_w/CACHE_LINE_COUNT);

	uint64_t address = WEIGHT_START;
	int cnt = tot_req;

	for (int i = 0; i < cnt; i++) {
		dram->DRAMRequest(address, READ);
		dram->AddTransaction();
		dram->UpdateCycle();
		address += CACHE_LINE_BYTE;
		cycle++;
	}

	while (tot_req != 0) {
		dram->AddTransaction();
		dram->UpdateCycle();
		cycle++;
	}

	for (int i = 0; i < MAX_DRAM; i++) {
		if (dram->WReadComplete())
			dram->GetReadData(true);
	}

	//cout<<"FILL WEIGHT COMPLETE"<<endl;
	xwflag.w_fill_complete = true;
}

void GlobalBuffer::ReceiveData(XRData data) {
	//cout<<"GB) RECEIVE DATA: "<<dec<<data.ifvalue<<endl;
	if (xwflag.w_fill_complete) {
		xwflag.can_transfer = true;
	}
	x_data = data;
}

// !!!!!!!! 캐시 교체 알고리즘 구성 시 변경 허용 
void GlobalBuffer::ReceiveData(ERData data) {
	//cout<<"GB) RECEIVE DATA: "<<dec<<data.colindex<<endl;
	/*it maybe will be changed*/

	/* new code.... */

	// 만약 fold가 변경되면 전체 cache를 비워줄 것!
	// !!! hash func을 block_idx와 같이 하는 것으로 바꾸면서
	// deprecated!!!
	// if(before_pre_w_fold != pre_w_fold) {
	// 	for(int i = 0; i < setN; i++) {
	// 		for(int j = 0; j < wayN; j++) {
	// 			cache[i][j] = -1;
	// 		}
	// 	}
	// 	if(CACHE_REPL_ALGO == FIFO_ALGO) {
	// 		fifo->clear();
	// 	} else if(CACHE_REPL_ALGO == LRU_ALGO) {
	// 		lru->clear();
	// 	} else {
	// 		cout << "unknown algo !!" << endl;
	// 	}
	// 	before_pre_w_fold = pre_w_fold;
	// }

	uint64_t to = data.colindex;

	bool hit = false;
	uint64_t setIdx = data.colindex%setN;
	bool hasExist = false;

	// 우선 이미 있는 정보인지 체크
	for(int i = 0; i < wayN; i++) {
		// 만약 캐시 해당 set에 이미 정보가 있다면
		if(cache[setIdx][i] == to && weight_blk_idx[setIdx][i] == pre_w_fold) {
			hasExist = true;
			hit = true; // 있었기 때문에 hit!

			// LRU 알고리즘의 경우 사용 시 반영
			if(CACHE_REPL_ALGO == LRU_ALGO) {
				lru->access(setIdx, to);
			}

			break;
		}
	}

	// 만약 지금 캐시에 로드되어있지 않다면!!!
	if(!hit) {
		//cout<<"NO HIT! "<<dec<<data.colindex<<", Cycle: "<<cycle<<endl;
		axwflag.requested[to] = true;
		Request(data);
	} else { // 로드되어있다면!!!
		axwflag.can_transfer = true;
	}
	axwflag.can_receive = false;
	a_data = data;

	/**********************/
	// 이전 구현체임!
	// 만약 이전에 요청한 사항이 없었다면
	// 요청을 이번에 보낸다고 표시하고 (요청한 적 있다고)
	// Request!

	// if (!axwflag.requested[data.colindex]) {
	// 	axwflag.requested[data.colindex] = true;
	// 	Request(data);
	// }
	// else {
	// 	for (int i = 0; i < w_data.size(); i++) {
	// 		if (data.colindex == w_data[i])
	// 			axwflag.can_transfer = true;
	// 	}
	// }
	// axwflag.can_receive = false;
	// a_data = data;

	/**************************/

}

// !!!!!!!! 캐시 교체 알고리즘 구성 시 변경 허용 
void GlobalBuffer::ReceiveData(uint64_t address) {
	//cout<<"GB) RECEIVE WEIGHT: "<<hex<<address<<endl;
	/*it maybe will be changed*/

	/* new code .....*/

	uint64_t col = addr_col_table[address];
	uint64_t to = col;
	uint64_t setIdx = col%setN; // 몇번째 set에 해당하는지 결정
	bool hasExist = false; // 빈 공간이나 이미 정보가 있었는지에 대한 flag

	// 주의!!! 이미 캐시 위에는 없었기 때문에 들어오는 함수임!!!!!

	// 빈공간이 있다면 채우기
	for(int i = 0; i < wayN; i++) {
		// 만약 캐시 해당 set에 빈 공간이 있다면
		if(cache[setIdx][i] == -1) {
			cache[setIdx][i] = to;
			weight_blk_idx[setIdx][i] = pre_w_fold;
			
			// 교체 알고리즘 클래스 관리
			if(CACHE_REPL_ALGO == FIFO_ALGO) {
				fifo->access(setIdx, to);
			} else if(CACHE_REPL_ALGO == LRU_ALGO) {
				lru->access(setIdx, to);
			}

			hasExist = true;
			break;
		}
	}
	// 만약 공간이 없었다면 replacement algo 적용해서 비우자
	if(!hasExist) {
		if(CACHE_REPL_ALGO == FIFO_ALGO) {
			// 교체할 대상 받아옴
			int replaceVal = fifo->replace(setIdx);
			// flag에서 이제 없앨 예정이라고 표시
			axwflag.requested[replaceVal] = false;
			for(int i = 0; i < wayN; i++) {
				if(cache[setIdx][i] == replaceVal) {
					cache[setIdx][i] = to;
					weight_blk_idx[setIdx][i] = pre_w_fold;
					fifo->access(setIdx, to);
					break;
				}
			}

		} else if(CACHE_REPL_ALGO == LRU_ALGO) {
			int replaceVal = lru->replace(setIdx);
			// flag에서 이제 없앨 예정이라고 표시
			axwflag.requested[replaceVal] = false;
			for(int i = 0; i < wayN; i++) {
				if(cache[setIdx][i] == replaceVal) {
					cache[setIdx][i] = to;
					weight_blk_idx[setIdx][i] = pre_w_fold;
					lru->access(setIdx, to);
					break;
				}
			}
		} else if(CACHE_REPL_ALGO == BF_ALGO) {
			// todo!!!!!!! blkIdx 설정을 해야함!!!
			// int replaceVal = bf->replace(setIdx, blkIdx, &cache[setIdx]);
			// // flag에서 이제 없앨 예정이라고 표시
			// axwflag.requested[replaceVal] = false;
			// for(int i = 0; i < wayN; i++) {
			// 	if(cache[setIdx][i] == replaceVal) {
			// 		cache[setIdx][i] = to;
			// 		break;
			// 	}
			// }
		} else {
			cout << "[Warning] unknown algorithm selected!" << endl;
		}
	}
	
	axwflag.can_transfer = true; 

	/*********************/
	// 이전 코드 구현체
	// uint64_t col = addr_col_table[address];
	// if (axwflag.cache_full)
	// 	CacheReplacement(); // if cache is full, then run cache replacement

	// w_data.push_back(col); // store to cache
	// axwflag.can_transfer = true; 

	// c_space += CACHE_LINE_BYTE;
	// if (c_space == gbsize)
	// 	axwflag.cache_full = true;
	/**************************/
}

// !!!!!!!! 캐시 교체 알고리즘 구성 시 변경 허용 
// 그냥 위에서 직접적으로 구현하도록 구성하였음
// void GlobalBuffer::CacheReplacement() {
// 	/*it maybe will be changed*/
// 	// ALGO 별로 다르게 처리
// 	if(CACHE_REPL_ALGO == 0) { // 1. FIFO
// 		axwflag.requested[w_data[0]] = false;
// 		// 위에서 RecieveData 함수에서
// 		// w_data에 push_back으로 집어넣어두기 때문에...
// 		w_data.erase(w_data.begin());
// 		c_space -= CACHE_LINE_BYTE;
// 	} else if(CACHE_REPL_ALGO == 1) { // 2. LRU

// 	} else if(CACHE_REPL_ALGO == 2) { // 3. RRiP

// 	} else if(CACHE_REPL_ALGO == 3) { // 4. BF

// 	} else {
// 		cout << "Unknown Cache Replacement Algo... do FIFO" << endl;
// 		axwflag.requested[w_data[0]] = false;
// 		w_data.erase(w_data.begin());
// 		c_space -= CACHE_LINE_BYTE;
// 	}

// 	/**************************/
// }

void GlobalBuffer::Request(ERData data) {
	uint64_t address = XW_START + (data.colindex * w_fold + pre_w_fold) * UNIT_BYTE;
	addr_col_table[address] = data.colindex;
	dram->DRAMRequest(address, READ, id); 
	//cout<<"GB) WEIGHT REQUEST: "<<hex<<address<<endl;
}

XRData GlobalBuffer::TransferXData() {
	//cout<<"GB) WEIGHT READ COMPLETE"<<endl;
	xwflag.can_transfer = false;
	XRData ret = x_data;
	return ret;
}

ERData GlobalBuffer::TransferAData() {
	//cout<<"GB) WEIGHT READ COMPLETE"<<endl;
	axwflag.can_transfer = false;
	axwflag.can_receive = true;
	ERData ret = a_data;
	return ret;
}