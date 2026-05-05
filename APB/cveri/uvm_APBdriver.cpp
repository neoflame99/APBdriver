#include "apb_driver.h"

#include <cstdlib>
#include <deque>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <systemc.h>
#include <tlm.h>
#include <uvm>

using namespace std;

class apb_run_status {
public:
    uint32_t expected_items;
    uint32_t sent_items;
    uint32_t completed_items;
    sc_core::sc_event all_items_sent;
    sc_core::sc_event all_items_completed;

    apb_run_status()
        : expected_items(0), sent_items(0), completed_items(0) {}

    void reset(uint32_t expected) {
        expected_items = expected;
        sent_items = 0;
        completed_items = 0;
    }

    void mark_sent() {
        ++sent_items;
        if (expected_items != 0 && sent_items >= expected_items) {
            all_items_sent.notify(sc_core::SC_ZERO_TIME);
        }
    }

    void mark_completed() {
        ++completed_items;
        if (expected_items != 0 && completed_items >= expected_items) {
            all_items_completed.notify(sc_core::SC_ZERO_TIME);
        }
    }

    void wait_all_completed() {
        while (completed_items < expected_items) {
            sc_core::wait(all_items_completed);
        }
    }
};

class apb_req : public uvm::uvm_sequence_item {
public:
    uint32_t addr;
    uint32_t wdat;
    bool wr_n;

    UVM_OBJECT_UTILS(apb_req)

    apb_req(const string& name = "apb_req")
        : uvm::uvm_sequence_item(name), addr(0), wdat(0), wr_n(true) {}

    string convert2string() const override {
        stringstream ss;
        ss << "apb_req addr = 0x" << hex << addr
           << ", wdat = 0x" << wdat
           << ", op = " << (wr_n ? "WRITE" : "READ");
        return ss.str();
    }

    void do_copy(const uvm::uvm_object& rhs) override {
        const apb_req* rhs_ = dynamic_cast<const apb_req*>(&rhs);
        if (rhs_ == nullptr) {
            UVM_FATAL("apb_req", "do_copy cast failed");
        }
        uvm::uvm_sequence_item::do_copy(rhs);
        addr = rhs_->addr;
        wdat = rhs_->wdat;
        wr_n = rhs_->wr_n;
    }

    bool do_compare(const uvm::uvm_object& rhs,
                    const uvm::uvm_comparer* comparer = nullptr) const override {
        (void)comparer;
        const apb_req* rhs_ = dynamic_cast<const apb_req*>(&rhs);
        if (rhs_ == nullptr) {
            UVM_FATAL("apb_req", "do_compare cast failed");
        }
        return addr == rhs_->addr && wdat == rhs_->wdat && wr_n == rhs_->wr_n;
    }
};

class apb_base_sequence : public uvm::uvm_sequence<apb_req> {
public:
    UVM_OBJECT_UTILS(apb_base_sequence)

    apb_base_sequence(const string& name = "apb_base_sequence")
        : uvm::uvm_sequence<apb_req>(name) {}

    static uint32_t expected_item_count() {
        return (ADR_DEP * 4) + 2;
    }

    void body() override {
        vector<uint32_t> rand_addr;

        UVM_INFO(get_name(), "Phase 0: sequential write", uvm::UVM_MEDIUM);
        for (uint32_t i = 0; i < ADR_DEP; ++i) {
            send_item(ADR_MIN + i, static_cast<uint32_t>(rand()), true);
        }

        UVM_INFO(get_name(), "Phase 1: sequential read", uvm::UVM_MEDIUM);
        for (uint32_t i = 0; i < ADR_DEP; ++i) {
            send_item(ADR_MIN + i, 0, false);
        }

        UVM_INFO(get_name(), "Phase 2: random write", uvm::UVM_MEDIUM);
        for (uint32_t i = 0; i < ADR_DEP; ++i) {
            const uint32_t addr = (rand() % ADR_DEP) + ADR_MIN;
            rand_addr.push_back(addr);
            send_item(addr, static_cast<uint32_t>(rand()), true);
        }

        UVM_INFO(get_name(), "Phase 3: random read", uvm::UVM_MEDIUM);
        for (uint32_t addr : rand_addr) {
            send_item(addr, 0, false);
        }

        UVM_INFO(get_name(), "Phase 4: out-of-range write/read error path", uvm::UVM_MEDIUM);
        send_item(ADR_MAX + 2, 0xDEADBEEF, true);
        send_item(ADR_MAX + 2, 0, false);

        UVM_INFO(get_name(), "APB sequence completed", uvm::UVM_LOW);
    }

private:
    void send_item(uint32_t addr, uint32_t wdat, bool wr_n) {
        apb_req* req = apb_req::type_id::create("req");
        start_item(req);
        req->addr = addr;
        req->wdat = wdat;
        req->wr_n = wr_n;
        finish_item(req);
    }
};

