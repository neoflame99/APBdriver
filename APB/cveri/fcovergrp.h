
#ifndef __FCOVERGRP_H__
#define __FCOVERGRP_H__
#include <iostream>
#include <systemc.h>
#include <fc4sc.hpp>
#include "apb_driver.h"

using namespace std;

class fcovergrp : public fc4sc::covergroup
{
public:
  uint32_t PADDR;
  uint32_t PWDATA;
  uint32_t PRDATA;
  bool PWRITE;

  CG_CONS(fcovergrp) 
  // using convergroup::sample();
  // => fcovergrp(string inst_name) : fc4sc::covergroup(inst_name)
  { }
  //COVERPOINT(uint32_t,  R_PADDR_CVP, PADDR, !PWRITE ) {
  COVERPOINT(uint32_t,  R_PADDR_CVP, PADDR ) {
    bin<uint32_t>("read_bins", interval(ADR_MIN, ADR_MAX)),
    ignore_bin<uint32_t>("ignore_bins",interval(ADR_MAX+1, PSELRNG))
  };

  COVERPOINT(uint32_t,  W_PADDR_CVP, PADDR, PWRITE ) {
    bin<uint32_t>("write_bins", interval(ADR_MIN, ADR_MAX)),
    ignore_bin<uint32_t>("ignore_bins",interval(ADR_MAX+1, PSELRNG))
  };

};
#endif
