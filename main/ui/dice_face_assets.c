#include "dice_face_assets.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "png.h"

#define FACE_ASSET_MAX_CACHE 64
#define FACE_RENDER_MAX_CACHE 160
#define FACE_ASSET_MAX_DIMENSION 96
#define FACE_ASSET_PATH_MAX 127

typedef struct {
    bool used;
    char path[FACE_ASSET_PATH_MAX + 1];
    dice_face_bitmap_t bitmap;
} face_asset_cache_entry_t;

typedef struct {
    bool used;
    char path[FACE_ASSET_PATH_MAX + 1];
    dice_face_art_mode_t art_mode;
    uint32_t ink_color;
    uint16_t width;
    uint16_t height;
    lv_color32_t *pixels;
    size_t pixel_count;
    lv_image_dsc_t image_dsc;
} face_render_cache_entry_t;

static const char *TAG = "face_assets";
static face_asset_cache_entry_t s_cache[FACE_ASSET_MAX_CACHE];
static face_render_cache_entry_t s_render_cache[FACE_RENDER_MAX_CACHE];

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] |
        ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static int32_t read_le32s(const uint8_t *data)
{
    return (int32_t)read_le32(data);
}

static void *image_alloc(size_t size)
{
    void *memory = heap_caps_malloc(
        size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (memory == NULL) {
        memory = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }

    return memory;
}

static bool read_file(
    const char *path,
    uint8_t **out_data,
    size_t *out_size)
{
    *out_data = NULL;
    *out_size = 0;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGW(TAG, "Could not open %s", path);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return false;
    }

    rewind(file);

    uint8_t *buffer = image_alloc((size_t)size);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }

    size_t read_size = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    if (read_size != (size_t)size) {
        free(buffer);
        return false;
    }

    *out_data = buffer;
    *out_size = read_size;
    return true;
}


static bool decode_png(const uint8_t *data, size_t size, dice_face_bitmap_t *out_bitmap)
{
    if (size < 8 || png_sig_cmp((png_const_bytep)data, 0, 8) != 0) return false;
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, data, size)) return false;
    if (image.width == 0 || image.height == 0 || image.width > FACE_ASSET_MAX_DIMENSION || image.height > FACE_ASSET_MAX_DIMENSION) { png_image_free(&image); return false; }
    image.format = PNG_FORMAT_RGBA;
    size_t bytes = PNG_IMAGE_SIZE(image);
    uint8_t *rgba = image_alloc(bytes);
    if (!rgba) { png_image_free(&image); return false; }
    if (!png_image_finish_read(&image, NULL, rgba, 0, NULL)) { free(rgba); png_image_free(&image); return false; }
    out_bitmap->width = (uint16_t)image.width;
    out_bitmap->height = (uint16_t)image.height;
    out_bitmap->rgba = rgba;
    out_bitmap->rgba_size = bytes;
    png_image_free(&image);
    return true;
}