template <class REQ = apb_req>
class apb_sequencer : public uvm::uvm_sequencer<REQ> {
public:
    UVM_COMPONENT_PARAM_UTILS(apb_sequencer<REQ>)

    apb_sequencer(uvm::uvm_component_name name)
        : uvm::uvm_sequencer<REQ>(name) {}
};

template <class REQ = apb_req>
class apb_uvm_driver : public uvm::uvm_driver<REQ> {
public:
    tlm::tlm_fifo<REQ>* req_fifo;
    tlm::tlm_fifo<REQ>* observed_req_fifo;

    UVM_COMPONENT_PARAM_UTILS(apb_uvm_driver<REQ>)

    apb_uvm_driver(uvm::uvm_component_name name)
        : uvm::uvm_driver<REQ>(name), req_fifo(nullptr), observed_req_fifo(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_driver<REQ>::build_phase(phase);
        if (!uvm::uvm_config_db<tlm::tlm_fifo<REQ>*>::get(this, "", "req_fifo", req_fifo)) {
            UVM_FATAL(this->get_name(), "req_fifo is not configured");
        }
        if (!uvm::uvm_config_db<tlm::tlm_fifo<REQ>*>::get(this, "", "observed_req_fifo", observed_req_fifo)) {
            UVM_FATAL(this->get_name(), "observed_req_fifo is not configured");
        }
    }

    void run_phase(uvm::uvm_phase& phase) override {
        (void)phase;
        REQ req;

        while(true) {
            this->seq_item_port->get_next_item(req);
            UVM_INFO(this->get_name(), req.convert2string(), uvm::UVM_MEDIUM);
            req_fifo->put(req);
            observed_req_fifo->put(req);
            this->seq_item_port->item_done();
        }
    }
};

class apb_req_monitor : public uvm::uvm_monitor {
public:
    uvm::uvm_analysis_port<apb_req>  a_req_port;
    tlm::tlm_fifo<apb_req>* observed_req_fifo;

    UVM_COMPONENT_UTILS(apb_req_monitor)

    apb_req_monitor(uvm::uvm_component_name name)
        : uvm::uvm_monitor(name), a_req_port("a_req_port"), observed_req_fifo(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_monitor::build_phase(phase);
        if (!uvm::uvm_config_db<tlm::tlm_fifo<apb_req>*>::get(
                this, "", "observed_req_fifo", observed_req_fifo)) {
            UVM_FATAL(get_name(),
                     "observed_req_fifo is not configured");
        }
    }

    void run_phase(uvm::uvm_phase& phase) override {
        (void)phase;

        if (observed_req_fifo == nullptr ) {
            UVM_FATAL(get_name(), "observed_req_fifo is not configured");
        }

        while (true) {
            apb_req req = observed_req_fifo->get();
            UVM_INFO(get_name(), "\nObserved " + req.convert2string(), uvm::UVM_MEDIUM);
            a_req_port.write(req);
        }
    }
};
class apb_rdat_monitor : public uvm::uvm_monitor {
public:
    uvm::uvm_analysis_port<uint32_t> a_rdat_port;
    tlm::tlm_fifo<uint32_t>* observed_rdat_fifo;

    UVM_COMPONENT_UTILS(apb_rdat_monitor)

    apb_rdat_monitor(uvm::uvm_component_name name)
        : uvm::uvm_monitor(name), a_rdat_port("a_rdat_port"), observed_rdat_fifo(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_monitor::build_phase(phase);
        if (!uvm::uvm_config_db<tlm::tlm_fifo<uint32_t>*>::get(
                this, "", "observed_rdat_fifo", observed_rdat_fifo)) {
            UVM_FATAL(get_name(),
                     "observed_rdat_fifo is not configured");
        }
    }

    void run_phase(uvm::uvm_phase& phase) override {
        (void)phase;

        if (observed_rdat_fifo == nullptr) {
            UVM_FATAL(get_name(), "observed_rdat_fifo is not configured");
        }

        while (true) {
            uint32_t rdat = observed_rdat_fifo->get();
            stringstream ss;
            ss << "\nObserved RDAT: " << std::hex << rdat;
            UVM_INFO(get_name(), ss.str(), uvm::UVM_MEDIUM);
            a_rdat_port.write(rdat);
           
        }
    }
};

