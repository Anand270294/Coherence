#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include "cache.hpp"

int clk_cycles = 0;
const std::string dir = "./trace_files/";
const int num_processors = 4;

void Dragon_protocol(std::vector<cache *> cacheArray, int processor, int op, unsigned long addr)
{
    // Check if the Instruction is a non-load/store instruction
    if (op == 2)
    {
        cacheArray.at(processor)->access_cache(addr, op);
        return;
    }

    // Check if there is exist a copy of the target addr in all Caches
    bool copy = false;
    for (int i = 0; i < num_processors; i++)
    {
        if (i != num_processors && cacheArray.at(i)->find_set(addr) != NULL && cacheArray.at(i)->find_set(addr)->getFlag() != INVALID)
        {
            copy = true; // A valid copy of this block exist in a Cache
        }
    }

    // Check if the block has been xfered to Current Cache
    bool c_cxfered = false;

    // If current Address is not available in the current Cache:
    if (cacheArray.at(processor)->find_set(addr) == NULL || cacheArray.at(processor)->find_set(addr)->getFlag() == INVALID)
    {
        if (op == 0)
        {
            if (copy == true)
            {
                cacheArray.at(processor)->access_cache(addr, op, true);          // Block is obtained from another cache
                cacheArray.at(processor)->find_set(addr)->setFlag(SHARED_CLEAN); // Set the flag of the block to SHARED CLEAN since another valid copy
                                                                                 // exist in the cache

                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL && cacheArray.at(i)->find_set(addr)->getFlag() != INVALID)
                    {
                        // Find the first Cache that contains the Block and transfer Block
                        if (c_cxfered == false)
                        {
                            cacheArray.at(i)->c_to_c_xfers++;
                            cacheArray.at(i)->CtoCxfer();

                            // Check if it is an access to a Private or Shared Block
                            if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED || cacheArray.at(i)->find_set(addr)->getFlag() == EXCLUSIVE)
                            {
                                cacheArray.at(processor)->access_to_private++;
                            }
                            else
                            {
                                cacheArray.at(processor)->access_to_shared++;
                            }

                            c_cxfered = true;
                        }

                        // BusRead Occurs Blk is transfered from this cache to Current Processor Cache
                        // Flags are updated in each Cache accordingly
                        if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED)
                        {
                            //cacheArray.at(processor)->access_to_private++;
                            cacheArray.at(i)->find_set(addr)->setFlag(SHARED_MODIFIED);
                        }
                        if (cacheArray.at(i)->find_set(addr)->getFlag() == EXCLUSIVE)
                        {
                            //cacheArray.at(processor)->access_to_private++;
                            cacheArray.at(i)->find_set(addr)->setFlag(SHARED_CLEAN);
                        }
                        if (cacheArray.at(i)->find_set(addr)->getFlag() == SHARED_MODIFIED)
                        {
                            //Since its already shared, considered access to shared
                            //cacheArray.at(processor)->access_to_shared++;
                        }
                    }
                }
            }
            else
            {
                // no copy, set exclusive and get block from main memory
                cacheArray.at(processor)->access_cache(addr, op);
                cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            }
            return;
        }
        else if (op == 1)
        {
            //Store Operation
            cacheArray.at(processor)->access_cache(addr, op);

            if (copy == true)
            {
                cacheArray.at(processor)->find_set(addr)->setFlag(Flags::SHARED_MODIFIED);

                // Broadcast the Modified Block to the other Caches which has a copy
                cacheArray.at(processor)->c_to_c_xfers++;
                cacheArray.at(processor)->CtoCxfer();

                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL && cacheArray.at(i)->find_set(addr)->getFlag() != INVALID)
                    {
                        // Set all Caches with a copy to SHARED CLEAN
                        cacheArray.at(i)->find_set(addr)->setFlag(SHARED_CLEAN);
                        // Delay clock cycle for each block that has a copy to recieve block data
                        cacheArray.at(i)->CtoCxfer();
                    }
                }
            }
            else
            {
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            }
            return;
        }
    }

    // for EXCLUSIVE state
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == EXCLUSIVE)
    {
        if (op == 0)
        {
            // Flag remains Exclusive since to changes is made
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            return;
        }
        else if (op == 1)
        {
            // Flag must be changed to MODIFIED
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
    }

    // for MODIFIED state, no state change and no Bus Update
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == MODIFIED)
    {
        cacheArray.at(processor)->access_cache(addr, op);
        cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);

        return;
    }

    //for SHARED_CLEAN state
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == SHARED_CLEAN)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(SHARED_CLEAN);
        }

        if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);

            if (copy == true)
            {
                // Broadcast the Modified Block to the other Caches which has a copy
                cacheArray.at(processor)->c_to_c_xfers++;
                cacheArray.at(processor)->CtoCxfer();

                cacheArray.at(processor)->find_set(addr)->setFlag(SHARED_MODIFIED);
            }
            else
            {
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            }

            for (int i = 0; i < num_processors; i++)
            {
                if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                {
                    // change SHARED_MODIFIED state to SHARED_CLEAN if any since a Bus Update was issued
                    if (cacheArray.at(i)->find_set(addr)->getFlag() == SHARED_MODIFIED)
                    {
                        cacheArray.at(i)->find_set(addr)->setFlag(SHARED_CLEAN);
                        // Delay clock cycle to recieve new block data
                        cacheArray.at(i)->CtoCxfer();
                    }
                }
            }
        }
        return;
    }

    // for SHARED_MODIFIED state
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == SHARED_MODIFIED)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(SHARED_MODIFIED);
        }
        else if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            if (copy == true)
            {
                // when write to it and there are other copies, the state unchanged
                cacheArray.at(processor)->find_set(addr)->setFlag(SHARED_MODIFIED);
            }
            else
            {
                // otherwise, change to MODIFIED
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            }
        }
    }
    return;
}