static bool decode_bmp(
    const uint8_t *data,
    size_t size,
    dice_face_bitmap_t *out_bitmap)
{
    if (size < 54 ||
        data[0] != 'B' ||
        data[1] != 'M') {
        ESP_LOGW(TAG, "BMP signature or file size is invalid");
        return false;
    }

    uint32_t declared_file_size = read_le32(&data[2]);
    uint32_t pixel_offset = read_le32(&data[10]);
    uint32_t dib_size = read_le32(&data[14]);

    /*
     * Explicitly support the common Windows BMP headers:
     * - BITMAPINFOHEADER: 40 bytes
     * - BITMAPV4HEADER:   108 bytes
     * - BITMAPV5HEADER:   124 bytes
     */
    if (!(dib_size == 40 ||
          dib_size == 108 ||
          dib_size == 124)) {
        ESP_LOGW(
            TAG,
            "Unsupported BMP DIB header size: %u",
            (unsigned)dib_size);
        return false;
    }

    if (size < 14u + dib_size ||
        pixel_offset >= size ||
        (declared_file_size != 0 &&
         declared_file_size > size)) {
        ESP_LOGW(
            TAG,
            "BMP offsets are invalid: file=%u pixel=%u actual=%u",
            (unsigned)declared_file_size,
            (unsigned)pixel_offset,
            (unsigned)size);
        return false;
    }

    int32_t width = read_le32s(&data[18]);
    int32_t height_raw = read_le32s(&data[22]);
    uint16_t planes = read_le16(&data[26]);
    uint16_t bpp = read_le16(&data[28]);
    uint32_t compression = read_le32(&data[30]);
    uint32_t colors_used = read_le32(&data[46]);

    if (planes != 1 ||
        width <= 0 ||
        height_raw == 0 ||
        width > FACE_ASSET_MAX_DIMENSION ||
        abs(height_raw) > FACE_ASSET_MAX_DIMENSION) {
        ESP_LOGW(
            TAG,
            "BMP dimensions are invalid: %ld x %ld planes=%u",
            (long)width,
            (long)height_raw,
            (unsigned)planes);
        return false;
    }

    /*
     * BI_RGB is expected for 1/4/8/24-bit BMPs.
     * BI_BITFIELDS is also accepted for 32-bit V4/V5 BMPs.
     */
    if (!(compression == 0 ||
          (compression == 3 && bpp == 32))) {
        ESP_LOGW(
            TAG,
            "Unsupported BMP compression=%u bpp=%u",
            (unsigned)compression,
            (unsigned)bpp);
        return false;
    }

    bool top_down = height_raw < 0;
    uint32_t height =
        (uint32_t)(top_down ? -height_raw : height_raw);
    uint32_t abs_width = (uint32_t)width;

    if (!(bpp == 1 ||
          bpp == 4 ||
          bpp == 8 ||
          bpp == 24 ||
          bpp == 32)) {
        ESP_LOGW(
            TAG,
            "Unsupported BMP bit depth: %u",
            (unsigned)bpp);
        return false;
    }

    uint32_t palette_entries = 0;

    if (bpp <= 8) {
        palette_entries =
            colors_used != 0
                ? colors_used
                : (1u << bpp);

        uint32_t maximum_entries = 1u << bpp;
        if (palette_entries == 0 ||
            palette_entries > maximum_entries) {
            ESP_LOGW(
                TAG,
                "Invalid BMP palette size: %u",
                (unsigned)palette_entries);
            return false;
        }
    }

    size_t palette_size =
        (size_t)palette_entries * 4u;
    size_t minimum_header_end =
        14u + dib_size;
    size_t palette_offset = minimum_header_end;

    /*
     * V4/V5 writers may place optional profile or mask data between the DIB
     * header and pixel array. For indexed images, the palette is the final
     * palette_size bytes before the declared pixel offset.
     */
    if (palette_entries > 0) {
        if ((size_t)pixel_offset <
            minimum_header_end + palette_size) {
            ESP_LOGW(
                TAG,
                "BMP palette overlaps header or pixel data");
            return false;
        }

        palette_offset =
            (size_t)pixel_offset - palette_size;

        if (palette_offset < minimum_header_end ||
            palette_offset + palette_size > size) {
            ESP_LOGW(
                TAG,
                "BMP palette offset is invalid");
            return false;
        }
    }

    uint32_t row_size =
        ((abs_width * bpp + 31u) / 32u) * 4u;
    size_t pixel_bytes =
        (size_t)row_size * height;

    if ((size_t)pixel_offset + pixel_bytes > size) {
        ESP_LOGW(
            TAG,
            "BMP pixel data is truncated: need=%u have=%u",
            (unsigned)((size_t)pixel_offset + pixel_bytes),
            (unsigned)size);
        return false;
    }

    size_t out_size =
        (size_t)abs_width * height * 4u;
    uint8_t *rgba = image_alloc(out_size);

    if (rgba == NULL) {
        ESP_LOGW(
            TAG,
            "Could not allocate %u bytes for BMP decode",
            (unsigned)out_size);
        return false;
    }

    memset(rgba, 0, out_size);

    for (uint32_t y = 0;
         y < height;
         ++y) {
        uint32_t source_y =
            top_down ? y : (height - 1u - y);
        const uint8_t *row =
            data + pixel_offset + source_y * row_size;

        for (uint32_t x = 0;
             x < abs_width;
             ++x) {
            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            uint8_t a = 255;

            if (bpp == 1 ||
                bpp == 4 ||
                bpp == 8) {
                uint32_t palette_index = 0;

                if (bpp == 1) {
                    uint8_t byte =
                        row[x / 8u];
                    palette_index =
                        (byte >>
                         (7u - (x % 8u))) &
                        0x01u;
                } else if (bpp == 4) {
                    uint8_t byte =
                        row[x / 2u];
                    palette_index =
                        (x % 2u == 0)
                            ? ((byte >> 4) & 0x0Fu)
                            : (byte & 0x0Fu);
                } else {
                    palette_index = row[x];
                }

                if (palette_index >=
                    palette_entries) {
                    a = 0;
                } else {
                    const uint8_t *entry =
                        data +
                        palette_offset +
                        palette_index * 4u;

                    b = entry[0];
                    g = entry[1];
                    r = entry[2];
                    a = 255;
                }
            } else if (bpp == 24) {
                const uint8_t *pixel =
                    row + x * 3u;

                b = pixel[0];
                g = pixel[1];
                r = pixel[2];
            } else {
                const uint8_t *pixel =
                    row + x * 4u;

                b = pixel[0];
                g = pixel[1];
                r = pixel[2];

                /*
                 * Many BI_RGB 32-bit BMP writers leave alpha zero even though
                 * the image is fully opaque. Treat zero alpha as opaque.
                 */
                a = pixel[3] == 0
                    ? 255
                    : pixel[3];
            }

            uint8_t *target =
                rgba +
                ((y * abs_width + x) * 4u);

            target[0] = r;
            target[1] = g;
            target[2] = b;
            target[3] = a;
        }
    }

    out_bitmap->width =
        (uint16_t)abs_width;
    out_bitmap->height =
        (uint16_t)height;
    out_bitmap->rgba = rgba;
    out_bitmap->rgba_size =
        out_size;

    ESP_LOGI(
        TAG,
        "Decoded BMP %ux%u, %u-bit, DIB=%u",
        (unsigned)abs_width,
        (unsigned)height,
        (unsigned)bpp,
        (unsigned)dib_size);

    return true;
}

