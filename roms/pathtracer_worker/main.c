#include <stdint.h>
#include "wagnostic.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    float cam_x, cam_y, cam_z;
    float pitch, yaw;
    uint32_t start_y;
    uint32_t end_y;
    uint32_t worker_id;
    uint32_t num_workers;
} PathTracerJob;

typedef struct { float x, y, z; } Vec3;
static inline Vec3 vec_add(Vec3 a, Vec3 b) { return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline Vec3 vec_sub(Vec3 a, Vec3 b) { return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline Vec3 vec_mul(Vec3 a, float s) { return (Vec3){a.x * s, a.y * s, a.z * s}; }
static inline Vec3 vec_mul_v(Vec3 a, Vec3 b) { return (Vec3){a.x * b.x, a.y * b.y, a.z * b.z}; }
static inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float length(Vec3 v) { return __builtin_sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
static inline Vec3 normalize(Vec3 v) { float l = length(v); return l > 0.0f ? vec_mul(v, 1.0f/l) : (Vec3){0,0,0}; }
static inline Vec3 reflect(Vec3 v, Vec3 n) { return vec_sub(v, vec_mul(n, 2.0f * dot(v, n))); }

static inline uint32_t pcg_hash(uint32_t input) {
    uint32_t state = input * 747796405u + 2891336453u;
    uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

static inline float rand_f(uint32_t *seed) {
    *seed = pcg_hash(*seed);
    return (float)(*seed) / 4294967296.0f;
}

static inline float sin_fast(float x) {
    while (x > 3.14159265f) x -= 6.283185307f;
    while (x < -3.14159265f) x += 6.283185307f;
    float abs_x = x < 0.0f ? -x : x;
    return (16.0f * x * (3.14159265f - abs_x)) / (49.348022f - 4.0f * abs_x * (3.14159265f - abs_x));
}

static inline float cos_fast(float x) { return sin_fast(x + 1.570796326f); }

static inline Vec3 rand_unit_vector(uint32_t *seed) {
    float z = rand_f(seed) * 2.0f - 1.0f;
    float a = rand_f(seed) * 6.283185307f;
    float r = __builtin_sqrtf(1.0f - z * z);
    if (r < 0.0f) r = 0.0f;
    return (Vec3){r * cos_fast(a), r * sin_fast(a), z};
}

typedef struct {
    float t;
    Vec3 normal;
    Vec3 albedo;
    int mat; // 0=Diffuse, 1=Emissive, 2=Metal, 3=Glass
    float ior;
} HitRecord;

static inline int hit_sphere(Vec3 center, float radius, Vec3 ro, Vec3 rd, float t_min, float t_max, HitRecord *rec, Vec3 albedo, int mat, float ior) {
    Vec3 oc = vec_sub(ro, center);
    float a = dot(rd, rd);
    float half_b = dot(oc, rd);
    float c = dot(oc, oc) - radius*radius;
    float discriminant = half_b*half_b - a*c;
    if (discriminant > 0.0f) {
        float sqrtd = __builtin_sqrtf(discriminant);
        float root = (-half_b - sqrtd) / a;
        if (root < t_max && root > t_min) {
            rec->t = root;
            Vec3 p = vec_add(ro, vec_mul(rd, rec->t));
            rec->normal = vec_mul(vec_sub(p, center), 1.0f / radius);
            rec->albedo = albedo;
            rec->mat = mat;
            rec->ior = ior;
            return 1;
        }
        root = (-half_b + sqrtd) / a;
        if (root < t_max && root > t_min) {
            rec->t = root;
            Vec3 p = vec_add(ro, vec_mul(rd, rec->t));
            rec->normal = vec_mul(vec_sub(p, center), 1.0f / radius);
            rec->albedo = albedo;
            rec->mat = mat;
            rec->ior = ior;
            return 1;
        }
    }
    return 0;
}

static inline int hit_scene(Vec3 ro, Vec3 rd, float t_min, float t_max, HitRecord *rec) {
    HitRecord temp_rec;
    int hit_anything = 0;
    float closest = t_max;

    // Ground checkerboard
    if (hit_sphere((Vec3){0.0f, -1000.5f, -1.0f}, 1000.0f, ro, rd, t_min, closest, &temp_rec, (Vec3){0.8f, 0.8f, 0.8f}, 0, 1.0f)) {
        hit_anything = 1;
        closest = temp_rec.t;
        *rec = temp_rec;
        Vec3 p = vec_add(ro, vec_mul(rd, rec->t));
        int cx = (int)__builtin_floorf(p.x * 2.0f);
        int cz = (int)__builtin_floorf(p.z * 2.0f);
        if ((cx + cz) % 2 == 0) rec->albedo = (Vec3){0.3f, 0.3f, 0.35f};
        else rec->albedo = (Vec3){0.9f, 0.9f, 0.95f};
    }

    // Center Glass Sphere
    if (hit_sphere((Vec3){0.0f, 0.0f, -1.5f}, 0.5f, ro, rd, t_min, closest, &temp_rec, (Vec3){1.0f, 1.0f, 1.0f}, 3, 1.5f)) {
        hit_anything = 1;
        closest = temp_rec.t;
        *rec = temp_rec;
    }

    // Right Polished Gold Metal Sphere
    if (hit_sphere((Vec3){1.1f, 0.0f, -1.3f}, 0.5f, ro, rd, t_min, closest, &temp_rec, (Vec3){0.85f, 0.7f, 0.2f}, 2, 1.0f)) {
        hit_anything = 1;
        closest = temp_rec.t;
        *rec = temp_rec;
    }

    // Left Matte Blue Sphere
    if (hit_sphere((Vec3){-1.1f, 0.0f, -1.3f}, 0.5f, ro, rd, t_min, closest, &temp_rec, (Vec3){0.2f, 0.4f, 0.9f}, 0, 1.0f)) {
        hit_anything = 1;
        closest = temp_rec.t;
        *rec = temp_rec;
    }

    // Warm Overhead Light
    if (hit_sphere((Vec3){0.0f, 5.0f, -2.0f}, 2.0f, ro, rd, t_min, closest, &temp_rec, (Vec3){12.0f, 10.0f, 8.0f}, 1, 1.0f)) {
        hit_anything = 1;
        closest = temp_rec.t;
        *rec = temp_rec;
    }

    return hit_anything;
}

static inline int refract(Vec3 v, Vec3 n, float ni_over_nt, Vec3 *refracted) {
    Vec3 uv = normalize(v);
    float dt = dot(uv, n);
    float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1.0f - dt * dt);
    if (discriminant > 0.0f) {
        *refracted = vec_sub(vec_mul(vec_sub(uv, vec_mul(n, dt)), ni_over_nt), vec_mul(n, __builtin_sqrtf(discriminant)));
        return 1;
    }
    return 0;
}

static inline float schlick(float cosine, float ior) {
    float r0 = (1.0f - ior) / (1.0f + ior);
    r0 = r0 * r0;
    float one_minus_c = 1.0f - cosine;
    return r0 + (1.0f - r0) * (one_minus_c*one_minus_c*one_minus_c*one_minus_c*one_minus_c);
}

static Vec3 ray_color(Vec3 ro, Vec3 rd, uint32_t *seed, int fast_mode) {
    Vec3 cur_ro = ro;
    Vec3 cur_rd = rd;
    Vec3 throughput = {1.0f, 1.0f, 1.0f};
    Vec3 accum_light = {0.0f, 0.0f, 0.0f};

    int max_depth = fast_mode ? 2 : 4;

    for (int depth = 0; depth < max_depth; depth++) {
        HitRecord rec;
        if (hit_scene(cur_ro, cur_rd, 0.001f, 10000.0f, &rec)) {
            if (rec.mat == 1) {
                accum_light = vec_add(accum_light, vec_mul_v(throughput, rec.albedo));
                break;
            } else if (rec.mat == 0) {
                Vec3 target = vec_add(vec_add(vec_add(cur_ro, vec_mul(cur_rd, rec.t)), rec.normal), rand_unit_vector(seed));
                cur_ro = vec_add(cur_ro, vec_mul(cur_rd, rec.t));
                cur_rd = normalize(vec_sub(target, cur_ro));
                throughput = vec_mul_v(throughput, rec.albedo);
            } else if (rec.mat == 2) {
                Vec3 p = vec_add(cur_ro, vec_mul(cur_rd, rec.t));
                Vec3 ref = reflect(normalize(cur_rd), rec.normal);
                cur_ro = p;
                cur_rd = normalize(vec_add(ref, vec_mul(rand_unit_vector(seed), 0.05f)));
                throughput = vec_mul_v(throughput, rec.albedo);
            } else if (rec.mat == 3) {
                Vec3 outward_normal;
                Vec3 reflected = reflect(cur_rd, rec.normal);
                float ni_over_nt;
                Vec3 refracted;
                float reflect_prob;
                float cosine;
                Vec3 p = vec_add(cur_ro, vec_mul(cur_rd, rec.t));

                if (dot(cur_rd, rec.normal) > 0.0f) {
                    outward_normal = vec_mul(rec.normal, -1.0f);
                    ni_over_nt = rec.ior;
                    cosine = rec.ior * dot(cur_rd, rec.normal) / length(cur_rd);
                } else {
                    outward_normal = rec.normal;
                    ni_over_nt = 1.0f / rec.ior;
                    cosine = -dot(cur_rd, rec.normal) / length(cur_rd);
                }

                if (refract(cur_rd, outward_normal, ni_over_nt, &refracted)) {
                    reflect_prob = schlick(cosine, rec.ior);
                } else {
                    reflect_prob = 1.0f;
                }

                if (rand_f(seed) < reflect_prob) {
                    cur_ro = p;
                    cur_rd = reflected;
                } else {
                    cur_ro = p;
                    cur_rd = refracted;
                }
            }
        } else {
            Vec3 unit_direction = normalize(cur_rd);
            float t = 0.5f * (unit_direction.y + 1.0f);
            Vec3 sky = vec_add(vec_mul((Vec3){1.0f, 1.0f, 1.0f}, (1.0f - t)), vec_mul((Vec3){0.4f, 0.6f, 0.9f}, t));
            accum_light = vec_add(accum_light, vec_mul_v(throughput, vec_mul(sky, 0.4f)));
            break;
        }
    }
    return accum_light;
}

#define MAX_WIDTH 320
#define MAX_HEIGHT 240

static float g_acc_buffer[MAX_WIDTH * MAX_HEIGHT * 3];
static uint32_t g_slice_pixels[MAX_WIDTH * MAX_HEIGHT];

int32_t winit(void) {
    return 0;
}

int32_t wupdate(void) {
    PathTracerJob job;
    int r = wask("master", &job, sizeof(job), -1);
    if (r != WIPC_OK) return WUPDATE_OK;

    uint32_t width = job.width > MAX_WIDTH ? MAX_WIDTH : job.width;
    uint32_t height = job.height > MAX_HEIGHT ? MAX_HEIGHT : job.height;
    uint32_t start_y = job.start_y;
    uint32_t end_y = job.end_y > height ? height : job.end_y;
    if (start_y >= end_y || width == 0 || height == 0) return WUPDATE_OK;

    int fast_mode = (job.frame_count == 0);
    Vec3 camera = {job.cam_x, job.cam_y, job.cam_z};

    float cp = cos_fast(job.pitch);
    float sp = sin_fast(job.pitch);
    float cy = cos_fast(job.yaw);
    float sy = sin_fast(job.yaw);

    Vec3 w = {-cp*sy, -sp, cp*cy};
    Vec3 world_up = {0, 1, 0};
    Vec3 u = normalize((Vec3){w.y*world_up.z - w.z*world_up.y, w.z*world_up.x - w.x*world_up.z, w.x*world_up.y - w.y*world_up.x});
    Vec3 v = {w.y*u.z - w.z*u.y, w.z*u.x - w.x*u.z, w.x*u.y - w.y*u.x};

    uint32_t slice_row_count = end_y - start_y;
    const int spp = fast_mode ? 1 : 2;

    for (uint32_t y = start_y; y < end_y; ++y) {
        uint32_t local_y = y - start_y;
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t seed = pcg_hash(y * width + x + job.frame_count * 719393 + job.worker_id * 10007);

            Vec3 col_acc = {0.0f, 0.0f, 0.0f};
            for (int s = 0; s < spp; s++) {
                float u_coord = (float)(x + (fast_mode ? 0.5f : rand_f(&seed))) / (float)(width - 1);
                float v_coord = (float)(y + (fast_mode ? 0.5f : rand_f(&seed))) / (float)(height - 1);

                Vec3 horizontal = vec_mul(u, 2.0f);
                Vec3 vertical = vec_mul(v, 1.5f);
                Vec3 lower_left = vec_sub(vec_sub(camera, vec_mul(horizontal, 0.5f)), vec_mul(vertical, 0.5f));
                lower_left = vec_add(lower_left, w);

                Vec3 rd = vec_add(lower_left, vec_mul(horizontal, u_coord));
                rd = vec_add(rd, vec_mul(vertical, v_coord));
                rd = normalize(vec_sub(rd, camera));

                Vec3 c = ray_color(camera, rd, &seed, fast_mode);
                col_acc = vec_add(col_acc, c);
            }

            Vec3 color = vec_mul(col_acc, 1.0f / (float)spp);

            uint32_t offset = (y * width + x) * 3;
            if (job.frame_count == 0) {
                g_acc_buffer[offset + 0] = color.x;
                g_acc_buffer[offset + 1] = color.y;
                g_acc_buffer[offset + 2] = color.z;
            } else {
                g_acc_buffer[offset + 0] += color.x;
                g_acc_buffer[offset + 1] += color.y;
                g_acc_buffer[offset + 2] += color.z;
            }

            float scale = 1.0f / (float)(job.frame_count + 1);
            float cr = __builtin_sqrtf(g_acc_buffer[offset + 0] * scale);
            float cg = __builtin_sqrtf(g_acc_buffer[offset + 1] * scale);
            float cb = __builtin_sqrtf(g_acc_buffer[offset + 2] * scale);

            if (cr > 1.0f) cr = 1.0f; if (cr < 0.0f) cr = 0.0f;
            if (cg > 1.0f) cg = 1.0f; if (cg < 0.0f) cg = 0.0f;
            if (cb > 1.0f) cb = 1.0f; if (cb < 0.0f) cb = 0.0f;

            uint8_t r8 = (uint8_t)(cr * 255.0f);
            uint8_t g8 = (uint8_t)(cg * 255.0f);
            uint8_t b8 = (uint8_t)(cb * 255.0f);

            g_slice_pixels[local_y * width + x] = (0xFF000000u) | ((uint32_t)b8 << 16) | ((uint32_t)g8 << 8) | (uint32_t)r8;
        }
    }

    uint32_t slice_bytes = slice_row_count * width * 4;
    wtell("master", g_slice_pixels, slice_bytes, -1);

    return WUPDATE_OK;
}

int32_t wexit(void) {
    return 0;
}
