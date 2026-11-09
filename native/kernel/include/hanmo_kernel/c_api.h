/* 内核的 C 接口（设计第 14 节）：给沙盒用 Python 自带的 ctypes 驱动内核，也给 Godot 桥接用。
 * 只用 C 类型。实验中的心智和包用回调接上；接口版本与 kAbiVersion 一致。 */
#ifndef HANMO_KERNEL_C_API_H
#define HANMO_KERNEL_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hk_kernel hk_kernel;

/* 引用：表种类与 C++ 的 Table 相同（0 无、1 行动者、2 地方、3 物品种类、4 事实、5 包的表）。 */
typedef struct {
  uint32_t id;
  uint8_t table;
  uint16_t package;
} hk_ref;

typedef struct {
  uint32_t actor;
  uint16_t verb;
  hk_ref target;
  hk_ref object;
  int64_t qty;
  uint32_t manner;
  uint32_t tiers[6];
} hk_action;

typedef struct {
  uint16_t kind;
  hk_ref about;
  hk_ref other;
  int64_t value;
  uint32_t when;
  uint8_t source;
  uint8_t hops;
  uint16_t credence;
  uint32_t told_by;
  uint8_t fresh;
} hk_memory;

typedef struct {
  uint64_t id;
  uint32_t when;
  uint16_t kind;
  uint32_t actor;
  hk_ref target;
  hk_ref object;
  int64_t qty;
  uint32_t place;
} hk_fact;

typedef struct {
  uint32_t now;
  uint64_t people, named, deaths, hungry, facts, memories;
} hk_stats;

/* 一次细想的记录。指针只在回调期间有效。 */
typedef struct {
  uint32_t when;
  uint32_t actor;
  const float *situation;
  uint32_t situation_len;
  const uint32_t *candidate_recipes;
  const float *candidate_values;
  uint32_t candidates;
  int32_t chosen;
  float idle_value;
} hk_decision;

typedef void (*hk_recorder_fn)(const hk_decision *record, void *user);
/* 外挂心智：只能用 hk_known_* 读这个人自己的所知。返回 0 表示什么也不做。 */
typedef int (*hk_mind_fn)(hk_kernel *k, uint32_t actor, hk_action *out, void *user);
/* 外挂包的订阅回调：收到的是真值。 */
typedef void (*hk_fact_fn)(hk_kernel *k, const hk_fact *fact, void *user);

uint32_t hk_abi_version(void);
hk_kernel *hk_create(const char *content_dir, uint64_t seed);
void hk_destroy(hk_kernel *k);
int hk_start(hk_kernel *k);
uint32_t hk_advance(hk_kernel *k, uint32_t half_days);
/* 返回拒绝原因编号；0 表示成功，*fact_out 为产生的事实。 */
int32_t hk_submit(hk_kernel *k, const hk_action *action, uint64_t *fact_out);
int32_t hk_preview(const hk_kernel *k, const hk_action *action, int64_t *qty_effective);
int hk_save(const hk_kernel *k, const char *path);
int hk_load(hk_kernel *k, const char *path);
void hk_stats_get(const hk_kernel *k, hk_stats *out);
uint64_t hk_digest(const hk_kernel *k);

void hk_set_recorder(hk_kernel *k, hk_recorder_fn fn, void *user);
/* actor 为 0 表示默认心智。 */
void hk_set_external_mind(hk_kernel *k, uint32_t actor, hk_mind_fn fn, void *user);
/* fact_kind 为 0 表示全部。 */
void hk_subscribe(hk_kernel *k, uint16_t fact_kind, hk_fact_fn fn, void *user);

uint32_t hk_known_count(const hk_kernel *k, uint32_t actor);
int hk_known_get(const hk_kernel *k, uint32_t actor, uint32_t index, hk_memory *out);

#ifdef __cplusplus
}
#endif

#endif /* HANMO_KERNEL_C_API_H */