class apb_agent : public uvm::uvm_agent {
public:
    apb_sequencer<apb_req>* sequencer;
    apb_uvm_driver<apb_req>* driver;
    apb_req_monitor* req_monitor;
    apb_rdat_monitor* rdat_monitor;

    UVM_COMPONENT_UTILS(apb_agent)

    apb_agent(uvm::uvm_component_name name)
        : uvm::uvm_agent(name), sequencer(nullptr), driver(nullptr), req_monitor(nullptr), rdat_monitor(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_agent::build_phase(phase);
        req_monitor = apb_req_monitor::type_id::create("req_monitor", this);
        rdat_monitor = apb_rdat_monitor::type_id::create("rdat_monitor", this);
        assert(req_monitor != nullptr);
        assert(rdat_monitor != nullptr);

        if (get_is_active() == uvm::UVM_ACTIVE) {
            sequencer = apb_sequencer<apb_req>::type_id::create("sequencer", this);
            driver = apb_uvm_driver<apb_req>::type_id::create("driver", this);
            assert(sequencer != nullptr);
            assert(driver != nullptr);
        }
    }

    void connect_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_agent::connect_phase(phase);
        if (get_is_active() == uvm::UVM_ACTIVE) {
            driver->seq_item_port(sequencer->seq_item_export);
        }
    }
};

class apb_scoreboard : public uvm::uvm_scoreboard {
public:
    uvm::uvm_analysis_imp<apb_req, apb_scoreboard> req_export;
    uvm::uvm_analysis_imp<uint32_t, apb_scoreboard> rdat_export;

    UVM_COMPONENT_UTILS(apb_scoreboard)

    apb_scoreboard(uvm::uvm_component_name name)
        : uvm::uvm_scoreboard(name),
          req_export("req_export", this),
          rdat_export("rdat_export", this),
          write_count(0),
          read_count(0),
          compare_count(0),
          error_count(0) {}

    void write(const apb_req& req) {
        if (req.wr_n) {
            if (is_valid_addr(req.addr)) {
                ref_mem[req.addr] = req.wdat;
                ++write_count;
            }
            return;
        }

        if (is_valid_addr(req.addr)) {
            expected_rdat.push_back(ref_mem[req.addr]);
            ++read_count;
        } else {
            UVM_INFO(get_name(), "Ignoring out-of-range read request in scoreboard", uvm::UVM_MEDIUM);
        }
    }

    void write(const uint32_t& rdat) {
        if (expected_rdat.empty()) {
            UVM_INFO(get_name(), "Ignoring read data with no pending in-range read", uvm::UVM_MEDIUM);
            return;
        }

        const uint32_t exp = expected_rdat.front();
        expected_rdat.pop_front();
        ++compare_count;

        if (rdat != exp) {
            stringstream ss;
            ss << "Read data mismatch: expected=0x" << hex << exp
               << " actual=0x" << rdat;
            ++error_count;
            UVM_ERROR(get_name(), ss.str());
        }
    }

    void report_phase(uvm::uvm_phase& phase) override {
        (void)phase;
        stringstream ss;
        ss << "\n <== APB scoreboard summary ==>\n writes = " << dec << write_count
           << "\n reads = " << read_count
           << "\n compares = " << compare_count
           << "\n pending_expected = " << expected_rdat.size()
           << "\n errors = " << error_count
           << "\n <============================>";

        if (error_count == 0 && expected_rdat.empty()) {
            UVM_INFO(get_name(), ss.str(), uvm::UVM_LOW);
        } else {
            UVM_ERROR(get_name(), ss.str());
        }
    }

private:
    map<uint32_t, uint32_t> ref_mem;
    deque<uint32_t> expected_rdat;
    uint32_t write_count;
    uint32_t read_count;
    uint32_t compare_count;
    uint32_t error_count;

    bool is_valid_addr(uint32_t addr) const {
        return addr >= ADR_MIN && addr <= ADR_MAX;
    }
};

class apb_env : public uvm::uvm_env {
public:
    apb_agent* agent;
    apb_scoreboard* scoreboard;

    UVM_COMPONENT_UTILS(apb_env)