static face_asset_cache_entry_t *find_or_allocate_entry(const char *path)
{
    face_asset_cache_entry_t *empty = NULL;

    for (size_t index = 0; index < FACE_ASSET_MAX_CACHE; ++index) {
        if (s_cache[index].used) {
            if (strcmp(s_cache[index].path, path) == 0) {
                return &s_cache[index];
            }
        } else if (empty == NULL) {
            empty = &s_cache[index];
        }
    }

    return empty;
}

bool dice_face_assets_get(
    const char *absolute_path,
    const dice_face_bitmap_t **out_bitmap)
{
    if (absolute_path == NULL || out_bitmap == NULL) {
        return false;
    }

    *out_bitmap = NULL;

    face_asset_cache_entry_t *entry =
        find_or_allocate_entry(absolute_path);

    if (entry == NULL) {
        ESP_LOGW(TAG, "Asset cache full; could not load %s", absolute_path);
        return false;
    }

    if (entry->used) {
        *out_bitmap = &entry->bitmap;
        return true;
    }

    uint8_t *file_data = NULL;
    size_t file_size = 0;

    if (!read_file(absolute_path, &file_data, &file_size)) {
        return false;
    }

    dice_face_bitmap_t bitmap = {0};
    bool ok = decode_png(file_data, file_size, &bitmap);
    if (!ok) ok = decode_bmp(file_data, file_size, &bitmap);
    free(file_data);

    if (!ok) {
        ESP_LOGW(TAG, "PNG/BMP decode failed for %s", absolute_path);
        return false;
    }

    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    strncpy(entry->path, absolute_path, sizeof(entry->path) - 1);
    entry->bitmap = bitmap;
    *out_bitmap = &entry->bitmap;
    return true;
}


