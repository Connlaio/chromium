# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from telemetry import benchmark
from core import perf_benchmark
from core import path_util
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement

from benchmarks.pagesets import media_router_perf_pages
from benchmarks import media_router_measurements
from benchmarks import media_router_timeline_metric


class _BaseCastBenchmark(perf_benchmark.PerfBenchmark):
  options = {'page_repeat': 6}

  page_set = media_router_perf_pages.MediaRouterDialogPageSet

  def SetExtraBrowserOptions(self, options):
    options.clear_sytem_cache_for_browser_and_profile_on_start = True
    # This flag is required to enable the communication between the page and
    # the test extension.
    options.disable_background_networking = False

    # TODO: find a better way to find extension location.
    options.AppendExtraBrowserArgs([
        '--load-extension=' + ','.join([
            os.path.join(path_util.GetChromiumSrcDir(), 'out',
            'Release', 'mr_extension', 'release'),
             os.path.join(path_util.GetChromiumSrcDir(), 'out',
             'Release', 'media_router', 'test_extension')]),
        '--whitelisted-extension-id=enhhojjnijigcajfphajepfemndkmdlo',
        '--media-router=1',
        '--enable-stats-collection-bindings'
    ])


class TraceEventCastBenckmark(_BaseCastBenchmark):

  def CreateTimelineBasedMeasurementOptions(self):
    media_router_category = 'media_router'
    category_filter = tracing_category_filter.TracingCategoryFilter(
        media_router_category)
    category_filter.AddIncludedCategory('blink.console')
    options = timeline_based_measurement.Options(category_filter)
    options.SetLegacyTimelineBasedMetrics([
        media_router_timeline_metric.MediaRouterMetric()])
    return options

  @classmethod
  def Name(cls):
    return 'media_router.dialog.latency.tracing'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    """Only drops the first result."""
    return not is_first_result


class HistogramCastBenckmark(_BaseCastBenchmark):

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterDialogTest()

  @classmethod
  def Name(cls):
    return 'media_router.dialog.latency.histogram'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    """Only drops the first result."""
    return not is_first_result


class CPUMemoryCastBenckmark(_BaseCastBenchmark):
  options = {'page_repeat': 1}

  page_set = media_router_perf_pages.MediaRouterCPUMemoryPageSet

  def CreatePageTest(self, options):
    return media_router_measurements.MediaRouterCPUMemoryTest()

  @classmethod
  def Name(cls):
    return 'media_router.cpu_memory'
