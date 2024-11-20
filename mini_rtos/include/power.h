#ifndef POWER_H
#define POWER_H

#include <stdint.h>
#include "rtos_types.h"

/* Power States */
typedef enum {
    POWER_STATE_RUN = 0,
    POWER_STATE_SLEEP,
    POWER_STATE_STOP,
    POWER_STATE_STANDBY
} power_state_t;

/* Clock Domains */
typedef enum {
    CLOCK_DOMAIN_CPU = 0,
    CLOCK_DOMAIN_AHB,
    CLOCK_DOMAIN_APB1,
    CLOCK_DOMAIN_APB2,
    CLOCK_DOMAIN_COUNT
} clock_domain_t;

/* Power Management Configuration */
typedef struct {
    uint8_t dynamic_voltage_scaling;
    uint8_t adaptive_tick;
    uint8_t deep_sleep_enabled;
    uint32_t sleep_threshold;
    uint32_t wakeup_sources;
} power_config_t;

/* Clock Configuration */
typedef struct {
    uint32_t frequency;
    uint8_t enabled;
    uint8_t auto_gating;
    uint8_t dynamic_scaling;
} clock_config_t;

/* Power Domain */
typedef struct {
    power_state_t state;
    uint32_t voltage;
    clock_config_t clocks[CLOCK_DOMAIN_COUNT];
    uint32_t active_peripherals;
} power_domain_t;

/* Power Management Functions */
void power_mgmt_init(power_config_t *config);
int set_power_state(power_state_t state);
power_state_t get_power_state(void);

/* Clock Management */
int set_clock_frequency(clock_domain_t domain, uint32_t freq);
uint32_t get_clock_frequency(clock_domain_t domain);
void enable_clock_domain(clock_domain_t domain);
void disable_clock_domain(clock_domain_t domain);

/* Dynamic Voltage and Frequency Scaling */
typedef struct {
    uint32_t voltage;
    uint32_t frequency;
    uint32_t power_consumption;
} dvfs_point_t;

int set_dvfs_point(dvfs_point_t *point);
void enable_dvfs(void);
void disable_dvfs(void);

/* Peripheral Power Management */
int enable_peripheral(uint32_t peripheral_id);
int disable_peripheral(uint32_t peripheral_id);
uint8_t is_peripheral_enabled(uint32_t peripheral_id);

/* Power Statistics */
typedef struct {
    uint32_t time_in_state[4];  /* Time spent in each power state */
    uint32_t state_transitions;
    uint32_t clock_switches;
    float avg_power_consumption;
    uint32_t peak_power_consumption;
    uint32_t energy_used;
} power_stats_t;

void get_power_stats(power_stats_t *stats);
void reset_power_stats(void);

/* Power Events and Callbacks */
typedef void (*power_callback_t)(power_state_t old_state, power_state_t new_state);
int register_power_callback(power_callback_t callback);
void unregister_power_callback(power_callback_t callback);

/* Thermal Management */
typedef struct {
    int32_t current_temp;
    int32_t max_temp;
    int32_t min_temp;
    uint8_t throttling_active;
} thermal_stats_t;

void get_thermal_stats(thermal_stats_t *stats);
void set_thermal_threshold(int32_t temp);
void handle_thermal_event(void);

#endif /* POWER_H */
