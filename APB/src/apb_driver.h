#include <stdio.h>
#include <iostream>
#include <systemc.h>
#include "connections/connections.h"

#ifndef __APB_IF_H__
#define __APB_IF_H__
using namespace std;
struct PReq{
    uint32_t addr;
    uint32_t wdat;
    bool     wr_n; // true: write, false: read
    PReq():addr(0), wdat(0), wr_n(0){}
    static const uint32_t width = sizeof(uint32_t) + sizeof(uint32_t)+ sizeof(bool);
    template<uint32_t Size> void Marshall(Marshaller<Size>& m){
        m & addr;
        m & wdat;
        m & wr_n;
    }
    inline friend ostream& operator<<(ostream& os, const PReq& ot){
        os << "addr: " << ot.addr <<", wdat: " << ot.wdat <<", wr_n: " << ot.wr_n << endl;
        return os;
    } 
    inline friend void sc_trace(sc_trace_file *ptf, const PReq& v, const string& Name){
        sc_trace(ptf, v.addr, Name+".addr");
        sc_trace(ptf, v.wdat, Name+".wdat");
        sc_trace(ptf, v.wr_n, Name+".wr_n");
    }
    bool operator==(PReq& ot){
        bool res = *this == ot;
        return res;
    }
};
SC_MODULE(APBdriver){
    sc_in<bool> clk;
    sc_in<bool> rstn;
    //-- APB Interface --//
    sc_out<bool> PSEL;
    sc_out<bool> PENABLE;
    sc_out<bool> PWRITE;
    sc_out<sc_biguint<32>> PADDR ;
    sc_out<sc_biguint<32>> PWDATA;
    sc_in <sc_biguint<32>> PRDATA;
    sc_in <bool> PREADY;
    sc_in <bool> PSLVERR;
    Connections::In <PReq> PReqIf;
    Connections::Out<uint32_t> PRdat; 

    SC_HAS_PROCESS(APBdriver);
    APBdriver(sc_module_name _nm): sc_module(_nm),
    clk("clk"), rstn("rstn"),
    PSEL("PSEL"), PENABLE("PENABLE"), PWRITE("PWRITE"), PADDR("PADDR"),
    PWDATA("PWDATA"), PRDATA("PRDATA"), PREADY("PREADY"), PSLVERR("PSLVERR"),
    PReqIf("PReqIf"), PRdat("PRdat")
    {
        SC_THREAD(apbctrl);
        sensitive << clk.pos();
        async_reset_signal_is(rstn, false);

        SC_THREAD(procreq);
        sensitive << clk.pos();
        async_reset_signal_is(rstn, false);
    }

    sc_signal<PReq> preq;
    sc_signal <bool> req_ready;
    sc_signal <bool> req_avail;
    sc_signal <bool> rdat_valid;
    sc_signal <uint32_t> rdat;

    void procreq();
    void apbctrl();
};

template<uint32_t DEP=256>
SC_MODULE(APBslave){
    sc_in<bool> clk;
    sc_in<bool> rstn;
    //-- APB Interface --//
    sc_in <bool> PSEL;
    sc_in <bool> PENABLE;
    sc_in <bool> PWRITE;
    sc_in <sc_biguint<32>> PADDR ;
    sc_in <sc_biguint<32>> PWDATA;
    sc_in <sc_biguint<32>> PRDATA;
    sc_out<bool> PREADY;
    sc_out<bool> PSLVERR;

    bool     PRDY_DLY; // apply delay on PREADY
    uint32_t regbank[DEP];
    SC_HAS_PROCESS(APBslave);
    APBslave(sc_module_name _nm): sc_module(_nm),
    clk("clk"), rstn("rstn"),
    PSEL("PSEL"), PENABLE("PENABLE"), PWRITE("PWRITE"), PADDR("PADDR"),
    PWDATA("PWDATA"), PRDATA("PRDATA"), PREADY("PREADY"), PSLVERR("PSLVERR")
    {
        SC_THREAD(run);
        sensitive << clk.pos();
        async_reset_signal_is(rstn, false);
    }
    void set_prdydly(bool v){
        PRDY_DLY = v;
    }

    void run(){
        PREADY = false;
        PSLVERR = false;
        int32_t dlycnt;
        wait();
        while(1){
            do{
                wait();
            }while(!PSEL);
            dlycnt = (PRDY_DLY)? rand()&3 : 0 ;
            while(dlycnt > 0){
                wait();
                dlycnt--;
            }
            PREADY = true;
            do{
                wait();
            }while(!PENABLE);
            if(PWRITE){
                if(PADDR.read() < DEP){
                    regbank[PADDR.read()] = PWDATA.read();
                    PSLVERR = false;
                }else{
                    PSLVERR = true;
                }
            }else{
                if(PADDR.read() < DEP){
                    PRDATA = regbank[PADDR.read()];
                    PSLVERR = false;
                }else{
                    PSLVERR = true;
                }
            }
            PREADY = false;
            wait();
        }
    }
};
#endif
