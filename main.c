/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include <stdio.h>
#include <stdlib.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>


#define ENCODING_SOURCE_OUT MMAL_ENCODING_I420
#define ENCODING_ISP_OUT    MMAL_ENCODING_I420
#define WIDTH  3265
#define HEIGHT 4096
#define ZERO_COPY 0


#define _check_mmal(x) \
    do { \
        MMAL_STATUS_T status = (x); \
        if (status != MMAL_SUCCESS) { \
            fprintf(stderr, "%s:%d: %s: %s (0x%08x)\n", __FILE__, __LINE__, #x, mmal_status_to_string(status), status); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)


static MMAL_STATUS_T config_port(MMAL_PORT_T *port,
                                 const MMAL_FOURCC_T encoding,
                                 const int width, const int height)
{
    port->format->encoding = encoding;
    port->format->es->video.width  = VCOS_ALIGN_UP(width,  32);
    port->format->es->video.height = VCOS_ALIGN_UP(height, 16);
    port->format->es->video.crop.x = 0;
    port->format->es->video.crop.y = 0;
    port->format->es->video.crop.width  = width;
    port->format->es->video.crop.height = height;
    return mmal_port_format_commit(port);
}

static void callback_control(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    fprintf(stderr, "%s is called by %s\n", __func__, port->name);
    mmal_buffer_header_release(buffer);
}

int main()
{
    MMAL_COMPONENT_T *cp_source = NULL, *cp_isp = NULL, *cp_render = NULL;
    MMAL_CONNECTION_T *conn_source_isp = NULL, *conn_isp_render = NULL;
    MMAL_PARAMETER_VIDEO_SOURCE_PATTERN_T source_pattern = {
        .hdr =  {
            .id = MMAL_PARAMETER_VIDEO_SOURCE_PATTERN,
            .size = sizeof(source_pattern)
        },
        .pattern = MMAL_VIDEO_SOURCE_PATTERN_RANDOM
    };

    /* Setup the source component. */
    _check_mmal(mmal_component_create("vc.ril.source", &cp_source));
    _check_mmal(mmal_port_enable(cp_source->control, callback_control));
    _check_mmal(config_port(cp_source->output[0],
                            ENCODING_SOURCE_OUT, WIDTH, HEIGHT));
    _check_mmal(mmal_port_parameter_set(cp_source->output[0],
                                        &source_pattern.hdr));
    _check_mmal(mmal_port_parameter_set_boolean(cp_source->output[0],
                                                MMAL_PARAMETER_ZERO_COPY,
                                                ZERO_COPY));
    _check_mmal(mmal_component_enable(cp_source));

    /* Setup the isp component. */
    _check_mmal(mmal_component_create("vc.ril.isp", &cp_isp));
    _check_mmal(mmal_port_enable(cp_isp->control, callback_control));
    _check_mmal(config_port(cp_isp->input[0],
                            ENCODING_SOURCE_OUT, WIDTH, HEIGHT));
    _check_mmal(config_port(cp_isp->output[0],
                            ENCODING_ISP_OUT, WIDTH, HEIGHT));
    _check_mmal(mmal_port_parameter_set_boolean(cp_isp->input[0],
                                                MMAL_PARAMETER_ZERO_COPY,
                                                ZERO_COPY));
    _check_mmal(mmal_port_parameter_set_boolean(cp_isp->output[0],
                                                MMAL_PARAMETER_ZERO_COPY,
                                                ZERO_COPY));
    _check_mmal(mmal_component_enable(cp_isp));

    /* Setup the render component. */
    _check_mmal(mmal_component_create("vc.ril.video_render", &cp_render));
    _check_mmal(mmal_port_enable(cp_render->control, callback_control));
    _check_mmal(config_port(cp_render->input[0],
                            ENCODING_ISP_OUT, WIDTH, HEIGHT));
    _check_mmal(mmal_port_parameter_set_boolean(cp_render->input[0],
                                                MMAL_PARAMETER_ZERO_COPY,
                                                ZERO_COPY));
    _check_mmal(mmal_component_enable(cp_render));

    /* Connect isp[0] -- [0]render */
    _check_mmal(mmal_connection_create(&conn_source_isp,
                                       cp_source->output[0], cp_isp->input[0],
                                       MMAL_CONNECTION_FLAG_TUNNELLING));
    _check_mmal(mmal_connection_enable(conn_source_isp));

    /* Connect source[0] -- [0]isp. */
    _check_mmal(mmal_connection_create(&conn_isp_render,
                                       cp_isp->output[0], cp_render->input[0],
                                       MMAL_CONNECTION_FLAG_TUNNELLING));
    _check_mmal(mmal_connection_enable(conn_isp_render));

    sleep(5);

    return 0;
}
