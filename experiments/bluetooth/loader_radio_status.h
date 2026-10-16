/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-1 loader USB diagnostics. Fixed metadata only; no RF payload or writes. */
uint32_t np_ble_loader_status(unsigned i){
    extern uint32_t np_loader_radio_power(void);
    extern uint32_t np_rx_crc32(const uint8_t *,uint32_t);
    uint32_t mac0=0,mac1=0,ready=0;
    for(unsigned k=0;k<4;k++)mac0|=(uint32_t)ble_mac[k]<<(8*k);
    for(unsigned k=0;k<2;k++)mac1|=(uint32_t)ble_mac[4+k]<<(8*k);
    if(ble_on)for(unsigned k=0;k<NTASK;k++)if(tasks[k].name)
        ready|=(1u|(tasks[k].ready?2u:0u)|(tasks[k].waiting_q?4u:0u)|(tasks[k].sleeping?8u:0u))<<(4*k);
    uint32_t v[]={np_state,np_error,(uint32_t)np_rf_result,np_rf.bank,np_rf.record,np_rf.records,
        ble_diag.stage,ble_diag.trim,ble_diag.fatal,ble_diag.heap_high,ble_diag.alloc_fails,ble_diag.irq_leaks,
        mac0,mac1,np_rf.xosc[0]|(uint32_t)np_rf.xosc[1]<<8,ble_diag.delays,ble_diag.t4_stalls,ble_diag.irq_lowered,
        fm1_clk_hz(FM1_CLK_SYS),fm1_clk_hz(FM1_CLK_HSB),fm1_clk_hz(FM1_CLK_LSB),ble_irqs_requested[0],ble_irqs_requested[1],fm1_ble_irq_depth,
        ble_events,ble_last_event,run_max,ready,core_irq_is_masked(29),core_irq_is_masked(45),
        *(volatile uint32_t *)0x1000cu,*(volatile uint32_t *)0x10010u,FM1B_CLK_CON2,FM1B_PLL_CON0,
        *(volatile uint32_t *)0x28000u,*(volatile uint32_t *)0x28004u,
        np_radio_irq_calls,np_radio_irq_rets,np_radio_irq_icfg,np_loader_radio_power(),
        *(volatile uint32_t *)0x10800u,*(volatile uint32_t *)0x10808u,
        np_rx_crc32(np_rf.trim,sizeof np_rf.trim),np_rx_crc32(np_diag_adv,np_diag_adv_size),np_rx_crc32(np_diag_rsp,np_diag_rsp_size),
        np_rf.trim[0]|(uint32_t)np_rf.trim[1]<<8,*(volatile uint32_t *)(0x01c7fe00u+29u*4u),*(volatile uint32_t *)(0x01c7fe00u+45u*4u)};
    return i<sizeof v/sizeof v[0]?v[i]:0;
}
