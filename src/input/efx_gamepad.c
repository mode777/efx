#include "input/efx_gamepad.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* parsed mapping database cap (the vendored SDL table is ~1300 entries; the
 * platform backend normalizes directly, so the DB path is exercised by tests
 * and synthetic descriptors — cap generously but bounded). */
#define EFX_GP_MAPPING_MAX 512

#define EFX_GP_ELEM_NONE 0
#define EFX_GP_ELEM_BUTTON 1
#define EFX_GP_ELEM_AXIS 2
#define EFX_GP_ELEM_HAT 3

typedef struct {
    unsigned char kind;
    unsigned char index; /* button/axis index, or (hat << 4) | bit */
    float scale;
    float offset;
} efx_gp_element;

typedef struct {
    int valid;
    char guid[33];
    efx_gp_element buttons[EFX_GPB_COUNT];
    efx_gp_element axes[EFX_GPA_COUNT];
} efx_gp_mapping;

typedef struct {
    int connected;
    int normalized;   /* staging: descriptor uses the standard layout */
    int mapped;       /* live: resolved a mapping */
    int mapped_hint;  /* staging: backend's mapped flag for normalized pads */
    char name[128];
    char guid[33];
    efx_gp_mapping mapping;
    int raw_button_count;
    unsigned char raw_buttons[EFX_GAMEPAD_RAW_BUTTON_MAX];
    int raw_axis_count;
    float raw_axes[EFX_GAMEPAD_RAW_AXIS_MAX];
    int raw_hat_count;
    float raw_hats[EFX_GAMEPAD_RAW_HAT_MAX];
    float axes[EFX_GPA_COUNT];
    unsigned char down[EFX_GPB_COUNT];
    unsigned char pressed[EFX_GPB_COUNT];
    unsigned char released[EFX_GPB_COUNT];
    unsigned char prev[EFX_GPB_COUNT];
} efx_gp_slot;

static efx_gp_slot LIVE[EFX_GAMEPAD_MAX];
static efx_gp_slot STAGE[EFX_GAMEPAD_MAX];
static int staged;

static efx_gp_mapping MAPPINGS[EFX_GP_MAPPING_MAX];
static int mapping_count;
static efx_gp_mapping STANDARD;
static int standard_ready;

static efx_gamepad_source_fn SOURCE;
static void *SOURCE_UD;

