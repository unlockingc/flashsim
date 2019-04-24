#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ssd.h"
#include <string.h>

using namespace ssd;

int main(int argc, char **argv){
    if( argc != 3 ){
        fprintf(stderr , "Please type the Raid and tracefile:\nexample: raidnew raid5 trace1.txt\nraid you can choose:\n\traid5\n\traid6\n\tsaRaid\n\twlRaid\n\tdiffRaid \n\n");
    }
    
    load_config();
    //load_test_config();
    printf("\n");

    RaidParent* raid;
    string raid_type = argv[1];
    string trace_file = argv[2];
    ulong device_size =(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE* RAID_NUMBER_OF_PHYSICAL_SSDS)/2;
    ulong pages_per_ssd = SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE;
    TraceReader trace_reader( trace_file, device_size );
    
    if( raid_type == "raid5" ){
        raid = new Raid5(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1 );
    } else if( raid_type == "raid6" ){
        raid = new Raid6(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 2 );
    } else if( raid_type == "saRaid" ){
        raid = new SaRaid(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1);
    } else if( raid_type == "wlRaid" ){
        raid = new WlRaid(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1 );
    } else if( raid_type == "diffRaid" ){
        raid = new DiffRaid(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1 );
    } else {
        fprintf(stderr, "error raid type %s\nsupport types are:\n\traid5\n\traid6\n\tsaRaid\n\twlRaid\n\tdiffRaid \n\n", raid_type.c_str());
        exit(1);
    }
    
    TraceRecord op;
    while( trace_reader.read_next(op) ){
        raid->event_arrive(op);
    }

    

    //printf("Num reads : %lu\n", num_reads);
    //printf("Num writes: %lu\n", num_writes);

    // printf("Total Write time :  %5.20lf\n", write_total);
    // printf("Avg read time : %5.20lf\n", read_total / num_reads);
    // printf("Avg write time: %5.20lf\n", write_total / num_writes);

    raid->raid_ssd->print_statistics();
    ssd->raid_ssd_ftl_statistics();


    delete raid;
    return 0;
}
