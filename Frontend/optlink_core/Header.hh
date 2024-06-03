// -*- C++ -*-

// Author: Shuhei Hayakawa

#ifndef VME_HEADER_HH
#define VME_HEADER_HH

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <byteswap.h>

#ifdef fOpticalLink

 typedef uint32_t dtype32;
 typedef uint64_t dtype64;

#else

 #include <gef/gefcmn_vme.h>
 typedef GEF_UINT32 dtype32;
 typedef GEF_UINT64 dtype64;

#endif


namespace vme
{

//____________________________________________________________________________
/* VME */
struct MasterHeader
{
  dtype32  m_magic;
  dtype32  m_data_size;
  dtype32  m_nblock;
};
const dtype32     MasterMagic      = 0x00564d45U;
const std::size_t MasterHeaderSize = sizeof(MasterHeader)/sizeof(dtype32);

//____________________________________________________________________________
/* VMEBOARD */
struct ModuleHeader
{
  dtype64 m_magic;
  dtype64 m_vme_address;
  dtype64 m_data_size;
  dtype64 m_n_times_read_device;
  dtype64 m_module_type[2];
  dtype64 m_tv_sec;
  dtype64 m_tv_nsec;
};
const dtype64     ModuleMagic      = 0x766d65626f617264ULL;
const std::size_t ModuleHeaderSize = sizeof(ModuleHeader)/sizeof(dtype32);

//____________________________________________________________________________
void
SetMasterHeader( dtype32  data_size,
		 dtype32  nblock,
		 dtype32* position );
void
SetModuleHeader( dtype64  vme_address,
		 dtype64  data_size,
		 dtype32* position );

}

#endif
