#include <new>
#include <assert.h>
#include <stdio.h>
#include "../ssd.h"
#include <iostream>
#include <cstdlib>

using namespace ssd;

DiffRaid::DiffRaid(std::vector<uint> parity_dis_, uint ssd_count_, uint pages_per_ssd_, uint parity_count_, double ssd_erasures_, uint pages_per_sblock_, bool shifting_):\
RaidParent( ssd_count_, pages_per_ssd_, parity_count_, ssd_erasures_, pages_per_sblock_  ),\
parity_dis(parity_dis_),parity_loop(0),shifting(shifting_)
{
    assert(ssd_count == 5);
    assert( parity_dis.size() == ssd_count );
    for( int i = 0; i < parity_dis.size(); i++ ){
        parity_loop += parity_dis[i];
    }
    
    init();
}

void DiffRaid::init_map(){
    std::vector<uint> parity_b(ssd_count, 0);
    parity_b[0] = parity_dis[0];
    for( int i = 1; i < parity_dis.size(); i++ ){
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

// double DiffRaid::event_arrive( const TraceRecord& op ){
    
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
//             num_writes[stripe_id] = std::vector<double>(ssd_count,0);
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

void DiffRaid::swap_ssd( uint ssd_id, double time ) {
	//get used pages
	double size = 0;
	Address temp(0, 0, 0, 0, 0, BLOCK);
	for( int i = 0; i < SSD_SIZE; i ++ ){
		temp.package = i;
		for( int j = 0; j < PACKAGE_SIZE; j ++ )
		{
			temp.die = j;
			for( int p = 0; p < DIE_SIZE; p ++ ){
				temp.plane = p;
				for( int q = 0; q < PLANE_SIZE; q ++ ){
					temp.block = q;
					size += (double)raid_ssd.Ssds[ssd_id].get_num_valid(temp);
				}
			}
		}
	}

	//get new ssd
	raid_ssd.swap_ssd( ssd_id );

	//renew ssd
	erasure_left[ssd_id] = ssd_erasures;
    ssd_dead[ssd_id] ++;

	//record data migrate cost
	MigrationRecord temp_m;
	temp_m.time = time;
	temp_m.size = size;
	temp_m.ssd_id = ssd_id;
	migrations.push_back(temp_m);

    print_migrate_data(migrations.size() - 1, migrations.size() - 1, stdout);
	
    //todo: debug1
    //erasure_left[ssd_id] -= temp_m.size;

    adjust_parity_distribution(ssd_id, time, size);
    
    
}

void DiffRaid::adjust_parity_distribution( uint ssd_id, double time, double size ){
    int min_id = 0;
    int min = parity_dis[0];
    int next_ssd = (ssd_id + 1) % ssd_count;
    for( int i = 0; i < ssd_count; i ++ ){
        if( parity_dis[i] < min ){
            min = parity_dis[i];
            min_id = i;
        }
    }

    //find where to shift        
    if( min == parity_dis[ssd_id] ){
        fprintf(stderr, "worning: ssd just swapped has the min parities(diff raid)\n");
        return;
    } else if( min == parity_dis[next_ssd] ){
        min_id = next_ssd;
    }

    if( shifting ){
        //shift
        int p = 0;
        int moved = 0, temp = 0;
        int need = parity_dis[ssd_id] - parity_dis[next_ssd];
        int need_min = parity_dis[next_ssd] - parity_dis[min_id];
        assert(need > 0);
        assert(need_min >= 0);
        for( int i = 0; i < stripe_count; i ++ ){
            if( smap[i][ssd_count - 1] == ssd_id ){
                if( moved < need ){
                    temp = next_ssd < ssd_id? next_ssd + ssd_count - ssd_id : next_ssd - ssd_id;
                    for( int j = 0; j < ssd_count; j ++ ){
                        smap[i][j] = (smap[i][j] + temp) % ssd_count;     
                    }
                    moved ++;
                } else if( moved < need + need_min ){
                    temp = min_id < ssd_id? min_id + ssd_count - ssd_id : min_id - ssd_id;
                    for( int j = 0; j < ssd_count; j ++ ){
                        smap[i][j] = (smap[i][j] + temp) % ssd_count;     
                    }
                    moved ++;
                }
            }

            p ++;
            if( p == parity_loop ){
                moved = 0;
                p = 0;
            }
        }

        //record work load
        double percent = (double)(  parity_dis[ssd_id] - min  ) / (double) parity_dis[ssd_id];
        
        //record data migrate cost
        MigrationRecord temp_s;
        temp_s.time = time;
        temp_s.size = size * percent;
        temp_s.wtype = 1;

        for( int i = 0; i < ssd_count; i ++ ){
            temp_s.ssd_id = i;
            migrations.push_back(temp_s);
            //todo: debug1
            //erasure_left[i] -= temp_s.size;
            print_migrate_data(migrations.size() - 1, migrations.size() - 1, stdout);
        }

    } else {
        //swap
        int p = 0;
        int moved = 0, temp = 0;
        int need = parity_dis[ssd_id] - parity_dis[next_ssd];
        int need_min = parity_dis[next_ssd] - parity_dis[min_id];
        assert(need > 0);
        assert(need_min >= 0);

        int next_logic_id = 0, min_logic_id = 0;

        for( int i = 0; i < stripe_count; i ++ ){
            if( smap[i][ssd_count - 1] == ssd_id ){
                if( moved < need ){
                    for( int p = 0; p < ssd_count; p ++ ){
                        if(smap[i][p] == next_ssd){
                            next_logic_id = p;
                        }
                    }
                    std::swap( smap[i][ssd_count - 1], smap[i][next_logic_id] );
                    moved ++;
                } else if( moved < need + need_min ){
                    for( int p = 0; p < ssd_count; p ++ ){
                        if(smap[i][p] == next_ssd){
                            min_logic_id = p;
                        }
                    }
                    std::swap( smap[i][ssd_count - 1], smap[i][min_logic_id] );
                    moved ++;
                }
            }

            p ++;
            if( p == parity_loop ){
                moved = 0;
                p = 0;
            }
        }

        double percent1 = (double)(  parity_dis[ssd_id] - parity_dis[next_ssd]  ) / (double) parity_dis[ssd_id];
        double percent2 = (double)(  parity_dis[next_ssd] - min ) / (double) parity_dis[ssd_id];
        
        //record data migrate cost
        MigrationRecord temp_s;
        temp_s.time = time;
        temp_s.wtype = 1;

        temp_s.size = size * (percent1 + percent2);
        temp_s.ssd_id = ssd_id;
        migrations.push_back(temp_s);
        print_migrate_data(migrations.size() - 1, migrations.size() - 1, stdout);

        erasure_left[ssd_id] -= temp_s.size;

        temp_s.size = size * (percent1);
        temp_s.ssd_id = next_ssd;
        migrations.push_back(temp_s);
        erasure_left[next_ssd] -= temp_s.size;


        temp_s.size = size * (percent2);
        temp_s.ssd_id = min_id;
        migrations.push_back(temp_s);
        
        //todo: debug1
        //erasure_left[min_id] -= temp_s.size;
    }

    //update dis
    ulong temp = parity_dis[next_ssd];
    parity_dis[next_ssd] = parity_dis[ssd_id];
    parity_dis[ssd_id] = min;
    if( min_id != next_ssd ){
        parity_dis[min_id] =temp;
    }
}