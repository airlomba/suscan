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

#ifndef _GUI_INSPECTOR_H
#define _GUI_INSPECTOR_H

#include <object.h>
#include "symsrc.h"

#include "spectrum.h"
#include "codec.h"

#include "constellation.h"
#include "waveform.h"
#include "transmtx.h"
#include "histogram.h"
#include "modemctl.h"
#include "estimatorui.h"

#include <sigutils/decider.h>

#define SUSCAN_GUI_INSPECTOR_SPECTRUM_AGC_ALPHA .5
#define SUSCAN_GUI_INSPECTOR_SPECTRUM_MODE SUGTK_SPECTRUM_MODE_SPECTROGRAM

struct suscan_gui;

#define SUSCAN_GUI_INSPECTOR_AS_SYMSRC(insp) &(insp)->_parent;

struct suscan_gui_inspector {
  struct suscan_gui_symsrc _parent; /* Inherits from symbol source */
  int index; /* Back reference */
  SUHANDLE inshnd; /* Inspector handle (relative to current analyzer) */
  SUBOOL dead; /* Owner analyzer has been destroyed */
  SUBOOL recording; /* Symbol recorder enabled */

  /* Current inspector configuration and cached values */
  char *class; /* Inspector class */
  suscan_config_t *config;
  unsigned int baudrate;
  su_decider_t decider;
  struct sigutils_decider_params decider_params;

  /* GTK Builder */
  GtkBuilder     *builder;

  /* Widgets */
  GtkEventBox         *pageLabelEventBox;
  GtkLabel            *pageLabel;
  GtkGrid             *channelInspectorGrid;
  GtkToggleToolButton *autoScrollToggleButton;
  GtkToggleToolButton *autoFitToggleButton;
  GtkNotebook         *constellationNotebook;
  SuGtkConstellation  *constellation;
  SuGtkWaveForm       *phasePlot;
  SuGtkTransMtx       *transMatrix;
  SuGtkSpectrum       *spectrum;
  SuGtkHistogram      *histogram;
  GtkAlignment        *constellationAlignment;
  GtkAlignment        *transAlignment;
  GtkAlignment        *phasePlotAlignment;
  GtkAlignment        *spectrumAlignment;
  GtkAlignment        *histogramAlignment;
  GtkGrid             *controlsGrid;
  GtkGrid             *estimatorGrid;
  GtkComboBoxText     *spectrumSourceComboBoxText;

  /* Modem controls generated by config */
  struct suscan_gui_modemctl_set modemctl_set;

  /* Estimator UIs */
  PTR_LIST(suscan_gui_estimatorui_t, estimator);

  /* Symbol recorder widgets */
  GtkGrid        *recorderGrid;
  SuGtkSymView   *symbolView;
  GtkSpinButton  *offsetSpinButton;
  GtkSpinButton  *widthSpinButton;
  GtkNotebook    *codecNotebook;
  GtkScrollbar   *symViewScrollbar;
  GtkAdjustment  *symViewScrollAdjustment;

  /* Channel summary */
  GtkLabel       *freqLabel;
  GtkLabel       *bwLabel;
  GtkLabel       *snrLabel;

  /* Progress dialog */
  GtkDialog      *progressDialog;
  GtkProgressBar *progressBar;

  struct sigutils_channel channel;
};

typedef struct suscan_gui_inspector suscan_gui_inspector_t;

SUINLINE unsigned int
suscan_gui_inspector_get_bits(const suscan_gui_inspector_t *insp)
{
  return insp->decider_params.bits;
}

/* Inspector GUI functions */
SUBOOL suscan_gui_inspector_feed_w_batch(
    suscan_gui_inspector_t *inspector,
    const struct timeval *arrival,
    const struct suscan_analyzer_sample_batch_msg *msg);

suscan_gui_inspector_t *suscan_gui_inspector_new(
    const char *class,
    const struct sigutils_channel *channel,
    const suscan_config_t *config,
    SUHANDLE handle);

SUBOOL suscan_gui_inspector_commit_config(suscan_gui_inspector_t *insp);

SUBOOL suscan_gui_inspector_refresh_on_config(suscan_gui_inspector_t *insp);

SUBOOL suscan_gui_inspector_set_config(
    suscan_gui_inspector_t *insp,
    const suscan_config_t *config);

void suscan_gui_inspector_detach(suscan_gui_inspector_t *insp);

void suscan_gui_inspector_close(suscan_gui_inspector_t *insp);

SUBOOL suscan_gui_inspector_populate_codec_menu(
    suscan_gui_inspector_t *inspector,
    SuGtkSymView *view,
    void *(*create_priv) (void *, struct suscan_gui_codec_cfg_ui *),
    void *private,
    GCallback on_encode,
    GCallback on_decode);

SUBOOL suscan_gui_inspector_remove_codec(
    suscan_gui_inspector_t *gui,
    suscan_gui_codec_t *codec);

SUBOOL suscan_gui_inspector_add_codec(
    suscan_gui_inspector_t *inspector,
    suscan_gui_codec_t *codec);

SUBOOL suscan_gui_inspector_add_estimatorui(
    suscan_gui_inspector_t *inspector,
    const struct suscan_estimator_class *class,
    uint32_t estimator_id);

void suscan_gui_inspector_add_spectrum_source(
    suscan_gui_inspector_t *inspector,
    const struct suscan_spectsrc_class *class,
    uint32_t estimator_id);

SUBOOL suscan_gui_inspector_open_codec_tab(
    suscan_gui_inspector_t *inspector,
    struct suscan_gui_codec_cfg_ui *ui,
    unsigned int bits,
    unsigned int direction,
    const SuGtkSymView *view,
    suscan_symbuf_t *source);

suscan_object_t *suscan_gui_inspector_serialize(
    const suscan_gui_inspector_t *inspector);

void suscan_gui_inspector_destroy(suscan_gui_inspector_t *inspector);

#endif /* _GUI_INSPECTOR_H */
