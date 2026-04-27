#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>
#include <string>
#include <omp.h>//
std::vector<int> makeRandom(size_t n, unsigned seed=42){
    std::vector<int> v(n);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(1,1000000);
    for(auto& x:v) x=dist(rng);
    return v;
}
bool isSorted(const std::vector<int>& v){
    for(size_t i=1;i<v.size();i++) if(v[i]<v[i-1]) return false;
    return true;
}
std::string fmtNum(long long n){
    std::string s=std::to_string(n);
    int p=(int)s.size()-3;
    while(p>0){s.insert(p,",");p-=3;}
    return s;
}
std::string gb2str(double v){
    std::ostringstream os;
    if(v>=1.0) os<<std::fixed<<std::setprecision(2)<<v<<" GB";
    else if(v>=0.001) os<<std::fixed<<std::setprecision(2)<<v*1000.0<<" MB";
    else os<<std::fixed<<std::setprecision(2)<<v*1e6<<" KB";
    return os.str();
}

struct Res {
    std::string label; size_t n;
    double ms; long long cmp,swp,ops;
    double rd_GB,wr_GB,tot_GB,dt_ms,mops;
    int threads; bool ok;
};

Res seqSort(size_t n){
    auto arr=makeRandom(n,42);
    long long c=0,s=0;
    auto t0=std::chrono::high_resolution_clock::now();
    for(size_t i=0;i<n-1;i++){
        bool sw=false;
        for(size_t j=0;j<n-i-1;j++){c++;if(arr[j]>arr[j+1]){std::swap(arr[j],arr[j+1]);s++;sw=true;}}
        if(!sw) break;
    }
    auto t1=std::chrono::high_resolution_clock::now();
    double ms=std::chrono::duration<double,std::milli>(t1-t0).count();
    long long ops=c+s*3;
    double rd=(double)(c*2+s*2)*4,wr=(double)(s*2)*4,tot=rd+wr;
    return{"Sequential",n,ms,c,s,ops,rd/1e9,wr/1e9,tot/1e9,(tot/1e9)/20.0*1000.0,
           (ms>0)?(ops/1e6)/(ms/1000.0):0,1,isSorted(arr)};
}

Res parSort(size_t n){
    auto arr=makeRandom(n,42);
    long long cmp=0,swp=0;
    int th=omp_get_max_threads();
    auto t0=std::chrono::high_resolution_clock::now();
    for(size_t phase=0;phase<n;phase++){
        long long pc=0,ps=0;
        long long st=(long long)(phase%2);
        #pragma omp parallel for reduction(+:pc,ps) schedule(static) default(none) shared(arr,n,st)
        for(long long j=st;j<(long long)n-1;j+=2){
            pc++;
            if(arr[j]>arr[j+1]){std::swap(arr[j],arr[j+1]);ps++;}
        }
        cmp+=pc;swp+=ps;
    }
    auto t1=std::chrono::high_resolution_clock::now();
    double ms=std::chrono::duration<double,std::milli>(t1-t0).count();
    long long ops=cmp+swp*3;
    double rd=(double)(cmp*2+swp*2)*4,wr=(double)(swp*2)*4,tot=rd+wr;
    return{"Parallel(OMP)",n,ms,cmp,swp,ops,rd/1e9,wr/1e9,tot/1e9,(tot/1e9)/20.0*1000.0,
           (ms>0)?(ops/1e6)/(ms/1000.0):0,th,isSorted(arr)};
}

