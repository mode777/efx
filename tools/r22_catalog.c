/* R22 spike-only harness (throwaway branch, never merged): runs the error
 * catalog through the desktop quickjs binding headlessly — same output
 * contract as `player --script` (efx.log lines on stdout, exit 0) but with a
 * mock GPU sink, so it runs without a display. */
#include "render/render.h"
#include "resource/resource.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>

static void *mock_create(void *ud, int w, int h, const uint8_t *rgba,
                         int wrap, int filter, int mipmaps) {
    (void)ud; (void)rgba; (void)wrap; (void)filter; (void)mipmaps;
    return malloc((size_t)(w * h * 4 > 0 ? w * h * 4 : 1));
}

static void mock_destroy(void *ud, void *native) {
    (void)ud;
    free(native);
}

static const efx_render_sink g_sink = {
    NULL, mock_create, mock_destroy, NULL, NULL, NULL, NULL, NULL,
};

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: r22_catalog <script.js> <resource-root>\n");
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

    efx_render_install_sink(&g_sink);
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