    apb_env(uvm::uvm_component_name name)
        : uvm::uvm_env(name), agent(nullptr), scoreboard(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_env::build_phase(phase);
        uvm::uvm_config_db<int>::set(this, "agent", "is_active", uvm::UVM_ACTIVE);
        agent = apb_agent::type_id::create("agent", this);
        scoreboard = apb_scoreboard::type_id::create("scoreboard", this);
        assert(agent != nullptr);
        assert(scoreboard != nullptr);
    }

    void connect_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_env::connect_phase(phase);
        agent->req_monitor->a_req_port.connect(scoreboard->req_export);
        agent->rdat_monitor->a_rdat_port.connect(scoreboard->rdat_export);
    }
};

class apb_test : public uvm::uvm_test {
public:
    apb_env* env;
    apb_run_status* run_status;

    UVM_COMPONENT_UTILS(apb_test)

    apb_test(uvm::uvm_component_name name)
        : uvm::uvm_test(name), env(nullptr), run_status(nullptr) {}

    void build_phase(uvm::uvm_phase& phase) override {
        uvm::uvm_test::build_phase(phase);
        env = apb_env::type_id::create("env", this);
        assert(env != nullptr);
        if (!uvm::uvm_config_db<apb_run_status*>::get(this, "", "run_status", run_status)) {
            UVM_FATAL(get_name(), "run_status is not configured");
        }
    }

    void run_phase(uvm::uvm_phase& phase) override {
        apb_base_sequence seq("seq");

        UVM_INFO(get_name(), "*** APB UVM-SystemC test started ***", uvm::UVM_LOW);
        phase.raise_objection(this);
        run_status->reset(apb_base_sequence::expected_item_count());
        sc_core::wait(5, sc_core::SC_NS);
        seq.start(env->agent->sequencer, nullptr);
        run_status->wait_all_completed();
        phase.drop_objection(this);
        UVM_INFO(get_name(), "*** APB UVM-SystemC test completed ***", uvm::UVM_LOW);
    }
};

SC_MODULE(DriverBridge) {
    sc_in<bool> clk;
    Connections::Out<PReq> PReqIf;
    tlm::tlm_fifo<apb_req> req_fifo;
    apb_run_status run_status;

    SC_HAS_PROCESS(DriverBridge);

    DriverBridge(sc_module_name nm)
        : sc_module(nm), clk("clk"), PReqIf("PReqIf"), req_fifo("req_fifo", 4) {
        SC_THREAD(send);
        sensitive << clk.pos();
    }

    void send() {
        PReqIf.Reset();
        wait();

        while (true) {
            const apb_req req = req_fifo.get();
            PReq preq;
            preq.addr = req.addr;
            preq.wdat = req.wdat;
            preq.wr_n = req.wr_n;
            PReqIf.Push(preq);
            run_status.mark_sent();
            wait();
        }
    }
};

SC_MODULE(MonitorBridge) {
    sc_in<bool> clk;
    Connections::In<uint32_t> PRdat;
    tlm::tlm_fifo<uint32_t> observed_rdat_fifo;

    SC_HAS_PROCESS(MonitorBridge);

    MonitorBridge(sc_module_name nm)
        : sc_module(nm), clk("clk"), PRdat("PRdat")
        , observed_rdat_fifo("observed_rdat_fifo", 16) {
        SC_THREAD(rcv);
        sensitive << clk.pos();
    }

    void rcv() {
        PRdat.Reset();
        wait();

        while (true) {
            uint32_t rdat = 0;
            if (PRdat.PopNB(rdat)) {
                stringstream ss;
                ss << "Read data observed: 0x" << hex << rdat;
                UVM_INFO("MonitorBridge", ss.str(), uvm::UVM_MEDIUM);
                observed_rdat_fifo.put(rdat);
            }
            wait();
        }
    }
};