static face_render_cache_entry_t *find_render_entry(
    const char *absolute_path,
    dice_face_art_mode_t art_mode,
    uint32_t ink_color,
    int target_width,
    int target_height)
{
    for (size_t index = 0; index < FACE_RENDER_MAX_CACHE; ++index) {
        face_render_cache_entry_t *entry = &s_render_cache[index];

        if (!entry->used) {
            continue;
        }

        if (entry->art_mode == art_mode &&
            entry->ink_color == ink_color &&
            entry->width == (uint16_t)target_width &&
            entry->height == (uint16_t)target_height &&
            strcmp(entry->path, absolute_path) == 0) {
            return entry;
        }
    }

    return NULL;
}

static face_render_cache_entry_t *allocate_render_entry(void)
{
    for (size_t index = 0; index < FACE_RENDER_MAX_CACHE; ++index) {
        if (!s_render_cache[index].used) {
            return &s_render_cache[index];
        }
    }

    face_render_cache_entry_t *entry = &s_render_cache[0];
    if (entry->pixels != NULL) {
        free(entry->pixels);
    }
    memset(entry, 0, sizeof(*entry));
    return entry;
}

bool dice_face_assets_get_rendered(
    const char *absolute_path,
    dice_face_art_mode_t art_mode,
    uint32_t ink_color,
    int target_width,
    int target_height,
    const lv_image_dsc_t **out_image)
{
    if (absolute_path == NULL ||
        out_image == NULL ||
        target_width <= 0 ||
        target_height <= 0) {
        return false;
    }

    face_render_cache_entry_t *existing = find_render_entry(
        absolute_path,
        art_mode,
        ink_color,
        target_width,
        target_height);

    if (existing != NULL) {
        *out_image = &existing->image_dsc;
        return true;
    }

    const dice_face_bitmap_t *bitmap = NULL;
    if (!dice_face_assets_get(absolute_path, &bitmap) || bitmap == NULL) {
        return false;
    }

    face_render_cache_entry_t *entry = allocate_render_entry();
    if (entry == NULL) {
        return false;
    }

    size_t pixel_count =
        (size_t)target_width * (size_t)target_height;
    size_t byte_count =
        pixel_count * sizeof(lv_color32_t);

    lv_color32_t *pixels = image_alloc(byte_count);
    if (pixels == NULL) {
        ESP_LOGW(
            TAG,
            "Could not allocate %u bytes for rendered face asset",
            (unsigned)byte_count);
        return false;
    }

    if (!dice_face_assets_render(
            bitmap,
            art_mode,
            ink_color,
            target_width,
            target_height,
            pixels)) {
        free(pixels);
        return false;
    }

    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    snprintf(entry->path, sizeof(entry->path), "%s", absolute_path);
    entry->art_mode = art_mode;
    entry->ink_color = ink_color;
    entry->width = (uint16_t)target_width;
    entry->height = (uint16_t)target_height;
    entry->pixels = pixels;
    entry->pixel_count = pixel_count;
    entry->image_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    entry->image_dsc.header.w = target_width;
    entry->image_dsc.header.h = target_height;
    entry->image_dsc.header.stride =
        target_width * (int)sizeof(lv_color32_t);
    entry->image_dsc.data_size = (uint32_t)byte_count;
    entry->image_dsc.data = (const uint8_t *)pixels;

    ESP_LOGI(
        TAG,
        "Cached rendered face image: %s %dx%d mode=%s",
        absolute_path,
        target_width,
        target_height,
        art_mode == DICE_FACE_ART_INDEXED ? "indexed" : "mask");

    *out_image = &entry->image_dsc;
    return true;
}

