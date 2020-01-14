Assumptions:

1. All Caches irregardless of the protocol starts at a invalid state

2. Recieving/Sending a block of data from Cache to Cache will delay both Caches with the associated delay

3.For MESI/Dragon, the first cache to be found having a valid copy of the Block will be the sender to Cache requesting the Block


4. Cache to Cache transfers are only registered to sender of the block not the reciever to prevent double counting of data traffic



FOR MESIF:

The FORWARD flag to indicatet the block is clean and shared. There may be SHARED flags without any FORWARD flag indicating the Block has been MODIFIED.
The block with the FORWARD flag is tasked to broadcast valid copies of the Block when requested and it can discard the Block without write back since it is a clean copy.

The Block to get the recieve the SHARED data last will be given the FORWARD flag to minimize the chance of accessing main memory for the block.