void MESI_protocol(std::vector<cache *> cacheArray, int processor, int op, unsigned long addr)
{
    // Check if the Instruction is a non-load/store instruction
    if (op == 2)
    {
        cacheArray.at(processor)->access_cache(addr, op);
        return;
    }

    //If Address in not available in the Current Cache
    if (cacheArray.at(processor)->find_set(addr) == NULL)
    {
        bool copy = false;
        for (int i = 0; i < num_processors; i++)
        {
            if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
            {
                copy = true;
            }
        }
        if (op == 0)
        {
            if (copy == true)
            {
                //Cache to Cache Transfer when Line exists in another processor and set Flag to SHARED
                cacheArray.at(processor)->access_cache(addr, op, true);
                cacheArray.at(processor)->find_set(addr)->setFlag(SHARED);

                // Check if the block has been xfered to Current Cache
                bool c_cxfered = false;

                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                    {
                        // Find the 1st Cache to have a copy of the Block
                        if (c_cxfered == false)
                        {
                            // Send block from the first available cache to the Current Cache
                            cacheArray.at(i)->c_to_c_xfers++;
                            cacheArray.at(i)->CtoCxfer();

                            // Check if it is a shared or private access
                            if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED || cacheArray.at(i)->find_set(addr)->getFlag() == EXCLUSIVE)
                            {
                                cacheArray.at(processor)->access_to_private++;
                            }
                            else
                            {
                                cacheArray.at(processor)->access_to_shared++;
                            }
                        }

                        if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED)
                        {
                            // Write back to Main Memory delay clock cycles
                            cacheArray.at(i)->writeBack();
                        }
                        // Since the Block sent to another all other caches set the SHARED flag
                        cacheArray.at(i)->find_set(addr)->setFlag(SHARED);
                    }
                }
            }
            else
            {
                // If no copy is found change set the flag to Exclusive
                cacheArray.at(processor)->access_cache(addr, op);
                cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            }
            return;
        }
        else if (op == 1)
        {
            //Processor Write/Store
            if (copy == true)
            {
                // Cache to Cache transfer to obtain copy
                cacheArray.at(processor)->access_cache(addr, op, true);
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);

                // Check if the block has been xfered to Current Cache
                bool c_cxfered = false;

                //Generates BusRDx to get a copy and invalidate other caches if a copy exist in other caches
                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                    {
                        // Find the 1st Cache to have a copy of the Block
                        if (c_cxfered == false)
                        {
                            // Send block from the first available cache to the Current Cache
                            cacheArray.at(i)->c_to_c_xfers++;
                            cacheArray.at(i)->CtoCxfer();

                            // Check if it is a shared or private access
                            if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED || cacheArray.at(i)->find_set(addr)->getFlag() == EXCLUSIVE)
                            {
                                cacheArray.at(processor)->access_to_private++;
                            }
                            else
                            {
                                cacheArray.at(processor)->access_to_shared++;
                            }
                        }

                        if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED)
                        {
                            // Write back to Main Memory delay clock cycles
                            cacheArray.at(i)->writeBack();
                        }

                        // Invalidate all copies that have a copy
                        cacheArray.at(i)->find_set(addr)->invalidate();
                    }
                }
            }
            else
            {
                // If no copy exist then get block from Main Memory
                cacheArray.at(processor)->access_cache(addr, op);
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            }
            return;
        }
    }

    //For Exclusive State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == EXCLUSIVE)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            return;
        }
        else if (op == 1)
        {
            // Store is issued then cahnge the flag to MODIFIED
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
    }

    //For SHARED State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == SHARED)
    {
        // If load instr is issued
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            return;
        }
        // If store instr is issued
        else if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);

            //Generates BusUPGR to invalidate other caches
            for (int i = 0; i < num_processors; i++)
            {
                if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                {
                    if (cacheArray.at(i)->find_set(addr)->getFlag() == SHARED)
                    {
                        cacheArray.at(i)->num_invalidation++;
                    }
                    else
                    {
                        cacheArray.at(i)->num_invalidation++;
                    }
                    cacheArray.at(i)->find_set(addr)->invalidate();
                }
            }
            return;
        }
    }

    //For MODIFIED State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == MODIFIED)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
        else if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
    }

    return;
}

