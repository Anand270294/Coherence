#include "cache.hpp"
#include <vector>
#include <cmath>

unsigned long cache::calculate_tag(unsigned long addr)
{
    return (addr >> (bits_Blk + bits_Sets)); // Tag value = 32 - (bits for sets) - (bits for block offset)
}

unsigned long cache::calculate_setInd(unsigned long addr)
{
    return ((addr >> bits_Blk) & tagMask);
}

cache::cache(int Size, int Assc, int Blk_size)
{

    // Initialize attrbutes
    size = (unsigned int)Size;                       // Size of the Cache
    assoc = (unsigned int)Assc;                      // Associativity
    blkSize = (unsigned int)Blk_size;                // Block Size
    sets = (unsigned int)(size / (blkSize * assoc)); // Number of Sets available
    bits_Sets = (unsigned int)(log2(sets));          // Number of bits required for Set Index
    bits_Blk = (unsigned int)(log2(blkSize));        // Number of bits required for Block Offset
    BusUpdate = (unsigned int)(2 * (blkSize / 4));   // Number of cycles to delay for a BusUpdate

    // Initialize all data variables to 0
    ops = loads = store = curr_cycle = idle_cycles = load_miss = store_miss = write_backs = c_to_c_xfers = num_invalidation = 0;
    access_to_private = access_to_shared = num_of_interventions = num_of_flushes = 0;

    // Memory Accesses always delays cycle by 100
    MEM_access = 100;

    tagMask = 0;
    for (int i = 0; i < bits_Sets; i++)
    {
        tagMask <<= 1;
        tagMask |= 1;
    }

    // Generate 2_D Vector as the Cache
    for (i = 0; i < sets; i++)
    {
        std::vector<block> tmp;
        for (j = 0; j < assoc; j++)
        {
            block b = block();
            tmp.push_back(b);
        }
        Cache.push_back(tmp);
    }
}

void cache::access_cache(unsigned long addr, int op, bool c_c)
{
    // Update Current Cycle +1
    curr_cycle++;

    // If an CPU operation is issued delay cycles
    if (op == 2)
    {
        curr_cycle += addr;
        ops++;
    }

    // If a Load Instruction is issued
    if (op == 0)
    {
        loads++;
        block *blk = find_set(addr);

        if (blk == NULL)
        {
            load_miss++;       // Update the miss
            replace_blk(addr); // Replace a block in Cache with one from Main Memory/Cache to Cache transfer
            if (c_c == false)
            {
                curr_cycle += MEM_access; // Delay for 100 Clock Cycles when Loading from Memory
                idle_cycles += MEM_access;
            }
            else
            {
                // Delay for 2 * (BlkSize/4) cycles for the Cache to get block from another Block
                CtoCxfer();
            }
        }
        else
        {
            curr_cycle += 1; // Hit is 1 cycle delay
            updateLRU(addr); // Update LRU
        }
    }

    // If a store Instruction is issued
    if (op == 1)
    {
        store++;
        block *blk = find_set(addr);

        if (blk == NULL)
        {
            store_miss++;      // Update Store misses
            replace_blk(addr); // Replace a block in Cache with one from Main Memory/Cache to Cache transfer
            if (c_c == false)
            {
                curr_cycle += MEM_access; // Delay for 100 Clock Cycles when Loading from Memory
                idle_cycles += MEM_access;
            }
            else
            {
                // Delay for 2 * (BlkSize/4) cycles for the Cache to get block from another Block
                CtoCxfer();
            }
        }
        else
        {
            curr_cycle += 1; // Hit is 1 cycle delay
            updateLRU(addr); // Update LRU and set Flag to Modified
            blk->setFlag(Flags::MODIFIED);
        }
    }
}

block *cache::find_set(unsigned long addr)
{
    // Decrypt tag
    unsigned long tag = calculate_tag(addr);

    // Decrypt set index
    unsigned long setInd = calculate_setInd(addr);

    // Search through the cache
    std::vector<block> *set = &(Cache.at(setInd));

    for (i = 0; i < assoc; i++)
    {
        if (set->at(i).isValid())
        { // Check if the block is valid
            if (set->at(i).getTag() == tag)
            { // Check if the tag matches
                break;
            }
        }
    }

    if (i == assoc)
    {
        return NULL;
    }
    else
    {
        return &(set->at(i));
    }
}

void cache::updateLRU(unsigned long addr)
{ // Insert the most recently used address and place it at the front

    unsigned long tag = calculate_tag(addr);
    unsigned long setInd = calculate_setInd(addr);

    std::vector<block> *set = &(Cache.at(setInd));

    for (i = 0; i < assoc; i++)
    {
        if (set->at(i).getTag() == tag)
        {
            block MRU = set->at(i);
            set->erase(set->begin() + i);   // Remove the block from the index
            set->insert(set->begin(), MRU); // Insert MRU block to the front of the set
            return;
        }
    }
}

block *cache::getLRU(unsigned long addr)
{
    unsigned long tag = calculate_tag(addr);
    unsigned long setInd = calculate_setInd(addr);

    std::vector<block> *set = &(Cache.at(setInd));

    block *LRU = &(set->at(assoc - 1));

    return LRU; // Ensure that the LRU is valid block
}

block *cache::replace_blk(unsigned long addr)
{ // new addr as input to be inserted

    block *victim = getLRU(addr);

    if (victim == NULL)
    { // Ensure no null ptr is returned
        return NULL;
    }

    if (victim->getFlag() == Flags::MODIFIED || victim->getFlag() == Flags::SHARED_MODIFIED)
    {
        curr_cycle += MEM_access; // Delay By another 100 cycles to send data back to Main Memory
        idle_cycles += MEM_access;
    }

    (victim)->setTag(calculate_tag(addr));
    (victim)->setFlag(Flags::MODIFIED);

    updateLRU(addr); // Update the "victim" to be the MRU

    return victim;
}

void cache::CtoCxfer()
{
    curr_cycle += 2 * (blkSize / 4);
    idle_cycles += 2 * (blkSize / 4);
}

void cache::writeBack()
{
    curr_cycle += MEM_access;
    idle_cycles += MEM_access;
}

// Getter function for the printing Data collected
unsigned int cache::getNoOperations() { return ops; }
unsigned int cache::getNoLoads() { return loads; }
unsigned int cache::getNoStore() { return store; }
unsigned int cache::getNoCycles() { return curr_cycle; }
unsigned int cache::getNoIdleCyc() { return idle_cycles; }
unsigned int cache::getNoLmiss() { return load_miss; }
unsigned int cache::getNoSmiss() { return store_miss; }
unsigned int cache::getblkSize() { return blkSize; }
