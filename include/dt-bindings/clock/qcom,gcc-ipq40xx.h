/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
#ifndef __QCOM_CLK_IPQ40XX_H__
#define __QCOM_CLK_IPQ40XX_H__

#define ACC_CLK_SRC                                     1
#define AUDIO_CLK_SRC                                   2
#define BLSP1_QUP1_I2C_APPS_CLK_SRC                     3
#define BLSP1_QUP1_SPI_APPS_CLK_SRC                     4
#define BLSP1_QUP2_I2C_APPS_CLK_SRC                     5
#define BLSP1_QUP2_SPI_APPS_CLK_SRC                     6
#define BLSP1_UART1_APPS_CLK_SRC                        7
#define BLSP1_UART2_APPS_CLK_SRC                        8
#define GCC_XO_CLK_SRC                                  9
#define PCNOC_BFDCD_CLK_SRC                             10
#define QDSS_AT_CLK_SRC                                 11
#define QDSS_TSCTR_CLK_SRC                              12
#define QDSS_STM_CLK_SRC                                13
#define QDSS_TRACECLKIN_CLK_SRC                         14
#define SYSTEM_NOC_BFDCD_CLK_SRC                        15
#define SYSTEM_NOC_125M_CLK_SRC                         16
#define GP1_CLK_SRC                                     17
#define GP2_CLK_SRC                                     18
#define GP3_CLK_SRC                                     19
#define SDCC1_APPS_CLK_SRC                              20
#define FEPHY_125M_DLY_CLK_SRC                          21
#define WCSS2G_CLK_SRC                                  22
#define WCSS5G_CLK_SRC                                  23
#define APSS_AHB_SRC                                    24
#define APSS_BIMC_SRC                                   25
#define APSS_CMD_SRC                                    26
#define USB_MOCK_UTMI_SRC                               27
#define SLEEP_CLK_SRC                                   28
#define GCC_APSS_AHB_CLK                                29
#define GCC_AUDIO_AHB_CLK                               30
#define GCC_AUDIO_PWM_CLK                               31
#define GCC_BIMC_AHB_CLK                                32
#define GCC_BIMC_SYSNOC_AXI_CLK                         33
#define GCC_BLSP1_AHB_CLK                               34
#define GCC_BLSP1_QUP1_I2C_APPS_CLK                     35
#define GCC_BLSP1_QUP1_SPI_APPS_CLK                     36
#define GCC_BLSP1_QUP2_I2C_APPS_CLK                     37
#define GCC_BLSP1_QUP2_SPI_APPS_CLK                     38
#define GCC_BLSP1_UART1_APPS_CLK                        39
#define GCC_BLSP1_UART2_APPS_CLK                        40
#define GCC_DCD_XO_CLK                                  41
#define GCC_GP1_CLK                                     42
#define GCC_GP2_CLK                                     43
#define GCC_GP3_CLK                                     44
#define GCC_BOOT_ROM_AHB_CLK                            45
#define GCC_CRYPTO_AHB_CLK                              46
#define GCC_ESS_CLK                                     47
#define GCC_IMEM_AXI_CLK                                48
#define GCC_IMEM_CFG_AHB_CLK                            49
#define GCC_MPM_AHB_CLK                                 50
#define GCC_NOC_CONF_XPU_AHB_CLK                        51
#define GCC_PCIE_AHB_CLK                                52
#define GCC_PCIE_AXI_M_CLK                              53
#define GCC_PCIE_AXI_S_CLK                              54
#define GCC_PCNOC_AHB_CLK                               55
#define GCC_PCNOC_AT_CLK                                56
#define GCC_SRIF_AHB_CLK                                57
#define GCC_PCNOC_BUS_TIMEOUT0_AHB_CLK                  58
#define GCC_PCNOC_BUS_TIMEOUT1_AHB_CLK                  59
#define GCC_PCNOC_BUS_TIMEOUT2_AHB_CLK                  60
#define GCC_PCNOC_BUS_TIMEOUT3_AHB_CLK                  61
#define GCC_PCNOC_BUS_TIMEOUT4_AHB_CLK                  62
#define GCC_PCNOC_BUS_TIMEOUT5_AHB_CLK                  63
#define GCC_PCNOC_BUS_TIMEOUT6_AHB_CLK                  64
#define GCC_PCNOC_BUS_TIMEOUT7_AHB_CLK                  65
#define GCC_PCNOC_BUS_TIMEOUT8_AHB_CLK                  66
#define GCC_PCNOC_BUS_TIMEOUT9_AHB_CLK                  67
#define GCC_PCNOC_MPU_CFG_AHB_CLK                       68
#define GCC_PCNOC_XPU_AHB_CLK                           69
#define GCC_PRNG_AHB_CLK                                70
#define GCC_QDSS_AT_CLK                                 71
#define GCC_QDSS_CFG_AHB_CLK                            72
#define GCC_QDSS_DAP_AHB_CLK                            73
#define GCC_QDSS_DAP_CLK                                74
#define GCC_QDSS_ETR_USB_CLK                            75
#define GCC_QDSS_STM_CLK                                76
#define GCC_QDSS_TRACECLKIN_CLK                         77
#define GCC_QDSS_TSCTR_DIV2_CLK                         78
#define GCC_QDSS_TSCTR_DIV8_CLK                         79
#define GCC_QPIC_AHB_CLK                                80
#define GCC_QPIC_CLK                                    81
#define GCC_SDCC1_AHB_CLK                               82
#define GCC_SDCC1_APPS_CLK                              83
#define GCC_SEC_CTRL_ACC_CLK                            84
#define GCC_SEC_CTRL_AHB_CLK                            85
#define GCC_SEC_CTRL_BOOT_ROM_PATCH_CLK                 86
#define GCC_SNOC_BUS_TIMEOUT0_AHB_CLK                   87
#define GCC_SNOC_BUS_TIMEOUT1_AHB_CLK                   88
#define GCC_SNOC_BUS_TIMEOUT2_AHB_CLK                   89
#define GCC_SNOC_BUS_TIMEOUT3_AHB_CLK                   90
#define GCC_SPDM_CFG_AHB_CLK                            91
#define GCC_SPDM_FF_CLK                                 92
#define GCC_SPDM_MSTR_AHB_CLK                           93
#define GCC_SPDM_PCNOC_CY_CLK                           94
#define GCC_SNOC_PCNOC_AHB_CLK                          95
#define GCC_SYS_NOC_125M_CLK                            96
#define GCC_SYS_NOC_AT_CLK                              97
#define GCC_SYS_NOC_AXI_CLK                             98
#define GCC_SYS_NOC_QDSS_STM_AXI_CLK                    99
#define GCC_TCSR_AHB_CLK                                100
#define GCC_TLMM_AHB_CLK                                101
#define GCC_USB2_MASTER_CLK                             102
#define GCC_USB2_SLEEP_CLK                              103
#define GCC_USB3_MASTER_CLK                             104
#define GCC_USB3_SLEEP_CLK                              105
#define GCC_WCSS2G_REF_CLK                              106
#define GCC_WCSS2G_RTC_CLK                              107
#define GCC_WCSS5G_REF_CLK                              108
#define GCC_WCSS5G_RTC_CLK                              109
#define GCC_USB3_MOCK_UTMI_CLK_SRC                      110
#define GCC_APPS_CLK_SRC                                111
#define GCC_APPS_AHB_CLK_SRC                            112
#define GCC_APPS_BIMC_CLK_SRC                           113

#endif
