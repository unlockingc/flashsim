#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"

using namespace ssd;

RaidParent::RaidParent( uint ssd_count_, uint pages_per_ssd_, uint parity_count_, uint pages_per_sblock_ )\
:ssd_count(ssd_count_),pages_per_ssd(pages_per_ssd_),pages_per_sblock(pages_per_sblock_),\
stripe_count(pages_per_ssd_/pages_per_sblock_), parity_count(parity_count_), ssd_record(ssd_record_),\
ssd_erasures(ssd_erasures_),ssd_reads(ssd_count_,0),ssd_write(ssd_count_,0)\
smap(pages_per_ssd_/pages_per_sblock_,std::vector<uint>(ssd_count_,0))
{
	init_map( init_raid_type );
}

void RaidParent::init(){
	this->init_map();
}

virtual RaidParent::init_map(){
	fprintf(stderr,"error: virtual method RaidParent::init_map in parent class should not be called");
}


virtual double RaidParent::event_arrive( const TraceRecord& op){
	fprintf(stderr,"error: virtual method RaidParent::event_arrive in parent class should not be called");
}

void RaidParent::check_erasure_and_swap_ssd( int opSize, uint* ssd_ids, int num, double time ){
	for( int i = 0; i < num; i++ ){
		if( erasure_left[ssd_ids[i]] - opSize <= 0 ){
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

	//record data migrate cost
	MigrationRecord temp;
	temp.time = time;
	temp.size = size;
	temp.ssd_id = ssd_id;
	migration.push_back(temp);

	erasure_left[ssd_id] -= temp.size;
}