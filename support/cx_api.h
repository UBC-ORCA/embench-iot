// GENERAL CXU CONTROL
#define CSR_MCX_SELECTOR 0xBC0
#define CSR_CX_STATUS    0x801
#define MCX_SHAMT_CXU_ID    0
#define MCX_SHAMT_STATE_ID  16
#define MCX_SHAMT_CXE       28
#define MCX_SHAMT_VERSION   29

#define MCX_ENABLE \
    do { \
        asm volatile ("csrw %[csr], %[rs];" :: [rs]  "r" (1 << MCX_SHAMT_VERSION), [csr] "i" (CSR_MCX_SELECTOR));\
    } while (0)

#define MCX_SELECT(cxu_id, state_id) \
    asm volatile ("csrw %[csr], %[rs];" :: \
    [rs]  "r" ((1 << MCX_SHAMT_VERSION) |  /* enable muxing */ \
              (0  << MCX_SHAMT_CXE) | /* disable exceptions */ \
              (state_id << MCX_SHAMT_STATE_ID) | \
              (cxu_id   << MCX_SHAMT_CXU_ID)), \
    [csr] "i" (CSR_MCX_SELECTOR));

// CX FENCE
#define CX_FENCE_SCALAR_READ(base_address, end_address)                                               \
  do {                                                                                                \
    asm volatile ("cx_reg 1008,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)

#define CX_FENCE_SCALAR_WRITE(base_address, end_address)                                              \
  do {                                                                                                \
    asm volatile ("cx_reg 1009,x31,%[ba],%[ea]" :: [ba] "r" (base_address), [ea] "r" (end_address));  \
  } while(0)

// CX PERF
#define CX_PERF(id) ({                                                                                \
  int retval;                                                                                         \
  asm volatile ("cx_reg " #id ",%[cnt],x0,x0" : [cnt] "=r" (retval)                                  \
                                                 : );                                                 \
  retval;                                                                                             \
  })

