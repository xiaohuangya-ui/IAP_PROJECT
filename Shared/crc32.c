#include "image_contract.h"
/* 来源：G4D_OPT2B3_OPT3_Candidate 04B_TRIAL/User/User_Preflight_CRCSize/
 * crc32_ieee.c；保留16项 nibble 算法，移除 C04/容量探针耦合。 */
static const uint32_t table[16] = {
    0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
    0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
    0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
    0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
};
uint32_t CRC32_Update(uint32_t state, const uint8_t *data, uint32_t length)
{
    uint32_t i;
    for (i = 0U; i < length; ++i) {
        state ^= data[i];
        state = (state >> 4) ^ table[state & 15U];
        state = (state >> 4) ^ table[state & 15U];
    }
    return state;
}
uint32_t CRC32_IEEE(const uint8_t *data, uint32_t length)
{
    return CRC32_Update(0xFFFFFFFFU, data, length) ^ 0xFFFFFFFFU;
}
