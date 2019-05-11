#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <map>
#include <cstring>
#include <ctime>
#include <chrono>
#include <random>
#include <algorithm>

using namespace std;
using namespace std::chrono;

default_random_engine e;
uniform_real_distribution<double> u(0,10000);





void gen_data(std::map<ulong, std::vector<double>>& nw, ulong counts){
    for( ulong i = 0; i < counts; i ++ ){
         nw[i] = vector<double>(5, 0);
        for( int j = 0; j < 5; j ++ ){
            nw[i][j] = u(e);
        }
    }
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

int main(){

        std::map<ulong, std::vector<double>> num_reads;
        num_reads.clear();


        int ssd_count = 5;
        double ssd_reads[ssd_count];
        double need[ssd_count];
        for( double p = 10 ; p < 10e4; p += 100 ){
            num_reads.clear();      
            gen_data(num_reads, (ulong)(p));

            auto start = system_clock::now();
            //这里只移动parity,不过parity是被加权过的，排序，统计总权值，分给每个ssd
            std::vector<Read_pair> read_rank;
            read_rank.clear();
            std::map<ulong, std::vector<double>>::iterator it;
            uint max_id, min_id;
            ulong max = 0, min = 0;
            for(it=num_reads.begin();it!=num_reads.end();++it){
                min = it->second[0];
                max = it->second[0];
                max_id = 0;
                min_id = 0;
                for( int i = 0; i < ssd_count; i ++ ){
                    if( it->second[i] > max ){
                        max_id = i;
                        max = it->second[i];
                    } else if( it->second[i] < min ){
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
            double read_need[ssd_count];
            for( int i = 0; i < ssd_count; i++ ){
                read_mean += ssd_reads[i];
            }
            read_mean = read_mean/ssd_count;

            for( int i = 0; i < ssd_count; i++ ){
                need[i] = ssd_reads[i] - read_mean;
            }

            //fprintf(stdout, "rebalance_read_cost,%lf,%lf,=,%lf\n", op.arrive_time,total_miged_read, double(duration.count()) * microseconds::period::num / microseconds::period::den);

            //record alogrithm time
            auto end   = system_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            printf("%ld,\t%lf\n", (ulong)(p), double(duration.count()) * microseconds::period::num / microseconds::period::den );
        }
        return 0;
}