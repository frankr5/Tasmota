/********************************************************************
 * Tasmota lib
 *
 * To use: `import tasmota`
 *******************************************************************/
#include "be_constobj.h"
#include "be_ctypes.h"

extern struct TasmotaGlobal_t TasmotaGlobal;
extern struct TSettings * Settings;
extern const be_ctypes_structure_t be_tasmota_global_struct;
extern const be_ctypes_structure_t be_tasmota_settings_struct;

extern int l_getFreeHeap(bvm *vm);
extern int l_arch(bvm *vm);
extern int be_mqtt_publish(bvm *vm);
extern int l_publish_result(bvm *vm);
extern int l_publish_rule(bvm *vm);
extern int l_cmd(bvm *vm);
extern int l_getoption(bvm *vm);
extern int l_millis(bvm *vm);
extern int l_timereached(bvm *vm);
extern int l_rtc(bvm *vm);
extern int l_time_dump(bvm *vm);
extern int l_strftime(bvm *vm);
extern int l_strptime(bvm *vm);
extern int l_memory(bvm *vm);
extern int l_wifi(bvm *vm);
extern int l_eth(bvm *vm);
extern int l_yield(bvm *vm);
extern int l_delay(bvm *vm);
extern int l_scaleuint(bvm *vm);
extern int l_logInfo(bvm *vm);
extern int l_save(bvm *vm);
extern int t_random_byte(bvm *vm);

extern int l_read_sensors(bvm *vm);

extern int l_respCmnd(bvm *vm);
extern int l_respCmndStr(bvm *vm);
extern int l_respCmndDone(bvm *vm);
extern int l_respCmndError(bvm *vm);
extern int l_respCmndFailed(bvm *vm);
extern int l_resolveCmnd(bvm *vm);

extern int l_respAppend(bvm *vm);
extern int l_webSend(bvm *vm);
extern int l_webSendDecimal(bvm *vm);

extern int l_getlight(bvm *vm);
extern int l_setlight(bvm *vm);
extern int l_getpower(bvm *vm);
extern int l_setpower(bvm *vm);
extern int l_getswitch(bvm *vm);

extern int l_i2cenabled(bvm *vm);
extern int tasm_find_op(bvm *vm);

#include "solidify/solidified_tasmota_class.h"

#include "be_fixed_be_class_tasmota.h"


/* @const_object_info_begin
class be_class_tasmota (scope: global, name: Tasmota) {
    _fl, var
    _rules, var
    _timers, var
    _crons, var
    _ccmd, var
    _drivers, var
    wire1, var
    wire2, var
    cmd_res, var
    global, var
    settings, var
    wd, var
    _debug_present, var

    _global_def, comptr(&be_tasmota_global_struct)
    _settings_def, comptr(&be_tasmota_settings_struct)
    _global_addr, comptr(&TasmotaGlobal)
    _settings_ptr, comptr(&Settings)

    init, closure(Tasmota_init_closure)

    get_free_heap, func(l_getFreeHeap)
    arch, func(l_arch)
    publish, func(be_mqtt_publish)
    publish_result, func(l_publish_result)
    publish_rule, func(l_publish_rule)
    _cmd, func(l_cmd)
    get_option, func(l_getoption)
    millis, func(l_millis)
    time_reached, func(l_timereached)
    rtc, func(l_rtc)
    time_dump, func(l_time_dump)
    strftime, func(l_strftime)
    strptime, func(l_strptime)
    memory, func(l_memory)
    wifi, func(l_wifi)
    eth, func(l_eth)
    yield, func(l_yield)
    delay, func(l_delay)
    scale_uint, func(l_scaleuint)
    log, func(l_logInfo)
    save, func(l_save)

    read_sensors, func(l_read_sensors)

    resp_cmnd, func(l_respCmnd)
    resp_cmnd_str, func(l_respCmndStr)
    resp_cmnd_done, func(l_respCmndDone)
    resp_cmnd_error, func(l_respCmndError)
    resp_cmnd_failed, func(l_respCmndFailed)
    resolvecmnd, func(l_resolveCmnd)

    response_append, func(l_respAppend)
    web_send, func(l_webSend)
    web_send_decimal, func(l_webSendDecimal)

    get_power, func(l_getpower)
    set_power, func(l_setpower)
    get_switch, func(l_getswitch)     // deprecated
    get_switches, func(l_getswitch)

    i2c_enabled, func(l_i2cenabled)

    fast_loop, closure(Tasmota_fast_loop_closure)
    add_fast_loop, closure(Tasmota_add_fast_loop_closure)
    remove_fast_loop, closure(Tasmota_remove_fast_loop_closure)
    cmd, closure(Tasmota_cmd_closure)
    _find_op, func(tasm_find_op)        // new C version for finding a rule operator
    find_key_i, closure(Tasmota_find_key_i_closure)
    find_op, closure(Tasmota_find_op_closure)
    add_rule, closure(Tasmota_add_rule_closure)
    remove_rule, closure(Tasmota_remove_rule_closure)
    try_rule, closure(Tasmota_try_rule_closure)
    exec_rules, closure(Tasmota_exec_rules_closure)
    exec_tele, closure(Tasmota_exec_tele_closure)
    set_timer, closure(Tasmota_set_timer_closure)
    run_deferred, closure(Tasmota_run_deferred_closure)
    remove_timer, closure(Tasmota_remove_timer_closure)
    add_cmd, closure(Tasmota_add_cmd_closure)
    remove_cmd, closure(Tasmota_remove_cmd_closure)
    exec_cmd, closure(Tasmota_exec_cmd_closure)
    gc, closure(Tasmota_gc_closure)
    event, closure(Tasmota_event_closure)
    add_driver, closure(Tasmota_add_driver_closure)
    remove_driver, closure(Tasmota_remove_driver_closure)
    load, closure(Tasmota_load_closure)
    wire_scan, closure(Tasmota_wire_scan_closure)
    time_str, closure(Tasmota_time_str_closure)

    add_cron, closure(Tasmota_add_cron_closure)
    run_cron, closure(Tasmota_run_cron_closure)
    next_cron, closure(Tasmota_next_cron_closure)
    remove_cron, closure(Tasmota_remove_cron_closure)

    check_not_method, closure(Tasmota_check_not_method_closure)

    hs2rgb, closure(Tasmota_hs2rgb_closure)

    gen_cb, closure(Tasmota_gen_cb_closure)

    get_light, closure(Tasmota_get_light_closure)
    set_light, closure(Tasmota_set_light_closure)
}
@const_object_info_end */