void MESIF_protocol(std::vector<cache *> cacheArray, int processor, int op, unsigned long addr)
{
    // Check if the Instruction is a non-load/store instruction
    if (op == 2)
    {
        cacheArray.at(processor)->access_cache(addr, op);
        return;
    }

    //If Address in not available in the Current Cache
    if (cacheArray.at(processor)->find_set(addr) == NULL)
    {
        bool copy = false;
        for (int i = 0; i < num_processors; i++)
        {
            if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
            {
                copy = true;
            }
        }
        if (op == 0)
        {
            if (copy == true)
            {
                //Cache to Cache Transfer when Line exists in another processor and set Flag to FORWARD
                // Since Current Cache is the Most Recent to get a copy
                cacheArray.at(processor)->access_cache(addr, op, true);
                cacheArray.at(processor)->find_set(addr)->setFlag(FORWARD);

                // Check if the block has been xfered to Current Cache
                bool c_cxfered = false;

                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                    {
                        // Find the Forward blk Cache to have a copy of the Block
                        if (c_cxfered == false)
                        {
                            // Send block from the Forward Cache cache to the Current Cache
                            // Forward Cache becomes SHARED STATE
                            if (cacheArray.at(i)->find_set(addr)->getFlag() == FORWARD)
                            {
                                cacheArray.at(i)->c_to_c_xfers++;
                                cacheArray.at(i)->CtoCxfer();
                                cacheArray.at(processor)->access_to_shared++;
                            }
                            else if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED)
                            {
                                // Send MODIFIED COPY over to Current Cache
                                cacheArray.at(i)->c_to_c_xfers++;
                                cacheArray.at(i)->CtoCxfer();

                                // Write Back to Main Memory
                                cacheArray.at(i)->writeBack();
                                cacheArray.at(processor)->access_to_private++;
                            }
                        }
                        // Since the Block sent to another all other caches set the SHARED flag
                        cacheArray.at(i)->find_set(addr)->setFlag(SHARED);
                    }
                }
            }
            else
            {
                // If no copy is found change set the flag to Exclusive
                cacheArray.at(processor)->access_cache(addr, op);
                cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            }
            return;
        }
        else if (op == 1)
        {
            //Processor Write/Store
            if (copy == true)
            {
                // Cache to Cache transfer to obtain copy
                cacheArray.at(processor)->access_cache(addr, op, true);
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);

                // Check if the block has been xfered to Current Cache
                bool c_cxfered = false;

                //Generates BusRDx to get a copy and invalidate other caches if a copy exist in other caches
                for (int i = 0; i < num_processors; i++)
                {
                    if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                    {
                        // Find the 1st Cache to have a copy of the Block
                        if (c_cxfered == false)
                        {
                            // Send block from the first available cache to the Current Cache
                            cacheArray.at(i)->c_to_c_xfers++;
                            cacheArray.at(i)->CtoCxfer();

                            // Check if it is a shared or private access
                            if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED || cacheArray.at(i)->find_set(addr)->getFlag() == EXCLUSIVE)
                            {
                                cacheArray.at(processor)->access_to_private++;
                            }
                            else
                            {
                                cacheArray.at(processor)->access_to_shared++;
                            }
                        }

                        if (cacheArray.at(i)->find_set(addr)->getFlag() == MODIFIED)
                        {
                            // Write back to Main Memory delay clock cycles
                            cacheArray.at(i)->writeBack();
                        }

                        // Invalidate all copies that have a copy
                        cacheArray.at(i)->find_set(addr)->invalidate();
                    }
                }
            }
            else
            {
                // If no copy exist then get block from Main Memory
                cacheArray.at(processor)->access_cache(addr, op);
                cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            }
            return;
        }
    }

    //For Exclusive State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == EXCLUSIVE)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(EXCLUSIVE);
            return;
        }
        else if (op == 1)
        {
            // Store is issued then cahnge the flag to MODIFIED
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
    }

    //For SHARED State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == SHARED)
    {
        // If load instr is issued
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            return;
        }
        // If store instr is issued
        else if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);

            //Generates BusUPGR to invalidate other caches
            for (int i = 0; i < num_processors; i++)
            {
                if (i != processor && cacheArray.at(i)->find_set(addr) != NULL)
                {
                    if (cacheArray.at(i)->find_set(addr)->getFlag() == SHARED)
                    {
                        cacheArray.at(i)->num_invalidation++;
                    }
                    else
                    {
                        cacheArray.at(i)->num_invalidation++;
                    }
                    cacheArray.at(i)->find_set(addr)->invalidate();
                }
            }
            return;
        }
    }

    //For MODIFIED State
    if (cacheArray.at(processor)->find_set(addr)->getFlag() == MODIFIED)
    {
        if (op == 0)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
        else if (op == 1)
        {
            cacheArray.at(processor)->access_cache(addr, op);
            cacheArray.at(processor)->find_set(addr)->setFlag(MODIFIED);
            return;
        }
    }

    return;
}

