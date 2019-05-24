#include <new>
#include <assert.h>
#include <stdio.h>
#include "../ssd.h"
#include <algorithm>
#include <chrono>
#include <ctime>

using namespace std::chrono;
using namespace std;
using namespace ssd;

SaRaid::SaRaid(uint ssd_count_, uint pages_per_ssd_, uint parity_count_, double ssd_erasures_, uint pages_per_sblock_, double time_thre_, double max_mig_, double diff_percent_, double var_thre_, bool read_opt_):\
RaidParent( ssd_count_, pages_per_ssd_, parity_count_, ssd_erasures_, pages_per_sblock_  ), time_thre(time_thre_), last_rtime(0), diff_erasures(ssd_count_, 0),\
max_mig(max_mig_),diff_percent(diff_percent_),var_thre(var_thre_), read_opt(read_opt_)
{
    assert(ssd_count == 5);
    assert(parity_count == 1);

    double diff_step = ssd_erasures * diff_percent;
 
    int mid = ssd_count / 2;
    diff_erasures[0] = diff_step * mid;

    for( int i = 1; i < ssd_count; i++ ){
        diff_erasures[i] = diff_erasures[i-1] - diff_step;
    }

    init();
}

void SaRaid::init_map(){

    std::vector<uint> parity_dis(ssd_count, 0);
    std::vector<uint> parity_b(ssd_count, 0);
    int mid = ssd_count /2;
    int diff_temp = (int)(diff_percent * 100);
    parity_dis[0] = 100 - diff_temp * mid; 
    parity_b[0] = parity_dis[0];
    for( int i = 1; i < parity_dis.size(); i++ ){
        parity_dis[i] = parity_dis[i - 1] + diff_temp;
        parity_b[i] += parity_b[i - 1] + parity_dis[i];
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


// double SaRaid::event_arrive( const TraceRecord& op ){
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
//}


struct Stripe_oc{
    uint id;
    ulong val;
    Stripe_oc():id(0),val(0){}
    Stripe_oc( uint id_, ulong val_ ):id(id_), val(val_){} 
};

bool operator < (Stripe_oc const & a, Stripe_oc const & b)
{
    return a.val < b.val;
}

struct Read_pair{
    uint id;
    uint ssd1,ssd2;
    ulong val;
    Read_pair():val(0){}
    Read_pair( uint id_, uint ssd1_, uint ssd2_, ulong val_ ):id(id_),ssd1(ssd1_), ssd2(ssd2_),val(val_){} 
};

bool operator < (Read_pair const & a, Read_pair const & b)
{
    return a.val < b.val;
}

double sigmod( double x ){
    return x/(1 + std::abs(x));
}

uint SaRaid::get_migrate_blocks_for_write( double var, double max_mig_period ){
    double x = (var/var_thre);
    double percent = (sigmod(9*x - 9) + sigmod(9))/2.0;
    return (uint) (max_mig * percent);
    //return (uint) (max_mig_period * percent);
}

inline void print_rebalance_workload( uint ssd1, uint ssd2, uint stripe_id, double time, double size, FILE* stream ){
    fprintf( stream,"rebalance,%lf,=,%d,%d,%d,%lf\n", time,ssd1,ssd2,stripe_id,size);
}

void SaRaid::check_reblance(const TraceRecord& op){

    if( need_reblance(op) ){
        //record algorithm time
        auto start = system_clock::now();
        //写均衡
        double aim[ssd_count];
        double need[ssd_count];
        double rebalanced[ssd_count];
        double mean = 0;
        double total_need = 0;
        double erasure_real[ssd_count];
        for( int i = 0; i < ssd_count; i ++ ){
            erasure_real[i] = erasure_left[i] - ssd_erasures * ssd_dead[i];
            mean += erasure_real[i];
        }
        mean = mean / (double)ssd_count;

        double var = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            aim[i] = mean + diff_erasures[i];
            need[i] = aim[i] - erasure_real[i];
            total_need += (need[i] > 0)?need[i]:0;
            var += ((erasure_real[i] - aim[i])/ssd_erasures) * ((erasure_real[i] - aim[i])/ssd_erasures);
        }
    
        var = var / ssd_count;
        double max_mig_period = 0;
        for( int i = 0; i < ssd_count; i ++ ){
            max_mig_period += ssd_writes[i];
        }
        
        uint block_to_mig = get_migrate_blocks_for_write(var, max_mig_period);

        fprintf(stdout, "max_mig_period,%lf,%lf,%d,=,%lf,%lf,%lf,%lf,%lf\n", op.arrive_time,max_mig_period,block_to_mig,ssd_writes[0],ssd_writes[1],ssd_writes[2],ssd_writes[3],ssd_writes[4]);
        
        //这里只移动parity,不过parity是被加权过的，排序，统计总权值，分给每个ssd
        std::vector<Stripe_oc> stripe_rank;
        stripe_rank.clear();
        std::map<uint, std::vector<double>>::iterator it;
        for(it=num_writes.begin();it!=num_writes.end();++it){
            Stripe_oc temp(it->first, it->second[smap[it->first][ssd_count - 1]]);
            stripe_rank.push_back(temp);
        }

        std::sort( stripe_rank.begin(), stripe_rank.end() );

        std::vector<Stripe_oc> stripe_selected;
        stripe_selected.clear();
        bool mark = false;
        double total_weight = 0;
        for( int i = stripe_rank.size()-1; i >= 0; i-- ){
            //todo: ensure that parity_count > 1 work
            if(need[smap[stripe_rank[i].id][ssd_count-1]] > 0){
                stripe_selected.push_back(stripe_rank[i]);
                total_weight +=stripe_rank[i].val;

                if( stripe_selected.size() >= block_to_mig ){
                    break;
                }
            }
        }

        for( int i = 0; i < ssd_count; i ++ ){
            rebalanced[i] = (need[i]/total_need) * total_weight;
        }

        int a = 0;
        uint temp_id = 0;
        while( rebalanced[a] >= 0 && a < (ssd_count - 1) ){
            a ++;
        }

        assert( a < ssd_count );

        //record alogrithm time
        auto end   = system_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        double total_mig = 0;
        for( int i = 0; i < stripe_selected.size(); i++ ){
            if( rebalanced[a] >= 0 ){
                break;
            }
            //todo: output the balance workload
            total_mig ++;
            //print_rebalance_workload( smap[stripe_selected[i].id][ssd_count-1], a, stripe_selected[i].id, op.arrive_time,1, stdout ); 
            
            //todo: debug3
            // for( int j = 0; j < pages_per_sblock; j ++)
            // {
            //     raid_ssd.Ssds[smap[stripe_selected[i].id][ssd_count-1]].event_arrive(READ, stripe_selected[i].id*pages_per_sblock + j, 1, op.arrive_time);
            //     raid_ssd.Ssds[a].event_arrive(READ, stripe_selected[i].id*pages_per_sblock + j, 1, op.arrive_time);
            //     raid_ssd.Ssds[smap[stripe_selected[i].id][ssd_count-1]].event_arrive(WRITE, stripe_selected[i].id*pages_per_sblock + j, 1, op.arrive_time);
            //     raid_ssd.Ssds[a].event_arrive(WRITE, stripe_selected[i].id*pages_per_sblock + j, 1, op.arrive_time);
            // }
            
            
            for(int k = 0; k < ssd_count; k ++){
                if( smap[stripe_selected[i].id][k] == a ){
                    temp_id = k;
                }
            }

            assert( smap[stripe_selected[i].id][temp_id] == a );

            smap[stripe_selected[i].id][temp_id] = smap[stripe_selected[i].id][ssd_count-1];
            smap[stripe_selected[i].id][ssd_count-1] = a;
            
            rebalanced[a] += stripe_selected[i].val;
            if( rebalanced[a] >= 0 ){
                while( rebalanced[a] >= 0 && a < (ssd_count - 1) ){
                    a ++;
                }
                assert( a < ssd_count );
            }
        }
        fprintf(stdout, "rebalance_write_cost,%lf,%lf,=,%lf\n", op.arrive_time,total_mig,double(duration.count()) * microseconds::period::num / microseconds::period::den);
        //读均衡
        if(read_opt){
            //record algorithm time
            auto start = system_clock::now();
        
            std::vector<Read_pair> read_rank;
            read_rank.clear();
            std::map<uint, std::vector<double>>::iterator it;
            uint max_id, min_id;
            ulong max = 0, min = 0;
            for(it=num_reads.begin();it!=num_reads.end();++it){
                max = it->second[0];
                max_id = 0;
                min_id = smap[it->first][ssd_count - 1] == 0? 1:0;
                min = smap[it->first][ssd_count - 1] == 0?it->second[1] : it->second[0];
                
                for( int i = 0; i < ssd_count; i ++ ){
                    if( it->second[i] > max ){
                        max_id = i;
                        max = it->second[i];
                    } else if( it->second[i] < min && i != smap[it->first][ssd_count - 1] ){
                        min_id = i;
                        min = it->second[i];
                    }
                }
                if( max - min > 10)//todo: give a num
                {
                    Read_pair temp(it->first, max_id, min_id, max - min);
                    read_rank.push_back(temp);
                }
            }
        

            std::sort( read_rank.begin(),read_rank.end() );

            int miged = 0;

            double read_mean = 0;
            //double read_need[ssd_count];
            for( int i = 0; i < ssd_count; i++ ){
                read_mean += ssd_reads[i];
            }
            read_mean = read_mean/ssd_count;

            for( int i = 0; i < ssd_count; i++ ){
                need[i] = ssd_reads[i] - read_mean;
            }

            //record alogrithm time
            auto end   = system_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            double total_miged_read = 0;
            uint block_to_mig = get_migrate_blocks_for_write(var, max_mig); 
            for( int i = read_rank.size()-1; i>=0; i -- ){
                if(miged >= block_to_mig){
                    break;
                }

                if( need[read_rank[i].ssd1] > 0 && need[read_rank[i].ssd2] < 0 ){
                    total_miged_read ++;
                    for( int k = 0; k < ssd_count; k ++ ){
                        if( smap[read_rank[i].id][k] == read_rank[i].ssd1 ){
                            smap[read_rank[i].id][k] = read_rank[i].ssd2;
                            continue;
                        } else if( smap[read_rank[i].id][k] == read_rank[i].ssd2 ){
                            smap[read_rank[i].id][k] = read_rank[i].ssd1;
                            continue;
                        }
                    }
                    //print_rebalance_workload( read_rank[i].ssd1, read_rank[i].ssd2, read_rank[i].id, op.arrive_time,1, stdout ); 
            
                    //todo: debug3
                    // for( int j = 0; j < pages_per_sblock; j ++)
                    // {
                    //     raid_ssd.Ssds[read_rank[i].ssd1].event_arrive(READ, read_rank[i].id*pages_per_sblock + j, 1, op.arrive_time);
                    //     raid_ssd.Ssds[read_rank[i].ssd2].event_arrive(WRITE, read_rank[i].id*pages_per_sblock + j, 1, op.arrive_time);
                    //     raid_ssd.Ssds[read_rank[i].ssd2].event_arrive(READ, read_rank[i].id*pages_per_sblock + j, 1, op.arrive_time);
                    //     raid_ssd.Ssds[read_rank[i].ssd1].event_arrive(WRITE, read_rank[i].id*pages_per_sblock + j, 1, op.arrive_time);
                    // }
                }
            }
            fprintf(stdout, "rebalance_read_cost,%lf,%lf,=,%lf\n", op.arrive_time,total_miged_read, double(duration.count()) * microseconds::period::num / microseconds::period::den);
        }
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



bool SaRaid::need_reblance(const TraceRecord& op){
    
    // if( op.arrive_time - last_rtime > time_thre ){
    //     last_rtime = op.arrive_time;
    //     return true;
    // }
    if((total_writes >= REBALANCE_THRE)){
        print_workload( stdout, op.arrive_time );
        return true;
    }
    return false;
}

// void SaRaid::print_ssd_erasures( FILE* stream, double time ){
//     double mean = 0;
//     fprintf(stream, "erasures_data,%lf,=,",time);
//     double erasure_real[ssd_count];
// 	for( int i = 0; i < ssd_count; i ++ ){
//         erasure_real[i] = (erasure_left[i] - ssd_erasures * ssd_dead[i]);
//         fprintf(stream, "%lf,", erasure_real[i]);
// 		mean += erasure_real[i];
// 	}

//     mean /= (double) ssd_count;
    

//     double var = 0;
//     double aim[ssd_count];
//     for( int i = 0; i < ssd_count; i ++ ) { 
//         aim[i] = mean + diff_erasures[i];
//         var += ((erasure_real[i] - aim[i])/ssd_erasures) * ((erasure_real[i] - aim[i])/ssd_erasures);
//     }

//     var = var / ssd_count;
//     fprintf(stream, "=,%lf,%lf\n", mean, var);

//     fprintf(stream, "erasures_aim,%lf,=,",time);
//     for( int i = 0; i < ssd_count; i++ ){
//         fprintf(stream, "%lf,", aim[i]);
//     }
//     fprintf(stream, "=,%lf,%lf\n", mean, var);
    
//     fprintf(stream, "erasures_need,%lf,=,",time);
//     for( int i = 0; i < ssd_count; i++ ){
//         fprintf(stream, "%lf,", aim[i] - erasure_real[i]);
//     }
//     fprintf(stream, "=,%lf,%lf\n", mean, var);
// }

void SaRaid::print_ssd_erasures( FILE* stream, double time ){
    double mean = 0;
    fprintf(stream, "erasures_data,%lf,=,",time);
    double erasure_real[ssd_count];
	for( int i = 0; i < ssd_count; i ++ ){
        erasure_real[i] = (erasure_left[i] - ssd_erasures * ssd_dead[i]);
        fprintf(stream, "%lf,", erasure_real[i]);
		mean += erasure_real[i];
	}

    mean /= (double) ssd_count;
    

    double var = 0;
    double aim[ssd_count];
    for( int i = 0; i < ssd_count; i ++ ) { 
        aim[i] = mean + diff_erasures[i];
        var += ((erasure_real[i] - aim[i])/ssd_erasures) * ((erasure_real[i] - aim[i])/ssd_erasures);
    }

    var = var / ssd_count;
    fprintf(stream, "=,%lf,%lf\n", mean, var);

    fprintf(stream, "erasures_aim,%lf,=,",time);
    for( int i = 0; i < ssd_count; i++ ){
        fprintf(stream, "%lf,", aim[i]);
    }
    fprintf(stream, "=,%lf,%lf\n", mean, var);
    
    fprintf(stream, "erasures_need,%lf,=,",time);
    for( int i = 0; i < ssd_count; i++ ){
        fprintf(stream, "%lf,", aim[i] - erasure_real[i]);
    }
    fprintf(stream, "=,%lf,%lf\n", mean, var);
}

// void SaRaid::check_and_print_stat( const TraceRecord& op,FILE* stream ){
// 	if( need_print( op ) ){
//         print_ssd_erasures( stream, op.arrive_time );
// 		raid_ssd.write_statistics(stream, op.arrive_time);
//         raid_ssd.reset_statistics();
// 	}
// }
