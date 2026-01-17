-- ABOUTME: SQL queries for constructing disk map from extent data
-- ABOUTME: Uses sweep-line algorithm to identify all claimants for each disk region

-- Step 1: Create boundaries table from all extent start and end points
CREATE TEMP TABLE boundaries AS
SELECT DISTINCT PhysicalStart AS pos FROM extents
UNION
SELECT DISTINCT PhysicalEnd AS pos FROM extents
ORDER BY pos;

-- Step 2: Create intervals from consecutive boundaries
CREATE TEMP TABLE intervals AS
SELECT
    pos AS start,
    LEAD(pos) OVER (ORDER BY pos) AS end
FROM boundaries
WHERE LEAD(pos) OVER (ORDER BY pos) IS NOT NULL;

-- Step 3: Create diskmap with claimants using sweep-line algorithm
-- For each interval, find all extents that contain it
CREATE TABLE diskmap AS
SELECT
    i.start AS PhysicalStart,
    i.end AS PhysicalEnd,
    LIST({inode: e.Inode, path: e.Path}) AS Claimants
FROM intervals i
LEFT JOIN extents e
    ON e.PhysicalStart <= i.start
    AND e.PhysicalEnd >= i.end
GROUP BY i.start, i.end
ORDER BY i.start;
