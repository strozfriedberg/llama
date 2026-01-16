// ABOUTME: DuckDB batch type for Extent records
// ABOUTME: Uses boost::pfr reflection for serialization

#pragma once

#include "extent.h"
#include "llamaduck.h"

using ExtentBatch = DBBatch<Extent>;