void dice_face_assets_preload_set(
    const dice_set_definition_t *set)
{
    /*
     * The device is fully powered off while hibernating, so PSRAM cannot
     * preserve the rendered cache. Preload only the full-size face used for
     * pools of up to 18 dice. Smaller variants are rendered on demand.
     */
    static const int sizes[] = {48};
    size_t requested_renders = 0;

    if (set == NULL) {
        return;
    }

    for (size_t die_index = 0; die_index < set->die_count; ++die_index) {
        const dice_set_die_t *die = &set->dice[die_index];

        for (size_t face_index = 0; face_index < die->face_count; ++face_index) {
            const dice_set_face_t *face = &die->faces[face_index];

            if (face->display_mode != DICE_FACE_DISPLAY_IMAGE ||
                face->image_path[0] == '\0') {
                continue;
            }

            char absolute_path[DICE_SET_PATH_MAX * 2 + 4];
            int written = snprintf(
                absolute_path,
                sizeof(absolute_path),
                "%s/%s",
                set->folder_path,
                face->image_path);

            if (written <= 0 ||
                written >= (int)sizeof(absolute_path)) {
                continue;
            }

            dice_face_art_mode_t effective_mode =
                face->art_mode == DICE_FACE_ART_INDEXED
                    ? DICE_FACE_ART_INDEXED
                    : DICE_FACE_ART_MASK;

            for (size_t size_index = 0;
                 size_index < sizeof(sizes) / sizeof(sizes[0]);
                 ++size_index) {
                const lv_image_dsc_t *unused = NULL;
                ++requested_renders;
                (void)dice_face_assets_get_rendered(
                    absolute_path,
                    effective_mode,
                    face->ink_color,
                    sizes[size_index],
                    sizes[size_index],
                    &unused);
            }
        }
    }

    ESP_LOGI(
        TAG,
        "Preloaded face assets for set '%s' (%u render requests; source cache %u, render cache %u)",
        set->name,
        (unsigned)requested_renders,
        (unsigned)FACE_ASSET_MAX_CACHE,
        (unsigned)FACE_RENDER_MAX_CACHE);
}


void dice_face_assets_release_all(void)
{
    for (size_t index = 0; index < FACE_ASSET_MAX_CACHE; ++index) {
        if (s_cache[index].used && s_cache[index].bitmap.rgba != NULL) {
            free(s_cache[index].bitmap.rgba);
        }
        memset(&s_cache[index], 0, sizeof(s_cache[index]));
    }

    for (size_t index = 0; index < FACE_RENDER_MAX_CACHE; ++index) {
        if (s_render_cache[index].used && s_render_cache[index].pixels != NULL) {
            free(s_render_cache[index].pixels);
        }
        memset(&s_render_cache[index], 0, sizeof(s_render_cache[index]));
    }
}