SC_MODULE(TB_APBDRIVER) {
    sc_in<bool> clk;

    sc_signal<bool> rstn;
    sc_signal<bool> PSEL;
    sc_signal<bool> PENABLE;
    sc_signal<bool> PWRITE;
    sc_signal<sc_biguint<32>> PADDR;
    sc_signal<sc_biguint<32>> PWDATA;
    sc_signal<sc_biguint<32>> PRDATA;
    sc_signal<bool> PREADY;
    sc_signal<bool> PSLVERR;

    Connections::Combinational<PReq> PReqIf;
    Connections::Combinational<uint32_t> PRdat;

    APBdriver apb_drv;
    APBslave<256> apb_slv;
    DriverBridge drv_bridge;
    MonitorBridge mon_bridge;

    SC_HAS_PROCESS(TB_APBDRIVER);

    TB_APBDRIVER(sc_module_name nm)
        : sc_module(nm),
          clk("clk"),
          PReqIf("PReqIf"),
          PRdat("PRdat"),
          apb_drv("APBDRV"),
          apb_slv("APBSLV", true),
          drv_bridge("DRV_BRIDGE"),
          mon_bridge("MON_BRIDGE") {
        drv_bridge.clk(clk);
        drv_bridge.PReqIf(PReqIf);

        mon_bridge.clk(clk);
        mon_bridge.PRdat(PRdat);

        apb_drv.clk(clk);
        apb_drv.rstn(rstn);
        apb_drv.PSEL(PSEL);
        apb_drv.PENABLE(PENABLE);
        apb_drv.PWRITE(PWRITE);
        apb_drv.PADDR(PADDR);
        apb_drv.PWDATA(PWDATA);
        apb_drv.PRDATA(PRDATA);
        apb_drv.PREADY(PREADY);
        apb_drv.PSLVERR(PSLVERR);
        apb_drv.PReqIf(PReqIf);
        apb_drv.PRdat(PRdat);

        apb_slv.clk(clk);
        apb_slv.rstn(rstn);
        apb_slv.PSEL(PSEL);
        apb_slv.PENABLE(PENABLE);
        apb_slv.PWRITE(PWRITE);
        apb_slv.PADDR(PADDR);
        apb_slv.PWDATA(PWDATA);
        apb_slv.PRDATA(PRDATA);
        apb_slv.PREADY(PREADY);
        apb_slv.PSLVERR(PSLVERR);

        SC_THREAD(rstn_gen);
        sensitive << clk.pos();

        SC_THREAD(track_apb_completions);
        sensitive << clk.pos();
        dont_initialize();
    }

    void rstn_gen() {
        rstn = false;
        wait(2);
        rstn = true;

        while (true) {
            wait();
        }
    }

    void track_apb_completions() {
        wait();
        while (true) {
            if (PSEL.read() && PENABLE.read() && PREADY.read()) {
                drv_bridge.run_status.mark_completed();
            }
            wait();
        }
    }
};

int sc_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    sc_clock clk("clk", 1, SC_NS);
    TB_APBDRIVER tb("tb");
    tb.clk(clk);
    tlm::tlm_fifo<apb_req> observed_req_fifo("observed_req_fifo", 16);

    uvm::uvm_config_db<tlm::tlm_fifo<apb_req>*>::set(
        uvm::uvm_root::get(), "*env.agent.driver", "req_fifo", &tb.drv_bridge.req_fifo);
    uvm::uvm_config_db<tlm::tlm_fifo<apb_req>*>::set(
        uvm::uvm_root::get(), "*env.agent.driver", "observed_req_fifo", &observed_req_fifo);
    uvm::uvm_config_db<tlm::tlm_fifo<apb_req>*>::set(
        uvm::uvm_root::get(), "*env.agent.req_monitor", "observed_req_fifo", &observed_req_fifo);
    uvm::uvm_config_db<tlm::tlm_fifo<uint32_t>*>::set(
        uvm::uvm_root::get(), "*env.agent.rdat_monitor", "observed_rdat_fifo", &tb.mon_bridge.observed_rdat_fifo);
    uvm::uvm_config_db<apb_run_status*>::set(
        uvm::uvm_root::get(), "apb_test", "run_status", &tb.drv_bridge.run_status);

    sc_trace_file* tf = sc_create_vcd_trace_file("tb_wave");
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
    sc_trace(tf, tb.apb_drv.PReqIf.vld, "apb_drv_PReqIf_vld");
    sc_trace(tf, tb.apb_drv.PReqIf.rdy, "apb_drv_PReqIf_rdy");
    sc_trace(tf, tb.apb_drv.PReqIf.dat, "apb_drv_PReqIf_dat");
    sc_trace(tf, tb.apb_drv.PRdat.vld, "apb_drv_PRdat_vld");
    sc_trace(tf, tb.apb_drv.PRdat.rdy, "apb_drv_PRdat_rdy");
    sc_trace(tf, tb.apb_drv.PRdat.dat, "apb_drv_PRdat_dat");

    cout << "\n========================================\n";
    cout << "  APB Driver UVM-SystemC Testbench\n";
    cout << "========================================\n\n";

    uvm::run_test("apb_test");

    sc_close_vcd_trace_file(tf);
    cout << "\nWaveform saved to: tb_wave.vcd\n";

    return 0;
}