void run_trace(std::vector<cache *> cacheArray, std::string protocol, std::string path0, std::string path1, std::string path2, std::string path3)
{
    unsigned int x = 1;
    // Read Files for 4 processors
    std::ifstream p0(path0, std::ios::in);
    std::ifstream p1(path1, std::ios::in);
    std::ifstream p2(path2, std::ios::in);
    std::ifstream p3(path3, std::ios::in);

    while (!p0.eof() | !p1.eof() | !p2.eof() | !p3.eof())
    {
        std::string p0_op, p1_op, p2_op, p3_op, p0_value, p1_value, p2_value, p3_value;

        if (!p0.eof())
        {
            p0 >> p0_op >> p0_value;
            unsigned long value0 = std::stoul(p0_value, nullptr, 16);
            if (protocol == "Dragon")
                Dragon_protocol(cacheArray, 0, std::stoi(p0_op), value0);
            if (protocol == "MESI")
                MESI_protocol(cacheArray, 0, std::stoi(p0_op), value0);
            if (protocol == "MESIF")
                MESIF_protocol(cacheArray, 0, std::stoi(p0_op), value0);
        }
        if (!p1.eof())
        {
            p1 >> p1_op >> p1_value;
            unsigned long value1 = std::stoul(p1_value, nullptr, 16);
            if (protocol == "Dragon")
                Dragon_protocol(cacheArray, 1, std::stoi(p1_op), value1);
            if (protocol == "MESI")
                MESI_protocol(cacheArray, 1, std::stoi(p1_op), value1);
            if (protocol == "MESIF")
                MESIF_protocol(cacheArray, 1, std::stoi(p1_op), value1);
        }
        if (!p2.eof())
        {
            p2 >> p2_op >> p2_value;
            unsigned long value2 = std::stoul(p2_value, nullptr, 16);
            if (protocol == "Dragon")
                Dragon_protocol(cacheArray, 2, std::stoi(p2_op), value2);
            if (protocol == "MESI")
                MESI_protocol(cacheArray, 2, std::stoi(p2_op), value2);
            if (protocol == "MESIF")
                MESIF_protocol(cacheArray, 2, std::stoi(p2_op), value2);
        }
        if (!p3.eof())
        {
            p3 >> p3_op >> p3_value;
            unsigned long value3 = std::stoul(p3_value, nullptr, 16);
            if (protocol == "Dragon")
                Dragon_protocol(cacheArray, 3, std::stoi(p3_op), value3);
            if (protocol == "MESI")
                MESI_protocol(cacheArray, 3, std::stoi(p3_op), value3);
            if (protocol == "MESIF")
                MESIF_protocol(cacheArray, 3, std::stoi(p3_op), value3);
        }
    }
}

