
#include "apb_driver.h"
#include <vector>
#include <systemc.h>

template <uint32_t RNG=256>
SC_MODULE(REQGEN){
    sc_in <bool> clk;
    Connections::Out<PReq> PReqIf;
    Connections::In <uint32_t> PRdat; 

    SC_HAS_PROCESS(REQGEN);
    REQGEN(sc_module_name _nm): sc_module(_nm)
    {
        SC_THREAD(gen);
        sensitive << clk.pos();

        SC_THREAD(rcv);
        sensitive << clk.pos();
    }
    sc_signal<uint32_t> wrphase;
    vector<uint32_t> v_wd, v_rd;
    PReq req;
    void gen(){
        PReqIf.Reset();
        wrphase = 0;
        uint32_t addr = 0;
        wait();
        while(1){
            req.addr = addr;
            if(wrphase == 0){
                req.wdat = rand();   
                req.wr_n = true;
                v_wd.push_back(req.wdat);
            }else if(wrphase == 1){
                req.wr_n = false;
            }
            PReqIf.Push(req);
            addr++;
            if(addr >= RNG){
                addr = 0;
                if(wrphase==1){ chk(); }
                wrphase++;
            }
            wait();
        }
    }
    void rcv(){
        PRdat.Reset();
        uint32_t rdat;
        wait();
        while(1){
            rdat = PRdat.Pop();
            if(wrphase == 1){
                v_rd.push_back(rdat);
            }
        }
    }
    void chk(){
        if(v_wd.size() != v_rd.size()){
            string msg = string("v_wd's size("+to_string(v_wd.size()))+string( ") is not the same with v_rd's size(") +to_string(v_rd.size())+string(")");
            SC_REPORT_ERROR("FAIL", msg.c_str());
        }
        uint32_t sz = v_wd.size();
        for(uint32_t k=0; k < sz; k++){
            if(v_wd[k] != v_rd[k]){
                string msg = string("v_wd[")+to_string(k)+string("]=")+to_string(v_wd[k])+
                string(" != v_rd[")+to_string(k)+string("]=")+to_string(v_rd[k]);
                SC_REPORT_ERROR("FAIL", msg.c_str());
            }
        }
        SC_REPORT_INFO("INFO","SUCCESS");
        sc_stop();
    }
};
SC_MODULE(TB_APBDRIVER){

    sc_in <bool>    clk;

    sc_signal<bool> rstn; 
    sc_signal<bool> PSEL;
    sc_signal<bool> PENABLE;
    sc_signal<bool> PWRITE;
    sc_signal<sc_biguint<32>> PADDR ;
    sc_signal<sc_biguint<32>> PWDATA;
    sc_signal<sc_biguint<32>> PRDATA;
    sc_signal<bool> PREADY;
    sc_signal<bool> PSLVERR;
    Connections::Combinational<PReq> PReqIf;
    Connections::Combinational<uint32_t> PRdat; 

    APBdriver     apb_drv;
    APBslave<256> apb_slv;
    REQGEN<256>   req_gen; 

    SC_HAS_PROCESS(TB_APBDRIVER);
    TB_APBDRIVER(sc_module_name _nm): sc_module(_nm),
    apb_drv("APBDRV"), apb_slv("APBSLV"), req_gen("REQGEN")
    {
        req_gen.clk    (clk    );
        req_gen.PReqIf (PReqIf );
        req_gen.PRdat  (PRdat  ); 

        apb_drv.clk    (clk    );
        apb_drv.rstn   (rstn   );
        apb_drv.PSEL   (PSEL   );
        apb_drv.PENABLE(PENABLE);
        apb_drv.PWRITE (PWRITE );
        apb_drv.PADDR  (PADDR  );
        apb_drv.PWDATA (PWDATA );
        apb_drv.PRDATA (PRDATA );
        apb_drv.PREADY (PREADY );
        apb_drv.PSLVERR(PSLVERR);
        apb_drv.PReqIf (PReqIf );
        apb_drv.PRdat  (PRdat  ); 

        apb_slv.clk    (clk    );
        apb_slv.rstn   (rstn   );
        apb_slv.PSEL   (PSEL   );
        apb_slv.PENABLE(PENABLE);
        apb_slv.PWRITE (PWRITE );
        apb_slv.PADDR  (PADDR  );
        apb_slv.PWDATA (PWDATA );
        apb_slv.PRDATA (PRDATA );
        apb_slv.PREADY (PREADY );
        apb_slv.PSLVERR(PSLVERR);

        SC_THREAD(rstn_gen);
        sensitive << clk.pos();
    }
    void rstn_gen(){
        rstn = 0;
        wait();
        rstn = 1;
    }
};
int sc_main(int argc, char *argv[]){
    sc_clock clk("clk", 1, SC_NS);

    TB_APBDRIVER tb("tb");
    tb.clk(clk);

    sc_trace_file *tf = sc_create_vcd_trace_file("tb_wave");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, tb.rstn, "rstn");
    sc_trace(tf, tb.PSEL, "PSEL");
    sc_trace(tf, tb.PENABLE, "PENABLE");
    sc_trace(tf, tb.PWRITE, "PWRITE");
    sc_trace(tf, tb.PADDR, "PADDR");
    sc_trace(tf, tb.PWDATA, "PWDATA");
    sc_trace(tf, tb.PRDATA, "PRDATA");
    sc_trace(tf, tb.PREADY, "PREADY");
    sc_trace(tf, tb.PSLVERR, "PSLVERR");
    sc_trace(tf, tb.req_gen.wrphase, "wrphase");
    sc_trace(tf, tb.req_gen.req.addr, "req_addr");
    sc_trace(tf, tb.req_gen.req.wdat, "req_wdat");
    sc_trace(tf, tb.req_gen.req.wr_n, "req_wr_n");
    sc_trace(tf, tb.req_gen.PReqIf.vld, "PReqIf_vld");
    sc_trace(tf, tb.req_gen.PReqIf.rdy, "PReqIf_rdy");
    sc_trace(tf, tb.req_gen.PReqIf.dat, "req_gen_PReqIf_dat");
    sc_trace(tf, tb.req_gen.PRdat.vld, "PRdat_vld");
    sc_trace(tf, tb.req_gen.PRdat.rdy, "PRdat_rdy");
    sc_trace(tf, tb.req_gen.PRdat.dat, "PRdat_dat");

    sc_trace(tf, tb.apb_drv.PSEL, "apb_drv_PSEL");
    sc_trace(tf, tb.apb_drv.PENABLE, "apb_drv_PENABLE");
    sc_trace(tf, tb.apb_drv.PWRITE, "apb_drv_PWRITE");
    sc_trace(tf, tb.apb_drv.PADDR, "apb_drv_PADDR");
    sc_trace(tf, tb.apb_drv.PWDATA, "apb_drv_PWDATA");
    sc_trace(tf, tb.apb_drv.PRDATA, "apb_drv_PRDATA");
    sc_trace(tf, tb.apb_drv.PREADY, "apb_drv_PREADY");
    sc_trace(tf, tb.apb_drv.PSLVERR, "apb_drv_PSLVERR");
    sc_trace(tf, tb.apb_drv.PReqIf.vld, "apb_drv_PReqIf_vld");
    sc_trace(tf, tb.apb_drv.PReqIf.rdy, "apb_drv_PReqIf_rdy");
    sc_trace(tf, tb.apb_drv.PReqIf.dat, "apb_drv_PReqIf_dat");
    sc_trace(tf, tb.apb_drv.PRdat.vld, "apb_drv_PRdat_vld");
    sc_trace(tf, tb.apb_drv.PRdat.rdy, "apb_drv_PRdat_rdy");
    sc_trace(tf, tb.apb_drv.PRdat.dat, "apb_drv_PRdat_dat");

    sc_trace(tf, tb.apb_slv.PSEL, "apb_slv_PSEL");
    sc_trace(tf, tb.apb_slv.PENABLE, "apb_slv_PENABLE");
    sc_trace(tf, tb.apb_slv.PWRITE, "apb_slv_PWRITE");
    sc_trace(tf, tb.apb_slv.PADDR, "apb_slv_PADDR");
    sc_trace(tf, tb.apb_slv.PWDATA, "apb_slv_PWDATA");
    sc_trace(tf, tb.apb_slv.PRDATA, "apb_slv_PRDATA");
    sc_trace(tf, tb.apb_slv.PREADY, "apb_slv_PREADY");
    sc_trace(tf, tb.apb_slv.PSLVERR, "apb_slv_PSLVERR");


    sc_start();

    sc_close_vcd_trace_file(tf);
    return 0;
}
