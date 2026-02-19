#include "apb_driver.h"

void APBdriver::procreq(){
    PReqIf.Reset();
    PRdat.Reset();
    req_avail = false;
    wait();
    while(1){
        preq = PReqIf.Pop();
        req_avail = true;
        do{
            wait();
        }while(!req_ready);
        if(!preq.wr_n){
            do{
                wait();
            }while(!rdat_valid);
            PRdat.Push(rdat);
        }
        cout <<"preq.addr: " << preq.addr <<", preq.wr_n: " << preq.wr_n << endl;
        wait();
    }
}
void APBdriver::apbctrl(){
    req_ready = false;
    req_rdy_dly = false;
    rdat_valid = false;
    PSEL    = false;
    PWRITE  = false;
    PENABLE = false;
    PADDR   = 0;
    PWDATA  = 0;
    wait();
    while(1){
        do{
            wait();
            rdat_valid = false;
        }while( !req_avail);
        req_rdy_dly = req_ready;
        if(preq.addr >= ADR_MIN && preq.addr <= PSELRNG){
            PSEL = true;
        }else {
            PSEL = false;
        }
        req_ready = false;
        PSEL   = true;
        PADDR  = preq.addr; 
        PWDATA = preq.wdat;
        PWRITE = preq.wr_n;
        wait();
        if(req_rdy_dly){
            PENABLE= true;
        }
        do{
            wait();
        }while(!PREADY);
        PENABLE = false;
        if(!PWRITE){
            rdat = PRDATA.read().to_uint();
            rdat_vaild = true;
        }
        if(PSLVERR){
            string msg = string("Addr: ")+to_string(PADDR.read().to_uint());
            if(PWRITE){
                msg += string(", Wdata: ")+to_string(PWDATA.read().to_uint())+": Writing can't be done!";
            }else{
                msg += ": Reading can't be done!";
            }
            SC_REPORT_WARNING("PSLVERR", msg.c_str());
        }
        req_ready = true;
    }
}
