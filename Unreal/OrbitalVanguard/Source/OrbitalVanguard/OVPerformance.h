#pragma once

#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

// 기존 상태 판정을 바꾸지 않는 CSV/Insights 계측. 중첩 구간은 합산하지 않는다.
CSV_DECLARE_CATEGORY_EXTERN(OrbitalVanguard);
