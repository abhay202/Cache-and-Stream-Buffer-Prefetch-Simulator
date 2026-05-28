#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "sim.h"

int memoryTraffic = 0;  

struct CacheBlock {
    uint32_t tag;
    bool valid;
    bool dirty;
};

class CacheSet {
public:
    CacheBlock* blocks;
    int* lruFind;
    int assoc;

    CacheSet(int assoc) : assoc(assoc) {
        blocks = new CacheBlock[assoc];
        lruFind = new int[assoc];
        for (int i = 0; i < assoc; ++i) {
            lruFind[i] = i;
        }
    }

    ~CacheSet() {
        delete[] blocks;
        delete[] lruFind;
    }

    int getLRU() {
        return lruFind[0];
    }

    void updateLRU(int accessedIndex) {
        int pos = -1;
        for (int i = 0; i < assoc; ++i) {
            if (lruFind[i] == accessedIndex) {
                pos = i;
                break;
            }
        }
        if (pos != -1) {
            for (int i = pos; i < assoc - 1; ++i) {
                lruFind[i] = lruFind[i + 1];
            }
            lruFind[assoc - 1] = accessedIndex;
        } else {
            std::cerr << "Error: Block index not found." << std::endl;
        }
    }
};

class Cache {
public:
    int numSets;
    int assoc;
    int blockSize;
    CacheSet** sets;
    int indexBits;
    int blockOffset;
    int tagBits;

    Cache* nextLevelCache;

    int l1Reads;
    int l1ReadMisses;
    int l1Writes;
    int l1WriteMisses;
    int l1Writebacks;
    int l1Prefetches;

    int l2Reads;
    int l2ReadMisses;
    int l2Writes;
    int l2WriteMisses;
    int l2Writebacks;
    int l2Prefetches;
    int l2PrefetchReads;
    int l2PrefetchReadMisses;

    Cache(int cacheSize, int assoc, int blockSize, Cache* nextLevel = nullptr)
        : assoc(assoc), blockSize(blockSize), nextLevelCache(nextLevel),
          l1Reads(0), l1ReadMisses(0), l1Writes(0), l1WriteMisses(0),
          l1Writebacks(0), l1Prefetches(0),
          l2Reads(0), l2ReadMisses(0), l2Writes(0), l2WriteMisses(0),
          l2Writebacks(0), l2Prefetches(0), l2PrefetchReads(0), l2PrefetchReadMisses(0) {

        if (cacheSize == 0 || blockSize == 0 || assoc == 0) {
            std::cerr << "Error: Cache size, block size, and associativity cann't be zero.\n";
            exit(EXIT_FAILURE);
        }

        numSets = cacheSize / (blockSize * assoc);
        if (numSets == 0) {
            std::cerr << "Error: numSets cannot be zero.\n";
            exit(EXIT_FAILURE);
        }

        indexBits = static_cast<int>(std::log2(numSets));
        blockOffset = static_cast<int>(std::log2(blockSize));
        tagBits = 32 - (indexBits + blockOffset);

        sets = new CacheSet*[numSets];
        for (int i = 0; i < numSets; ++i) {
            sets[i] = new CacheSet(assoc);
        }
    }

    ~Cache() {
        for (int i = 0; i < numSets; ++i) {
            delete sets[i];
        }
        delete[] sets;
    }

    void addressDecode(uint32_t address, uint32_t& tag, uint32_t& index) {
        uint32_t indexMask = (1 << indexBits) - 1;
        index = (address >> blockOffset) & indexMask;
        tag = address >> (blockOffset + indexBits);
    }

    void removeBlock(CacheSet* set, int blockIndex, uint32_t index, bool isL1 = true) {
        CacheBlock& block = set->blocks[blockIndex];

        if (block.valid && block.dirty) {
            uint32_t evictedTag = block.tag;
            uint32_t evictedAddress = (evictedTag << (indexBits + blockOffset)) | (index << blockOffset);

            if (nextLevelCache) {
                nextLevelCache->request('w', evictedAddress, false);
            } else {
                memoryTraffic++;
            }

            if (isL1) {
                l1Writebacks++;
            } else {
                l2Writebacks++;
            }
        }
    }

