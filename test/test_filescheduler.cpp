// ABOUTME: Tests for FileScheduler::BucketState disk-locality bucketing logic.
// ABOUTME: Verifies bucket distribution, threshold dispatch, priority ordering, and flush behavior.

#include <catch2/catch_test_macros.hpp>

#include "entry.h"
#include "filescheduler.h"

namespace {
  std::unique_ptr<Entry> makeEntry(uint64_t addr, uint64_t diskOffset, uint64_t fileSize) {
    auto e = std::make_unique<Entry>(addr);
    e->DiskOffset = diskOffset;
    e->FileSize = fileSize;
    return e;
  }
}

TEST_CASE("bucketDistributionByDiskOffset") {
  // 1GB filesystem = 4 buckets of 256MB each
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Entry at 100MB should go to bucket 0
  buckets.addToBucket(makeEntry(1, 100 * 1024 * 1024, 1000));
  // Entry at 300MB should go to bucket 1
  buckets.addToBucket(makeEntry(2, 300 * 1024 * 1024, 1000));
  // Entry at 700MB should go to bucket 2
  buckets.addToBucket(makeEntry(3, 700 * 1024 * 1024, 1000));

  REQUIRE(buckets.bucketEntryCount(0) == 1);
  REQUIRE(buckets.bucketEntryCount(1) == 1);
  REQUIRE(buckets.bucketEntryCount(2) == 1);
  REQUIRE(buckets.bucketEntryCount(3) == 0);
}

TEST_CASE("residentFilesGoToResidentBucket") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // DiskOffset == 0 means resident
  buckets.addToBucket(makeEntry(1, 0, 500));
  buckets.addToBucket(makeEntry(2, 0, 500));

  REQUIRE(buckets.residentEntryCount() == 2);
}

TEST_CASE("bucketDispatchesWhenByteThresholdReached") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  REQUIRE(!buckets.hasPendingBatches());

  // Add entries to bucket 0 totaling > 256MB
  uint64_t bytesPerEntry = 128 * 1024 * 1024; // 128MB each
  buckets.addToBucket(makeEntry(1, 10, bytesPerEntry));
  REQUIRE(!buckets.hasPendingBatches());

  buckets.addToBucket(makeEntry(2, 20, bytesPerEntry));
  // 256MB total — should trigger
  REQUIRE(buckets.hasPendingBatches());
}

TEST_CASE("priorityQueueDispatchesLargestFirst") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(2ULL * 1024 * 1024 * 1024); // 2GB

  // Put 200MB in bucket 0 and 400MB in bucket 2 (two entries of 200MB each).
  // Neither individual entry hits 256MB, but bucket 2's pair does.
  constexpr uint64_t MB200 = 200ULL * 1024 * 1024;

  buckets.addToBucket(makeEntry(1, 10, MB200));
  // Bucket 0: 200MB, not yet dispatched

  buckets.addToBucket(makeEntry(2, 512ULL * 1024 * 1024 + 10, MB200));
  // Bucket 2: 200MB, not yet dispatched

  buckets.addToBucket(makeEntry(3, 512ULL * 1024 * 1024 + 20, MB200));
  // Bucket 2: 400MB >= 256MB threshold, dispatched

  REQUIRE(buckets.hasPendingBatches());

  // Flush remaining buckets so we can compare priorities
  buckets.flushAllBuckets();

  // Largest (400MB bucket 2) should come first
  auto batch1 = buckets.popBatch();
  REQUIRE(batch1.TotalBytes == 2 * MB200);
  REQUIRE(batch1.Entries.size() == 2);

  auto batch2 = buckets.popBatch();
  REQUIRE(batch2.TotalBytes == MB200);
  REQUIRE(batch2.Entries.size() == 1);
}

TEST_CASE("flushAllBucketsPushesPartialBuckets") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Add small entries that don't reach threshold
  buckets.addToBucket(makeEntry(1, 10, 1000));
  buckets.addToBucket(makeEntry(2, 300 * 1024 * 1024, 2000));
  buckets.addToBucket(makeEntry(3, 0, 500)); // resident

  REQUIRE(!buckets.hasPendingBatches());

  buckets.flushAllBuckets();

  // All three non-empty buckets should now be pending
  REQUIRE(buckets.hasPendingBatches());

  // Pop all three and verify
  size_t totalEntries = 0;
  while (buckets.hasPendingBatches()) {
    auto batch = buckets.popBatch();
    totalEntries += batch.Entries.size();
  }
  REQUIRE(totalEntries == 3);
}

TEST_CASE("startFilesystemFlushesExistingBuckets") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  buckets.addToBucket(makeEntry(1, 10, 1000));

  // Starting a new filesystem should flush the old bucket
  buckets.startFilesystem(512 * 1024 * 1024);

  REQUIRE(buckets.hasPendingBatches());
  auto batch = buckets.popBatch();
  REQUIRE(batch.Entries.size() == 1);
}

TEST_CASE("largeFileBecomesSingleEntryBatch") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  uint64_t largeSize = 300 * 1024 * 1024; // 300MB > threshold
  buckets.addLargeFile(makeEntry(1, 500 * 1024 * 1024, largeSize));

  REQUIRE(buckets.hasPendingBatches());
  auto batch = buckets.popBatch();
  REQUIRE(batch.Entries.size() == 1);
  REQUIRE(batch.TotalBytes == largeSize);
  REQUIRE(batch.BucketIndex == FileScheduler::LARGE_FILE_BUCKET_INDEX);
}

TEST_CASE("entryBeyondFsSizeGoesToLastBucket") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(512 * 1024 * 1024); // 512MB = 2 buckets

  // Entry at 600MB (beyond fs size) should clamp to last bucket
  buckets.addToBucket(makeEntry(1, 600 * 1024 * 1024, 1000));

  REQUIRE(buckets.bucketEntryCount(1) == 1);
}

TEST_CASE("popBatchReturnsBucketIndex") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Fill bucket 1 past threshold
  buckets.addToBucket(makeEntry(1, 300 * 1024 * 1024, 256 * 1024 * 1024));

  auto batch = buckets.popBatch();
  REQUIRE(batch.BucketIndex == 1);
}

TEST_CASE("residentBucketHasSentinelIndex") {
  FileScheduler::BucketState buckets;
  buckets.startFilesystem(1024 * 1024 * 1024);

  // Fill resident bucket past threshold
  buckets.addToBucket(makeEntry(1, 0, 256 * 1024 * 1024));

  auto batch = buckets.popBatch();
  REQUIRE(batch.BucketIndex == FileScheduler::RESIDENT_BUCKET_INDEX);
}
