#ifndef MPU_H
#define MPU_H

#include <stdint.h>

/* Memory Access Permissions */
typedef enum {
    MPU_NO_ACCESS = 0,
    MPU_READ_ONLY = 1,
    MPU_READ_WRITE = 2,
    MPU_PRIVILEGED_RW = 3
} mpu_access_t;

/* Memory Region Attributes */
typedef enum {
    MPU_NORMAL = 0,
    MPU_DEVICE = 1,
    MPU_STRONGLY_ORDERED = 2,
    MPU_CACHEABLE = 3
} mpu_attr_t;

/* Memory Region Configuration */
typedef struct {
    void *base_addr;
    uint32_t size;
    mpu_access_t access;
    mpu_attr_t attr;
    uint8_t executable;
    uint8_t shareable;
    uint8_t cacheable;
    uint8_t bufferable;
} mpu_region_t;

/* Memory Domain */
typedef struct {
    mpu_region_t regions[8];  /* STM32F1 supports 8 MPU regions */
    uint8_t region_count;
    uint8_t domain_id;
} memory_domain_t;

/* MPU Functions */
void mpu_init(void);
int mpu_configure_region(uint8_t region_num, mpu_region_t *config);
void mpu_enable(void);
void mpu_disable(void);

/* Memory Domain Management */
memory_domain_t *memory_domain_create(void);
void memory_domain_delete(memory_domain_t *domain);
int memory_domain_add_region(memory_domain_t *domain, mpu_region_t *region);
int memory_domain_remove_region(memory_domain_t *domain, void *base_addr);
int memory_domain_assign_task(memory_domain_t *domain, tcb_t *task);

/* Memory Protection Fault Handlers */
void mem_fault_handler(void);
void bus_fault_handler(void);
void usage_fault_handler(void);

/* Memory Access Validation */
int validate_memory_access(void *addr, uint32_t size, mpu_access_t required_access);

#endif /* MPU_H */
