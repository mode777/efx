#ifndef EFX_RUNTIME_INTERNAL_H
#define EFX_RUNTIME_INTERNAL_H

#include "quickjs.h"
#include "resource/resource.h"

/* lifecycle hooks (ADR 0016): ordered registration, unsubscribe marks an
   entry inactive but keeps its JSValue until runtime teardown, so a hook can
   safely unsubscribe itself while it is being called */
struct efx_hook_entry {
    JSValue fn;
    int active;
};

struct efx_hook_list {
    struct efx_hook_entry *entries;
    int count;
    int cap;
};

struct efx_host_state {
    int quit_requested;
    int quit_code;
    JSValue quit_sentinel;
    char **args;
    int arg_count;
    /* F2 resource state */
    JSValue white_texture;
    int has_white_texture;
    /* lifecycle hook lists (F1 contract, ADR 0016) */
    struct efx_hook_list update_hooks;
    struct efx_hook_list render_hooks;
    /* F9 input event callback lists (same unsubscribe machinery) */
    struct efx_hook_list input_key_down;
    struct efx_hook_list input_key_up;
    struct efx_hook_list input_char;
    struct efx_hook_list input_mouse_down;
    struct efx_hook_list input_mouse_up;
    struct efx_hook_list input_mouse_move;
    struct efx_hook_list input_mouse_wheel;
    /* F6a resource root (owned by the player, not the runtime) */
    struct efx_resource *resource;
};

/* stable selector for the host callback lists (update/render + F9 input);
 * used by the shared registration/unsubscribe machinery and the dispatch */
#define EFX_HOOK_LIST_UPDATE 0
#define EFX_HOOK_LIST_RENDER 1
#define EFX_HOOK_LIST_KB_DOWN 2
#define EFX_HOOK_LIST_KB_UP 3
#define EFX_HOOK_LIST_KB_CHAR 4
#define EFX_HOOK_LIST_MOUSE_DOWN 5
#define EFX_HOOK_LIST_MOUSE_UP 6
#define EFX_HOOK_LIST_MOUSE_MOVE 7
#define EFX_HOOK_LIST_MOUSE_WHEEL 8
#define EFX_HOOK_LIST_COUNT 9

struct efx_hook_list *efx_host_hook_list(struct efx_host_state *h, int which);

/* append a duplicated reference; returns the stable entry index or -1 */
int efx_hooks_append(JSContext *ctx, struct efx_hook_list *list, JSValueConst fn);
int efx_hooks_active(const struct efx_hook_list *list);
void efx_hooks_free_all(JSContext *ctx, struct efx_hook_list *list);

#endif