/* bounded, always-terminating copy (avoids MSVC's strncpy deprecation) */
static void copy_str(char *dst, size_t cap, const char *src) {
    if (cap == 0 || !src) {
        return;
    }
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int CONNECT_EVENTS[EFX_GAMEPAD_MAX];
static int connect_event_count;
static int DISCONNECT_EVENTS[EFX_GAMEPAD_MAX];
static int disconnect_event_count;

/* --------------------------------------------------------- name tables */

static const char *const BUTTON_NAMES[EFX_GPB_COUNT] = {
    "south", "east", "west", "north",
    "leftShoulder", "rightShoulder", "leftTrigger", "rightTrigger",
    "back", "start", "guide", "leftStick", "rightStick",
    "dpadUp", "dpadDown", "dpadLeft", "dpadRight",
};

static const char *const AXIS_NAMES[EFX_GPA_COUNT] = {
    "leftX", "leftY", "rightX", "rightY", "leftTrigger", "rightTrigger",
};

int efx_input_gamepad_button_id(const char *name) {
    if (!name) {
        return -1;
    }
    for (int i = 0; i < EFX_GPB_COUNT; i++) {
        if (strcmp(BUTTON_NAMES[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *efx_input_gamepad_button_name(int button) {
    if (button < 0 || button >= EFX_GPB_COUNT) {
        return NULL;
    }
    return BUTTON_NAMES[button];
}

int efx_input_gamepad_axis_id(const char *name) {
    if (!name) {
        return -1;
    }
    for (int i = 0; i < EFX_GPA_COUNT; i++) {
        if (strcmp(AXIS_NAMES[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *efx_input_gamepad_axis_name(int axis) {
    if (axis < 0 || axis >= EFX_GPA_COUNT) {
        return NULL;
    }
    return AXIS_NAMES[axis];
}

/* -------------------------------------------------------- mapping model */

static void element_none(efx_gp_element *e) {
    memset(e, 0, sizeof(*e));
    e->kind = EFX_GP_ELEM_NONE;
}

static efx_gp_mapping *mapping_find(const char *guid) {
    for (int i = 0; i < mapping_count; i++) {
        if (strncmp(MAPPINGS[i].guid, guid, 32) == 0) {
            return &MAPPINGS[i];
        }
    }
    /* permissive: ignore the last four bytes (version/CRC differ per device) */
    for (int i = 0; i < mapping_count; i++) {
        if (strncmp(MAPPINGS[i].guid, guid, 24) == 0) {
            return &MAPPINGS[i];
        }
    }
    /* broader permissive: bus + vendor/product prefix (Windows non-Xbox pads
       reached through the primary path may differ past the vendor bytes) */
    for (int i = 0; i < mapping_count; i++) {
        if (strncmp(MAPPINGS[i].guid, guid, 8) == 0) {
            return &MAPPINGS[i];
        }
    }
    return NULL;
}

/* parse a value token after the field name and ':' */
static int parse_value(efx_gp_element *e, const char *p) {
    int invert = 0;
    int sign = 0; /* 0 none, '+' positive half, '-' negative half */
    for (;;) {
        if (*p == '~') {
            invert = !invert;
            p++;
        } else if (*p == '+') {
            sign = '+';
            p++;
        } else if (*p == '-') {
            sign = '-';
            p++;
        } else {
            break;
        }
    }

    if (*p == 'a') {
        char *end = NULL;
        long idx = strtol(p + 1, &end, 10);
        e->kind = EFX_GP_ELEM_AXIS;
        e->index = (unsigned char)idx;
        switch (sign) {
        case '+':
            e->scale = 0.5f;
            e->offset = 0.5f;
            break;
        case '-':
            e->scale = -0.5f;
            e->offset = 0.5f;
            break;
        default:
            e->scale = 1.0f;
            e->offset = 0.0f;
            break;
        }
        if (invert) {
            e->scale = -e->scale;
            e->offset = -e->offset;
        }
        return 1;
    }
    if (*p == 'b') {
        char *end = NULL;
        long idx = strtol(p + 1, &end, 10);
        e->kind = EFX_GP_ELEM_BUTTON;
        e->index = (unsigned char)idx;
        e->scale = 1.0f;
        e->offset = 0.0f;
        return 1;
    }
    if (*p == 'h') {
        char *end = NULL;
        long hat = strtol(p + 1, &end, 10);
        long bit = 0;
        if (end && *end == '.') {
            bit = strtol(end + 1, &end, 10);
        }
        e->kind = EFX_GP_ELEM_HAT;
        e->index = (unsigned char)((hat << 4) | (bit & 0xf));
        e->scale = 1.0f;
        e->offset = 0.0f;
        return 1;
    }
    return 0;
}

typedef struct {
    const char *name;
    int is_axis;   /* 0 = semantic button field, 1 = semantic axis field */
    int id;
} gp_field;

static const gp_field FIELDS[] = {
    {"a", 0, EFX_GPB_SOUTH},
    {"b", 0, EFX_GPB_EAST},
    {"x", 0, EFX_GPB_WEST},
    {"y", 0, EFX_GPB_NORTH},
    {"back", 0, EFX_GPB_BACK},
    {"start", 0, EFX_GPB_START},
    {"guide", 0, EFX_GPB_GUIDE},
    {"leftshoulder", 0, EFX_GPB_LEFT_SHOULDER},
    {"rightshoulder", 0, EFX_GPB_RIGHT_SHOULDER},
    {"leftstick", 0, EFX_GPB_LEFT_STICK},
    {"rightstick", 0, EFX_GPB_RIGHT_STICK},
    {"dpup", 0, EFX_GPB_DPAD_UP},
    {"dpdown", 0, EFX_GPB_DPAD_DOWN},
    {"dpleft", 0, EFX_GPB_DPAD_LEFT},
    {"dpright", 0, EFX_GPB_DPAD_RIGHT},
    {"leftx", 1, EFX_GPA_LEFT_X},
    {"lefty", 1, EFX_GPA_LEFT_Y},
    {"rightx", 1, EFX_GPA_RIGHT_X},
    {"righty", 1, EFX_GPA_RIGHT_Y},
};
#define FIELD_COUNT (int)(sizeof(FIELDS) / sizeof(FIELDS[0]))

static void mapping_init(efx_gp_mapping *m) {
    memset(m, 0, sizeof(*m));
    for (int i = 0; i < EFX_GPB_COUNT; i++) {
        element_none(&m->buttons[i]);
    }
    for (int i = 0; i < EFX_GPA_COUNT; i++) {
        element_none(&m->axes[i]);
    }
}

static int parse_mapping_line(efx_gp_mapping *m, const char *line) {
    mapping_init(m);

    /* guid (32 hex), then name, then comma-separated fields */
    const char *p = line;
    const char *comma = strchr(p, ',');
    if (!comma) {
        return 0;
    }
    size_t glen = (size_t)(comma - p);
    if (glen != 32) {
        return 0;
    }
    for (size_t i = 0; i < 32; i++) {
        char c = p[i];
        if (c >= 'A' && c <= 'F') {
            c = (char)(c + ('a' - 'A'));
        }
        m->guid[i] = c;
    }
    m->guid[32] = '\0';

    p = comma + 1;
    comma = strchr(p, ',');
    if (!comma) {
        return 0;
    }
    p = comma + 1;

    /* fields */
    while (*p) {
        const char *end = strchr(p, ',');
        if (!end) {
            end = p + strlen(p);
        }
        const char *colon = memchr(p, ':', (size_t)(end - p));
        if (colon) {
            size_t nlen = (size_t)(colon - p);
            for (int f = 0; f < FIELD_COUNT; f++) {
                if (strlen(FIELDS[f].name) != nlen ||
                    strncmp(FIELDS[f].name, p, nlen) != 0) {
                    continue;
                }
                efx_gp_element e;
                element_none(&e);
                if (parse_value(&e, colon + 1)) {
                    if (FIELDS[f].is_axis) {
                        m->axes[FIELDS[f].id] = e;
                    } else {
                        m->buttons[FIELDS[f].id] = e;
                    }
                }
                break;
            }
            /* triggers appear once and feed both the button and the axis */
            if ((nlen == 11 && strncmp("lefttrigger", p, 11) == 0) ||
                (nlen == 12 && strncmp("righttrigger", p, 12) == 0)) {
                int bid = (nlen == 11) ? EFX_GPB_LEFT_TRIGGER
                                       : EFX_GPB_RIGHT_TRIGGER;
                int aid = (nlen == 11) ? EFX_GPA_LEFT_TRIGGER
                                       : EFX_GPA_RIGHT_TRIGGER;
                efx_gp_element e;
                element_none(&e);
                if (parse_value(&e, colon + 1)) {
                    m->buttons[bid] = e;
                    m->axes[aid] = e;
                }
            }
        }
        p = (*end == ',') ? end + 1 : end;
    }

    m->valid = 1;
    return 1;
}

int efx_input_gamepad_load_mapping_line(const char *line) {
    if (!line || mapping_count >= EFX_GP_MAPPING_MAX) {
        return 0;
    }
    efx_gp_mapping m;
    if (!parse_mapping_line(&m, line)) {
        return 0;
    }
    MAPPINGS[mapping_count++] = m;
    return 1;
}

int efx_input_gamepad_load_mappings(const char *const *lines, int count) {
    int ok = 0;
    for (int i = 0; i < count; i++) {
        if (efx_input_gamepad_load_mapping_line(lines[i])) {
            ok++;
        }
    }
    return ok;
}

void efx_input_gamepad_clear_mappings(void) {
    mapping_count = 0;
}

static void standard_init(void) {
    if (standard_ready) {
        return;
    }
    mapping_init(&STANDARD);
    static const int btn_map[EFX_GPB_COUNT] = {
        EFX_GPB_SOUTH, EFX_GPB_EAST, EFX_GPB_WEST, EFX_GPB_NORTH,
        EFX_GPB_LEFT_SHOULDER, EFX_GPB_RIGHT_SHOULDER,
        EFX_GPB_LEFT_TRIGGER, EFX_GPB_RIGHT_TRIGGER,
        EFX_GPB_BACK, EFX_GPB_START, EFX_GPB_GUIDE,
        EFX_GPB_LEFT_STICK, EFX_GPB_RIGHT_STICK,
        EFX_GPB_DPAD_UP, EFX_GPB_DPAD_DOWN,
        EFX_GPB_DPAD_LEFT, EFX_GPB_DPAD_RIGHT,
    };
    for (int i = 0; i < EFX_GPB_COUNT; i++) {
        efx_gp_element e;
        element_none(&e);
        e.kind = EFX_GP_ELEM_BUTTON;
        e.index = (unsigned char)btn_map[i];
        e.scale = 1.0f;
        STANDARD.buttons[i] = e;
    }
    for (int i = 0; i < EFX_GPA_COUNT; i++) {
        efx_gp_element e;
        element_none(&e);
        e.kind = EFX_GP_ELEM_AXIS;
        e.index = (unsigned char)i;
        e.scale = 1.0f;
        e.offset = 0.0f;
        STANDARD.axes[i] = e;
    }
    STANDARD.valid = 1;
    standard_ready = 1;
}

/* --------------------------------------------------------- evaluation */

static float axis_range_clamp(int axis, float v) {
    if (axis == EFX_GPA_LEFT_TRIGGER || axis == EFX_GPA_RIGHT_TRIGGER) {
        if (v < 0.0f) {
            return 0.0f;
        }
        if (v > 1.0f) {
            return 1.0f;
        }
        return v;
    }
    if (v < -1.0f) {
        return -1.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

static int hat_bit_down(const efx_gp_slot *s, const efx_gp_element *e) {
    int hat = (e->index >> 4) & 0xf;
    int bit = e->index & 0xf;
    int xi = hat * 2;
    int yi = hat * 2 + 1;
    if (yi >= s->raw_hat_count) {
        return 0;
    }
    float x = s->raw_hats[xi];
    float y = s->raw_hats[yi];
    /* SDL hat encoding: 1=up, 2=right, 4=down, 8=left. Hat Y grows down. */
    switch (bit) {
    case 1:
        return y < -0.5f;
    case 2:
        return x > 0.5f;
    case 4:
        return y > 0.5f;
    case 8:
        return x < -0.5f;
    default:
        return 0;
    }
}

static float element_axis_value(const efx_gp_slot *s, const efx_gp_element *e,
                                int axis) {
    switch (e->kind) {
    case EFX_GP_ELEM_AXIS:
        if (e->index < s->raw_axis_count) {
            return s->raw_axes[e->index] * e->scale + e->offset;
        }
        return 0.0f;
    case EFX_GP_ELEM_BUTTON:
        if (e->index < s->raw_button_count) {
            return s->raw_buttons[e->index] ? 1.0f : 0.0f;
        }
        return 0.0f;
    case EFX_GP_ELEM_HAT:
        return hat_bit_down(s, e) ? 1.0f : 0.0f;
    default:
        (void)axis;
        return 0.0f;
    }
}

static int element_button_down(const efx_gp_slot *s, const efx_gp_element *e) {
    switch (e->kind) {
    case EFX_GP_ELEM_BUTTON:
        return (e->index < s->raw_button_count) ? s->raw_buttons[e->index] : 0;
    case EFX_GP_ELEM_AXIS:
        return element_axis_value(s, e, 0) >= EFX_GAMEPAD_TRIGGER_THRESHOLD;
    case EFX_GP_ELEM_HAT:
        return hat_bit_down(s, e);
    default:
        return 0;
    }
}

/* ------------------------------------------------------- bank process */

static void slot_clear_state(efx_gp_slot *s) {
    memset(s->down, 0, sizeof(s->down));
    memset(s->pressed, 0, sizeof(s->pressed));
    memset(s->released, 0, sizeof(s->released));
    memset(s->prev, 0, sizeof(s->prev));
    memset(s->axes, 0, sizeof(s->axes));
}

static void slot_process(int slot) {
    const efx_gp_slot *dev = &STAGE[slot];
    efx_gp_slot *s = &LIVE[slot];

    if (!dev->connected) {
        if (s->connected) {
            s->connected = 0;
            s->mapped = 0;
            slot_clear_state(s);
            if (disconnect_event_count < EFX_GAMEPAD_MAX) {
                DISCONNECT_EVENTS[disconnect_event_count++] = slot;
            }
        }
        return;
    }

    if (!s->connected) {
        s->connected = 1;
        memset(s->name, 0, sizeof(s->name));
        if (dev->name[0]) {
            copy_str(s->name, sizeof(s->name), dev->name);
        }
        memset(s->guid, 0, sizeof(s->guid));
        copy_str(s->guid, sizeof(s->guid), dev->guid);
        slot_clear_state(s);
        if (dev->normalized) {
            standard_init();
            s->mapping = STANDARD;
            s->mapped = dev->mapped_hint ? 1 : 1;
        } else {
            efx_gp_mapping *m = mapping_find(s->guid);
            if (m) {
                s->mapping = *m;
                s->mapped = 1;
            } else {
                mapping_init(&s->mapping);
                s->mapped = 0;
            }
        }
        if (connect_event_count < EFX_GAMEPAD_MAX) {
            CONNECT_EVENTS[connect_event_count++] = slot;
        }
    }

    s->raw_button_count = dev->raw_button_count;
    if (s->raw_button_count > EFX_GAMEPAD_RAW_BUTTON_MAX) {
        s->raw_button_count = EFX_GAMEPAD_RAW_BUTTON_MAX;
    }
    memset(s->raw_buttons, 0, sizeof(s->raw_buttons));
    if (s->raw_button_count > 0) {
        memcpy(s->raw_buttons, dev->raw_buttons, (size_t)s->raw_button_count);
    }
    s->raw_axis_count = dev->raw_axis_count;
    if (s->raw_axis_count > EFX_GAMEPAD_RAW_AXIS_MAX) {
        s->raw_axis_count = EFX_GAMEPAD_RAW_AXIS_MAX;
    }
    memset(s->raw_axes, 0, sizeof(s->raw_axes));
    for (int i = 0; i < s->raw_axis_count; i++) {
        s->raw_axes[i] = dev->raw_axes[i];
    }
    s->raw_hat_count = dev->raw_hat_count;
    if (s->raw_hat_count > EFX_GAMEPAD_RAW_HAT_MAX) {
        s->raw_hat_count = EFX_GAMEPAD_RAW_HAT_MAX;
    }
    memset(s->raw_hats, 0, sizeof(s->raw_hats));
    for (int i = 0; i < s->raw_hat_count; i++) {
        s->raw_hats[i] = dev->raw_hats[i];
    }

    for (int b = 0; b < EFX_GPB_COUNT; b++) {
        int level = element_button_down(s, &s->mapping.buttons[b]);
        s->pressed[b] = (unsigned char)(level && !s->prev[b]);
        s->released[b] = (unsigned char)(!level && s->prev[b]);
        s->down[b] = (unsigned char)level;
        s->prev[b] = (unsigned char)level;
    }
    for (int a = 0; a < EFX_GPA_COUNT; a++) {
        float v = element_axis_value(s, &s->mapping.axes[a], a);
        s->axes[a] = axis_range_clamp(a, v);
    }
}

/* ------------------------------------------------------------ lifecycle */

void efx_input_gamepad_poll(void) {
    connect_event_count = 0;
    disconnect_event_count = 0;

    if (SOURCE) {
        efx_gamepad_device devs[EFX_GAMEPAD_MAX];
        memset(devs, 0, sizeof(devs));
        int n = SOURCE(devs, EFX_GAMEPAD_MAX, SOURCE_UD);
        if (n >= 0) {
            if (n > EFX_GAMEPAD_MAX) {
                n = EFX_GAMEPAD_MAX;
            }
            for (int i = 0; i < EFX_GAMEPAD_MAX; i++) {
                memset(&STAGE[i], 0, sizeof(STAGE[i]));
            }
            for (int i = 0; i < n; i++) {
                STAGE[i].connected = devs[i].connected;
                STAGE[i].normalized = devs[i].normalized;
                STAGE[i].mapped_hint = devs[i].mapped_hint;
                if (devs[i].name[0]) {
                    copy_str(STAGE[i].name, sizeof(STAGE[i].name),
                             devs[i].name);
                }
                if (!devs[i].normalized && devs[i].guid[0]) {
                    copy_str(STAGE[i].guid, sizeof(STAGE[i].guid),
                             devs[i].guid);
                }
                STAGE[i].raw_button_count = devs[i].raw_button_count;
                if (STAGE[i].raw_button_count > EFX_GAMEPAD_RAW_BUTTON_MAX) {
                    STAGE[i].raw_button_count = EFX_GAMEPAD_RAW_BUTTON_MAX;
                }
                if (STAGE[i].raw_button_count > 0) {
                    memcpy(STAGE[i].raw_buttons, devs[i].raw_buttons,
                           (size_t)STAGE[i].raw_button_count);
                }
                STAGE[i].raw_axis_count = devs[i].raw_axis_count;
                if (STAGE[i].raw_axis_count > EFX_GAMEPAD_RAW_AXIS_MAX) {
                    STAGE[i].raw_axis_count = EFX_GAMEPAD_RAW_AXIS_MAX;
                }
                for (int a = 0; a < STAGE[i].raw_axis_count; a++) {
                    STAGE[i].raw_axes[a] = devs[i].raw_axes[a];
                }
                STAGE[i].raw_hat_count = devs[i].raw_hat_count;
                if (STAGE[i].raw_hat_count > EFX_GAMEPAD_RAW_HAT_MAX) {
                    STAGE[i].raw_hat_count = EFX_GAMEPAD_RAW_HAT_MAX;
                }
                for (int h = 0; h < STAGE[i].raw_hat_count; h++) {
                    STAGE[i].raw_hats[h] = devs[i].raw_hats[h];
                }
            }
            staged = 1;
        }
    }

    if (!staged) {
        return;
    }

    for (int slot = 0; slot < EFX_GAMEPAD_MAX; slot++) {
        slot_process(slot);
    }
    staged = 0;
}

void efx_input_gamepad_reset(void) {
    memset(LIVE, 0, sizeof(LIVE));
    memset(STAGE, 0, sizeof(STAGE));
    staged = 0;
    connect_event_count = 0;
    disconnect_event_count = 0;
}

void efx_input_gamepad_end_frame(void) {
    for (int slot = 0; slot < EFX_GAMEPAD_MAX; slot++) {
        memset(LIVE[slot].pressed, 0, sizeof(LIVE[slot].pressed));
        memset(LIVE[slot].released, 0, sizeof(LIVE[slot].released));
    }
    connect_event_count = 0;
    disconnect_event_count = 0;
}

/* -------------------------------------------------------------- queries */

void efx_input_gamepad_set_source(efx_gamepad_source_fn fn, void *ud) {
    SOURCE = fn;
    SOURCE_UD = ud;
}

int efx_input_gamepad_count(void) {
    int n = 0;
    for (int slot = 0; slot < EFX_GAMEPAD_MAX; slot++) {
        if (LIVE[slot].connected) {
            n++;
        }
    }
    return n;
}

int efx_input_gamepad_connected(int slot) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return 0;
    }
    return LIVE[slot].connected;
}

const char *efx_input_gamepad_name(int slot) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return NULL;
    }
    return LIVE[slot].name;
}

int efx_input_gamepad_mapped(int slot) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return 0;
    }
    return LIVE[slot].mapped;
}

int efx_input_gamepad_button_is_down(int slot, int button) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || button < 0 ||
        button >= EFX_GPB_COUNT) {
        return 0;
    }
    return LIVE[slot].down[button];
}

int efx_input_gamepad_button_is_pressed(int slot, int button) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || button < 0 ||
        button >= EFX_GPB_COUNT) {
        return 0;
    }
    return LIVE[slot].pressed[button];
}

int efx_input_gamepad_button_is_released(int slot, int button) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || button < 0 ||
        button >= EFX_GPB_COUNT) {
        return 0;
    }
    return LIVE[slot].released[button];
}

float efx_input_gamepad_axis(int slot, int axis) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || axis < 0 ||
        axis >= EFX_GPA_COUNT) {
        return 0.0f;
    }
    return LIVE[slot].axes[axis];
}