void print_data(std::vector<cache *> cacheArray, std::string protocol)
{
    auto max_cycle = *std::max_element(cacheArray.begin(), cacheArray.end(),
                                       cache::byCycle());

    if (protocol == "Dragon")
    {
        std::cout << "Protocol : Dragon 4-state " << std::endl;
        std::cout << "Overall Execution Cycle:  " << max_cycle->getNoCycles() << std::endl;
        std::cout << std::endl;
    }

    if (protocol == "MESI")
    {
        std::cout << "Protocol : MESI " << std::endl;
        std::cout << "Overall Execution Cycle:  " << max_cycle->getNoCycles() << std::endl;
        std::cout << std::endl;
    }

    if (protocol == "MESIF")
    {
        std::cout << "Protocol : MESIF " << std::endl;
        std::cout << "Overall Execution Cycle:  " << max_cycle->getNoCycles() << std::endl;
        std::cout << std::endl;
    }

    for (int i = 0; i < num_processors; i++)
    {
        std::cout << "************ Simulation results for Cache " << i << " ************ :" << std::endl;
        std::cout << "Number of cycles:                       " << cacheArray.at(i)->getNoCycles() << std::endl;
        std::cout << "Number of idle cycles:                  " << cacheArray.at(i)->getNoIdleCyc() << std::endl;
        std::cout << "Number of compute cycles                " << cacheArray.at(i)->getNoCycles() - cacheArray.at(i)->getNoIdleCyc() << std::endl;
        std::cout << std::endl;
        std::cout << "Number of loads:                        " << cacheArray.at(i)->getNoLoads() << std::endl;
        std::cout << "Number of load misses:                  " << cacheArray.at(i)->getNoLmiss() << std::endl;
        std::cout << "Number of stores:                       " << cacheArray.at(i)->getNoStore() << std::endl;
        std::cout << "Number of store misses:                 " << cacheArray.at(i)->getNoSmiss() << std::endl;
        std::cout << std::endl;
        std::cout << "Total miss rate:                        " << (float)(cacheArray.at(i)->getNoLmiss() + cacheArray.at(i)->getNoSmiss()) / (float)(cacheArray.at(i)->getNoLoads() + cacheArray.at(i)->getNoStore()) * 100.0 << '%' << std::endl;
        std::cout << std::endl;
        std::cout << "Cache-Cache Data Traffic:               " << cacheArray.at(i)->c_to_c_xfers * cacheArray.at(i)->getblkSize() << " Bytes" << std::endl;
        std::cout << "Number of Invalidations:                " << cacheArray.at(i)->num_invalidation << std::endl;
        std::cout << "Number of Shared Access:                " << cacheArray.at(i)->access_to_shared << std::endl;
        std::cout << "Number of Private Access:               " << cacheArray.at(i)->access_to_private << std::endl;
        std::cout << std::endl;
        std::cout << std::endl;
    }
    return;
}

//Create Instr class for the instr and value of 32-bit
//std::vector<> intst_core1;
int main(int argc, char *argv[])
{
    //If in correct inputs provided throw error
    if (argv[1] == NULL)
    {
        printf("input format: ");
        printf("./cache_protocols <protocol> <data_set> <cache_size> <assoc> <block_size>\n");
        exit(0);
    }

    // Set variable input for the type of process to be run
    std::string protocol = argv[1];      // Type of protocol to be selected 0: MESI , 1: Dragon 4-State
    std::string data_set = argv[2];      // Data set to be selected blackscholes, bodytrack, fluidanimates
    int cache_size = std::stoi(argv[3]); // Cache Size (in Bytes)
    int assoc = std::stoi(argv[4]);      // Associativity for the Caches
    int blk_size = std::stoi(argv[5]);   // Block Size (in Bytes)

    std::vector<cache *> CacheArray;

    // Initialize all Caches for the
    for (int i = 0; i < num_processors; i++)
    {
        //cache *c = &(cache(cache_size, assoc, blk_size));
        CacheArray.emplace_back(new cache(cache_size, assoc, blk_size));
    }

    // Set the path files for the trace files for each processor
    std::string path0 = dir + data_set + "/" + data_set + "_0.data";
    std::string path1 = dir + data_set + "/" + data_set + "_1.data";
    std::string path2 = dir + data_set + "/" + data_set + "_2.data";
    std::string path3 = dir + data_set + "/" + data_set + "_3.data";

    run_trace(CacheArray, protocol, path0, path1, path2, path3);

    print_data(CacheArray, protocol);

    return 0;
}