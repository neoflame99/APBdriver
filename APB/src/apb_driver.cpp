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
        wait();
    }
}
void APBdriver::apbctrl(){
    req_ready = false;
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
        }while( !req_avail);
        req_ready = false;
        PSEL   = true;
        PADDR  = preq.addr; 
        PWDATA = preq.wdat;
        PWRITE = preq.wr_n;
        wait();
        PENABLE= true;
        do{
            wait();
        }while(!PREADY);
        if(!PWRITE){
            rdat = PRDATA.read().to_uint();
            rdat_vaild = true;
        }
        if(PSLVERR){
            string msg = string("Addr: ")+to_string(preq.addr);
            if(PWRITE){
                msg += string(", Wdata: ")+to_string(preq.wdat)+": Writing can't be done!";
            }else{
                msg += string(", Rdata: ")+to_string(PRDATA.read().to_uint())+": Reading can't be done!";
            }
            SC_REPORT_INFO("PSLVERR", msg.c_str());
        }
        req_ready = true;
    }
}
