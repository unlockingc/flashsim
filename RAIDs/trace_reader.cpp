#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"
#include <string.h>
#include <stdlib.h>

using namespace ssd;

TraceReader::TraceReader( std::string filename_, ulong device_size_ ):device_size(device_size_),filename(filename_),line_now(0){
    if((trace_file = fopen( filename.c_str(), "r")) == NULL){
        fprintf(stderr,"Please provide trace file name\n");
        exit(-1);
    }

    fseek(trace_file, 0, SEEK_SET);
    printf("open trace file %s succeed\n", filename.c_str());
}

bool TraceReader::read_next( TraceRecord& op ){
    if(fgets(buffer, 80, trace_file) != NULL){
        sscanf(buffer, "%u,%lu,%u,%c,%lf", &op.diskno, &op.vaddr, &op.size, &op.op, &op.arrive_time);
        op.vaddr = op.vaddr%device_size;
        line_now ++;
    } else{
        op.diskno = 0;
        op.vaddr = 0;
        op.size = 0;
        op.arrive_time = 0;
        op.op = 'r';
        return false;
    }

    return true;
}