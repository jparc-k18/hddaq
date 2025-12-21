#ifndef ENDIAN_H_INCLUDED_
#define ENDIAN_H_INCLUDED_

/* Big endian means BIG NATIVE AMERICAN.
 * Little endian meand little native american.
 */

#include <cstdint>

namespace Endian
{
  inline uint16_t getBigEndian16(const void* buf)
  {
    return ((uint16_t)((uint8_t*)buf)[0] << 8) |
      ((uint16_t)((uint8_t*)buf)[1] << 0);
  }

  inline uint32_t getBigEndian32(const void* buf)
  {
    return ((uint32_t)((uint8_t*)buf)[0] << 24) |
      ((uint32_t)((uint8_t*)buf)[1] << 16) |
      ((uint32_t)((uint8_t*)buf)[2] <<  8) |
      ((uint32_t)((uint8_t*)buf)[3] <<  0);
  }

  inline uint16_t getLittleEndian16(const void* buf)
  {
    return ((uint16_t)((uint8_t*)buf)[0] << 0) |
      ((uint16_t)((uint8_t*)buf)[1] << 8);
  }

  inline uint32_t getLittleEndian32(const void* buf)
  {
    return ((uint32_t)((uint8_t*)buf)[0] <<  0) |
      ((uint32_t)((uint8_t*)buf)[1] <<  8) |
      ((uint32_t)((uint8_t*)buf)[2] << 16) |
      ((uint32_t)((uint8_t*)buf)[3] << 24);
  }

  inline void setBigEndian16(void* buf, uint16_t val)
  {
    ((uint8_t*)buf)[0] = (val >> 8) & 0xff;
    ((uint8_t*)buf)[1] = (val >> 0) & 0xff;
  }

  inline void setBigEndian32(void* buf, uint32_t val)
  {
    ((uint8_t*)buf)[0] = (val >> 24) & 0xff;
    ((uint8_t*)buf)[1] = (val >> 16) & 0xff;
    ((uint8_t*)buf)[2] = (val >>  8) & 0xff;
    ((uint8_t*)buf)[3] = (val >>  0) & 0xff;
  }

  inline void setLittleEndian16(void* buf, uint16_t val)
  {
    ((uint8_t*)buf)[0] = (val >> 0) & 0xff;
    ((uint8_t*)buf)[1] = (val >> 8) & 0xff;
  }

  inline void setLittleEndian32(void* buf, uint32_t val)
  {
    ((uint8_t*)buf)[0] = (val >>  0) & 0xff;
    ((uint8_t*)buf)[1] = (val >>  8) & 0xff;
    ((uint8_t*)buf)[2] = (val >> 16) & 0xff;
    ((uint8_t*)buf)[3] = (val >> 24) & 0xff;
  }
};

#endif
