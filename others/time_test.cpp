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

void gen_data(std::map<ulong, std::vector<double>>& nw, ulong counts){
    for( ulong i = 0; i < counts; i ++ ){
         nw[i] = vector<double>(5, 0);
        for( int j = 0; j < 5; j ++ ){
            nw[i][j] = u(e);
        }
    }
}

int main(){

        std::map<ulong, std::vector<double>> num_writes;
        num_writes.clear();


        for( double p = 10 ; p < 10e4; p += 100 ){
            num_writes.clear();      
            gen_data(num_writes, (ulong)(p));

            auto start = system_clock::now();
            //这里只移动parity,不过parity是被加权过的，排序，统计总权值，分给每个ssd
            std::vector<Stripe_oc> stripe_rank;
            stripe_rank.clear();
            std::map<ulong, std::vector<double>>::iterator it;
            for(it=num_writes.begin();it!=num_writes.end();++it){
                Stripe_oc temp(it->first, it->second[5 - 1]);
                stripe_rank.push_back(temp);
            }

            std::sort( stripe_rank.begin(), stripe_rank.end() );

            std::vector<Stripe_oc> stripe_selected;
            stripe_selected.clear();
            bool mark = false;
            double total_weight = 0;
            for( int i = stripe_rank.size()-1; i >= 0; i-- ){
                if(true){
                    stripe_selected.push_back(stripe_rank[i]);
                    total_weight +=stripe_rank[i].val;

                    if( stripe_selected.size() >= p ){
                        break;
                    }
                }
            }

            //record alogrithm time
            auto end   = system_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            printf("%ld,\t%lf\n", (ulong)(p), double(duration.count()) * microseconds::period::num / microseconds::period::den );
        }
        return 0;
}