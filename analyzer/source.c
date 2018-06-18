/*

  Copyright (C) 2017 Gonzalo José Carracedo Carballal

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>

*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <libgen.h>

#define SU_LOG_DOMAIN "source"
#include <confdb.h>
#include "source.h"

#ifdef _SU_SINGLE_PRECISION
#  define sf_read sf_read_float
#  define SUSCAN_SOAPY_SAMPFMT SOAPY_SDR_CF32
#else
#  define sf_read sf_read_double
#  define SUSCAN_SOAPY_SAMPFMT SOAPY_SDR_CF64
#endif

#ifdef bool /* Someone is using this for whatever reason */
#  undef bool
#endif /* bool */

/***************************** Source Config API *****************************/
SUPRIVATE void
suscan_source_float_keyval_destroy(struct suscan_source_float_keyval *kv)
{
  if (kv->key != NULL)
    free(kv->key);

  free(kv);
}

SUPRIVATE struct suscan_source_float_keyval *
suscan_source_float_keyval_new(const char *key, SUFLOAT val)
{
  struct suscan_source_float_keyval *new = NULL;

  SU_TRYCATCH(
      new = malloc(sizeof(struct suscan_source_float_keyval)),
      goto fail);

  SU_TRYCATCH(new->key = strdup(key), goto fail);

  return new;

fail:
  if (new != NULL)
    suscan_source_float_keyval_destroy(new);

  return NULL;
}

void
suscan_source_config_destroy(suscan_source_config_t *config)
{
  unsigned int i;

  if (config->label != NULL)
    free(config->label);

  if (config->path != NULL)
    free(config->path);

  if (config->soapy_args != NULL) {
    SoapySDRKwargs_clear(config->soapy_args);
    free(config->soapy_args);
  }

  if (config->antenna != NULL)
    free(config->antenna);

  for (i = 0; i < config->gain_count; ++i)
    if (config->gain_list[i] != NULL)
      suscan_source_float_keyval_destroy(config->gain_list[i]);

  if (config->gain_list != NULL)
    free(config->gain_list);

  free(config);
}

/* Getters & Setters */
SUBOOL
suscan_source_config_set_label(suscan_source_config_t *config, const char *label)
{
  char *dup = NULL;

  if (label != NULL)
    SU_TRYCATCH(dup = strdup(label), return SU_FALSE);

  if (config->label != NULL)
    free(config->label);

  config->label = dup;

  return SU_TRUE;
}

const char *
suscan_source_config_get_label(const suscan_source_config_t *config)
{
  if (config->label != NULL)
    return config->label;
  else
    return "Unlabeled source";
}

SUFLOAT
suscan_source_config_get_freq(const suscan_source_config_t *config)
{
  return config->freq;
}

void
suscan_source_config_set_freq(suscan_source_config_t *config, SUFLOAT freq)
{
  config->freq = freq;
}

SUFLOAT
suscan_source_config_get_bandwidth(const suscan_source_config_t *config)
{
  return config->bandwidth;
}

void
suscan_source_config_set_bandwidth(
    suscan_source_config_t *config,
    SUFLOAT bandwidth)
{
  config->bandwidth = bandwidth;
}

SUBOOL
suscan_source_config_get_iq_balance(const suscan_source_config_t *config)
{
  return config->iq_balance;
}

void
suscan_source_config_set_iq_balance(
    suscan_source_config_t *config,
    SUBOOL iq_balance)
{
  config->iq_balance = iq_balance;
}

SUBOOL
suscan_source_config_get_dc_remove(const suscan_source_config_t *config)
{
  return config->dc_remove;
}

void
suscan_source_config_set_dc_remove(
    suscan_source_config_t *config,
    SUBOOL dc_remove)
{
  config->dc_remove = dc_remove;
}

SUBOOL
suscan_source_config_get_loop(const suscan_source_config_t *config)
{
  return config->loop;
}

void
suscan_source_config_set_loop(suscan_source_config_t *config, SUBOOL loop)
{
  config->loop = loop;
}

const char *
suscan_source_config_get_path(const suscan_source_config_t *config)
{
  return config->path;
}

