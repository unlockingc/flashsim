#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"
#include <algorithm> 

using namespace ssd;

WlRaid::WlRaid(uint ssd_count_, uint pages_per_ssd_, uint parity_count_, double ssd_erasures_, uint pages_per_sblock_, double var_thre_):\
RaidParent( ssd_count_, pages_per_ssd_, parity_count_, ssd_erasures_, pages_per_sblock_  ),\
var_thre(var_thre_),erasure_used(ssd_count_,0),last_parity_dis(ssd_count, 1)
{
    assert(ssd_count == 5);
    assert(parity_count == 1);
    last_parity_loop = ssd_count;
    for( int i = 0; i < ssd_count; i ++ ){
        last_parity_dis[i] = 1;
    }
    init();
}

void WlRaid::init_map(){
    int temp1 = 0;
    int temp2 = 0;
    for( uint i = 0; i < stripe_count; i++ ){
        for( uint j = 0; j < ssd_count; j ++ ){
            smap[i][j] = (i + j)%ssd_count;
        }
    }
}

void WlRaid::redis_map( std::vector<ulong> new_parity_dis ){
    for( int i = 0; i < ssd_count; i ++ ){
        last_parity_dis[i] = new_parity_dis[i];
    }

    std::vector<ulong> parity_b(ssd_count, 0);
    parity_b[0] = new_parity_dis[0];
    for( int i = 1; i < new_parity_dis.size(); i++ ){
        parity_b[i] += parity_b[i - 1] + new_parity_dis[i];
    }

    int temp1 = 0;
    int temp2 = 0;
    int p = 0;
    int current = 0;

    for( uint i = 0; i < stripe_count; i++ ){
        smap[i][ssd_count - 1] = current;

        for( uint j = 0; j < ssd_count; j ++ ){
            smap[i][j] = (current + j + 1) % ssd_count;
        }

        p ++;
        if( p == parity_b[current] ){
            current ++;
            if( current == parity_b.size() ){
                current = 0;
                p = 0;
            }
        }
    }
}

double WlRaid::event_arrive( const TraceRecord& op ){
    check_reblance(op);

    
    //calculate tranlated addr and blocks need to be operated
    uint ssd_ids[1 + parity_count];
	int page_id = op.vaddr/(ssd_count-parity_count);
	int stripe_id = page_id/pages_per_sblock;
    int tranlated_addr = page_id; 
	uint logical_block = (op.vaddr%(ssd_count-parity_count));
	ssd_ids[0]= smap[stripe_id][logical_block];
	
    for( int i = 0; i < parity_count; i ++ ){
		ssd_ids[i + 1] = smap[stripe_id][ssd_count-(i+1)];
	}

    int opSize = op.size/4096 + (op.size%4096 == 0?0:1); //write or read the whole block
    
    
    //record data todo:figure out count as pages or bits
    if( op.op == 'w' ){
        check_erasure_and_swap_ssd( opSize, ssd_ids, 1 + parity_count, op.arrive_time );
        ssd_writes[ssd_ids[0]] += (double)opSize;
        erasure_left[ssd_ids[0]] -= 1;
        if(num_writes.find(stripe_id) != num_writes.end()){
            num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
        } else{
            num_writes[stripe_id] = std::vector<double>(ssd_count, 0);
            num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
        }
        //record the parity right
        for( int i = 0; i < parity_count; i ++ ){
            erasure_left[ssd_ids[i + 1]] -= 1;
            ssd_writes[ssd_ids[i + 1]] += (double)opSize;
            num_writes[stripe_id][ssd_ids[i + 1]];
        }
        
    } else if( op.op == 'r' ){
        ssd_reads[ssd_ids[0]] += (double)opSize;
        if(num_reads.find(stripe_id) != num_reads.end()){
            num_reads[stripe_id][ssd_ids[0]] += (double)opSize;
        } else{
            num_reads[stripe_id] = std::vector<double>(ssd_count,0);
            num_reads[stripe_id][ssd_ids[0]] = (double)opSize;
        }
    }

    double time = 0,single_time;
    for( int i = 0; i < 1 + parity_count; i ++ ){
        single_time = raid_ssd.Ssds[ssd_ids[i]].event_arrive(op.op == 'r'?READ:WRITE, tranlated_addr, 1, op.arrive_time);
        if( time < single_time ){
            time = single_time;
        }
    }
    return time;
}

ulong GetCommonN(ulong num1, ulong num2)
{
    if (num1 < num2) std:: swap(num1, num2);
    if (num1%num2 == 0)return num2;
    return GetCommonN(num2, num1%num2);
}
ulong lcm( ulong n, ulong m ){
    return n*m / GetCommonN(m, n);
}

void WlRaid::check_reblance(const TraceRecord& op){

    if( need_reblance(op) ){
        double mean = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            mean += erasure_used[i];
        }
        mean = mean / ssd_count;

        std::vector<ulong> parity_dis(ssd_count,0);
        for( int i = 0; i < ssd_count; i ++ ){
            parity_dis[i] = (ulong)((erasure_used[i] / mean) * 100.0);
        }

        ulong temp_lcm = lcm( parity_dis[0], parity_dis[1] );
        for( int i = 0; i < ssd_count; i ++ ){
            temp_lcm = lcm(temp_lcm, parity_dis[i]);
        }

        ulong new_parity_loop = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            parity_dis[i] = temp_lcm / parity_dis[i];
            new_parity_loop += parity_dis[i];
        }

        ulong parity_lcm = lcm( new_parity_loop, last_parity_loop );
        ulong a1 = parity_lcm / last_parity_loop, b1 = parity_lcm / new_parity_loop;

        ulong miged_per_loop = 0;
        ulong moved[ssd_count];
        for( int i = 0; i < ssd_count; i++ ){
            moved[i] = last_parity_dis[i]*a1 - parity_dis[i]*b1;
            if( moved[i] > 0 ){
                miged_per_loop += moved[i];
            }
        }

        int loops = stripe_count / parity_lcm + ( stripe_count / parity_lcm != 0 );
        ulong total_miged = miged_per_loop *( loops );
                


        for( int i = 0; i < loops; i++ ){
            for( int j = 0; j < ssd_count; j ++ ){
                if( moved[j] > 0 ) {
                    for( int p = 0; p < pages_per_sblock; p ++ ){
                        raid_ssd.Ssds[j].event_arrive(READ, i*parity_lcm*pages_per_sblock + p, abs(moved[j]), op.arrive_time);
                    }
                } else {
                    for( int p = 0; p < pages_per_sblock; p ++ ){
                        raid_ssd.Ssds[j].event_arrive(WRITE, i*parity_lcm*pages_per_sblock + p, abs(moved[j]), op.arrive_time);
                    }
                }
            }
        }

        redis_map( parity_dis );

        last_parity_loop = new_parity_loop;
    }

}



bool WlRaid::need_reblance(const TraceRecord& op){
    
    for( int i = 0; i< ssd_count; i++ ){
        erasure_used[i] = ssd_erasures - erasure_left[i];
    }

    double mean = 0;

    for( int i = 0; i < ssd_count; i ++ ){
        mean += erasure_used[i];
    }
    mean = mean / ssd_count;

    double var = 0;
    for( int i = 0; i < ssd_count; i ++ ){
        var += ((erasure_used[i] - mean)/ssd_erasures) * ((erasure_used[i] - mean)/ssd_erasures);
    }

    var = var/ssd_count;

    return var >= var_thre;
}

