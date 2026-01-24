# formatTimestamp() C++20 Rewrite

## Objective

Rewrite `formatTimestamp()` to use C++20 `std::chrono` instead of Boost DateTime, eliminating a dependency and improving performance.

## Motivation

The current implementation is slow and appears in profiling as a hot function (many filesystem timestamps to format). Performance improvements:
- Replace `strftime()` with `std::to_chars` (eliminate locale machinery overhead)
- Replace `std::ostringstream` with fixed-point integer arithmetic for fractional seconds
- Eliminate Boost DateTime overhead

This is the only function in llama using Boost DateTime, so removing it eliminates the dependency entirely.

## Function Signatures

```cpp
// Primary implementation - takes output string for reuse
void formatTimestamp(int64_t unix_time, uint32_t ns, std::string& out);

// Convenience wrapper - returns new string
std::string formatTimestamp(int64_t unix_time, uint32_t ns);
```

The void version does all the work. The returning version creates a temporary, calls the void version, returns it.

**Breaking change:** Removes current signature with `std::ostringstream& buf` parameter. All call sites must be updated.

## Special Cases

- `unix_time == 0 && ns == 0`: Return empty string (null timestamp)
- `unix_time == 0 && ns > 0`: Return "1970-01-01 00:00:00" + fractional
- Out of range dates: Return "1970-01-01 00:00:00" (sentinel for invalid data in forensics)
- `ns >= 1000000000`: Ignore fractional seconds (invalid nanoseconds)

## Implementation Approach

### Date/Time Conversion

1. Convert `int64_t unix_time` to `std::chrono::sys_seconds`
2. Decompose using `std::chrono::year_month_day` for date components
3. Use `std::chrono::hh_mm_ss` for time-of-day components
4. All calendar arithmetic done by C++20 chrono (no OS involvement, no timezone bugs)

### Formatting

1. Pre-reserve string capacity (~30 characters)
2. Use `std::to_chars` for each numeric field (year, month, day, hour, min, sec)
3. Manually construct "YYYY-MM-DD HH:MM:SS" format (ISO-8601 with 'T' elided)
4. For fractional seconds: fixed-point integer arithmetic
   - Convert nanoseconds to string with `std::to_chars`
   - Insert decimal point at correct position
   - Trim trailing zeros (no leading zero after decimal point)

### Performance Characteristics

- No heap allocations in hot path (aside from string growth if pre-reserve insufficient)
- No floating point (pure integer arithmetic)
- No locale machinery
- Native chrono calendar calculations

## Testing Strategy

### Phase 1 - Establish Baseline

1. Write comprehensive unit tests covering:
   - Zero handling (both components zero, only unix_time zero, only ns zero)
   - Positive timestamps (various real-world dates)
   - Negative timestamps (pre-1970, including NTFS epoch at 1601, year 1900)
   - Year boundaries (1969→1970, Y2K, 2038 32-bit overflow)
   - Fractional seconds (trailing zero trimming, various precisions, boundary at 1 billion ns)
   - Leap years (2000, 2020, non-leap 1900)
   - Edge cases from existing tests (real-world timestamps in test_tsktimestamps.cpp)

2. Run tests against current Boost implementation
3. Capture output as expected values (Boost IS the oracle - it was hard-won for portability)
4. Commit baseline tests - these must pass before and after rewrite

### Phase 2 - Performance Benchmarks

1. Write Catch2 benchmarks for:
   - Current Boost implementation
   - New C++20 implementation (with floating-point fractional for comparison)
   - New C++20 implementation (with fixed-point integer fractional)
2. Measure to verify performance improvement
3. Verify fixed-point is faster than floating-point

### Phase 3 - Rewrite

1. Implement C++20 version
2. All baseline tests must pass (identical output to Boost)
3. Benchmarks must show improvement

## Build System Changes

### Files to Modify

1. `configure.ac` (line 47): Change from `AX_CXX_COMPILE_STDCXX_17` to `AX_CXX_COMPILE_STDCXX_20`
2. `meson.build`: Update C++ standard to C++20
3. Remove Boost DateTime dependency from configure.ac (lines 109-112)

### Rollout Approach

1. Write and validate tests with current Boost implementation first
2. Update build system to C++20
3. Run full test suite to catch any C++20-related warnings/errors across codebase
4. Address compiler warnings
5. Rewrite formatTimestamp()
6. Remove Boost DateTime dependency from build system

### Risk Mitigation

**Unknown C++20 upgrade issues:**
- Other parts of codebase may trigger new warnings with C++20
- Dependencies may have C++20 compatibility issues
- We'll discover these when we run the full test suite after build system update

**Error handling during upgrade:**
- Simple warnings (deprecated features, etc.): Fix directly
- Non-trivial issues (API changes, dependency incompatibilities, unclear warnings): STOP and ask Jon for advice
- Don't try to work around or hack fixes for complex upgrade issues

The build system upgrade is a discovery phase - we need to see what breaks before committing to the rewrite.

## Date Range Support

Use `std::chrono`'s native range (Option B), but be aware of common C limits (like `time_t` ranges, potential 32-bit/64-bit differences).

## Success Criteria

1. All unit tests pass with identical output to Boost implementation
2. Benchmarks show measurable performance improvement
3. Boost DateTime dependency removed from build system
4. No regressions in full test suite
5. Clean build with no new compiler warnings
