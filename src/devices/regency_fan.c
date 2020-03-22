/** @file
    Decoder for Regency fan remotes

    Copyright (C) 2020 David E. Tiller

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 */

/**
 The device uses OOK_PULSE_PCM_RZ encoding.
 The packet starts with either a narrow (500 uS) start pulse or a long (1500 uS) pulse.
 - 0 is defined as a 1500 uS gap followed by a  500 uS pulse.
 - 1 is defined as a 1000 uS gap followed by a 1000 uS pulse.
 - 2 is defined as a  500 uS gap followed by a 1500 uS pulse.

 Transmissions consist of a '1' length packet of 20 'trits' (trinary digits)
 followed by a '3' length packet of 20 trits. These two packets are repeated
 some number of times.
 The trits represent a rolling code that changes on each keypress, a fixed
 16 trit device ID,  3 id trits (key pressed), and a 1 trit button id.
 All of the data is obfuscated and a 1 length and a 3 length packet are
 required to successfully decode a transmission.
 */

#include <stdlib.h>
#include "decoder.h"

#define NUM_BITS        20
#define NUM_BYTES       3

#define CMD_CHAN_BYTE   0
#define VALUE_BYTE      1
#define SUM_BYTE        2

#define CMD_STOP        1
#define CMD_FAN_SPEED   2
#define CMD_LIGHT_INT   4
#define CMD_LIGHT_DELAY 5
#define CMD_FAN_DIR     6

static char *command_names[] = {
    /* 0  */ "invalid",
    /* 1  */ "fan_speed",
    /* 2  */ "fan_speed",
    /* 3  */ "invalid",
    /* 4  */ "light_intensity",
    /* 5  */ "light_delay",
    /* 6  */ "fan_direction",
    /* 7  */ "invalid",
    /* 8  */ "invalid",
    /* 9  */ "invalid",
    /* 10 */ "invalid",
    /* 11 */ "invalid",
    /* 12 */ "invalid",
    /* 13 */ "invalid",
    /* 14 */ "invalid",
    /* 15 */ "invalid"
};

static int regency_fan_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{

    data_t *data = NULL;
    int index = 0; // a row index
    uint8_t *b = NULL; // bits of a row
    int num_bits = 0;
    int debug_output = decoder->verbose;
    int return_code = 0;

    if (debug_output > 1) {
        bitbuffer_printf(bitbuffer, "%s: ", __func__);
    }

    if (bitbuffer->num_rows < 1) {
        if (debug_output > 1) {
            fprintf(stderr, "No rows.\n");
        }

        return 0;
    }

    bitbuffer_invert(bitbuffer);
    bitbuffer_print(bitbuffer);

    for (int index = 0; index < bitbuffer->num_rows; index++) {
        num_bits = bitbuffer->bits_per_row[index];

        if (num_bits != NUM_BITS) {
            if (debug_output > 1) {
                fprintf(stderr, "Expected %d bits, got %d.\n", NUM_BITS, num_bits);
            }

            continue;
        }

        uint8_t bytes[NUM_BYTES];
        bitbuffer_extract_bytes(bitbuffer, index, 0, bytes, NUM_BITS);
        reflect_bytes(bytes, NUM_BYTES);

        // Calculate nibble sum and compare
        int checksum = add_nibbles(bytes, 2) & 0x0f;
        if (checksum != bytes[SUM_BYTE]) {
            if (debug_output > 1) {
                fprintf(stderr, "Checksum failure: expected %0x, got %0x\n", bytes[SUM_BYTE], checksum);
            }

            continue;
        }

        /*
         * Now that message "envelope" has been validated, start parsing data.
         */
        uint8_t command = bytes[CMD_CHAN_BYTE] >> 4;
        uint8_t channel = ~bytes[CMD_CHAN_BYTE] & 0x0f;
	uint32_t value = bytes[VALUE_BYTE];
        char value_string[64] = {0};

        switch(command) {
            case CMD_STOP:
                sprintf(value_string, "stop");
                break;

            case CMD_FAN_SPEED:
                sprintf(value_string, "speed %d", value);
                break;

            case CMD_LIGHT_INT:
                sprintf(value_string, "%d %%", value);
                break;

            case CMD_LIGHT_DELAY:
                sprintf(value_string, "%s", value == 0 ? "off" : "on");
                break;

            case CMD_FAN_DIR:
                sprintf(value_string, "%s", value == 0x07 ? "clockwise" : "counter-clockwise");
                break;

            default:
                if (debug_output > 1) {
                    fprintf(stderr, "Unknown command: %d\n", command);
                    continue;
                }
                break;
        }

        return_code = 1;

        data = data_make(
        "model",            "",     DATA_STRING,    "Regency-compatible Remote",
        "type",             "",     DATA_STRING,    "Ceiling Fan",
        "channel",          "",     DATA_INT,       channel,
        "command",          "",     DATA_STRING,    command_names[command],
        "value",            "",     DATA_STRING,    value_string,
        "mic",              "",     DATA_STRING,    "nibble_sum",
        NULL);

        decoder_output_data(decoder, data);
    }

    // Return 1 if message successfully decoded
    return return_code;
}

/*
 * List of fields that may appear in the output
 *
 * Used to determine what fields will be output in what
 * order for this device when using -F csv.
 *
 */
static char *output_fields[] = {
    "model",
    "device_id",
    "device_id_hex",
    "counter",
    "counter_hex",
    "id_bits",
    "button_pressed",
    NULL,
};

r_device regency_fan = {
    .name        = "Regency Fan Remote (-f 303.96M)",
    .modulation  = OOK_PULSE_PPM,
    .short_width = 365,  // Narrow gap is really a 1
    .long_width  = 880,  // Wide gap is really a 0
    .reset_limit = 8000, // this is short enough so we only get 1 row.
    .decode_fn   = &regency_fan_decode,
    .disabled    = 1, // disabled and hidden, use 0 if there is a MIC, 1 otherwise
    .fields      = output_fields,
};