int efx_input_gamepad_raw_button(int slot, int index) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || index < 0 ||
        index >= LIVE[slot].raw_button_count) {
        return 0;
    }
    return LIVE[slot].raw_buttons[index];
}

float efx_input_gamepad_raw_axis(int slot, int index) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX || index < 0 ||
        index >= LIVE[slot].raw_axis_count) {
        return 0.0f;
    }
    return LIVE[slot].raw_axes[index];
}

int efx_input_gamepad_connect_count(void) {
    return connect_event_count;
}

int efx_input_gamepad_connect_at(int i) {
    if (i < 0 || i >= connect_event_count) {
        return -1;
    }
    return CONNECT_EVENTS[i];
}

int efx_input_gamepad_disconnect_count(void) {
    return disconnect_event_count;
}

int efx_input_gamepad_disconnect_at(int i) {
    if (i < 0 || i >= disconnect_event_count) {
        return -1;
    }
    return DISCONNECT_EVENTS[i];
}

/* -------------------------------------------------------------- injection */

void efx_input_gamepad_inject_connect(int slot, const char *name,
                                      const char *guid, int normalized) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return;
    }
    STAGE[slot].connected = 1;
    STAGE[slot].normalized = normalized ? 1 : 0;
    STAGE[slot].mapped_hint = 1;
    STAGE[slot].name[0] = '\0';
    if (name) {
        copy_str(STAGE[slot].name, sizeof(STAGE[slot].name), name);
    }
    memset(STAGE[slot].guid, 0, sizeof(STAGE[slot].guid));
    if (!normalized && guid) {
        copy_str(STAGE[slot].guid, sizeof(STAGE[slot].guid), guid);
    }
    staged = 1;
}

