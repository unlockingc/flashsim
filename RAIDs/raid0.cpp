#include <new>
#include <assert.h>
#include <stdio.h>
#include "../ssd.h"

using namespace ssd;

Raid0::Raid0(uint ssd_count_, uint pages_per_ssd_, uint parity_count_, double ssd_erasures_, uint pages_per_sblock_):\
RaidParent( ssd_count_, pages_per_ssd_, parity_count_, ssd_erasures_, pages_per_sblock_  )
{
    //assert(ssd_count == 5);
    assert(parity_count == 0);
    init();
}

void Raid0::init_map(){
    int temp1 = 0;
    int temp2 = 0;
    for( uint i = 0; i < stripe_count; i++ ){
        for( uint j = 0; j < ssd_count; j ++ ){
            smap[i][j] = (i + j)%ssd_count;
        }
    }
}

// double Raid5::event_arrive( const TraceRecord& op ){
    
//     //calculate tranlated addr and blocks need to be operated
//     uint ssd_ids[1 + parity_count];
// 	int page_id = op.vaddr/(ssd_count-parity_count);
// 	int stripe_id = page_id/pages_per_sblock;
//     int tranlated_addr = page_id; 
// 	uint logical_block = (op.vaddr%(ssd_count-parity_count));
// 	ssd_ids[0]= smap[stripe_id][logical_block];
	
//     for( int i = 0; i < parity_count; i ++ ){
// 		ssd_ids[i + 1] = smap[stripe_id][ssd_count-(i+1)];
// 	}

//     int opSize = op.size/4096 + (op.size%4096 == 0?0:1); //write or read the whole block
    
//     //record data todo:figure out count as pages or bits
//     if( op.op == 'w' ){
//         check_erasure_and_swap_ssd( opSize, ssd_ids, 1 + parity_count, op.arrive_time );
//         ssd_writes[ssd_ids[0]] += (double)opSize;
//         if(num_writes.find(stripe_id) != num_writes.end()){
//             num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
//         } else{
//             num_writes[stripe_id] = std::vector<double>(ssd_count, 0);
//             num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
//         }
//         //record the parity right
//         for( int i = 0; i < parity_count; i ++ ){
//                 ssd_writes[ssd_ids[i + 1]] += (double)opSize;
//                 num_writes[stripe_id][ssd_ids[i + 1]];
//         }
        
//     } else if( op.op == 'r' ){
//         ssd_reads[ssd_ids[0]] += (double)opSize;
//         if(num_reads.find(stripe_id) != num_reads.end()){
//             num_reads[stripe_id][ssd_ids[0]] += (double)opSize;
//         } else{
//             num_reads[stripe_id] = std::vector<double>(ssd_count,0);
//             num_reads[stripe_id][ssd_ids[0]] = (double)opSize;
//         }
//     }

//     double time = 0,single_time;
//     for( int i = 0; i < 1 + parity_count; i ++ ){
//         single_time = raid_ssd.Ssds[ssd_ids[i]].event_arrive(op.op == 'r'?READ:WRITE, tranlated_addr, 1, op.arrive_time);
//         if( time < single_time ){
//             time = single_time;
//         }
//     }
//     return time;
// }
