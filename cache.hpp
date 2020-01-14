#include <iostream>
#include <vector>

enum Flags
{
    INVALID = 0,
    VALID,
    DIRTY,
    SHARED,
    MODIFIED,
    EXCLUSIVE,
    SHARED_CLEAN,
    SHARED_MODIFIED,
    FORWARD
};

class block
{
protected:
    unsigned int tag;
    unsigned int flag;

public:
    block()
    {
        tag = 0;
        flag = 0; // Set starting flag to INVALID for Cold/Compulsory Misses
    }
    unsigned int getTag() { return tag; }
    unsigned int getFlag() { return flag; }
    void setFlag(unsigned int flags) { flag = flags; }
    void setTag(unsigned int tags) { tag = tags; }
    void invalidate()
    {
        tag = 0;
        flag = Flags::INVALID;
    }
    bool isValid() { return (flag != Flags::INVALID); }
};

class cache
{
protected:
    unsigned int size, blkSize, assoc, sets, curr_cycle, bits_Sets, bits_Blk, tagMask, BusUpdate, MEM_access, i, j;
    unsigned int ops, loads, store, idle_cycles, load_miss, store_miss;

    // 2_D Vector for Simulating the Cache
    std::vector<std::vector<block>> Cache;

    unsigned long calculate_tag(unsigned long);
    unsigned long calculate_setInd(unsigned long);

public:
    unsigned int c_to_c_xfers, num_invalidation, access_to_private, access_to_shared, num_of_interventions, num_of_flushes, write_backs;

    struct byCycle
    {
        bool operator()(const cache *a, const cache *b) const
        {
            return a->curr_cycle < b->curr_cycle;
        }
    };

    cache(int, int, int);
    void access_cache(unsigned long, int, bool c_c = false);
    block *find_set(unsigned long);
    block *getLRU(unsigned long);
    void updateLRU(unsigned long);
    block *replace_blk(unsigned long);
    void CtoCxfer();
    void writeBack();
    unsigned int getNoOperations();
    unsigned int getNoLoads();
    unsigned int getNoStore();
    unsigned int getNoCycles();
    unsigned int getNoIdleCyc();
    unsigned int getNoLmiss();
    unsigned int getNoSmiss();
    unsigned int getblkSize();
};
