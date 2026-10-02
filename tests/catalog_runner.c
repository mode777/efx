/* Headless desktop runner for the error catalog (ADR 0049 work): runs a
 * script through the real quickjs runtime + bindings with no GPU sink — the
 * `player --script` output contract (efx.log lines on stdout, exit 0) without
 * a display, so the catalog's byte-compare runs in EFX_HEADLESS builds too.
 * Resources are CPU-only (ADR 0052). Not a player replacement: no window,
 * hooks, or run modes. */
#include "render/render.h"
#include "resource/resource.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: efx_catalog_runner <script.js> <resource-root>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("script");
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *code = malloc((size_t)n + 1);
    if (!code || fread(code, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "read failed\n");
        return 2;
    }
    code[n] = '\0';
    fclose(f);

    efx_render_reset_state();
    efx_render_set_viewport(1024, 600);
    efx_render_begin_frame();
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return 2;
    }
    int err = EFX_RESOURCE_OK;
    efx_resource *res = efx_resource_open(argv[2], &err);
    if (!res) {
        fprintf(stderr, "resource root failed\n");
        return 2;
    }
    efx_runtime_set_resource(rt, res);
    int rc = efx_runtime_eval_string(rt, "catalog", code);
    efx_runtime_collect(rt);
    efx_runtime_destroy(rt);
    efx_render_end_frame();
    efx_render_shutdown();
    efx_resource_close(res);
    return rc == 0 ? 0 : 1;
}