static inline uint8_t clamp_u8_from_int(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

typedef struct {
    float r;
    float g;
    float b;
    float a;
} rgba_float_sample_t;

static rgba_float_sample_t sample_bilinear(
    const dice_face_bitmap_t *bitmap,
    float source_x,
    float source_y)
{
    rgba_float_sample_t sample = {0};

    if (bitmap == NULL ||
        bitmap->rgba == NULL ||
        bitmap->width == 0 ||
        bitmap->height == 0) {
        return sample;
    }

    if (source_x < 0.0f) {
        source_x = 0.0f;
    }
    if (source_y < 0.0f) {
        source_y = 0.0f;
    }
    if (source_x > (float)(bitmap->width - 1)) {
        source_x = (float)(bitmap->width - 1);
    }
    if (source_y > (float)(bitmap->height - 1)) {
        source_y = (float)(bitmap->height - 1);
    }

    uint32_t x0 = (uint32_t)source_x;
    uint32_t y0 = (uint32_t)source_y;
    uint32_t x1 =
        x0 + 1u < bitmap->width
            ? x0 + 1u
            : x0;
    uint32_t y1 =
        y0 + 1u < bitmap->height
            ? y0 + 1u
            : y0;

    float fx = source_x - (float)x0;
    float fy = source_y - (float)y0;

    const uint8_t *p00 =
        bitmap->rgba + ((y0 * bitmap->width + x0) * 4u);
    const uint8_t *p10 =
        bitmap->rgba + ((y0 * bitmap->width + x1) * 4u);
    const uint8_t *p01 =
        bitmap->rgba + ((y1 * bitmap->width + x0) * 4u);
    const uint8_t *p11 =
        bitmap->rgba + ((y1 * bitmap->width + x1) * 4u);

    float w00 = (1.0f - fx) * (1.0f - fy);
    float w10 = fx * (1.0f - fy);
    float w01 = (1.0f - fx) * fy;
    float w11 = fx * fy;

    sample.r =
        p00[0] * w00 +
        p10[0] * w10 +
        p01[0] * w01 +
        p11[0] * w11;
    sample.g =
        p00[1] * w00 +
        p10[1] * w10 +
        p01[1] * w01 +
        p11[1] * w11;
    sample.b =
        p00[2] * w00 +
        p10[2] * w10 +
        p01[2] * w01 +
        p11[2] * w11;
    sample.a =
        p00[3] * w00 +
        p10[3] * w10 +
        p01[3] * w01 +
        p11[3] * w11;

    return sample;
}

static inline lv_color32_t make_lv_color32(
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a)
{
    lv_color32_t color;
    color.red = r;
    color.green = g;
    color.blue = b;
    color.alpha = a;
    return color;
}





bool dice_face_assets_render(
    const dice_face_bitmap_t *bitmap,
    dice_face_art_mode_t art_mode,
    uint32_t ink_color,
    int target_width,
    int target_height,
    lv_color32_t *out_pixels)
{
    if (bitmap == NULL ||
        out_pixels == NULL ||
        target_width <= 0 ||
        target_height <= 0) {
        return false;
    }

    uint8_t ink_r = (uint8_t)((ink_color >> 16) & 0xFFu);
    uint8_t ink_g = (uint8_t)((ink_color >> 8) & 0xFFu);
    uint8_t ink_b = (uint8_t)(ink_color & 0xFFu);

    for (int y = 0; y < target_height; ++y) {
        float source_y =
            ((float)y + 0.5f) *
                (float)bitmap->height /
                (float)target_height -
            0.5f;

        for (int x = 0; x < target_width; ++x) {
            float source_x =
                ((float)x + 0.5f) *
                    (float)bitmap->width /
                    (float)target_width -
                0.5f;

            rgba_float_sample_t sample =
                sample_bilinear(
                    bitmap,
                    source_x,
                    source_y);

            lv_color32_t output =
                make_lv_color32(0, 0, 0, 0);

            if (art_mode == DICE_FACE_ART_MASK) {
                /*
                 * Mask PNGs exported by Spiffy Roller Set Maker use the PNG
                 * alpha channel to distinguish ink from transparency. The RGB
                 * color of an opaque mask pixel is not meaningful and may be
                 * black, white, or any editor preview color. Using luminance
                 * here made opaque black mask pixels disappear completely.
                 */
                int alpha_u8 =
                    clamp_u8_from_int(
                        (int)(sample.a + 0.5f));

                if (alpha_u8 > 0) {
                    output = make_lv_color32(
                        ink_r,
                        ink_g,
                        ink_b,
                        alpha_u8);
                }
            } else {
                /*
                 * Indexed artwork may legitimately contain black palette
                 * pixels. Only the PNG alpha channel denotes transparency.
                 */
                bool transparent = sample.a <= 1.0f;

                if (!transparent) {
                    output = make_lv_color32(
                        clamp_u8_from_int(
                            (int)(sample.r + 0.5f)),
                        clamp_u8_from_int(
                            (int)(sample.g + 0.5f)),
                        clamp_u8_from_int(
                            (int)(sample.b + 0.5f)),
                        clamp_u8_from_int(
                            (int)(sample.a + 0.5f)));
                }
            }

            out_pixels[y * target_width + x] = output;
        }
    }

    return true;
}
