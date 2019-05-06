#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"

using namespace ssd;

RaidParent::RaidParent( uint ssd_count_, uint pages_per_ssd_, uint parity_count_, double ssd_erasures_, uint pages_per_sblock_ )\
:ssd_count(ssd_count_),pages_per_ssd(pages_per_ssd_),pages_per_sblock(pages_per_sblock_),\
stripe_count(pages_per_ssd_/pages_per_sblock_), parity_count(parity_count_),total_writes(0),total_reads(0),\
ssd_erasures(ssd_erasures_),ssd_reads(ssd_count_,0),ssd_writes(ssd_count_,0),last_rtimep(0),ssd_dead( ssd_count_,0 ),\
smap(pages_per_ssd_/pages_per_sblock_,std::vector<uint>(ssd_count_,0)),erasure_left(ssd_count_,0),last_print_time(0){
	for( int i = 0; i < ssd_count; i ++ ){
		erasure_left[i] = ssd_erasures;
	}
	raid_ssd.write_header(stdout);
}

void RaidParent::init(){
	this->init_map();
}

void RaidParent::init_map(){
	fprintf(stderr,"error: virtual method RaidParent::init_map in parent class should not be called");
}


double RaidParent::event_arrive( const TraceRecord& op){
	check_and_print_stat( op, stdout );
	check_reblance( op );
	//fprintf(stderr,"error: virtual method RaidParent::event_arrive in parent class should not be called");
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

    int opSize = op.size/4096 + (op.size%4096 != 0); //write or read the whole block
    
    //record data todo:figure out count as pages or bits
    if( op.op == 'w' ){
        check_erasure_and_swap_ssd( opSize, ssd_ids, 1 + parity_count, op.arrive_time );
		
		erasure_left[ssd_ids[0]] -= opSize;
        ssd_writes[ssd_ids[0]] += (double)opSize;
		total_writes += (double)opSize;
        
		if(num_writes.find(stripe_id) != num_writes.end()){
            num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
        } else{
            num_writes[stripe_id] = std::vector<double>(ssd_count, 0);
            num_writes[stripe_id][ssd_ids[0]] += (double)opSize;
        }
        //record the parity right
        for( int i = 0; i < parity_count; i ++ ){
                ssd_writes[ssd_ids[i + 1]] += (double)opSize;
				total_writes += (double)opSize;

                num_writes[stripe_id][ssd_ids[i + 1]] += opSize;
				erasure_left[ssd_ids[i + 1]] -= opSize;
        }
        
    } else if( op.op == 'r' ){
        ssd_reads[ssd_ids[0]] += (double)opSize;
		total_reads += (double)opSize;
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

void RaidParent::check_erasure_and_swap_ssd( int opSize, uint* ssd_ids, int num, double time ){
	for( int i = 0; i < num; i++ ){
		if( erasure_left[ssd_ids[i]] - opSize <= 0 ){
			printf(">>>>>>>SSD %d need to be swapped\n", ssd_ids[i]);
			swap_ssd( ssd_ids[i], time );
		}
	}	
}

void RaidParent::swap_ssd( uint ssd_id, double time ) {
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

	//todo: debug1
	//erasure_left[ssd_id] -= temp_m.size;

	print_migrate_data(migrations.size() - 1, migrations.size() - 1, stdout);
}

void RaidParent::print_ssd_erasures( FILE* stream, double time ){
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
    for( int i = 0; i < ssd_count; i ++ ) {
        var += ((erasure_real[i] - mean)/ssd_erasures) * ((erasure_real[i] - mean)/ssd_erasures);
    }

    var = var / ssd_count;
    fprintf(stream, "=,%lf,%lf\n", mean, var);
}

void RaidParent::check_and_print_stat( const TraceRecord& op,FILE* stream ){
	if( need_print( op ) ){
		print_ssd_erasures( stream,op.arrive_time );
		raid_ssd.write_statistics(stream, op.arrive_time);
		raid_ssd.reset_statistics();
	}
}

bool RaidParent::need_print( const TraceRecord& op ){
	if( op.arrive_time - last_print_time > PRINT_INV ){
		last_print_time = op.arrive_time;
		return true;
	}

	//todo: debug-1
	// if( op.arrive_time >= 28.324659 ){
	// 	return true;
	// }
	return false;
	//return false;
}

// bool need_print( const TraceRecord& op ){
// 	op_count ++;
// 	if( op_count > 10000 ){
// 		op_count = 0;
// 		return true;
// 	}

// 	return false;
// }


void RaidParent::print_workload( FILE* stream, double time ){
	fprintf(stream, "workload-write,%lf,%lf,=",time,total_writes);
	for( int i = 0; i < ssd_count; i ++ ) { 
        fprintf(stream, ",%lf",ssd_writes[i]);
    }

	fprintf(stream, "\nworkload-read,%lf,%lf,=",time,total_reads);
	for( int i = 0; i < ssd_count; i ++ ) { 
        fprintf(stream, ",%lf",ssd_reads[i]);
    }
	fprintf(stream, "\n");
}

void RaidParent::check_reblance(const TraceRecord& op){
	if( need_reblance( op ) ){
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
	return;
}

bool RaidParent::need_reblance(const TraceRecord& op){
    
    if((total_writes >= REBALANCE_THRE)){
        print_workload( stdout, op.arrive_time );
        return true;
    }
    return false;;
}

void RaidParent::print_migrate_data( uint start, uint end, FILE* stream ){
	assert( start < migrations.size() );
	assert( end < migrations.size() );
	
	for( int i = start; i <= end; i ++ ){
		fprintf( stream, "migration,%s,%lf,=,%d,%lf\n", (migrations[i].wtype == 0?"recovery":"redistribution"),migrations[i].time, migrations[i].ssd_id,migrations[i].size  );
	}
}