void efx_input_gamepad_inject_state(int slot, int raw_button_count,
                                    const unsigned char *raw_buttons,
                                    int raw_axis_count,
                                    const float *raw_axes) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return;
    }
    STAGE[slot].raw_button_count = raw_button_count;
    memset(STAGE[slot].raw_buttons, 0, sizeof(STAGE[slot].raw_buttons));
    if (raw_buttons && raw_button_count > 0) {
        int c = raw_button_count;
        if (c > EFX_GAMEPAD_RAW_BUTTON_MAX) {
            c = EFX_GAMEPAD_RAW_BUTTON_MAX;
        }
        memcpy(STAGE[slot].raw_buttons, raw_buttons, (size_t)c);
    }
    STAGE[slot].raw_axis_count = raw_axis_count;
    memset(STAGE[slot].raw_axes, 0, sizeof(STAGE[slot].raw_axes));
    if (raw_axes) {
        for (int i = 0; i < raw_axis_count && i < EFX_GAMEPAD_RAW_AXIS_MAX;
             i++) {
            STAGE[slot].raw_axes[i] = raw_axes[i];
        }
    }
    staged = 1;
}

void efx_input_gamepad_inject_hats(int slot, int raw_hat_count,
                                   const float *raw_hats) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return;
    }
    STAGE[slot].raw_hat_count = raw_hat_count;
    memset(STAGE[slot].raw_hats, 0, sizeof(STAGE[slot].raw_hats));
    if (raw_hats) {
        for (int i = 0; i < raw_hat_count && i < EFX_GAMEPAD_RAW_HAT_MAX; i++) {
            STAGE[slot].raw_hats[i] = raw_hats[i];
        }
    }
    staged = 1;
}

void efx_input_gamepad_inject_disconnect(int slot) {
    if (slot < 0 || slot >= EFX_GAMEPAD_MAX) {
        return;
    }
    STAGE[slot].connected = 0;
    staged = 1;
}

void efx_input_gamepad_inject_clear(void) {
    memset(STAGE, 0, sizeof(STAGE));
    staged = 0;
}