SUBOOL
suscan_source_config_set_path(suscan_source_config_t *config, const char *path)
{
  char *dup = NULL;

  if (path != NULL)
    SU_TRYCATCH(dup = strdup(path), return SU_FALSE);

  if (config->path != NULL)
    free(config->path);

  config->path = dup;

  return SU_TRUE;
}

const char *
suscan_source_config_get_antenna(const suscan_source_config_t *config)
{
  return config->antenna;
}

SUBOOL
suscan_source_config_set_antenna(
    suscan_source_config_t *config,
    const char *antenna)
{
  char *dup = NULL;

  if (antenna != NULL)
    SU_TRYCATCH(dup = strdup(antenna), return SU_FALSE);

  if (config->antenna != NULL)
    free(config->antenna);

  config->antenna = dup;

  return SU_TRUE;
}

unsigned int
suscan_source_config_get_samp_rate(const suscan_source_config_t *config)
{
  return config->samp_rate;
}

void
suscan_source_config_set_samp_rate(
    suscan_source_config_t *config,
    unsigned int samp_rate)
{
  config->samp_rate = samp_rate;
}

unsigned int
suscan_source_config_get_average(const suscan_source_config_t *config)
{
  return config->average;
}

SUBOOL
suscan_source_config_set_average(
    suscan_source_config_t *config,
    unsigned int average)
{
  if (average < 1) {
    SU_ERROR("Cannot set average to less than 1\n");
    return SU_FALSE;
  }

  config->average = average;

  return SU_TRUE;
}

unsigned int
suscan_source_config_get_channel(const suscan_source_config_t *config)
{
  return config->channel;
}

void
suscan_source_config_set_channel(
    suscan_source_config_t *config,
    unsigned int channel)
{
  config->channel = channel;
}

struct suscan_source_float_keyval *
suscan_source_config_lookup_gain(
    const suscan_source_config_t *config,
    const char *name)
{
  unsigned int i;

  for (i = 0; i < config->gain_count; ++i)
    if (strcmp(config->gain_list[i]->key, name) == 0)
      return config->gain_list[i];

  return NULL;
}

struct suscan_source_float_keyval *
suscan_source_config_assert_gain(
    suscan_source_config_t *config,
    const char *name)
{
  struct suscan_source_float_keyval *gain;

  if ((gain = suscan_source_config_lookup_gain(config, name)) != NULL)
    return gain;

  SU_TRYCATCH(gain = suscan_source_float_keyval_new(name, 0), goto fail);

  SU_TRYCATCH(PTR_LIST_APPEND_CHECK(config->gain, gain) != -1, goto fail);

  return gain;

fail:
  if (gain != NULL)
    suscan_source_float_keyval_destroy(gain);

  return NULL;
}

SUFLOAT
suscan_source_config_get_gain(
    const suscan_source_config_t *config,
    const char *name)
{
  struct suscan_source_float_keyval *gain;

  if ((gain = suscan_source_config_lookup_gain(config, name)) == NULL)
    return 0;

  return gain->val;
}

SUBOOL
suscan_source_config_set_gain(
    const suscan_source_config_t *config,
    const char *name,
    SUFLOAT value)
{
  struct suscan_source_float_keyval *gain;

  if ((gain = suscan_source_config_lookup_gain(config, name)) == NULL)
    return SU_FALSE;

  gain->val = value;

  return SU_TRUE;
}

char *
suscan_source_config_get_sdr_args(const suscan_source_config_t *config)
{
  return SoapySDRKwargs_toString(config->soapy_args);
}

SUBOOL
suscan_source_config_set_sdr_args(
    const suscan_source_config_t *config,
    const char *args)
{
  SoapySDRKwargs kwargs;

  /* Anything is possible? There should be a check somewhere here */
  kwargs = SoapySDRKwargs_fromString(args);

  SoapySDRKwargs_clear(config->soapy_args);

  *config->soapy_args = kwargs;

  return SU_TRUE;
}

suscan_source_config_t *
suscan_source_config_new(
    enum suscan_source_type type,
    enum suscan_source_format format)
{
  suscan_source_config_t *new = NULL;

  SU_TRYCATCH(new = calloc(1, sizeof(suscan_source_config_t)), goto fail);

  new->type = type;
  new->format = format;
  new->average = 1;

  SU_TRYCATCH(new->soapy_args = calloc(1, sizeof(SoapySDRKwargs)), goto fail);