    void request(char rw, uint32_t address, bool isL1 = true) {
        uint32_t tag, index;
        addressDecode(address, tag, index);
        CacheSet* set = sets[index];

        if (isL1) {
            if (rw == 'r') {
                l1Reads++;
            }
            if (rw == 'w') {
                l1Writes++;
            }
        } 
        else {
            if (rw == 'r') {
                l2Reads++;
            }
            if (rw == 'w') {
                l2Writes++;
            }
        }


        for (int i = 0; i < assoc; ++i) {
            CacheBlock& block = set->blocks[i];
            if (block.valid && block.tag == tag) {
                if (rw == 'w') {
                    block.dirty = true;
                }
                set->updateLRU(i);
                return;
            }
        }

        int lruIndex = set->getLRU();
        removeBlock(set, lruIndex, index, isL1);

        CacheBlock& newBlock = set->blocks[lruIndex];
        newBlock.tag = tag;
        newBlock.valid = true;
        newBlock.dirty = (rw == 'w');

        if (isL1) {
            if (rw == 'r') {
                l1ReadMisses++;
            }
            if (rw == 'w') {
                l1WriteMisses++;
            }
        }
         else {
            if (rw == 'r') {
                l2ReadMisses++;
            }
            if (rw == 'w') {
                l2WriteMisses++;
            }
        }

        if (nextLevelCache) {
            nextLevelCache->request('r', address, false);
        } else {
            memoryTraffic++;
        }

        set->updateLRU(lruIndex);
    }

float L1MissRate() const {
    int totalAccesses = l1Reads + l1Writes;
    int totalMisses = l1ReadMisses + l1WriteMisses;

    if (totalAccesses > 0) {
        return static_cast<float>(totalMisses) / (totalAccesses);
    } else {
        return 0.0;
    }
}

float L2MissRate() const {
    int totalAccesses = l2Reads;     
    int totalMisses = l2ReadMisses;  

    if (totalAccesses > 0) {
        return static_cast<float>(totalMisses) / (totalAccesses);
    } else {
        return 0.0;
    }
}


    void printContents() {
        for (int i = 0; i < numSets; ++i) {
            std::cout << "set " << std::setw(6) << std::dec << i << ":   ";
            CacheSet* set = sets[i];
            for (int j = assoc - 1; j >= 0; --j) {
                int blockIndex = set->lruFind[j];
                CacheBlock& block = set->blocks[blockIndex];
                if (block.valid) {
                    std::cout << std::hex << std::setw(8) << block.tag;
                    if (block.dirty) {
                        std::cout << " D";
                    } else {
                        std::cout << "  ";
                    }
                }
                if (j > 0) {
                    std::cout << " ";
                }
            }
            std::cout << std::endl;
        }
    }

    void printMeasurements() {
        std::cout << std::left << std::setw(30) << "a. L1 reads:" << std::dec << l1Reads << std::endl;
        std::cout << std::left << std::setw(30) << "b. L1 read misses:" << std::dec << l1ReadMisses << std::endl;
        std::cout << std::left << std::setw(30) << "c. L1 writes:" << std::dec << l1Writes << std::endl;
        std::cout << std::left << std::setw(30) << "d. L1 write misses:" << std::dec << l1WriteMisses << std::endl;
        std::cout << std::left << std::setw(30) << "e. L1 miss rate:" << std::fixed << std::setprecision(4) << L1MissRate() << std::endl;
        std::cout << std::left << std::setw(30) << "f. L1 writebacks:" << std::dec << l1Writebacks << std::endl;
        std::cout << std::left << std::setw(30) << "g. L1 prefetches:" << std::dec << l1Prefetches << std::endl;
    }

        void printL2zeroMeasurement(){
        std::cout << std::left << std::setw(30) << "h. L2 reads (demand):" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "i. L2 read misses (demand):" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "j. L2 reads (prefetch):" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "k. L2 read misses (prefetch):" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "l. L2 writes:" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "m. L2 write misses:" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "n. L2 miss rate:" << std::fixed << std::setprecision(4) << 0.0 << std::endl;
        std::cout << std::left << std::setw(30) << "o. L2 writebacks:" << 0 << std::endl;
        std::cout << std::left << std::setw(30) << "p. L2 prefetches:" << 0 << std::endl;
    }

    void printL2Measurements() {
        std::cout << std::left << std::setw(30) << "h. L2 reads (demand):" << std::dec << l2Reads << std::endl;
        std::cout << std::left << std::setw(30) << "i. L2 read misses (demand):" << std::dec << l2ReadMisses << std::endl;
        std::cout << std::left << std::setw(30) << "j. L2 reads (prefetch):" << std::dec << l2PrefetchReads << std::endl;
        std::cout << std::left << std::setw(30) << "k. L2 read misses (prefetch):" << std::dec << l2PrefetchReadMisses << std::endl;
        std::cout << std::left << std::setw(30) << "l. L2 writes:" << std::dec << l2Writes << std::endl;
        std::cout << std::left << std::setw(30) << "m. L2 write misses:" << std::dec << l2WriteMisses << std::endl;
        std::cout << std::left << std::setw(30) << "n. L2 miss rate:" << std::fixed << std::setprecision(4) << L2MissRate() << std::endl;
        std::cout << std::left << std::setw(30) << "o. L2 writebacks:" << std::dec << l2Writebacks << std::endl;
        std::cout << std::left << std::setw(30) << "p. L2 prefetches:" << std::dec << l2Prefetches << std::endl;
    }

};
