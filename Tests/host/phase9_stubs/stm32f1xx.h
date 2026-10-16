#ifndef P9_STUB_STM32
#define P9_STUB_STM32
#include <stdint.h>
typedef struct {uint32_t KR;} MockIwdg;
extern MockIwdg mock_iwdg;
#define IWDG (&mock_iwdg)
uint32_t __get_IPSR(void);
uint32_t __get_PRIMASK(void);
uint32_t __get_BASEPRI(void);
void NVIC_SystemReset(void);
#endif