  return new;

fail:
  if (new != NULL)
    suscan_source_config_destroy(new);

  return NULL;
}

suscan_source_config_t *
suscan_source_config_clone(const suscan_source_config_t *config)
{
  suscan_source_config_t *new = NULL;
  unsigned int i;

  SU_TRYCATCH(
      new = suscan_source_config_new(config->type, config->format),
      goto fail);

  SU_TRYCATCH(suscan_source_config_set_label(new, config->label), goto fail);
  SU_TRYCATCH(suscan_source_config_set_path(new, config->path), goto fail);
  SU_TRYCATCH(
        suscan_source_config_set_antenna(new, config->antenna),
        goto fail);

  for (i = 0; i < config->gain_count; ++i)
    SU_TRYCATCH(
        suscan_source_config_set_gain(
            new,
            config->gain_list[i]->key,
            config->gain_list[i]->val),
        goto fail);

  new->freq = config->freq;
  new->bandwidth = config->bandwidth;
  new->iq_balance = config->iq_balance;
  new->dc_remove = config->dc_remove;
  new->samp_rate = config->samp_rate;
  new->average = config->average;
  new->channel = config->channel;

  return new;

fail:
  if (new != NULL)
    suscan_source_config_destroy(new);

  return NULL;
}

SUPRIVATE const char *
suscan_source_config_helper_type_to_str(enum suscan_source_type type)
{
  switch (type) {
    case SUSCAN_SOURCE_TYPE_FILE:
      return "FILE";

    case SUSCAN_SOURCE_TYPE_SDR:
      return "SDR";
  }

  return NULL;
}

SUPRIVATE const char *
suscan_source_config_helper_format_to_str(enum suscan_source_format type)
{
  switch (type) {
    case SUSCAN_SOURCE_FORMAT_AUTO:
      return "AUTO";

    case SUSCAN_SOURCE_FORMAT_RAW:
      return "RAW";

    case SUSCAN_SOURCE_FORMAT_WAV:
      return "WAV";
  }

  return NULL;
}

suscan_object_t *
suscan_source_config_to_object(const suscan_source_config_t *cfg)
{
  suscan_object_t *new = NULL;
  const char *tmp;

#define SU_CFGSAVE(kind, field)                                 \
    SU_TRYCATCH(                                                \
        JOIN(suscan_object_set_field_, kind)(                   \
            new,                                                \
            STRINGIFY(field),                                   \
            cfg->field),                                        \
        goto fail)

  SU_TRYCATCH(new = suscan_object_new(SUSCAN_OBJECT_TYPE_OBJECT), goto fail);

  SU_TRYCATCH(
      tmp = suscan_source_config_helper_type_to_str(cfg->type),
      goto fail);

  SU_TRYCATCH(suscan_object_set_field_value(new, "type", tmp), goto fail);

  if (cfg->type == SUSCAN_SOURCE_TYPE_FILE) {
    SU_TRYCATCH(
          tmp = suscan_source_config_helper_format_to_str(cfg->format),
          goto fail);
    SU_TRYCATCH(suscan_object_set_field_value(new, "format", tmp), goto fail);
  }

  if (cfg->label != NULL)
    SU_CFGSAVE(value, path);

  if (cfg->path != NULL)
    SU_CFGSAVE(value, path);

  if (cfg->antenna != NULL)
    SU_CFGSAVE(value, antenna);

  SU_CFGSAVE(value, label);
  SU_CFGSAVE(float, freq);
  SU_CFGSAVE(float, bandwidth);
  SU_CFGSAVE(bool,  iq_balance);
  SU_CFGSAVE(bool,  dc_remove);
  SU_CFGSAVE(uint,  samp_rate);
  SU_CFGSAVE(uint,  average);
  SU_CFGSAVE(uint,  channel);

  return new;

fail:
  if (new != NULL)
    suscan_object_destroy(new);

  return NULL;
}

suscan_source_config_t *
suscan_source_config_from_object(const suscan_object_t *object)
{
  return NULL;
}

/****************************** Source API ***********************************/
void
suscan_source_destroy(suscan_source_t *source)
{
  if (source->sf != NULL)
    sf_close(source->sf);

  if (source->rx_stream != NULL)
    SoapySDRDevice_closeStream(source->sdr, source->rx_stream);

  if (source->sdr != NULL)
    SoapySDRDevice_unmake(source->sdr);

  if (source->config != NULL)
    suscan_source_config_destroy(source->config);

  free(source);
}

