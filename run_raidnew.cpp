#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ssd.h"
#include <string.h>

using namespace ssd;
using namespace std;

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
    
    ulong pages_per_ssd = SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE / 2;

    double ssd_erasures = RAID_SSD_ERASURS;/*todo: debug3*/

    if( raid_type == "raid0" ){
        raid = new Raid0(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 0, ssd_erasures );
    } else if( raid_type == "raid5" ){
        raid = new Raid5(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1, ssd_erasures );
    } else if( raid_type == "raid6" ){
        raid = new Raid6(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 2, ssd_erasures );
    } else if( raid_type == "saRaid" ){
        raid = new SaRaid(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1, ssd_erasures );
    } else if( raid_type == "wlRaid" ){
        raid = new WlRaid(RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1, ssd_erasures );
    } else if( raid_type == "diffRaid" ){
        vector<uint> parity_dis(RAID_NUMBER_OF_PHYSICAL_SSDS,0);
        for( int i = 0; i < RAID_NUMBER_OF_PHYSICAL_SSDS; i++ ){
            parity_dis[i] = 15;
        }
        parity_dis[0] = 40;
        raid = new DiffRaid(parity_dis,RAID_NUMBER_OF_PHYSICAL_SSDS, pages_per_ssd, 1, ssd_erasures );
    } else {
        fprintf(stderr, "error raid type %s\nsupport types are:\n\traid5\n\traid6\n\tsaRaid\n\twlRaid\n\tdiffRaid \n\n", raid_type.c_str());
        exit(1);
    }

    ulong device_size =(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE* (RAID_NUMBER_OF_PHYSICAL_SSDS - raid->parity_count))/2;
    TraceReader trace_reader( trace_file, device_size );
    
    double read_total = 0, write_total = 0;
    double num_reads = 0, num_writes = 0;


    printf("test start************************************************************\n");
    TraceRecord op;
    while( trace_reader.read_next(op) && (op.op == 'r' || op.op == 'w' )){
        (op.op == 'r'? read_total: write_total) +=raid->event_arrive(op);
        (op.op == 'r'? num_reads: num_writes) += ( op.size/4096 + (op.size%4096!=0) );
        if( trace_reader.line_now % 200 == 0 ){
            printf("---lines: %d, total_writes: %lf, total_reads: %lf\n", trace_reader.line_now, write_total/4096, read_total/4096);
        }
    }

    
    printf("test end**************************************************************\n");
    printf("Num reads : %lf\n", num_reads);
    printf("Num writes: %lf\n", num_writes);

    printf("Total Write time :  %5.20lf\n", write_total);
    printf("Avg read time : %5.20lf\n", read_total / num_reads);
    printf("Avg write time: %5.20lf\n", write_total / num_writes);

    //raid->raid_ssd.print_statistics();
    // raid->raid_ssd.print_ftl_statistics();

    delete raid;
    return 0;
}