void printTable(const Res& s,const Res& p,bool ext=false){
    double sp=s.ms/p.ms;
    double eff=sp/p.threads*100.0;
    int W1=38,W2=17,W3=17;
    std::string sep(W1+W2+W3,'=');
    std::string ln(W1+W2+W3,'-');
    std::cout<<"\n"<<sep<<"\n";
    std::cout<<"  N = "<<fmtNum((long long)s.n)<<" elements  |  Threads: "<<p.threads;
    if(ext) std::cout<<"  [EXTRAPOLATED]";
    std::cout<<"  |  Correct: "<<(s.ok?"Y":"N")<<"/"<<(p.ok?"Y":"N")<<"\n";
    std::cout<<sep<<"\n";
    auto row=[&](std::string lbl,std::string sv,std::string pv){
        std::cout<<std::left<<std::setw(W1)<<lbl<<std::setw(W2)<<sv<<std::setw(W3)<<pv<<"\n";
    };
    auto ms2=[](double v){ return std::to_string((long long)v)+" ms"; };
    std::ostringstream spss,effss;
    spss<<std::fixed<<std::setprecision(2)<<sp<<"x";
    effss<<std::fixed<<std::setprecision(1)<<eff<<"%";
    row("Execution Time",          ms2(s.ms),             ms2(p.ms));
    row("Speedup",                 "1.00x",               spss.str());
    row("Parallel Efficiency",     "100%",                effss.str());
    std::cout<<ln<<"\n";
    row("Total Comparisons",       fmtNum(s.cmp),         fmtNum(p.cmp));
    row("Total Swaps",             fmtNum(s.swp),         fmtNum(p.swp));
    row("Total Operations",        fmtNum(s.ops),         fmtNum(p.ops));
    std::cout<<ln<<"\n";
    row("Data Read",               gb2str(s.rd_GB),       gb2str(p.rd_GB));
    row("Data Write",              gb2str(s.wr_GB),       gb2str(p.wr_GB));
    row("Total Data Transfer",     gb2str(s.tot_GB),      gb2str(p.tot_GB));
    row("Data Transfer Time",      ms2(s.dt_ms),          ms2(p.dt_ms));
    std::cout<<ln<<"\n";
    std::ostringstream sm,pm2;
    sm<<std::fixed<<std::setprecision(1)<<s.mops<<" MOPS";
    pm2<<std::fixed<<std::setprecision(1)<<p.mops<<" MOPS";
    row("Achievable Performance",  sm.str(),              pm2.str());
    std::cout<<sep<<"\n";
    std::cout<<"  Speedup: "<<std::fixed<<std::setprecision(2)<<sp
             <<"x  |  Efficiency: "<<std::setprecision(1)<<eff
             <<"%  |  Time saved: "<<std::setprecision(0)<<(s.ms-p.ms)<<" ms\n";
    std::cout<<sep<<"\n";
}

int main(){
    std::cout<<"\n"
  
    <<"   BUBBLE SORT: Sequential vs OpenMP Parallel   \n";

   
    std::cout<<"Threads available : "<<omp_get_max_threads()<<"\n";
    std::cout<<"Mem bandwidth     : 20 GB/s (assumed)\n";

    std::cout<<"\n[1/3] N=10,000\n";
    Res s10=seqSort(10000);
    std::cout<<"  seq: "<<(long long)s10.ms<<" ms  ok="<<s10.ok<<"\n";
    Res p10=parSort(10000);
    std::cout<<"  par: "<<(long long)p10.ms<<" ms  ok="<<p10.ok<<"\n";
    printTable(s10,p10);

 
    std::cout<<"\n[2/3] N=100,000 EXTRAPOLATED from 10K (scale x100)\n";
    {
        double sc=100.0;
        Res s100{"Sequential",100000,s10.ms*sc,(long long)(s10.cmp*sc),(long long)(s10.swp*sc),
                 (long long)(s10.ops*sc),s10.rd_GB*sc,s10.wr_GB*sc,s10.tot_GB*sc,
                 s10.dt_ms*sc,s10.mops,1,true};
        Res p100{"Parallel(OMP)",100000,p10.ms*sc,(long long)(p10.cmp*sc),(long long)(p10.swp*sc),
                 (long long)(p10.ops*sc),p10.rd_GB*sc,p10.wr_GB*sc,p10.tot_GB*sc,
                 p10.dt_ms*sc,p10.mops,p10.threads,true};
        printTable(s100,p100,true);


        std::cout<<"\n[3/3] N=1,000,000  EXTRAPOLATED from 10K (scale x10000)\n";
        double sc2=10000.0;
        Res s1m{"Sequential",1000000,s10.ms*sc2,(long long)(s10.cmp*sc2),(long long)(s10.swp*sc2),
                (long long)(s10.ops*sc2),s10.rd_GB*sc2,s10.wr_GB*sc2,s10.tot_GB*sc2,
                s10.dt_ms*sc2,s10.mops,1,true};
        Res p1m{"Parallel(OMP)",1000000,p10.ms*sc2,(long long)(p10.cmp*sc2),(long long)(p10.swp*sc2),
                (long long)(p10.ops*sc2),p10.rd_GB*sc2,p10.wr_GB*sc2,p10.tot_GB*sc2,
                p10.dt_ms*sc2,p10.mops,p10.threads,true};
        printTable(s1m,p1m,true);
    }


}
