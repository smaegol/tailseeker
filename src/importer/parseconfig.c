/*
 * parseconfig.c
 *
 * Copyright (c) 2016 Hyeshik Chang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * - Hyeshik Chang <hyeshik@snu.ac.kr>
 */

#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "../contrib/ini.h"
#include "tailseq-import.h"


#define MATCH(n) strcasecmp(name, n) == 0
#define STARTSWITH(n) strncasecmp(name, n, sizeof(n) - 1) == 0

static int
feed_source_entry(struct TailseekerConfig *cfg,
                  const char *name, const char *value)
{
    if (MATCH("data-dir"))
        cfg->datadir = strdup(value);
    else if (MATCH("laneid"))
        cfg->laneid = strdup(value);
    else if (MATCH("lane"))
        cfg->lane = atoi(value);
    else if (MATCH("tile"))
        cfg->tile = atoi(value);
    else if (MATCH("threep-colormatrix"))
        cfg->threep_colormatrix_filename = strdup(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [source].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_read_format_entry(struct TailseekerConfig *cfg,
                       const char *name, const char *value)
{
    if (MATCH("total-cycles"))
        cfg->total_cycles = atoi(value);
    else if (MATCH("fivep-start"))
        cfg->fivep_start = atoi(value) - 1;
    else if (MATCH("fivep-length"))
        cfg->fivep_length = atoi(value);
    else if (MATCH("index-start"))
        cfg->index_start = atoi(value) - 1;
    else if (MATCH("index-length"))
        cfg->index_length = atoi(value);
    else if (MATCH("threep-start"))
        cfg->threep_start = atoi(value) - 1;
    else if (MATCH("threep-length"))
        cfg->threep_length = atoi(value);
    else if (MATCH("threep-seqqual-output-length"))
        cfg->threep_seqqual_output_length = atoi(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [read_format].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_options_entry(struct TailseekerConfig *cfg,
                   const char *name, const char *value)
{
    if (MATCH("keep-no-delimiter")) {
        if (strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0)
            cfg->keep_no_delimiter = 1;
        else if (strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0)
            cfg->keep_no_delimiter = 0;
        else {
            fprintf(stderr, "\"%s\" must be either yes or no.\n", name);
            return -1;
        }
    }
    else if (MATCH("keep-low-quality-balancer")) {
        if (strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0)
            cfg->keep_low_quality_balancer = 1;
        else if (strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0)
            cfg->keep_low_quality_balancer = 0;
        else {
            fprintf(stderr, "\"%s\" must be either yes or no.\n", name);
            return -1;
        }
    }
    else if (MATCH("threads"))
        cfg->threads = atoi(value);
    else if (MATCH("read-buffer-size"))
        cfg->read_buffer_size = (size_t)atoll(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [options].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_output_entry(struct TailseekerConfig *cfg,
                  const char *name, const char *value)
{
    if (MATCH("seqqual"))
        cfg->seqqual_output = strdup(value);
    else if (MATCH("taginfo"))
        cfg->taginfo_output = strdup(value);
    else if (MATCH("signal"))
        cfg->signal_output = strdup(value);
    else if (MATCH("signal-dists"))
        cfg->signal_dists_output = strdup(value);
    else if (MATCH("stats"))
        cfg->stats_output = strdup(value);
    else if (MATCH("length-dists"))
        cfg->length_dists_output = strdup(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [output].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_altcalls_entry(struct TailseekerConfig *cfg,
                    const char *name, const char *value)
{
    struct AlternativeCallInfo *newac;
    int first_cycle;

    first_cycle = atoi(name);
    if (first_cycle <= 0) {
        fprintf(stderr, "Alternative call position is out of range.\n");
        return -1;
    }

    newac = malloc(sizeof(struct AlternativeCallInfo));
    if (newac == NULL) {
        perror("feed_altcalls_entry");
        return -1;
    }

    newac->filename = strdup(value);
    newac->first_cycle = first_cycle - 1;
    newac->reader = NULL;

    if (newac->filename == NULL) {
        perror("feed_altcalls_entry");
        return -1;
    }

    newac->next = cfg->altcalls;
    cfg->altcalls = newac;

    return 0;
}


static int
feed_control_entry(struct TailseekerConfig *cfg,
                   const char *name, const char *value)
{
    if (MATCH("phix-match-name"))
        strncpy(cfg->controlinfo.name, value, CONTROL_NAME_MAX);
    else if (MATCH("phix-match-start"))
        cfg->controlinfo.first_cycle = atoi(value) - 1;
    else if (MATCH("phix-match-length"))
        cfg->controlinfo.read_length = atoi(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [control].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_balancer_entry(struct TailseekerConfig *cfg,
                    const char *name, const char *value)
{
    if (MATCH("start"))
        cfg->balancerparams.start = atoi(value) - 1;
    else if (MATCH("length"))
        cfg->balancerparams.length = atoi(value);
    else if (MATCH("minimum-occurrence"))
        cfg->balancerparams.minimum_occurrence = atoi(value);
    else if (MATCH("num-positive-samples"))
        cfg->balancerparams.num_positive_samples = atoi(value);
    else if (MATCH("num-negative-samples"))
        cfg->balancerparams.num_negative_samples = atoi(value);
    else if (MATCH("minimum-quality"))
        cfg->balancerparams.min_quality = atoi(value);
    else if (MATCH("minimum-qcpass-percent"))
        cfg->balancerparams.min_fraction_passes = atof(value) * 0.01;
    else {
        fprintf(stderr, "Unknown key \"%s\" in [balancer].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_polyA_seeder_entry(struct TailseekerConfig *cfg,
                        const char *name, const char *value)
{
    if (MATCH("seed-trigger-polya-length"))
        cfg->seederparams.seed_trigger_polya_length = atoi(value);
    else if (MATCH("negative-sample-polya-length"))
        cfg->seederparams.negative_sample_polya_length = atoi(value);
    else if (MATCH("max-cctr-scan-left-space"))
        cfg->seederparams.max_cctr_scan_left_space = atoi(value);
    else if (MATCH("max-cctr-scan-right-space"))
        cfg->seederparams.max_cctr_scan_right_space = atoi(value);
    else if (MATCH("required-cdf-contrast"))
        cfg->seederparams.required_cdf_contrast = atof(value);
    else if (MATCH("polya-boundary-pos"))
        cfg->seederparams.polya_boundary_pos = atoi(value);
    else if (MATCH("polya-sampling-gap"))
        cfg->seederparams.polya_sampling_gap = atoi(value);
    else if (MATCH("dist-sampling-bins"))
        cfg->seederparams.dist_sampling_bins = atoi(value);
    else if (MATCH("fair-sampling-fingerprint-length"))
        cfg->seederparams.fair_sampling_fingerprint_length = atoi(value);
    else if (MATCH("fair-sampling-hash-space-size"))
        cfg->seederparams.fair_sampling_hash_space_size = atoi(value);
    else if (MATCH("fair-sampling-max-count"))
        cfg->seederparams.fair_sampling_max_count = atoi(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [polyA_seeder].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_polyA_finder_entry(struct TailseekerConfig *cfg,
                        const char *name, const char *value)
{
    if (MATCH("polyA-weight-T"))
        cfg->finderparams.weights_polyA[(int)'T'] = atoi(value);
    else if (MATCH("polyA-weight-A"))
        cfg->finderparams.weights_polyA[(int)'A'] = atoi(value);
    else if (MATCH("polyA-weight-C"))
        cfg->finderparams.weights_polyA[(int)'C'] = atoi(value);
    else if (MATCH("polyA-weight-G"))
        cfg->finderparams.weights_polyA[(int)'G'] = atoi(value);
    else if (MATCH("polyA-weight-N"))
        cfg->finderparams.weights_polyA[(int)'N'] = atoi(value);
    else if (MATCH("nonA-weight-T"))
        cfg->finderparams.weights_nonA[(int)'T'] = atoi(value);
    else if (MATCH("nonA-weight-A"))
        cfg->finderparams.weights_nonA[(int)'A'] = atoi(value);
    else if (MATCH("nonA-weight-C"))
        cfg->finderparams.weights_nonA[(int)'C'] = atoi(value);
    else if (MATCH("nonA-weight-G"))
        cfg->finderparams.weights_nonA[(int)'G'] = atoi(value);
    else if (MATCH("nonA-weight-N"))
        cfg->finderparams.weights_nonA[(int)'N'] = atoi(value);
    else if (MATCH("minimum-polya-length"))
        cfg->finderparams.min_polya_length = atoi(value);
    else if (MATCH("maximum-modifications"))
        cfg->finderparams.max_terminal_modifications = atoi(value);
    else if (MATCH("signal-analysis-trigger"))
        cfg->finderparams.sigproc_trigger_polya_length = atoi(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [polyA_finder].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_polyA_ruler_entry(struct TailseekerConfig *cfg,
                       const char *name, const char *value)
{
    if (MATCH("dark-cycles-threshold"))
        cfg->rulerparams.dark_cycles_threshold = (float)atof(value);
    else if (MATCH("maximum-dark-cycles"))
        cfg->rulerparams.max_dark_cycles = atoi(value);
    else if (MATCH("t-intensity-k"))
        cfg->t_intensity_k = (float)atof(value);
    else if (MATCH("t-intensity-center"))
        cfg->t_intensity_center = (float)atof(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in [polyA_ruler].\n", name);
        return -1;
    }

    return 0;
}


static int
feed_sample_umi_entry(struct SampleInfo *sample, const char *name,
                      const char *value)
{
    const char *numberpart;
    struct UMIInterval *umi;
    int umino;

    numberpart = strrchr(name, ':');
    if (numberpart == NULL) {
        fprintf(stderr, "umi qualifiers must passed with a number.\n");
        return -1;
    }

    umino = atoi(numberpart + 1);
    if (umino < 1) {
        fprintf(stderr, "umi number must start from 1.\n");
        return -1;
    }

    /* Allocate or extend the UMI spaces */
    if (sample->umi_ranges_count < umino) {
        if (sample->umi_ranges == NULL) {
            sample->umi_ranges = calloc(umino, sizeof(struct UMIInterval));
            if (sample->umi_ranges == NULL) {
                perror("feed_sample_umi_entry");
                return -1;
            }
            memset(sample->umi_ranges, 0, sizeof(struct UMIInterval) * umino);
        }
        else {
            sample->umi_ranges = realloc(sample->umi_ranges,
                                      sizeof(struct UMIInterval) * umino);
            if (sample->umi_ranges == NULL) {
                perror("feed_sample_umi_entry");
                return -1;
            }
            memset(&sample->umi_ranges[sample->umi_ranges_count],
                   0, sizeof(struct UMIInterval) * (umino - sample->umi_ranges_count));
        }

        sample->umi_ranges_count = umino;
    }

    umi = &sample->umi_ranges[umino - 1];

    if (STARTSWITH("umi-start:"))
        umi->start = atoi(value) - 1;
    else if (STARTSWITH("umi-length:"))
        umi->length = atoi(value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in sample %s.\n", name, sample->name);
        return -1;
    }

    return 0;
}


static int
feed_sample_entry(struct TailseekerConfig *cfg, const char *samplename,
                  const char *name, const char *value)
{
    struct SampleInfo *sample;

    for (sample = cfg->samples; sample != NULL; sample = sample->next)
        if (*name != '\0' && strcmp(sample->name, samplename) == 0)
            break;

    if (sample == NULL) {
        sample = malloc(sizeof(struct SampleInfo));
        if (sample == NULL) {
            perror("feed_sample_entry");
            return -1;
        }

        memset(sample, 0, sizeof(struct SampleInfo));
        sample->name = strdup(samplename);
        sample->next = cfg->samples;
        sample->signal_dump_length = 0;
        cfg->samples = sample;
    }

    if (MATCH("index"))
        sample->index = strdup(value);
    else if (MATCH("maximum-index-mismatch"))
        sample->maximum_index_mismatches = atoi(value);
    else if (MATCH("delimiter-seq"))
        sample->delimiter = strdup(value);
    else if (MATCH("delimiter-start"))
        sample->delimiter_pos = atoi(value) - 1;
    else if (MATCH("maximum-delimiter-mismatch"))
        sample->maximum_delimiter_mismatches = atoi(value);
    else if (MATCH("fingerprint-seq"))
        sample->fingerprint = strdup(value);
    else if (MATCH("fingerprint-start"))
        sample->fingerprint_pos = atoi(value) - 1;
    else if (MATCH("maximum-fingerprint-mismatch"))
        sample->maximum_fingerprint_mismatches = atoi(value);
    else if (MATCH("limit-threep-processing"))
        sample->limit_threep_processing = atoi(value);
    else if (STARTSWITH("umi-"))
        return feed_sample_umi_entry(sample, name, value);
    else {
        fprintf(stderr, "Unknown key \"%s\" in sample %s.\n", name, sample->name);
        return -1;
    }

    return 0;
}
#undef STARTSWITH
#undef MATCH

static int
feed_entry(void *user,
           const char *section, const char *name, const char *value)
{
    struct TailseekerConfig *cfg=(struct TailseekerConfig *)user;

#define MATCH(s) strcasecmp(section, s) == 0
    if (MATCH("source"))
        return feed_source_entry(cfg, name, value);
    else if (MATCH("read_format"))
        return feed_read_format_entry(cfg, name, value);
    else if (MATCH("options"))
        return feed_options_entry(cfg, name, value);
    else if (MATCH("output"))
        return feed_output_entry(cfg, name, value);
    else if (MATCH("alternative_calls"))
        return feed_altcalls_entry(cfg, name, value);
    else if (MATCH("control"))
        return feed_control_entry(cfg, name, value);
    else if (MATCH("balancer"))
        return feed_balancer_entry(cfg, name, value);
    else if (MATCH("polyA_seeder"))
        return feed_polyA_seeder_entry(cfg, name, value);
    else if (MATCH("polyA_finder"))
        return feed_polyA_finder_entry(cfg, name, value);
    else if (MATCH("polyA_ruler"))
        return feed_polyA_ruler_entry(cfg, name, value);
    else if (strncasecmp(section, "sample:", 7) == 0)
        return feed_sample_entry(cfg, section + 7, name, value);
    else {
        fprintf(stderr, "Unknown section [%s] in the configuration.",
                        section);
        return -1;
    }
#undef MATCH

    return 0;
}


static void
set_default_configuration(struct TailseekerConfig *cfg)
{
    cfg->lane = cfg->tile = cfg->total_cycles = cfg->index_start =
        cfg->threep_start = cfg->threep_length =
        cfg->fivep_start = cfg->fivep_length = -1;
    cfg->threep_seqqual_output_length = 250;

    cfg->keep_no_delimiter = 0;
    cfg->keep_low_quality_balancer = 0;
    cfg->threads = 1;
    cfg->index_length = 6;

    cfg->read_buffer_size = 536870912; /* 500 MiB */

    cfg->balancerparams.start = 0;
    cfg->balancerparams.end = 20;
    cfg->balancerparams.minimum_occurrence = 2;
    cfg->balancerparams.num_positive_samples = 2;
    cfg->balancerparams.num_negative_samples = 4;
    cfg->balancerparams.min_quality = 25;
    cfg->balancerparams.min_fraction_passes = .70f;

    cfg->seederparams.seed_trigger_polya_length = 10;
    cfg->seederparams.negative_sample_polya_length = 0;
    cfg->seederparams.max_cctr_scan_left_space = 20;
    cfg->seederparams.max_cctr_scan_right_space = 20;
    cfg->seederparams.required_cdf_contrast = .35f;
    cfg->seederparams.polya_boundary_pos = 120;
    cfg->seederparams.polya_sampling_gap = 3;
    cfg->seederparams.dist_sampling_bins = 1000;
    cfg->seederparams.fair_sampling_fingerprint_length = 30;
    cfg->seederparams.fair_sampling_hash_space_size = 1048576;
    cfg->seederparams.fair_sampling_max_count = 5;

    cfg->finderparams.max_terminal_modifications = 20;
    cfg->finderparams.min_polya_length = 5;
    cfg->finderparams.sigproc_trigger_polya_length = 10;
    cfg->finderparams.weights_polyA[(int)'T'] = 2;
    cfg->finderparams.weights_polyA[(int)'A'] = cfg->finderparams.weights_polyA[(int)'C'] =
        cfg->finderparams.weights_polyA[(int)'G'] = -9;
    cfg->finderparams.weights_polyA[(int)'N'] = -1;
    cfg->finderparams.weights_nonA[(int)'A'] = 0;
    cfg->finderparams.weights_nonA[(int)'T'] = -1;
    cfg->finderparams.weights_nonA[(int)'C'] =
        cfg->finderparams.weights_nonA[(int)'G'] = -4;
    cfg->finderparams.weights_nonA[(int)'N'] = 0;

    cfg->rulerparams.dark_cycles_threshold = 10;
    cfg->rulerparams.max_dark_cycles = 5;
    cfg->t_intensity_k = 20.f;
    cfg->t_intensity_center = .75f;
}


static void
compute_derived_values(struct TailseekerConfig *cfg)
{
    struct SampleInfo *sample;
    int longest_umi_length=0;
    int nsamples;

    cfg->balancerparams.min_bases_passes = cfg->balancerparams.length *
                                           cfg->balancerparams.min_fraction_passes;
    cfg->balancerparams.end = cfg->balancerparams.start +
                              cfg->balancerparams.length;

    /* Fix UMI-related values */
    for (sample = cfg->samples; sample != NULL; sample = sample->next) {
        int i, umi_total_length;

        if (sample->delimiter != NULL)
            sample->delimiter_length = strlen(sample->delimiter);

        if (sample->fingerprint != NULL)
            sample->fingerprint_length = strlen(sample->fingerprint);

        umi_total_length = 0;
        for (i = 0; i < sample->umi_ranges_count; i++) {
            struct UMIInterval *umi;

            umi = &sample->umi_ranges[i];
            if (umi->length > 0) {
                umi->end = umi->start + umi->length;
                umi_total_length += umi->length;
            }
        }

        sample->umi_total_length = umi_total_length;
        if (sample->umi_total_length > longest_umi_length)
            longest_umi_length = sample->umi_total_length;
    }

    /* Add fake samples for PhiX control. */
    if (cfg->controlinfo.name[0] != '\0') {
        struct SampleInfo *control;

        control = malloc(sizeof(struct SampleInfo));
        memset(control, 0, sizeof(struct SampleInfo));
        control->name = strdup(cfg->controlinfo.name);

        control->index = malloc(cfg->index_length + 1);
        memset(control->index, 'X', cfg->index_length);
        control->index[cfg->index_length] = '\0';

        control->delimiter = strdup("");
        control->delimiter_pos = -1;
        control->maximum_index_mismatches = cfg->index_length;
        control->maximum_delimiter_mismatches = -1;
        control->fingerprint = strdup("");
        control->next = cfg->samples;
        cfg->controlinfo.barcode = control;

        cfg->samples = control;
    }

    /* Add a fall-back entry for "Unknown" reads. */
    {
        struct SampleInfo *fallback;

        fallback = malloc(sizeof(struct SampleInfo));
        memset(fallback, 0, sizeof(struct SampleInfo));
        fallback->name = strdup("Unknown");

        fallback->index = malloc(cfg->index_length + 1);
        memset(fallback->index, 'X', cfg->index_length);
        fallback->index[cfg->index_length] = '\0';

        fallback->delimiter = strdup("");
        fallback->delimiter_pos = -1;
        fallback->maximum_index_mismatches = cfg->index_length;
        fallback->maximum_delimiter_mismatches = -1;
        fallback->fingerprint = strdup("");
        fallback->next = cfg->samples;

        cfg->samples = fallback;
    }

    /* Compute pre-calculated table for T intensity scores. */
    precalc_score_tables(&cfg->rulerparams, cfg->t_intensity_k, cfg->t_intensity_center);

    /* Initialize stats locks */
    nsamples = 0;
    for (sample = cfg->samples; sample != NULL; sample = sample->next) {
        sample->numindex = nsamples++;
        pthread_mutex_init(&sample->signal_writer_lock, NULL);
        pthread_mutex_init(&sample->statslock, NULL);

        sample->signal_dump_length = cfg->threep_length;
        if (sample->delimiter != NULL)
            sample->signal_dump_length -=
                    sample->delimiter_pos + sample->delimiter_length;
        if (sample->limit_threep_processing > 0 &&
                sample->signal_dump_length > sample->limit_threep_processing)
            sample->signal_dump_length = sample->limit_threep_processing;

        if (sample->delimiter_pos >= 0)
            sample->delimiter_pos += cfg->threep_start;
    }
    cfg->num_samples = nsamples;

    /* Compute maximum write buffer sizes per output entry */
    if (cfg->threep_seqqual_output_length > cfg->threep_length)
        cfg->threep_seqqual_output_length = cfg->threep_length;

    cfg->max_bufsize_seqqual = nsamples * (
            MAX_CLUSTERID_LEN +
            cfg->fivep_length * 2 +
            cfg->threep_seqqual_output_length * 2 +
            6 /* field separators */);
    cfg->max_bufsize_taginfo = nsamples * (
            5 /* tabs and eol */ + 30 /* other fields */ +
            cfg->finderparams.max_terminal_modifications +
            longest_umi_length);

    /* Compute number of entries in a read buffer from the byte size. */
    {
        int memory_footprint_per_entry;
        size_t write_buffer_memory_footprint;

        memory_footprint_per_entry = 2 * cfg->total_cycles + /* 2 bytes for BCL */
                                     8 * cfg->threep_length; /* 8 bytes for CIF */

        write_buffer_memory_footprint = cfg->threads * NUM_CLUSTERS_PER_JOB *
                        (cfg->max_bufsize_seqqual + cfg->max_bufsize_taginfo);

        cfg->read_buffer_entry_count = (cfg->read_buffer_size - write_buffer_memory_footprint)
                                       / memory_footprint_per_entry;
    }
}


static int
check_configuration_requirements(struct TailseekerConfig *cfg)
{
    /* TODO */

    return 0;
}


struct TailseekerConfig *
parse_config(const char *filename)
{
    struct TailseekerConfig *cfg;

    cfg = (struct TailseekerConfig *)malloc(sizeof(*cfg));
    if (cfg == NULL)
        return NULL;

    memset(cfg, 0, sizeof(*cfg));
    set_default_configuration(cfg);

    if (ini_parse(filename, feed_entry, cfg) < 0) {
        fprintf(stderr, "Failed to parse %s.\n", filename);
        free(cfg);
        return NULL;
    }

    compute_derived_values(cfg);
    if (check_configuration_requirements(cfg) < 0) {
        free(cfg);
        return NULL;
    }

    return cfg;
}


#define free_if_not_null(v) if (v != NULL) free(v)
void
free_config(struct TailseekerConfig *cfg)
{
    free_if_not_null(cfg->datadir);
    free_if_not_null(cfg->laneid);

    free_if_not_null(cfg->seqqual_output);
    free_if_not_null(cfg->taginfo_output);
    free_if_not_null(cfg->signal_output);
    free_if_not_null(cfg->signal_dists_output);
    free_if_not_null(cfg->stats_output);
    free_if_not_null(cfg->length_dists_output);
    free_if_not_null(cfg->threep_colormatrix_filename);

    free_if_not_null(cfg->controlinfo.control_seq);

    while (cfg->samples != NULL) {
        struct SampleInfo *bk;

        if (cfg->samples->stream_seqqual != NULL)
            abort();

        pthread_mutex_destroy(&cfg->samples->signal_writer_lock);
        pthread_mutex_destroy(&cfg->samples->statslock);

        free_if_not_null(cfg->samples->name);
        free_if_not_null(cfg->samples->index);
        free_if_not_null(cfg->samples->delimiter);
        free_if_not_null(cfg->samples->fingerprint);
        free_if_not_null(cfg->samples->umi_ranges);
        bk = cfg->samples->next;
        free(cfg->samples);
        cfg->samples = bk;
    }

    while (cfg->altcalls != NULL) {
        struct AlternativeCallInfo *ac;

        free(cfg->altcalls->filename);
        ac = cfg->altcalls->next;
        free(cfg->altcalls);
        cfg->altcalls = ac;
    }

    free(cfg);
}
