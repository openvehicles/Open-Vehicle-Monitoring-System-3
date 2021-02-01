#! /bin/bash

# Still unclear:
# 01 ? Initialisation sends 010319020C000000 and gets 0601 F1 03 5902FF
# 06 ? Initialisation sends 060319020C000000 and gets 0606 F1 03 5902FF -> same
# 2C ? Same init thing as above
# 2D ? Ditto
# 37 ? Same init thing
# 43 ? And again
# 44 ? Again
# 61 ? And again

# Identified:
# 07 SME    Battery management electronics
# 10 ZGW    
# 12 EDME   Electrical digital motor electronics (low voltage ECU)
# 14 LIM    Charging interface module (this is the actual connector at the back of the car)
# 15 UCX2   Convenience charging electronics (the ac charger for the main battery)
# 1A EME    Electrical machine electronics (motor controller etc)
# 22 SAS    Optional equipment system (TPMS)
# 29 DSC    Dynamic stability control
# 30 EPS    Electromechanical power steering
# 35 TBX5
# 40 BDC    The body domain controller
# 56 FZD    Roof function center (seems to be the alarm mostly)
# 5D KAFAS  Camera based driver assist system
# 5E GWS    Gear selector switch
# 60 KOMBI  Instrument panel
# 63 NBTEVO Headunit high (is this the mid-mounted display)
# 67 ZBE6   iDrive controller
# 78 IHX    Integrated automatic heating / aircon - though its mainly the buttons I think)


perl ./generate_ecu_code.pl ../dev/sme_i1.json >../ecu_definitions/ecu_sme_defines.h  3>../ecu_definitions/ecu_sme_code.cpp 4>../ecu_definitions/ecu_sme_polls.cpp
perl ./generate_ecu_code.pl ../dev/edmei1.json >../ecu_definitions/ecu_edm_defines.h  3>../ecu_definitions/ecu_edm_code.cpp 4>../ecu_definitions/ecu_edm_polls.cpp
perl ./generate_ecu_code.pl ../dev/lim_i1.json >../ecu_definitions/ecu_lim_defines.h  3>../ecu_definitions/ecu_lim_code.cpp 4>../ecu_definitions/ecu_lim_polls.cpp
perl ./generate_ecu_code.pl ../dev/ucx2_i01.json >../ecu_definitions/ecu_kle_defines.h  3>../ecu_definitions/ecu_kle_code.cpp 4>../ecu_definitions/ecu_kle_polls.cpp
perl ./generate_ecu_code.pl ../dev/eme_i01.json >../ecu_definitions/ecu_eme_defines.h  3>../ecu_definitions/ecu_eme_code.cpp 4>../ecu_definitions/ecu_eme_polls.cpp
perl ./generate_ecu_code.pl ../dev/sas_i1.json >../ecu_definitions/ecu_sas_defines.h  3>../ecu_definitions/ecu_sas_code.cpp 4>../ecu_definitions/ecu_sas_polls.cpp
perl ./generate_ecu_code.pl ../dev/dsc_i1.json >../ecu_definitions/ecu_dsc_defines.h  3>../ecu_definitions/ecu_dsc_code.cpp 4>../ecu_definitions/ecu_dsc_polls.cpp
perl ./generate_ecu_code.pl ../dev/bdc.json >../ecu_definitions/ecu_bdc_defines.h  3>../ecu_definitions/ecu_bdc_code.cpp 4>../ecu_definitions/ecu_bdc_polls.cpp
perl ./generate_ecu_code.pl ../dev/fzd_f15.json >../ecu_definitions/ecu_fzd_defines.h  3>../ecu_definitions/ecu_fzd_code.cpp 4>../ecu_definitions/ecu_fzd_polls.cpp
perl ./generate_ecu_code.pl ../dev/kafas20.json >../ecu_definitions/ecu_kaf_defines.h  3>../ecu_definitions/ecu_kaf_code.cpp 4>../ecu_definitions/ecu_kaf_polls.cpp
perl ./generate_ecu_code.pl ../dev/komb25.json >../ecu_definitions/ecu_kom_defines.h  3>../ecu_definitions/ecu_kom_code.cpp 4>../ecu_definitions/ecu_kom_polls.cpp
perl ./generate_ecu_code.pl ../dev/nbtevo.json >../ecu_definitions/ecu_nbt_defines.h  3>../ecu_definitions/ecu_nbt_code.cpp 4>../ecu_definitions/ecu_nbt_polls.cpp
perl ./generate_ecu_code.pl ../dev/zbe6.json >../ecu_definitions/ecu_zbe_defines.h  3>../ecu_definitions/ecu_zbe_code.cpp 4>../ecu_definitions/ecu_zbe_polls.cpp
perl ./generate_ecu_code.pl ../dev/ihx_i1.json >../ecu_definitions/ecu_ihx_defines.h  3>../ecu_definitions/ecu_ihx_code.cpp 4>../ecu_definitions/ecu_ihx_polls.cpp
perl ./generate_ecu_code.pl ../dev/eps_i1.json >../ecu_definitions/ecu_eps_defines.h  3>../ecu_definitions/ecu_eps_code.cpp 4>../ecu_definitions/ecu_eps_polls.cpp