SUPRIVATE SUBOOL
suscan_source_open_file(suscan_source_t *source)
{
  if (source->config->path == NULL) {
    SU_ERROR("Cannot open file source: path not set\n");
    return SU_FALSE;
  }

  switch (source->config->format) {
    case SUSCAN_SOURCE_FORMAT_WAV:
    case SUSCAN_SOURCE_FORMAT_AUTO:
      /* Autodetect: open as wav and, if failed, attempt to open as raw */
      source->sf_info.format = 0;
      if ((source->sf = sf_open(
          source->config->path,
          SFM_READ,
          &source->sf_info)) != NULL) {
        source->config->samp_rate = source->sf_info.samplerate;
        SU_INFO(
            "Audio file source opened, sample rate = %d\n",
            source->config->samp_rate);
        break;
      } else if (source->config->format == SUSCAN_SOURCE_FORMAT_WAV) {
        SU_ERROR(
            "Failed to open %s as audio file: %s\n",
            source->config->path,
            sf_strerror(NULL));
        return SU_FALSE;
      } else {
        SU_INFO("Failed to open source as audio file, falling back to raw...\n");
      }

    case SUSCAN_SOURCE_FORMAT_RAW:
      source->sf_info.format = SF_FORMAT_RAW | SF_FORMAT_FLOAT | SF_ENDIAN_LITTLE;
      source->sf_info.channels = 2;
      source->sf_info.samplerate = source->config->samp_rate;
      if ((source->sf = sf_open(
          source->config->path,
          SFM_READ,
          &source->sf_info)) == NULL) {
        source->config->samp_rate = source->sf_info.samplerate;
        SU_ERROR(
            "Failed to open %s as raw file: %s\n",
            source->config->path,
            sf_strerror(NULL));
        return SU_FALSE;
      }

      break;
  }

  source->iq_file = source->sf_info.channels == 2;

  return SU_TRUE;
}

