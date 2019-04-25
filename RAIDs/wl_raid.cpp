#include <new>
#include <assert.h>
#include <stdio.h>
#include "../ssd.h"
#include <algorithm>
#include <chrono>
#include <ctime>


using namespace std::chrono;
using namespace ssd;
using namespace std;

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

// double WlRaid::event_arrive( const TraceRecord& op ){
//     check_reblance(op);

    
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
//         erasure_left[ssd_ids[0]] -= 1;
//         if(num_writes.find(stripe_id) != num_writes.end()){
//             num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
//         } else{
//             num_writes[stripe_id] = std::vector<double>(ssd_count, 0);
//             num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
//         }
//         //record the parity right
//         for( int i = 0; i < parity_count; i ++ ){
//             erasure_left[ssd_ids[i + 1]] -= 1;
//             ssd_writes[ssd_ids[i + 1]] += (double)opSize;
//             num_writes[stripe_id][ssd_ids[i + 1]];
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

ulong GetCommonN(ulong num1, ulong num2)
{
    if (num1 < num2) std:: swap(num1, num2);
    if (num1%num2 == 0)return num2;
    return GetCommonN(num2, num1%num2);
}
ulong lcm( ulong n, ulong m ){
    return n*m / GetCommonN(m, n);
}

inline void print_rebalance_workload( uint ssd1, uint ssd2, uint stripe_id, double time, double size, FILE* stream ){
    fprintf( stream,"rebalance,%lf,=,%d,%d,%d,%lf\n", time,ssd1,ssd2,stripe_id,size);
}

void WlRaid::check_reblance(const TraceRecord& op){

    if( need_reblance(op) ){
        //record algorithm time
        auto start = system_clock::now();

        double mean = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            mean += erasure_used[i];
        }
        mean = mean / ssd_count;

        std::vector<ulong> parity_dis(ssd_count,0);
        std::vector<double> parity_temp(ssd_count, 0);
        double temp_max = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            parity_temp[i] = ((erasure_used[i] / mean) * 100.0); //todo: find a proper number
            temp_max = temp_max > parity_temp[i] ? temp_max : parity_temp[i];
            parity_temp[i] = 1.0/parity_temp[i];
        }

        for( int i = 0; i < ssd_count; i ++ ){
            parity_temp[i] = temp_max * parity_temp[i] * 100.0;
            parity_dis[i] = (ulong) parity_temp[i];
        }

        ulong new_parity_loop = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            new_parity_loop += parity_dis[i];
        }

        ulong parity_lcm = lcm( new_parity_loop, last_parity_loop );
        double a1 = parity_lcm / last_parity_loop, b1 = parity_lcm / new_parity_loop;

        double miged_per_loop = 0;
        double moved[ssd_count];
        for( int i = 0; i < ssd_count; i++ ){
            moved[i] = (double)(last_parity_dis[i])*a1 - (double)(parity_dis[i])*b1;
            if( moved[i] > 0 ){
                miged_per_loop += moved[i];
            }
        }

        int loops = stripe_count / parity_lcm + ( stripe_count / parity_lcm != 0 );
        double total_miged = miged_per_loop *( loops );
                
        //record alogrithm time
        auto end   = system_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        for( int j = 0; j < ssd_count; j ++ ){
            if(moved[j] > 0){
                print_rebalance_workload( j, 0, 0, op.arrive_time, moved[j]*loops,stdout );
            }
        }

        //todo: debug3
        // for( int i = 0; i < loops; i++ ){
        //     for( int j = 0; j < ssd_count; j ++ ){
        //         if( moved[j] != 0){
        //             for( int p = 0; p < pages_per_sblock; p ++ ){
        //                 for( int q = 0; q < abs(moved[j]); q++ ){
        //                     raid_ssd.Ssds[j].event_arrive(READ, i*parity_lcm*pages_per_sblock + q + p, 1, op.arrive_time);
        //                     raid_ssd.Ssds[j].event_arrive(WRITE, i*parity_lcm*pages_per_sblock + q + p, 1, op.arrive_time);
        //                 }
        //             }
        //         }
        //     }
        // }
        
        start = system_clock::now();
        redis_map( parity_dis );

        //record alogrithm time
        end = system_clock::now();
        duration += duration_cast<microseconds>(end - start);
        fprintf(stdout, "rebalance_write_cost,%lf,%lf,=,%lf\n", op.arrive_time, total_miged, double(duration.count()) * microseconds::period::num / microseconds::period::den);
 
        last_parity_loop = new_parity_loop;

        //clean data
        num_writes.clear();
        num_reads.clear();
        
        for( int i = 0; i < ssd_count; i++ ) {
            ssd_writes[i] = 0;
            ssd_reads[i] = 0; 
		}
		total_writes = 0;
		total_reads = 0;
    }

}



bool WlRaid::need_reblance(const TraceRecord& op){
    
    for( int i = 0; i< ssd_count; i++ ){
        erasure_used[i] = ssd_erasures - erasure_left[i] + ssd_erasures * ssd_dead[i];
        erasure_used[i] = (double)((ulong)(erasure_used[i]) % (ulong)(3*ssd_erasures)); //todo: make sure max - min 3*ssd_erasures;
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

    var = var/(double)ssd_count;

    bool time_enough = false;
    if( total_writes > REBALANCE_THRE ){
        total_writes = 0;
        total_reads = 0;
        time_enough = true;
    }

    return (var >= var_thre) && time_enough;
}


//todo: debug-1 : if the var cross the thre, it always active the rebalance, that is stupid
// bool WlRaid::need_reblance(const TraceRecord& op){

//     for( int i = 0; i< ssd_count; i++ ){
//         erasure_used[i] = ssd_erasures - erasure_left[i] + ssd_erasures * ssd_dead[i];
//         erasure_used[i] = (double)((ulong)(erasure_used[i]) % (ulong)(3*ssd_erasures)); //todo: make sure max - min 3*ssd_erasures;
//     }
 
//     if( op.arrive_time - last_rtimep > 10 ){
//         last_rtimep = op.arrive_time;
//         return true;
//     }

//     return false;
//}


// void WlRaid::print_ssd_erasures( FILE* stream, double time ){
//     double mean = 0;
//     fprintf(stream, "erasure_data,%lf,=,",time);
//     for( int i = 0; i < ssd_count; i++ ){
//         mean += erasure_left[i];
//         fprintf(stream, "%lf,", erasure_left[i]);
//     }

//     mean /= (double) ssd_count;

//     double var = 0;
//     for( int i = 0; i < ssd_count; i ++ ) { 
//         var += ((erasure_left[i] - mean)/ssd_erasures) * ((erasure_left[i] - mean)/ssd_erasures);
//     }

//     var = var / ssd_count;
//     fprintf(stream, "0,%lf,%lf\n", mean, var);
// }

// void WlRaid::check_and_print_stat( const TraceRecord& op,FILE* stream ){
// 	if( need_print( op ) ){
//         print_ssd_erasures( stream,op.arrive_time );
// 		raid_ssd.write_statistics(stream, op.arrive_time);
//         raid_ssd.reset_statistics();
// 	}
// }