SUPRIVATE SUBOOL
suscan_source_open_sdr(suscan_source_t *source)
{
  unsigned int i;

  if ((source->sdr = SoapySDRDevice_make(source->config->soapy_args)) == NULL) {
    SU_ERROR("Failed to open SDR device: %s\n", SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  if (source->config->antenna != NULL)
    if (SoapySDRDevice_setAntenna(
        source->sdr,
        SOAPY_SDR_RX,
        source->config->channel,
        source->config->antenna) != 0) {
      SU_ERROR("Failed to set SDR antenna: %s\n", SoapySDRDevice_lastError());
      return SU_FALSE;
    }

  for (i = 0; i < source->config->gain_count; ++i)
    if (SoapySDRDevice_setGainElement(
        source->sdr,
        SOAPY_SDR_RX,
        source->config->channel,
        source->config->gain_list[i]->key,
        source->config->gain_list[i]->val) != 0)
      SU_WARNING(
          "Failed to set gain `%s' to %gdB, ignoring silently\n",
          source->config->gain_list[i]->key,
          source->config->gain_list[i]->val);


  if (SoapySDRDevice_setFrequency(
      source->sdr,
      SOAPY_SDR_RX,
      source->config->channel,
      source->config->freq,
      NULL) != 0) {
    SU_ERROR(
        "Failed to set SDR frequency: %s\n",
        SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  if (SoapySDRDevice_setBandwidth(
      source->sdr,
      SOAPY_SDR_RX,
      source->config->channel,
      source->config->bandwidth) != 0) {
    SU_ERROR(
        "Failed to set SDR IF bandwidth: %s\n",
        SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  if (SoapySDRDevice_setSampleRate(
      source->sdr,
      SOAPY_SDR_RX,
      source->config->channel,
      source->config->samp_rate) != 0) {
    SU_ERROR(
        "Failed to set sample rate: %s\n",
        SoapySDRDevice_lastError());
    return SU_FALSE;
  }
  /*
   * IQ-balance should be performed automatically. SoapySDR does not support
   * that yet.
   */
  source->soft_iq_balance = source->config->iq_balance;

  if (SoapySDRDevice_hasDCOffsetMode(
      source->sdr,
      SOAPY_SDR_RX,
      source->config->channel)) {
    if (SoapySDRDevice_setDCOffsetMode(
        source->sdr,
        SOAPY_SDR_RX,
        source->config->channel,
        source->config->dc_remove) != 0) {
      SU_ERROR(
          "Failed to set DC offset correction: %s\n",
          SoapySDRDevice_lastError());
      return SU_FALSE;
    }
  } else {
    source->soft_dc_correction = source->config->dc_remove;
  }

  /* All set: open SoapySDR stream */
  source->chan_array[0] = source->config->channel;

  if (SoapySDRDevice_setupStream(
      source->sdr,
      &source->rx_stream,
      SOAPY_SDR_RX,
      SUSCAN_SOAPY_SAMPFMT,
      source->chan_array,
      1,
      NULL) != 0) {
    SU_ERROR(
        "Failed to open RX stream on SDR device: %s\n",
        SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  /* SDR stream opened, activate it */
  if (SoapySDRDevice_activateStream(
      source->sdr,
      source->rx_stream,
      0,
      0,
      0) != 0) {
    SU_ERROR("Failed to activate stream: %s\n", SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  return SU_TRUE;
}

SUPRIVATE SUSCOUNT
suscan_source_read_file(suscan_source_t *source, SUCOMPLEX *buf, SUSCOUNT max)
{
  SUFLOAT *as_real;
  int got;
  unsigned int real_count, i;

  if (max > SUSCAN_SOURCE_DEFAULT_BUFSIZ)
    max = SUSCAN_SOURCE_DEFAULT_BUFSIZ;

  real_count = max * (source->iq_file ? 2 : 1);

  as_real = (SUFLOAT *) buf;

  got = sf_read(source->sf, as_real, real_count);

  if (got == 0 && source->config->loop) {
    if (sf_seek(source->sf, 0, SEEK_SET) == -1) {
      SU_ERROR("Failed to seek to the beginning of the stream\n");
      return 0;
    }

    got = sf_read(source->sf, as_real, real_count);
  }

  if (got > 0) {
    /* Real data mode: iteratively cast to complex */
    if (source->sf_info.channels == 1) {
      for (i = got - 1; i >= 0; --i)
        buf[i] = as_real[i];
    } else {
      got >>= 1;
    }
  }

  return got;
}

SUPRIVATE SUSCOUNT
suscan_source_read_sdr(suscan_source_t *source, SUCOMPLEX *buf, SUSCOUNT max)
{
  int result;

  result = SoapySDRDevice_readStream(
      source->sdr,
      source->rx_stream,
      (void * const*) &buf,
      max,
      NULL,
      NULL,
      0); /* TODO: set timeOut */

  if (result < 0) {
    SU_ERROR(
        "Failed to read samples from stream: %s\n",
        SoapySDRDevice_lastError());
    return SU_FALSE;
  }

  return result;
}

SUSDIFF
suscan_source_read(suscan_source_t *source, SUCOMPLEX *buffer, SUSCOUNT max)
{
  if (source->read == NULL) {
    SU_ERROR("Signal source has no read() operation\n");
    return -1;
  }

  return (source->read) (source, buffer, max);
}

suscan_source_t *
suscan_source_new(suscan_source_config_t *config)
{
  suscan_source_t *new = NULL;

  SU_TRYCATCH(new = calloc(1, sizeof(suscan_source_t)), goto fail);

  SU_TRYCATCH(new->config = suscan_source_config_clone(config), goto fail);

  switch (new->config->type) {
    case SUSCAN_SOURCE_TYPE_FILE:
      SU_TRYCATCH(suscan_source_open_file(new), goto fail);
      new->read = suscan_source_read_file;
      break;

    case SUSCAN_SOURCE_TYPE_SDR:
      SU_TRYCATCH(suscan_source_open_sdr(new), goto fail);
      new->read = suscan_source_read_sdr;
      break;

    default:
      SU_ERROR("Malformed config object\n");
      goto fail;
  }

  return new;

fail:
  if (new != NULL)
    suscan_source_destroy(new);

  return NULL;
}

/*************************** API initialization ******************************/
SUBOOL
suscan_init_sources(void)
{
  SU_TRYCATCH(suscan_confdb_use("sources"), return SU_FALSE);

  return SU_TRUE;
}
